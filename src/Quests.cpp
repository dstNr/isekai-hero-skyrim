#include "Quests.h"

#include "Sounds.h"
#include "System.h"
#include "UI/Toast.h"

#include <algorithm>
#include <random>

namespace Isekai::Quests {

    namespace {
        // A quarry the System can send you after.
        //
        // Targets are matched by KEYWORD, not by race. Vanilla tags every actor with an
        // ActorType* keyword, and creature mods follow suit, so a mod-added draugr counts
        // for "Slay Draugr" without us knowing it exists — the same semantic-filter
        // approach the skill tree's knowledge unlocks use. A race list would go stale the
        // moment anyone installs a creature pack.
        struct Quarry {
            std::uint32_t key;      // stable across releases; NEVER reuse a value —
                                    // a live objective in someone's save refers to it
            const char*   name;     // "Draugr", used in the objective sentence
            const char*   keyword;  // ActorType* editor ID, matched on the dying actor
            std::int32_t  count;    // base count, before the level scaling below
            std::int32_t  reward;   // System Points at the NORMAL reward scale, before scaling
            std::uint16_t minLevel; // the System does not send a level-3 character after dragons
        };

        // Counts and rewards are deliberately modest: this is meant to tick over in the
        // background of normal play, not to become the main way points are earned. The
        // common quarries are cheap and quick; the rare ones pay more because you cannot
        // go and farm them on demand.
        // Key 3 is RETIRED, not reused: it was "Falmer" on "ActorTypeFalmer", a keyword
        // that does not exist. Skyrim tags exactly fourteen ActorType* keywords and Falmer
        // is not one of them — they are ActorTypeNPC like any other humanoid, which is
        // useless here (it would count townspeople). The objective could therefore never
        // be completed by anyone who drew it. A save still holding key 3 finds no quarry,
        // and EnsureObjective rolls a fresh one on the next load.
        //
        // Every keyword below was read out of the masters themselves (see the allowed set
        // in tools/check.mjs, which now fails on an invented one). Do not add one from
        // memory: a wrong keyword is invisible until someone draws that objective and
        // then quietly never finishes it.
        //
        // minLevel is what ties the hunt to the character rather than to the calendar.
        // Skyrim level-scales most of its world, so a quarry is not "hard" by itself —
        // what makes it wrong for a level-4 character is being told to find three dragons
        // before the main quest has produced one. The gates are about REACHABILITY.
        constexpr Quarry kQuarries[] = {
            { 2, "wild beasts",     "ActorTypeAnimal",  20, 3,   1 },
            { 1, "Draugr",          "ActorTypeUndead",  25, 4,   1 },
            { 8, "trolls",          "ActorTypeTroll",    8, 5,  10 },
            { 4, "Dwarven automata","ActorTypeDwarven", 12, 5,  12 },
            { 5, "Daedra",          "ActorTypeDaedra",  10, 6,  15 },
            { 6, "giants",          "ActorTypeGiant",    5, 8,  20 },
            { 7, "dragons",         "ActorTypeDragon",   3, 12, 25 },
        };

        // How long the System waits after one objective before offering the next, in game
        // days. Objectives used to be permanent — finish one, the next was already there —
        // which made them a chore list rather than something the System hands you.
        constexpr float kCooldownDays = 1.0f;

        // Count and payout grow with the character. Both come off the same factor so a
        // bigger hunt is always worth proportionally more; without that, levelling would
        // quietly make objectives worse value.
        //
        // +2% per level, so level 50 is 2x the base and the curve never runs away. The
        // result is SNAPSHOTTED into the objective (State::questTarget/questReward) —
        // recomputing it live would move the finish line as you level mid-hunt.
        [[nodiscard]] float LevelFactor() {
            std::uint16_t level = 1;
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                level = player->GetLevel();
            }
            return 1.0f + static_cast<float>(level) * 0.02f;
        }

