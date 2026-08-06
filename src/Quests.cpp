#include "Quests.h"

#include "Sounds.h"
#include "System.h"

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
            std::int32_t  count;    // how many to slay
            std::int32_t  reward;   // System Points at the NORMAL reward scale
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
        constexpr Quarry kQuarries[] = {
            { 1, "Draugr",          "ActorTypeUndead",  25, 4 },
            { 2, "wild beasts",     "ActorTypeAnimal",  20, 3 },
            { 4, "Dwarven automata","ActorTypeDwarven", 12, 5 },
            { 5, "Daedra",          "ActorTypeDaedra",  10, 6 },
            { 6, "giants",          "ActorTypeGiant",    5, 8 },
            { 7, "dragons",         "ActorTypeDragon",   3, 12 },
            { 8, "trolls",          "ActorTypeTroll",    8, 5 },
        };

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

        // Pick a quarry other than the one just finished, so the System does not hand
        // back the same job twice in a row.
        void Roll(std::uint32_t a_avoidKey) {
            static std::mt19937 rng{ std::random_device{}() };

            std::vector<const Quarry*> pool;
            for (const auto& q : kQuarries) {
                if (q.key != a_avoidKey) {
                    pool.push_back(&q);
                }
            }
            if (pool.empty()) {
                return;  // only possible with a one-entry table
            }
            std::uniform_int_distribution<std::size_t> pick(0, pool.size() - 1);
            const auto* chosen = pool[pick(rng)];

            auto& state = GetState();
            state.questKey = chosen->key;
            state.questProgress = 0;

            logger::info("Quests: new objective — slay {} {}", chosen->count, chosen->name);
            const std::string msg = "[ SYSTEM ] New objective: slay " +
                                    std::to_string(chosen->count) + " " + chosen->name + ".";
            RE::DebugNotification(msg.c_str());
        }

        void Complete(const Quarry& a_quarry) {
            const auto payout =
                std::max(1, static_cast<std::int32_t>(std::lround(
                                static_cast<float>(a_quarry.reward) * RewardScale())));
            GrantSystemPoints(payout);
            Sounds::Play(Sounds::Sfx::LevelUp);
            logger::info("Quests: objective complete ({}) — paid {} System Point(s)",
                         a_quarry.name, payout);
            const std::string msg =
                "[ SYSTEM ] Objective complete.  +" + std::to_string(payout) + " System Points.";
            RE::DebugNotification(msg.c_str());

            Roll(a_quarry.key);  // straight into the next one; never leaves you idle
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
                if (state.questProgress >= quarry->count) {
                    // Copy first: Complete() rolls the next objective, which invalidates
                    // what `quarry` describes.
                    const Quarry finished = *quarry;
                    Complete(finished);
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
        // Also covers an objective whose quarry no longer exists (a save from a build
        // whose table listed something this one dropped) — Find returns null, so we roll.
        if (state.questKey == 0 || !Find(state.questKey)) {
            Roll(0);
        }
    }

    bool Active() {
        return GetState().reincarnated && Current() != nullptr;
    }

    std::string Text() {
        const auto* q = Current();
        if (!q) {
            return {};
        }
        return "Slay " + std::to_string(q->count) + " " + q->name;
    }

    std::int32_t Progress() {
        return Current() ? GetState().questProgress : 0;
    }

    std::int32_t Target() {
        const auto* q = Current();
        return q ? q->count : 0;
    }

    std::int32_t Reward() {
        const auto* q = Current();
        if (!q) {
            return 0;
        }
        return std::max(1, static_cast<std::int32_t>(
                               std::lround(static_cast<float>(q->reward) * RewardScale())));
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
