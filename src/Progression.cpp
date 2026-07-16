#include "Progression.h"

#include "Passives.h"
#include "Sounds.h"
#include "Storage.h"
#include "System.h"
#include "UI/Input.h"
#include "UI/LevelUpEffect.h"
#include "UI/SkillTreeWindow.h"
#include "UI/SystemWindow.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Isekai::Progression {

    namespace {

        // Set true to dump every quest editor ID the game knows to the log. That dump
        // is how the table below was verified against real game data — and it was worth
        // it: from MQ204 on, the chain is shifted by one from what it looks like
        // (MQ206 is Alduin's Bane, not The Fallen), and the Horn of Jurgen Windcaller
        // is not MQ106 at all but a sub-quest, MQ105Ustengrav.
        constexpr bool kDumpQuestIDs = false;

        // Editor ID prefixes worth dumping: main quest, factions, civil war, add-ons.
        constexpr std::string_view kDumpPrefixes[] = { "MQ", "C0", "MG", "TG", "DB",
                                                       "CW", "DLC1", "DLC2" };

        // A quest whose completion the System rewards.
        //
        // Keyed by editor ID, not FormID. Skyrim keeps quest editor IDs at runtime
        // (that is why `setstage MQ105 20` works in the console), so we get load-order
        // independence and DLC support for free, with no FormIDs to get wrong.
        struct Milestone {
            std::uint32_t key;         // stable across releases; never reuse a value
            const char*   editorID;    // e.g. "MQ105"
            const char*   questName;   // shown in the System panel
            Passive       passive;
            std::int32_t  dragonSouls; // 0 for most; the big beats pay souls
            bool          endpoint;    // pays perk points on top, scaled by the blessing
        };

        using AV = RE::ActorValue;

        // Amounts here are the NORMAL baseline. RewardScale() multiplies them by the
        // blessing (HERO x2, ASCENDED x4), so the table stays readable and the choice
        // made at the start keeps compounding for the whole run.
        //
        // Actor values are deliberately limited to ones that add onto a known base
        // (0, or a flat stat). Multiplier values like kShoutRecoveryMult have a base of
        // 1.0 and would need different handling — worth adding later, not worth guessing.
        //
        // Keys start at 1101, not 1001: an earlier build shipped a table whose editor
        // IDs were mismapped, and a save from it may already hold keys 1001-1016
        // against the wrong quests. Starting fresh means those stale keys match
        // nothing and simply lapse, rather than silently suppressing a real reward.
        //
        //  key    editor ID          quest name                       { passive: name, stat, actor value, base amount, is % }  souls  endpoint
        constexpr Milestone kMilestones[] = {
            // --- Main quest (names verified against the game's own quest table) ---
            { 1101, "MQ101",          "Unbound",                       { "Survivor", "Health", AV::kHealth, 25.0f, false },  0, false },
            { 1102, "MQ102",          "Before the Storm",              { "Wayfarer", "Carry Weight", AV::kCarryWeight, 25.0f, false },  0, false },
            { 1103, "MQ103",          "Bleak Falls Barrow",            { "Tomb Raider", "Stamina", AV::kStamina, 25.0f, false },  0, false },
            { 1104, "MQ104",          "Dragon Rising",                 { "Dragon Slayer", "Magic Resist", AV::kResistMagic, 5.0f, true },  1, false },
            { 1105, "MQ105",          "The Way of the Voice",          { "Voice Wielder", "Magicka", AV::kMagicka, 25.0f, false },  1, false },
            { 1106, "MQ105Ustengrav", "The Horn of Jurgen Windcaller", { "Horn Bearer", "Stamina", AV::kStamina, 25.0f, false },  0, false },
            { 1107, "MQ106",          "A Blade in the Dark",           { "Blade in the Dark", "Health", AV::kHealth, 25.0f, false },  1, false },
            { 1108, "MQ201",          "Diplomatic Immunity",           { "Infiltrator", "Stamina", AV::kStamina, 25.0f, false },  0, false },
            { 1109, "MQ202",          "A Cornered Rat",                { "Shadow Walker", "Carry Weight", AV::kCarryWeight, 25.0f, false },  0, false },
            { 1110, "MQ203",          "Alduin's Wall",                 { "Loremaster", "Magicka", AV::kMagicka, 25.0f, false },  1, false },
            { 1111, "MQ204",          "The Throat of the World",       { "Skyborn", "Magic Resist", AV::kResistMagic, 5.0f, true },  1, false },
            { 1112, "MQ205",          "Elder Knowledge",               { "Time Reader", "Magicka", AV::kMagicka, 25.0f, false },  1, false },
            { 1113, "MQ206",          "Alduin's Bane",                 { "Time Walker", "Magic Resist", AV::kResistMagic, 5.0f, true },  2, false },
            { 1114, "MQ301",          "The Fallen",                    { "Dragon Trapper", "Health", AV::kHealth, 25.0f, false },  2, false },
            { 1115, "MQ302",          "Season Unending",               { "Peacemaker", "Carry Weight", AV::kCarryWeight, 25.0f, false },  0, false },
            { 1116, "MQ303",          "The World-Eater's Eyrie",       { "Sky Breaker", "Health", AV::kHealth, 25.0f, false },  3, false },
            { 1117, "MQ304",          "Sovngarde",                     { "Soul Walker", "Health", AV::kHealth, 50.0f, false },  3, false },

            // --- Endpoint: the main quest ---
            { 1118, "MQ305",          "Dragonslayer",                  { "World Savior", "Health", AV::kHealth, 100.0f, false }, 10, true },

            // --- Dawnguard ---
            { 2201, "DLC1VQ01",        "Awakening",                    { "Dawnguard Recruit", "Health", AV::kHealth, 15.0f, false },  0, false },
            { 2202, "DLC1VQ02",        "Bloodline",                    { "Bloodline", "Health", AV::kHealth, 15.0f, false },  0, false },
            // The Prophet branches by allegiance; whichever the player walks, only one fires.
            { 2203, "DLC1VQ03Hunter",  "Prophet",                      { "Prophet's Ally", "Magicka", AV::kMagicka, 15.0f, false },  0, false },
            { 2204, "DLC1VQ03Vampire", "Prophet",                      { "Prophet's Ally", "Magicka", AV::kMagicka, 15.0f, false },  0, false },
            { 2205, "DLC1VQ04",        "Chasing Echoes",               { "Echo Chaser", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 2206, "DLC1VQ05",        "Beyond Death",                 { "Soul Cairn Walker", "Magicka", AV::kMagicka, 25.0f, false },  1, false },
            { 2207, "DLC1VQ06",        "Unseen Visions",               { "Seer", "Magicka", AV::kMagicka, 15.0f, false },  0, false },
            { 2208, "DLC1VQ07",        "Touching the Sky",             { "Sky Toucher", "Health", AV::kHealth, 25.0f, false },  1, false },
            { 2209, "DLC1VQDragon",    "Durnehviir",                   { "Soul Binder", "Magicka", AV::kMagicka, 15.0f, false },  2, false },

            // --- Endpoint: Dawnguard ---
            { 2101, "DLC1VQ08",        "Kindred Judgment",             { "Vampire's Bane", "Magic Resist", AV::kResistMagic, 15.0f, true },  5, true },

            // --- Dragonborn ---
            { 2301, "DLC2MQ01",        "Dragonborn",                   { "Marked", "Health", AV::kHealth, 15.0f, false },  0, false },
            { 2302, "DLC2MQ02",        "The Temple of Miraak",         { "Temple Delver", "Magicka", AV::kMagicka, 15.0f, false },  0, false },
            { 2303, "DLC2MQ03",        "The Fate of the Skaal",        { "Skaal-Friend", "Health", AV::kHealth, 15.0f, false },  0, false },
            { 2304, "DLC2MQ03B",       "Cleansing the Stones",         { "Stone Cleanser", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 2305, "DLC2MQ04",        "The Path of Knowledge",        { "Knowledge Seeker", "Magicka", AV::kMagicka, 25.0f, false },  1, false },
            { 2306, "DLC2MQ05",        "The Gardener of Men",          { "Apocrypha Walker", "Magicka", AV::kMagicka, 25.0f, false },  2, false },

            // --- Endpoint: Dragonborn ---
            { 2102, "DLC2MQ06",        "At the Summit of Apocrypha",   { "Miraak's Bane", "Magicka", AV::kMagicka, 100.0f, false }, 10, true },

            // --- The Companions ---
            { 3101, "C00",             "Take Up Arms",                 { "Whelp", "Health", AV::kHealth, 15.0f, false },  0, false },
            { 3102, "C01",             "Proving Honor",                { "Shield-Brother", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3103, "C02",             "Brotherhood",                  { "Blood-Kin", "Frost Resist", AV::kResistFrost, 5.0f, true },  0, false },
            { 3104, "C03",             "The Silver Hand",              { "Silver Bane", "Health", AV::kHealth, 15.0f, false },  0, false },
            { 3105, "C04",             "Blood's Honor",                { "Oathkeeper", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3106, "C05",             "Purity of Revenge",            { "Vengeance", "Fire Resist", AV::kResistFire, 5.0f, true },  0, false },
            { 3107, "C06",             "Glory of the Dead",            { "Harbinger", "Health", AV::kHealth, 50.0f, false },  2, false },

            // --- College of Winterhold (MG06 does not exist in the game data) ---
            { 3201, "MG01",            "First Lessons",                { "Apprentice", "Magicka", AV::kMagicka, 15.0f, false },  0, false },
            { 3202, "MG02",            "Under Saarthal",               { "Delver", "Magicka", AV::kMagicka, 15.0f, false },  0, false },
            { 3203, "MG03",            "Hitting the Books",            { "Scholar", "Shock Resist", AV::kResistShock, 5.0f, true },  0, false },
            { 3204, "MG04",            "Good Intentions",              { "Confidant", "Magicka", AV::kMagicka, 15.0f, false },  0, false },
            { 3205, "MG05",            "Containment",                  { "Warden", "Magic Resist", AV::kResistMagic, 5.0f, true },  0, false },
            { 3206, "MG07",            "The Staff of Magnus",          { "Staffbearer", "Magicka", AV::kMagicka, 25.0f, false },  0, false },
            { 3207, "MG08",            "The Eye of Magnus",            { "Arch-Mage", "Magicka", AV::kMagicka, 75.0f, false },  2, false },

            // --- Thieves Guild ---
            { 3301, "TG00",            "A Chance Arrangement",         { "Cutpurse", "Carry Weight", AV::kCarryWeight, 15.0f, false },  0, false },
            { 3302, "TG01",            "Taking Care of Business",      { "Collector", "Carry Weight", AV::kCarryWeight, 15.0f, false },  0, false },
            { 3303, "TG02",            "Loud and Clear",               { "Saboteur", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3304, "TG03",            "Dampened Spirits",             { "Meadwrecker", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3305, "TG04",            "Scoundrel's Folly",            { "Scoundrel", "Carry Weight", AV::kCarryWeight, 15.0f, false },  0, false },
            { 3306, "TG05",            "Speaking With Silence",        { "Silent Step", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3307, "TG06",            "Hard Answers",                 { "Answer-Seeker", "Carry Weight", AV::kCarryWeight, 15.0f, false },  0, false },
            { 3308, "TG07",            "The Pursuit",                  { "Pursuer", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3309, "TG08A",           "Trinity Restored",             { "Nightingale", "Shock Resist", AV::kResistShock, 5.0f, true },  0, false },
            { 3310, "TG08B",           "Blindsighted",                 { "Blindsighted", "Stamina", AV::kStamina, 25.0f, false },  0, false },
            { 3311, "TG09",            "Darkness Returns",             { "Keeper of the Key", "Carry Weight", AV::kCarryWeight, 25.0f, false },  0, false },
            { 3312, "TGLeadership",    "Under New Management",         { "Guild Master", "Carry Weight", AV::kCarryWeight, 50.0f, false },  2, false },

            // --- Dark Brotherhood ---
            { 3401, "DB01",            "Innocence Lost",               { "Initiate", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3402, "DB02",            "With Friends Like These...",   { "Sworn", "Health", AV::kHealth, 15.0f, false },  0, false },
            { 3403, "DB03",            "Mourning Never Comes",         { "Silencer", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3404, "DB04",            "Whispers in the Dark",         { "Whisperer", "Magicka", AV::kMagicka, 15.0f, false },  0, false },
            { 3405, "DB05",            "Bound Until Death",            { "Bound", "Health", AV::kHealth, 15.0f, false },  0, false },
            { 3406, "DB06",            "Breaching Security",           { "Breacher", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3407, "DB07",            "The Cure for Madness",         { "Cure-Bringer", "Health", AV::kHealth, 15.0f, false },  0, false },
            { 3408, "DB08",            "Recipe for Disaster",          { "Poisoner", "Disease Resist", AV::kResistDisease, 5.0f, true },  0, false },
            { 3409, "DB09",            "To Kill an Empire",            { "Emperor's End", "Health", AV::kHealth, 15.0f, false },  0, false },
            { 3410, "DB10",            "Death Incarnate",              { "Death Incarnate", "Stamina", AV::kStamina, 25.0f, false },  0, false },
            { 3411, "DB11",            "Hail Sithis!",                 { "Listener", "Stamina", AV::kStamina, 50.0f, false },  2, false },

            // --- Civil War ---
            // No milestone for the war's end: the finale runs through radiant siege
            // quests (CWSiegeObj fires per city), so there is no single quest that means
            // "the war is over". Only the scripted beats are tracked.
            { 3501, "CW01A",           "Joining the Legion",           { "Legionnaire", "Health", AV::kHealth, 15.0f, false },  0, false },
            { 3502, "CW01B",           "Joining the Stormcloaks",      { "Stormcloak", "Health", AV::kHealth, 15.0f, false },  0, false },
            { 3503, "CW02A",           "The Jagged Crown",             { "Crown Bearer", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3504, "CW02B",           "The Jagged Crown",             { "Crown Bearer", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3505, "CW03",            "Message to Whiterun",          { "Herald", "Carry Weight", AV::kCarryWeight, 15.0f, false },  0, false },
            { 3506, "CWMission03",     "A False Front",                { "False Front", "Stamina", AV::kStamina, 15.0f, false },  0, false },
            { 3507, "CWMission07",     "Compelling Tribute",           { "Tribute Taker", "Carry Weight", AV::kCarryWeight, 15.0f, false },  0, false },
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

        [[nodiscard]] std::int32_t ScaledSouls(std::int32_t a_base) {
            return static_cast<std::int32_t>(std::lround(static_cast<float>(a_base) * RewardScale()));
        }

        // Hand out a milestone's rewards. Main thread only.
        // Returns false if it was already paid out.
        bool Grant(const Milestone& a_milestone) {
            if (AlreadyGranted(a_milestone.key)) {
                return false;
            }
            GetState().grantedMilestones.push_back(a_milestone.key);

            // The passive itself is not applied here. Passives::Refresh() recomputes
            // every bonus from the full list of earned milestones and writes the totals
            // into the ability spells. Deriving beats accumulating: a double-apply is
            // then impossible, because we never add a delta to anything.
            Passives::Refresh();

            const std::int32_t souls = ScaledSouls(a_milestone.dragonSouls);
            GrantDragonSouls(souls);

            std::int32_t perks = 0;
            if (a_milestone.endpoint) {
                perks = MilestonePerkPoints();
                GrantPerkPoints(perks);
            }

            logger::info("Milestone granted: {} — passive '{}' ({}), perks +{}, souls +{} (scale x{})",
                         a_milestone.questName, a_milestone.passive.name,
                         PassiveEffectText(a_milestone.passive), perks, souls, RewardScale());
            return true;
        }

        // How long the flourish gets the screen to itself before the reward panel
        // slides in over it. Long enough for the rings to land, short enough not to
        // feel like waiting.
        constexpr std::uint32_t kFlourishLeadMs = 1600;

        // Fire the flourish, then bring up the panel once it has played out.
        void Celebrate(std::string a_title, std::string a_subtitle, std::string a_body) {
            Sounds::Play(Sounds::Sfx::LevelUp);
            UI::PlayLevelUpEffect(std::move(a_title), std::move(a_subtitle));

            DelayedMainThread(kFlourishLeadMs, [body = std::move(a_body)]() {
                UI::ShowSystemWindow("[ SYSTEM ]", body, { "ACCEPT" }, [](int) {});
            });
        }

        std::string RewardText(const Milestone& a_milestone, std::int32_t a_perks) {
            std::string text = "QUEST COMPLETE\n\n  ";
            text += a_milestone.questName;
            text += "\n\nREWARDS\n\n  Passive   ";
            text += a_milestone.passive.name;
            text += "\n            ";
            text += PassiveEffectText(a_milestone.passive);

            if (const auto souls = ScaledSouls(a_milestone.dragonSouls); souls > 0) {
                text += "\n\n  Dragon Souls  +" + std::to_string(souls);
            }
            if (a_perks > 0) {
                text += "\n  Perk Points   +" + std::to_string(a_perks);
            }
            return text;
        }

        void CelebrateMilestone(const Milestone& a_milestone) {
            const std::int32_t perks = a_milestone.endpoint ? MilestonePerkPoints() : 0;
            if (Grant(a_milestone)) {
                Celebrate(a_milestone.endpoint ? "MILESTONE" : "QUEST COMPLETE",
                          a_milestone.questName, RewardText(a_milestone, perks));
            }
        }

        // Check one quest and pay out if it just finished. Safe to call repeatedly.
        void CheckQuest(RE::FormID a_formID) {
            auto*       quest = RE::TESForm::LookupByID<RE::TESQuest>(a_formID);
            const auto* milestone = FindByQuest(quest);
            if (!milestone) {
                return;
            }

            // Logged even when nothing is paid out: without this we cannot tell a quest
            // event that never arrived from one that arrived and was rejected — and that
            // was exactly the ambiguity that made `completequest` look broken.
            logger::info("Quest event for '{}': completed={} alreadyGranted={}",
                         milestone->questName, quest->IsCompleted(),
                         AlreadyGranted(milestone->key));

            // Deliberately asking the engine whether the quest is done, rather than
            // matching a hard-coded completion stage number — one less thing to get
            // wrong, and it survives quests that finish on different stages.
            if (!quest->IsCompleted() || AlreadyGranted(milestone->key)) {
                return;
            }
            CelebrateMilestone(*milestone);
        }

        // ------------------------------------------------------------------
        // The status panel: where the flavour names live on after their reward
        // window is gone. In game the buffs only ever show up as the aggregated
        // "System: Health" abilities — this is the ledger of where they came from.
        // ------------------------------------------------------------------

        constexpr std::uint32_t kStatusKey = 0x44;  // DIK_F10

        [[nodiscard]] const char* StatName(RE::ActorValue a_av) {
            switch (a_av) {
            case AV::kHealth:        return "Health";
            case AV::kMagicka:       return "Magicka";
            case AV::kStamina:       return "Stamina";
            case AV::kCarryWeight:   return "Carry Weight";
            case AV::kResistMagic:   return "Magic Resist";
            case AV::kResistFire:    return "Fire Resist";
            case AV::kResistFrost:   return "Frost Resist";
            case AV::kResistDisease: return "Disease Resist";
            default:                 return "?";
            }
        }

        [[nodiscard]] std::string PadTo(std::string a_text, std::size_t a_width) {
            if (a_text.size() < a_width) {
                a_text.append(a_width - a_text.size(), ' ');
            }
            return a_text;
        }

        void ShowStatusPanel() {
            // Never replace a live panel: opening over the blessing selection would
            // throw away its callback, and the reincarnation would hang half-finished.
            if (UI::IsSystemWindowOpen() || UI::IsSkillTreeOpen()) {
                return;
            }

            std::string body;

            body += "POWER LEVEL   " + PowerName(GetState().power) + "\n";

            std::size_t earned = 0;
            for (const auto& m : kMilestones) {
                earned += AlreadyGranted(m.key) ? 1 : 0;
            }
            body += "MILESTONES    " + std::to_string(earned) + " / " +
                    std::to_string(std::size(kMilestones)) + "\n";

            // The aggregated totals — the same numbers the abilities carry in the
            // magic menu, so the two views can be checked against each other.
            std::map<RE::ActorValue, float> totals;
            for (const auto* p : EarnedPassives()) {
                totals[p->actorValue] += PassiveAmount(*p);
            }
            if (!totals.empty()) {
                body += "\nATTUNEMENTS\n";
                for (const auto& [av, total] : totals) {
                    body += "  " + PadTo(StatName(av), 16) + "+" +
                            std::to_string(static_cast<int>(total)) + "\n";
                }
            }

            // The ledger: every earned passive with the deed it came from.
            if (earned > 0) {
                body += "\nPASSIVES\n";
                for (const auto& m : kMilestones) {
                    if (!AlreadyGranted(m.key)) {
                        continue;
                    }
                    body += "  " + PadTo(m.passive.name, 20) + PadTo(PassiveEffectText(m.passive), 22) +
                            m.questName + "\n";
                }
            } else {
                body += "\nNo deeds recognised yet.";
            }

            // The storage lives behind a panel button, not an inventory item. Earlier
            // attempts as a consumable token and as a ring both fought the engine
            // (menu-on-menu, biped slot conflicts, phantom heal visuals); a button in
            // our own UI has none of those problems — after the panel closes there is
            // no menu in the way, and the chest opens directly into gameplay.
            // Icon actions stack top-right; their callback indices follow their
            // position in the list. The tree is open to every reincarnated soul
            // (NORMAL earns souls through milestones too); the storage stays a
            // blessing privilege.
            std::vector<UI::Choice>            choices;
            std::vector<std::function<void()>> actions;

            if (GetState().reincarnated) {
                choices.push_back({ "SKILL TREE",
                                    "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\spells_38_frame.png",
                                    /*iconOnly=*/true });
                actions.emplace_back([]() { UI::ShowSkillTree(); });
            }
            if (Storage::Available()) {
                choices.push_back({ "STORAGE",
                                    "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\spells_03_frame.png",
                                    /*iconOnly=*/true });
                actions.emplace_back([]() { Storage::Open(); });
            }
            choices.push_back({ "CLOSE", {} });

            auto onSelect = [actions = std::move(actions)](int a_idx) {
                if (a_idx >= 0 && static_cast<std::size_t>(a_idx) < actions.size()) {
                    actions[static_cast<std::size_t>(a_idx)]();
                }
            };

            // Instant reveal: this is a ledger, not a story beat — nobody wants to
            // watch half a minute of typewriter before they can read their own stats.
            // Wider than the story panels: the ledger rows run to ~70 monospace
            // characters, and at the default width the quest names wrapped mid-word.
            UI::ShowSystemWindow("[ SYSTEM ] STATUS", std::move(body), std::move(choices),
                                 std::move(onSelect), 100000.0f, 980.0f);
        }

        // F11: pay out the next milestone still owed, exactly as a real quest would.
        // Tuning an animation by replaying a quest is not a workable loop.
        constexpr std::uint32_t kDebugGrantKey = 0x57;  // DIK_F11

        void DebugGrantNext() {
            for (const auto& m : kMilestones) {
                if (!AlreadyGranted(m.key)) {
                    logger::info("Debug hotkey: granting '{}'", m.questName);
                    CelebrateMilestone(m);
                    return;
                }
            }
            logger::info("Debug hotkey: every milestone is already granted");
        }

        // Listening to two events, not one.
        //
        // A quest finished in normal play advances a stage, so the stage event alone
        // would do. But the console's `completequest` marks a quest done *without*
        // advancing a stage — no stage event, no reward. That made the whole feature
        // untestable without playing the quest for real, which is no way to trust a
        // trigger. Stopping the quest does fire the start/stop event, so we take both.
        // Grant() is idempotent, so a quest that fires both is still paid once.
        class QuestWatcher :
            public RE::BSTEventSink<RE::TESQuestStageEvent>,
            public RE::BSTEventSink<RE::TESQuestStartStopEvent> {
        public:
            static QuestWatcher* GetSingleton() {
                static QuestWatcher singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESQuestStageEvent*                a_event,
                RE::BSTEventSource<RE::TESQuestStageEvent>*) override {
                if (a_event) {
                    CheckQuest(a_event->formID);
                }
                return RE::BSEventNotifyControl::kContinue;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESQuestStartStopEvent*                a_event,
                RE::BSTEventSource<RE::TESQuestStartStopEvent>*) override {
                if (a_event && !a_event->started) {
                    CheckQuest(a_event->formID);
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            QuestWatcher() = default;
        };
    }

    float PassiveAmount(const Passive& a_passive) {
        return std::round(a_passive.baseAmount * RewardScale());
    }

    std::string PassiveEffectText(const Passive& a_passive) {
        std::string text = "+";
        text += std::to_string(static_cast<int>(PassiveAmount(a_passive)));
        text += a_passive.percent ? "% " : " ";
        text += a_passive.stat;
        return text;
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
                const bool interesting =
                    std::ranges::any_of(kDumpPrefixes, [&](std::string_view prefix) {
                        return std::string_view{ id }.starts_with(prefix);
                    });
                if (interesting) {
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
            source->AddEventSink<RE::TESQuestStartStopEvent>(QuestWatcher::GetSingleton());
            logger::info("Progression: quest watcher armed (stage + start/stop)");
        }

        UI::RegisterHotkey(kStatusKey, ShowStatusPanel);
        UI::RegisterHotkey(kDebugGrantKey, DebugGrantNext);
        logger::info("Progression: F10 opens the status panel, F11 grants the next milestone (debug)");
    }

    void CatchUpOnLoad() {
        std::vector<const Milestone*> caughtUp;
        std::int32_t                  perks = 0;
        std::int32_t                  souls = 0;

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
            souls += ScaledSouls(m.dragonSouls);
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
            body += "  " + std::string(m->passive.name) + "   " + PassiveEffectText(m->passive) + "\n";
        }
        if (souls > 0) {
            body += "\n  Dragon Souls  +" + std::to_string(souls);
        }
        if (perks > 0) {
            body += "\n  Perk Points   +" + std::to_string(perks);
        }

        logger::info("Progression: caught up on {} milestone(s) already completed",
                     caughtUp.size());
        Celebrate("SYNCHRONISED", std::to_string(caughtUp.size()) + " deeds recognised",
                  std::move(body));
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
