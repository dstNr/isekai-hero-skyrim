#include "Progression.h"

#include "System.h"
#include "UI/SystemWindow.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Isekai::Progression {

    namespace {

        // Set true to dump every quest editor ID the game knows to the log. The
        // milestone table below is written against those IDs, so this is how we
        // confirm them against the real game data instead of trusting memory.
        constexpr bool kDumpQuestIDs = true;

        // A quest whose completion the System rewards.
        //
        // Keyed by editor ID, not FormID. Skyrim keeps quest editor IDs at runtime
        // (that is why `setstage MQ105 20` works in the console), so we get load-order
        // independence and DLC support for free, with no FormIDs to get wrong.
        struct Milestone {
            std::uint32_t key;       // stable across releases; never reuse a value
            const char*   editorID;  // e.g. "MQ105"
            const char*   questName; // shown in the System panel
            Passive       passive;
            bool          endpoint;  // pays perk points on top, scaled by the blessing
        };

        using AV = RE::ActorValue;

        // Deliberately limited to actor values that add onto a known base (0, or a
        // flat stat). Multiplier values like kShoutRecoveryMult have a base of 1.0 and
        // would need different handling — worth adding later, not worth guessing now.
        constexpr Milestone kMilestones[] = {
            // --- Main quest ---
            { 1001, "MQ101", "Unbound",                       { "Survivor",       "+25 Health",         AV::kHealth,      25.0f }, false },
            { 1002, "MQ102", "Before the Storm",              { "Wayfarer",       "+25 Carry Weight",   AV::kCarryWeight, 25.0f }, false },
            { 1003, "MQ103", "Bleak Falls Barrow",            { "Tomb Raider",    "+25 Stamina",        AV::kStamina,     25.0f }, false },
            { 1004, "MQ104", "Dragon Rising",                 { "Dragon Slayer",  "+5% Magic Resist",   AV::kResistMagic,  5.0f }, false },
            { 1005, "MQ105", "The Way of the Voice",          { "Voice Wielder",  "+25 Magicka",        AV::kMagicka,     25.0f }, false },
            { 1006, "MQ106", "The Horn of Jurgen Windcaller", { "Tongue",         "+25 Magicka",        AV::kMagicka,     25.0f }, false },
            { 1007, "MQ201", "Diplomatic Immunity",           { "Infiltrator",    "+25 Stamina",        AV::kStamina,     25.0f }, false },
            { 1008, "MQ202", "A Cornered Rat",                { "Shadow Walker",  "+25 Stamina",        AV::kStamina,     25.0f }, false },
            { 1009, "MQ203", "Alduin's Wall",                 { "Loremaster",     "+25 Magicka",        AV::kMagicka,     25.0f }, false },
            { 1010, "MQ204", "Elder Knowledge",               { "Time Reader",    "+25 Magicka",        AV::kMagicka,     25.0f }, false },
            { 1011, "MQ205", "Alduin's Bane",                 { "Time Walker",    "+5% Magic Resist",   AV::kResistMagic,  5.0f }, false },
            { 1012, "MQ206", "The Fallen",                    { "Dragon Trapper", "+25 Health",         AV::kHealth,      25.0f }, false },
            { 1013, "MQ302", "Season Unending",               { "Peacemaker",     "+25 Carry Weight",   AV::kCarryWeight, 25.0f }, false },
            { 1014, "MQ303", "The World-Eater's Eyrie",       { "Skyborn",        "+25 Health",         AV::kHealth,      25.0f }, false },
            { 1015, "MQ304", "Sovngarde",                     { "Soul Walker",    "+50 Health",         AV::kHealth,      50.0f }, false },

            // --- Endpoint: the main quest ---
            { 1016, "MQ305", "Dragonslayer",                  { "World Savior",   "+100 Health",        AV::kHealth,     100.0f }, true },

            // --- Endpoints: the add-ons ---
            { 2001, "DLC1VQ08", "Kindred Judgment",           { "Vampire's Bane", "+15% Magic Resist",  AV::kResistMagic, 15.0f }, true },
            { 2002, "DLC2MQ06", "At the Summit of Apocrypha", { "Miraak's Bane",  "+100 Magicka",       AV::kMagicka,    100.0f }, true },
        };

        // editorID -> quest, built once. Empty until Install() runs.
        std::unordered_map<std::string, RE::TESQuest*> g_quests;

        [[nodiscard]] bool AlreadyGranted(std::uint32_t a_key) {
            const auto& granted = GetState().grantedMilestones;
            return std::find(granted.begin(), granted.end(), a_key) != granted.end();
        }

        [[nodiscard]] const Milestone* FindByQuest(const RE::TESQuest* a_quest) {
            if (!a_quest) {
                return nullptr;
            }
            const std::string_view editorID = a_quest->GetFormEditorID();
            for (const auto& m : kMilestones) {
                if (editorID == m.editorID) {
                    return &m;
                }
            }
            return nullptr;
        }

        void ApplyPassive(const Passive& a_passive) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* avOwner = player ? player->AsActorValueOwner() : nullptr;
            if (!avOwner) {
                return;
            }
            const float base = avOwner->GetBaseActorValue(a_passive.actorValue);
            avOwner->SetBaseActorValue(a_passive.actorValue, base + a_passive.amount);
        }

        // Hand out a milestone's rewards. Main thread only.
        // Returns false if it was already paid out.
        bool Grant(const Milestone& a_milestone) {
            if (AlreadyGranted(a_milestone.key)) {
                return false;
            }
            GetState().grantedMilestones.push_back(a_milestone.key);

            ApplyPassive(a_milestone.passive);

            std::int32_t perks = 0;
            if (a_milestone.endpoint) {
                perks = MilestonePerkPoints();
                GrantPerkPoints(perks);
            }

            logger::info("Milestone granted: {} — passive '{}' ({}), perks +{}",
                         a_milestone.questName, a_milestone.passive.name,
                         a_milestone.passive.effect, perks);
            return true;
        }

        std::string RewardText(const Milestone& a_milestone, std::int32_t a_perks) {
            std::string text = "QUEST COMPLETE\n\n  ";
            text += a_milestone.questName;
            text += "\n\nREWARDS\n\n  Passive   ";
            text += a_milestone.passive.name;
            text += "\n            ";
            text += a_milestone.passive.effect;
            if (a_perks > 0) {
                text += "\n\n  Perk Points   +" + std::to_string(a_perks);
            }
            return text;
        }

        class QuestWatcher : public RE::BSTEventSink<RE::TESQuestStageEvent> {
        public:
            static QuestWatcher* GetSingleton() {
                static QuestWatcher singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESQuestStageEvent*                a_event,
                RE::BSTEventSource<RE::TESQuestStageEvent>*) override {
                if (!a_event) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(a_event->formID);
                const auto* milestone = FindByQuest(quest);

                // Deliberately asking the engine whether the quest is done, rather than
                // matching a hard-coded completion stage number — one less thing to get
                // wrong, and it survives quests that finish on different stages.
                if (!milestone || !quest->IsCompleted() || AlreadyGranted(milestone->key)) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                const std::int32_t perks = milestone->endpoint ? MilestonePerkPoints() : 0;
                if (Grant(*milestone)) {
                    UI::ShowSystemWindow("[ SYSTEM ]", RewardText(*milestone, perks),
                                         { "ACCEPT" }, [](int) {});
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            QuestWatcher() = default;
        };
    }

    void Install() {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            logger::error("Progression: no data handler — quest tracking is off");
            return;
        }

        for (auto* quest : data->GetFormArray<RE::TESQuest>()) {
            if (!quest) {
                continue;
            }
            if (const char* id = quest->GetFormEditorID(); id && *id) {
                g_quests.emplace(id, quest);
            }
        }
        logger::info("Progression: indexed {} quests by editor ID", g_quests.size());

        if constexpr (kDumpQuestIDs) {
            for (const auto& [id, quest] : g_quests) {
                if (id.starts_with("MQ") || id.starts_with("DLC1") || id.starts_with("DLC2")) {
                    logger::info("  quest: {} = \"{}\"", id, quest->GetName());
                }
            }
        }

        // Loudly flag any milestone whose quest we cannot find: a typo'd editor ID
        // would otherwise just sit there silently rewarding nothing, forever.
        for (const auto& m : kMilestones) {
            if (!g_quests.contains(m.editorID)) {
                logger::warn("Progression: milestone '{}' has no quest '{}' — it will never fire",
                             m.questName, m.editorID);
            }
        }

        if (auto* source = RE::ScriptEventSourceHolder::GetSingleton()) {
            source->AddEventSink<RE::TESQuestStageEvent>(QuestWatcher::GetSingleton());
            logger::info("Progression: quest watcher armed");
        }
    }

    void CatchUpOnLoad() {
        std::vector<const Milestone*> caughtUp;
        std::int32_t                  perks = 0;

        for (const auto& m : kMilestones) {
            if (AlreadyGranted(m.key)) {
                continue;
            }
            const auto it = g_quests.find(m.editorID);
            if (it == g_quests.end() || !it->second->IsCompleted()) {
                continue;
            }
            if (m.endpoint) {
                perks += MilestonePerkPoints();
            }
            if (Grant(m)) {
                caughtUp.push_back(&m);
            }
        }

        if (caughtUp.empty()) {
            return;
        }

        // One summary rather than a stack of windows — on an existing save this could
        // otherwise be a dozen popups in a row.
        std::string body = "SYNCHRONISING...\n\nThe System has recognised deeds already done.\n\n";
        for (const auto* m : caughtUp) {
            body += "  " + std::string(m->passive.name) + "   " + m->passive.effect + "\n";
        }
        if (perks > 0) {
            body += "\n  Perk Points   +" + std::to_string(perks);
        }

        logger::info("Progression: caught up on {} milestone(s) already completed",
                     caughtUp.size());
        UI::ShowSystemWindow("[ SYSTEM ]", body, { "ACCEPT" }, [](int) {});
    }

    std::vector<const Passive*> EarnedPassives() {
        std::vector<const Passive*> earned;
        for (const auto& m : kMilestones) {
            if (AlreadyGranted(m.key)) {
                earned.push_back(&m.passive);
            }
        }
        return earned;
    }
}