        [[nodiscard]] std::uint16_t PlayerLevel() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            return player ? player->GetLevel() : 1;
        }

        [[nodiscard]] float GameDays() {
            auto* calendar = RE::Calendar::GetSingleton();
            return calendar ? calendar->GetCurrentGameTime() : 0.0f;
        }

        // One toast key for the whole feature, so the progress counter replaces itself
        // rather than stacking one line per kill.
        constexpr const char* kToastKey = "quest";

        [[nodiscard]] const Quarry* Find(std::uint32_t a_key) {
            for (const auto& q : kQuarries) {
                if (q.key == a_key) {
                    return &q;
                }
            }
            return nullptr;  // an objective from a future version, or a removed quarry
        }

        [[nodiscard]] const Quarry* Current() {
            const auto key = GetState().questKey;
            return key == 0 ? nullptr : Find(key);
        }

        // Pick a quarry other than the one just finished, from those the character is far
        // enough along to actually go and find.
        void Roll(std::uint32_t a_avoidKey) {
            static std::mt19937 rng{ std::random_device{}() };

            const auto level = PlayerLevel();

            std::vector<const Quarry*> pool;
            for (const auto& q : kQuarries) {
                if (q.key != a_avoidKey && level >= q.minLevel) {
                    pool.push_back(&q);
                }
            }
            // Below level 10 only two quarries are open, so "not the same one twice"
            // cannot always be honoured. Repeating beats handing out nothing.
            if (pool.empty()) {
                for (const auto& q : kQuarries) {
                    if (level >= q.minLevel) {
                        pool.push_back(&q);
                    }
                }
            }
            if (pool.empty()) {
                return;  // only possible if every quarry is gated above level 1
            }
            std::uniform_int_distribution<std::size_t> pick(0, pool.size() - 1);
            const auto* chosen = pool[pick(rng)];

            const float factor = LevelFactor();
            auto&       state = GetState();
            state.questKey = chosen->key;
            state.questProgress = 0;
            state.questTarget = std::max(
                1, static_cast<std::int32_t>(std::lround(chosen->count * factor)));
            state.questReward = std::max(
                1, static_cast<std::int32_t>(
                       std::lround(static_cast<float>(chosen->reward) * factor * RewardScale())));
            state.questNextDue = 0.0f;  // one is live now; the clock restarts on completion

            logger::info("Quests: new objective — slay {} {} (level {}, pays {})",
                         state.questTarget, chosen->name, level, state.questReward);
            UI::ShowToastBanner("[ SYSTEM ]  New objective:  slay " +
                                    std::to_string(state.questTarget) + " " + chosen->name,
                                kToastKey);
        }

        void Complete(const Quarry& a_quarry) {
            auto&      state = GetState();
            const auto payout = std::max(1, state.questReward);
            GrantSystemPoints(payout);
            Sounds::Play(Sounds::Sfx::LevelUp);
            logger::info("Quests: objective complete ({}) — paid {} System Point(s)",
                         a_quarry.name, payout);
            UI::ShowToastBanner("[ SYSTEM ]  Objective complete.  +" + std::to_string(payout) +
                                    " System Points",
                                kToastKey);

            // Stand down rather than roll straight into the next one. The System offering
            // work on its own schedule is the point; an objective that regenerates the
            // instant it is finished is a chore list.
            state.questKey = 0;
            state.questProgress = 0;
            state.questTarget = 0;
            state.questReward = 0;
            state.questNextDue = GameDays() + kCooldownDays;
        }

        // Does this death count toward the objective?
        //
        // The killer must be the player or one of their teammates (followers, summons,
        // a reanimated thrall). Strict "player only" reads as broken in play: a poison,
        // a rune, or a follower landing the final blow would all silently not count,
        // which is most of a mage's or a sneak's kills. Counting every nearby death
        // instead would fill the objective on its own during a dragon attack on a town.
        [[nodiscard]] bool CountsForPlayer(const RE::TESDeathEvent* a_event) {
            auto* killer = a_event->actorKiller ? a_event->actorKiller->As<RE::Actor>() : nullptr;
            if (!killer) {
                return false;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            return killer == player || killer->IsPlayerTeammate();
        }

        class DeathWatcher : public RE::BSTEventSink<RE::TESDeathEvent> {
        public:
            static DeathWatcher* GetSingleton() {
                static DeathWatcher singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESDeathEvent* a_event,
                RE::BSTEventSource<RE::TESDeathEvent>*) override {
                // TESDeathEvent fires twice per actor (dying, then dead). Take only the
                // second, or every kill would count double.
                if (!a_event || !a_event->dead || !a_event->actorDying) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const auto* quarry = Current();
                if (!quarry || !GetState().reincarnated) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                auto* dying = a_event->actorDying->As<RE::Actor>();
                if (!dying || dying == RE::PlayerCharacter::GetSingleton()) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (!CountsForPlayer(a_event)) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                // The keyword sits on the actor's base/race, and HasKeywordString walks
                // both — one call rather than us picking the right owner by hand.
                if (!dying->HasKeywordString(quarry->keyword)) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                auto& state = GetState();
                ++state.questProgress;
                if (state.questProgress >= state.questTarget) {
                    // Copy first: Complete() clears the objective, which invalidates what
                    // `quarry` describes.
                    const Quarry finished = *quarry;
                    Complete(finished);
                } else {
                    // The running tally, the way an MMO reports it: one line, top centre,
                    // replacing itself on every kill rather than stacking.
                    UI::ShowToast(std::string(quarry->name) + "   " +
                                      std::to_string(state.questProgress) + " / " +
                                      std::to_string(state.questTarget),
                                  kToastKey);
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            DeathWatcher() = default;
        };
    }

    void EnsureObjective() {
        auto& state = GetState();
        if (!state.reincarnated) {
            return;  // no System bound yet, no work handed out
        }

        // An objective whose quarry no longer exists (a save from a build whose table
        // listed something this one dropped): drop it and let the clock hand out the next.
        if (state.questKey != 0 && !Find(state.questKey)) {
            logger::info("Quests: objective {} is no longer a quarry — retiring it",
                         state.questKey);
            state.questKey = 0;
            state.questProgress = 0;
            state.questNextDue = 0.0f;
        }

        // A v11 save carries an objective with no snapshotted numbers, because they did
        // not exist yet. Size it now against the character it belongs to.
        if (state.questKey != 0 && state.questTarget <= 0) {
            if (const auto* q = Find(state.questKey)) {
                const float factor = LevelFactor();
                state.questTarget =
                    std::max(1, static_cast<std::int32_t>(std::lround(q->count * factor)));
                state.questReward = std::max(
                    1, static_cast<std::int32_t>(
                           std::lround(static_cast<float>(q->reward) * factor * RewardScale())));
                state.questProgress = std::min(state.questProgress, state.questTarget - 1);
                logger::info("Quests: sized a pre-v12 objective to {} / {}", state.questProgress,
                             state.questTarget);
            }
        }

        Tick();
    }

    void Tick() {
        auto& state = GetState();
        if (!state.reincarnated || state.questKey != 0) {
            return;  // not bound, or one is already running
        }
        // 0 means "the moment one is wanted" — a fresh character, or a save from before
        // the cooldown existed. Anything else is a game-time deadline.
        if (state.questNextDue > 0.0f && GameDays() < state.questNextDue) {
            return;
        }
        Roll(0);
    }

    bool Active() {
        return GetState().reincarnated && Current() != nullptr;
    }

    std::string Text() {
        const auto* q = Current();
        if (!q) {
            return {};
        }
        // Target/reward come from the snapshot, not the table: the table's numbers are
        // pre-scaling, so reading them here would advertise a different hunt from the one
        // the kill counter is actually measuring.
        return "Slay " + std::to_string(GetState().questTarget) + " " + q->name;
    }

    std::int32_t Progress() {
        return Current() ? GetState().questProgress : 0;
    }

    std::int32_t Target() {
        return Current() ? GetState().questTarget : 0;
    }

    std::int32_t Reward() {
        return Current() ? GetState().questReward : 0;
    }

    float DaysUntilNext() {
        const auto& state = GetState();
        if (state.questKey != 0 || state.questNextDue <= 0.0f) {
            return 0.0f;
        }
        return std::max(0.0f, state.questNextDue - GameDays());
    }

    void Reroll() {
        const auto* q = Current();
        Roll(q ? q->key : 0);
    }

    std::vector<std::pair<const char*, const char*>> QuarryKeywords() {
        std::vector<std::pair<const char*, const char*>> out;
        out.reserve(std::size(kQuarries));
        for (const auto& q : kQuarries) {
            out.emplace_back(q.name, q.keyword);
        }
        return out;
    }

    void Install() {
        if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
            holder->AddEventSink<RE::TESDeathEvent>(DeathWatcher::GetSingleton());
            logger::info("Quests: watching kills for System objectives");
        } else {
            logger::error("Quests: no event source holder — objectives will not progress");
        }
    }
}
