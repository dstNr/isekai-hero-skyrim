#include "Progression.h"

#include "Config.h"
#include "Loc.h"
#include "Passives.h"
#include "Quests.h"
#include "SkillTree.h"
#include "Shop.h"
#include "SkyrimNet.h"
#include "Storage.h"
#include "System.h"
#include "UI/Input.h"
#include "UI/LevelUpEffect.h"
#include "UI/Prisma.h"
#include "UI/SkillTreeWindow.h"
#include "UI/Style.h"
#include "UI/SystemWindow.h"

#include <algorithm>
#include <cctype>
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
        //  key    editor ID          quest name                       { passive: name, actor value, base amount, is % }  souls  endpoint
        constexpr Milestone kMilestones[] = {
            // --- Main quest (names verified against the game's own quest table) ---
            { 1101, "MQ101",          "Unbound",                       { "Survivor", AV::kHealth, 25.0f, false },  0, false },
            { 1102, "MQ102",          "Before the Storm",              { "Wayfarer", AV::kCarryWeight, 25.0f, false },  0, false },
            { 1103, "MQ103",          "Bleak Falls Barrow",            { "Tomb Raider", AV::kStamina, 25.0f, false },  0, false },
            { 1104, "MQ104",          "Dragon Rising",                 { "Dragon Slayer", AV::kResistMagic, 5.0f, true },  1, false },
            { 1105, "MQ105",          "The Way of the Voice",          { "Voice Wielder", AV::kMagicka, 25.0f, false },  1, false },
            { 1106, "MQ105Ustengrav", "The Horn of Jurgen Windcaller", { "Horn Bearer", AV::kStamina, 25.0f, false },  0, false },
            { 1107, "MQ106",          "A Blade in the Dark",           { "Blade in the Dark", AV::kHealth, 25.0f, false },  1, false },
            { 1108, "MQ201",          "Diplomatic Immunity",           { "Infiltrator", AV::kStamina, 25.0f, false },  0, false },
            { 1109, "MQ202",          "A Cornered Rat",                { "Shadow Walker", AV::kCarryWeight, 25.0f, false },  0, false },
            { 1110, "MQ203",          "Alduin's Wall",                 { "Loremaster", AV::kMagicka, 25.0f, false },  1, false },
            { 1111, "MQ204",          "The Throat of the World",       { "Skyborn", AV::kResistMagic, 5.0f, true },  1, false },
            { 1112, "MQ205",          "Elder Knowledge",               { "Time Reader", AV::kMagicka, 25.0f, false },  1, false },
            { 1113, "MQ206",          "Alduin's Bane",                 { "Time Walker", AV::kResistMagic, 5.0f, true },  2, false },
            { 1114, "MQ301",          "The Fallen",                    { "Dragon Trapper", AV::kHealth, 25.0f, false },  2, false },
            { 1115, "MQ302",          "Season Unending",               { "Peacemaker", AV::kCarryWeight, 25.0f, false },  0, false },
            { 1116, "MQ303",          "The World-Eater's Eyrie",       { "Sky Breaker", AV::kHealth, 25.0f, false },  3, false },
            { 1117, "MQ304",          "Sovngarde",                     { "Soul Walker", AV::kHealth, 50.0f, false },  3, false },

            // --- Endpoint: the main quest ---
            { 1118, "MQ305",          "Dragonslayer",                  { "World Savior", AV::kHealth, 100.0f, false }, 10, true },

            // --- Dawnguard ---
            { 2201, "DLC1VQ01",        "Awakening",                    { "Dawnguard Recruit", AV::kHealth, 15.0f, false },  0, false },
            { 2202, "DLC1VQ02",        "Bloodline",                    { "Bloodline", AV::kHealth, 15.0f, false },  0, false },
            // The Prophet branches by allegiance; whichever the player walks, only one fires.
            { 2203, "DLC1VQ03Hunter",  "Prophet",                      { "Prophet's Ally", AV::kMagicka, 15.0f, false },  0, false },
            { 2204, "DLC1VQ03Vampire", "Prophet",                      { "Prophet's Ally", AV::kMagicka, 15.0f, false },  0, false },
            { 2205, "DLC1VQ04",        "Chasing Echoes",               { "Echo Chaser", AV::kStamina, 15.0f, false },  0, false },
            { 2206, "DLC1VQ05",        "Beyond Death",                 { "Soul Cairn Walker", AV::kMagicka, 25.0f, false },  1, false },
            { 2207, "DLC1VQ06",        "Unseen Visions",               { "Seer", AV::kMagicka, 15.0f, false },  0, false },
            { 2208, "DLC1VQ07",        "Touching the Sky",             { "Sky Toucher", AV::kHealth, 25.0f, false },  1, false },
            { 2209, "DLC1VQDragon",    "Durnehviir",                   { "Soul Binder", AV::kMagicka, 15.0f, false },  2, false },

            // --- Endpoint: Dawnguard ---
            { 2101, "DLC1VQ08",        "Kindred Judgment",             { "Vampire's Bane", AV::kResistMagic, 15.0f, true },  5, true },

            // --- Dragonborn ---
            { 2301, "DLC2MQ01",        "Dragonborn",                   { "Marked", AV::kHealth, 15.0f, false },  0, false },
            { 2302, "DLC2MQ02",        "The Temple of Miraak",         { "Temple Delver", AV::kMagicka, 15.0f, false },  0, false },
            { 2303, "DLC2MQ03",        "The Fate of the Skaal",        { "Skaal-Friend", AV::kHealth, 15.0f, false },  0, false },
            { 2304, "DLC2MQ03B",       "Cleansing the Stones",         { "Stone Cleanser", AV::kStamina, 15.0f, false },  0, false },
            { 2305, "DLC2MQ04",        "The Path of Knowledge",        { "Knowledge Seeker", AV::kMagicka, 25.0f, false },  1, false },
            { 2306, "DLC2MQ05",        "The Gardener of Men",          { "Apocrypha Walker", AV::kMagicka, 25.0f, false },  2, false },

            // --- Endpoint: Dragonborn ---
            { 2102, "DLC2MQ06",        "At the Summit of Apocrypha",   { "Miraak's Bane", AV::kMagicka, 100.0f, false }, 10, true },

            // --- The Companions ---
            { 3101, "C00",             "Take Up Arms",                 { "Whelp", AV::kHealth, 15.0f, false },  0, false },
            { 3102, "C01",             "Proving Honor",                { "Shield-Brother", AV::kStamina, 15.0f, false },  0, false },
            { 3103, "C02",             "Brotherhood",                  { "Blood-Kin", AV::kResistFrost, 5.0f, true },  0, false },
            { 3104, "C03",             "The Silver Hand",              { "Silver Bane", AV::kHealth, 15.0f, false },  0, false },
            { 3105, "C04",             "Blood's Honor",                { "Oathkeeper", AV::kStamina, 15.0f, false },  0, false },
            { 3106, "C05",             "Purity of Revenge",            { "Vengeance", AV::kResistFire, 5.0f, true },  0, false },
            { 3107, "C06",             "Glory of the Dead",            { "Harbinger", AV::kHealth, 50.0f, false },  2, false },

            // --- College of Winterhold (MG06 does not exist in the game data) ---
            { 3201, "MG01",            "First Lessons",                { "Apprentice", AV::kMagicka, 15.0f, false },  0, false },
            { 3202, "MG02",            "Under Saarthal",               { "Delver", AV::kMagicka, 15.0f, false },  0, false },
            { 3203, "MG03",            "Hitting the Books",            { "Scholar", AV::kResistShock, 5.0f, true },  0, false },
            { 3204, "MG04",            "Good Intentions",              { "Confidant", AV::kMagicka, 15.0f, false },  0, false },
            { 3205, "MG05",            "Containment",                  { "Warden", AV::kResistMagic, 5.0f, true },  0, false },
            { 3206, "MG07",            "The Staff of Magnus",          { "Staffbearer", AV::kMagicka, 25.0f, false },  0, false },
            { 3207, "MG08",            "The Eye of Magnus",            { "Arch-Mage", AV::kMagicka, 75.0f, false },  2, false },

            // --- Thieves Guild ---
            { 3301, "TG00",            "A Chance Arrangement",         { "Cutpurse", AV::kCarryWeight, 15.0f, false },  0, false },
            { 3302, "TG01",            "Taking Care of Business",      { "Collector", AV::kCarryWeight, 15.0f, false },  0, false },
            { 3303, "TG02",            "Loud and Clear",               { "Saboteur", AV::kStamina, 15.0f, false },  0, false },
            { 3304, "TG03",            "Dampened Spirits",             { "Meadwrecker", AV::kStamina, 15.0f, false },  0, false },
            { 3305, "TG04",            "Scoundrel's Folly",            { "Scoundrel", AV::kCarryWeight, 15.0f, false },  0, false },
            { 3306, "TG05",            "Speaking With Silence",        { "Silent Step", AV::kStamina, 15.0f, false },  0, false },
            { 3307, "TG06",            "Hard Answers",                 { "Answer-Seeker", AV::kCarryWeight, 15.0f, false },  0, false },
            { 3308, "TG07",            "The Pursuit",                  { "Pursuer", AV::kStamina, 15.0f, false },  0, false },
            { 3309, "TG08A",           "Trinity Restored",             { "Nightingale", AV::kResistShock, 5.0f, true },  0, false },
            { 3310, "TG08B",           "Blindsighted",                 { "Blindsighted", AV::kStamina, 25.0f, false },  0, false },
            { 3311, "TG09",            "Darkness Returns",             { "Keeper of the Key", AV::kCarryWeight, 25.0f, false },  0, false },
            { 3312, "TGLeadership",    "Under New Management",         { "Guild Master", AV::kCarryWeight, 50.0f, false },  2, false },

            // --- Dark Brotherhood ---
            { 3401, "DB01",            "Innocence Lost",               { "Initiate", AV::kStamina, 15.0f, false },  0, false },
            { 3402, "DB02",            "With Friends Like These...",   { "Sworn", AV::kHealth, 15.0f, false },  0, false },
            { 3403, "DB03",            "Mourning Never Comes",         { "Silencer", AV::kStamina, 15.0f, false },  0, false },
            { 3404, "DB04",            "Whispers in the Dark",         { "Whisperer", AV::kMagicka, 15.0f, false },  0, false },
            { 3405, "DB05",            "Bound Until Death",            { "Bound", AV::kHealth, 15.0f, false },  0, false },
            { 3406, "DB06",            "Breaching Security",           { "Breacher", AV::kStamina, 15.0f, false },  0, false },
            { 3407, "DB07",            "The Cure for Madness",         { "Cure-Bringer", AV::kHealth, 15.0f, false },  0, false },
            { 3408, "DB08",            "Recipe for Disaster",          { "Poisoner", AV::kResistDisease, 5.0f, true },  0, false },
            { 3409, "DB09",            "To Kill an Empire",            { "Emperor's End", AV::kHealth, 15.0f, false },  0, false },
            { 3410, "DB10",            "Death Incarnate",              { "Death Incarnate", AV::kStamina, 25.0f, false },  0, false },
            { 3411, "DB11",            "Hail Sithis!",                 { "Listener", AV::kStamina, 50.0f, false },  2, false },

            // --- Civil War ---
            // No milestone for the war's end: the finale runs through radiant siege
            // quests (CWSiegeObj fires per city), so there is no single quest that means
            // "the war is over". Only the scripted beats are tracked.
            { 3501, "CW01A",           "Joining the Legion",           { "Legionnaire", AV::kHealth, 15.0f, false },  0, false },
            { 3502, "CW01B",           "Joining the Stormcloaks",      { "Stormcloak", AV::kHealth, 15.0f, false },  0, false },
            { 3503, "CW02A",           "The Jagged Crown",             { "Crown Bearer", AV::kStamina, 15.0f, false },  0, false },
            { 3504, "CW02B",           "The Jagged Crown",             { "Crown Bearer", AV::kStamina, 15.0f, false },  0, false },
            { 3505, "CW03",            "Message to Whiterun",          { "Herald", AV::kCarryWeight, 15.0f, false },  0, false },
            { 3506, "CWMission03",     "A False Front",                { "False Front", AV::kStamina, 15.0f, false },  0, false },
            { 3507, "CWMission07",     "Compelling Tribute",           { "Tribute Taker", AV::kCarryWeight, 15.0f, false },  0, false },
        };

        // editorID -> quest, built once. Empty until Install() runs.
        std::unordered_map<std::string, RE::TESQuest*> g_quests;

        [[nodiscard]] bool AlreadyGranted(std::uint32_t a_key) {
            const auto& granted = GetState().grantedMilestones;
            return std::find(granted.begin(), granted.end(), a_key) != granted.end();
        }

        // How many of the 79 milestones are already granted. Shared by the status panel,
        // the PrismaUI status JSON, and SystemRank below — previously computed inline in
        // the first two, drifting into three near-identical loops otherwise.
        [[nodiscard]] std::size_t MilestonesEarned() {
            std::size_t earned = 0;
            for (const auto& m : kMilestones) {
                earned += AlreadyGranted(m.key) ? 1 : 0;
            }
            return earned;
        }

        // The insignia PNG for the current System Rank ("S" -> icons\rank_s.png). Derived
        // from the letter rather than kept as a second table, so retuning the thresholds
        // in SystemRank can never leave the emblem showing a rank the text disagrees with.
        [[nodiscard]] std::string RankIcon() {
            std::string letter = SystemRank();
            if (letter.empty()) {
                return {};
            }
            letter[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(letter[0])));
            return "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\rank_" + letter.substr(0, 1) + ".png";
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

        // The quest's name as the player's own game spells it.
        //
        // No translation key for these, and deliberately: Skyrim already ships "Bleak Falls
        // Barrow" in every language it was localised into, and a German player reading a
        // German game should see the German quest name — not whatever a translator of this
        // mod happened to write. So we ask the game, and the table's English name is the
        // fallback for when the quest is not present at all (a missing DLC, a typo'd editor
        // ID). That is 79 strings a translator never has to touch, translated better than
        // we could do it.
        //
        // Logs deliberately keep the table name: a log that changes language with the
        // player's game is a log nobody can compare against anyone else's.
        [[nodiscard]] std::string QuestName(const Milestone& a_milestone) {
            if (const auto it = g_quests.find(a_milestone.editorID); it != g_quests.end()) {
                if (const char* name = it->second ? it->second->GetName() : nullptr; name && *name) {
                    return name;
                }
            }
            return a_milestone.questName;
        }

        // The passive's title, translated. Keyed by the milestone's numeric key rather
        // than by a slug of the English name: that number is already documented as stable
        // across releases and never reused, so there is no second naming rule to keep in
        // step between this file and tools/extract-strings.mjs — which writes these keys
        // into the template by reading the same table.
        [[nodiscard]] std::string PassiveName(const Milestone& a_milestone) {
            return Loc::Get("passive." + std::to_string(a_milestone.key), a_milestone.passive.name);
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

            // Every milestone feeds the skill tree's currency (docs/IDEAS.md).
            GrantSystemPoints(MilestoneSystemPoints(a_milestone.endpoint));

            std::int32_t perks = 0;
            if (a_milestone.endpoint) {
                perks = MilestonePerkPoints();
                GrantPerkPoints(perks);
            }

            logger::info("Milestone granted: {} — passive '{}' ({}), perks +{}, souls +{} (scale x{})",
                         a_milestone.questName, a_milestone.passive.name,
                         PassiveEffectText(a_milestone.passive), perks, souls, RewardScale());

            // Let SkyrimNet's AI NPCs know the System just recognised this deed, so they
            // can reference the hero's growing legend. No-op without SkyrimNet.
            SkyrimNet::PushEvent(
                "isekai_milestone",
                "The System recognised this hero's deed (" + std::string(a_milestone.questName) +
                    ") and granted them the title \"" + std::string(a_milestone.passive.name) +
                    "\".");
            return true;
        }

        // How long the flourish gets the screen to itself before the reward panel
        // slides in over it. Long enough for the rings to land, short enough not to
        // feel like waiting.
        constexpr std::uint32_t kFlourishLeadMs = 1600;

        // Open the reward panel — but never over a live one. ShowSystemWindow
        // replaces the open window outright, and clobbering, say, the blessing
        // selection would throw away its callback and hang the reincarnation.
        void ShowPanelWhenFree(std::string a_body) {
            if (UI::IsSystemScreenOpen()) {
                DelayedMainThread(1000, [body = std::move(a_body)]() mutable {
                    ShowPanelWhenFree(std::move(body));
                });
                return;
            }
            UI::ShowSystemWindow(L("window.system", "[ SYSTEM ]"), std::move(a_body),
                                 { L("button.accept", "ACCEPT") }, [](int) {});
        }

        // Fire the flourish, then bring up the panel once it has played out.
        void Celebrate(std::string a_title, std::string a_subtitle, std::string a_body) {
            UI::PlayLevelUpEffect(std::move(a_title), std::move(a_subtitle));

            DelayedMainThread(kFlourishLeadMs, [body = std::move(a_body)]() mutable {
                ShowPanelWhenFree(std::move(body));
            });
        }

        std::string RewardText(const Milestone& a_milestone, std::int32_t a_perks) {
            std::string text = LF("reward.header", "QUEST COMPLETE\n\n  {}\n\n", QuestName(a_milestone));
            text += LF("reward.passive", "REWARDS\n\n  Passive   {}\n            {}",
                       PassiveName(a_milestone), PassiveEffectText(a_milestone.passive));

            text += LF("reward.points", "\n\n  System Points +{}",
                       MilestoneSystemPoints(a_milestone.endpoint));
            if (const auto souls = ScaledSouls(a_milestone.dragonSouls); souls > 0) {
                text += LF("reward.souls", "\n  Dragon Souls  +{}", souls);
            }
            if (a_perks > 0) {
                text += LF("reward.perks", "\n  Perk Points   +{}", a_perks);
            }
            return text;
        }

        void CelebrateMilestone(const Milestone& a_milestone) {
            const std::int32_t perks = a_milestone.endpoint ? MilestonePerkPoints() : 0;
            if (Grant(a_milestone)) {
                Celebrate(a_milestone.endpoint ? L("flourish.milestone", "MILESTONE")
                                               : L("flourish.questComplete", "QUEST COMPLETE"),
                          QuestName(a_milestone), RewardText(a_milestone, perks));
            }
        }

        // Check one quest and pay out if it just finished. Safe to call repeatedly.
        void CheckQuest(RE::FormID a_formID) {
            // Nothing counts before the System is bound to the player. Alternate-start
            // setups complete the early main quests to skip the intro — those deeds
            // are swept up by the catch-up that runs right after the blessing instead.
            if (!GetState().reincarnated) {
                return;
            }

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

        // The status-panel hotkey is configurable now (Config::SystemMenuKey /
        // ::SystemMenuModifier, from IsekaiHero.ini). Defaults: Right Shift + S — "S" for
        // System, held with RShift because S alone is the backward-movement key.

        [[nodiscard]] const char* StatName(RE::ActorValue a_av) {
            switch (a_av) {
            case AV::kHealth:        return L("stat.health", "Health");
            case AV::kMagicka:       return L("stat.magicka", "Magicka");
            case AV::kStamina:       return L("stat.stamina", "Stamina");
            case AV::kCarryWeight:   return L("stat.carryWeight", "Carry Weight");
            case AV::kResistMagic:   return L("stat.resistMagic", "Magic Resist");
            case AV::kResistFire:    return L("stat.resistFire", "Fire Resist");
            case AV::kResistFrost:   return L("stat.resistFrost", "Frost Resist");
            case AV::kResistShock:   return L("stat.resistShock", "Shock Resist");
            case AV::kResistDisease: return L("stat.resistDisease", "Disease Resist");
            default:                 return "?";
            }
        }

        [[nodiscard]] std::string PadTo(std::string a_text, std::size_t a_width) {
            if (a_text.size() < a_width) {
                a_text.append(a_width - a_text.size(), ' ');
            }
            return a_text;
        }

        // JSON string escaping for the status payload — passive titles and quest names
        // can hold quotes/backslashes.
        [[nodiscard]] std::string JsonEsc(const std::string& a_s) {
            std::string o;
            o.reserve(a_s.size() + 8);
            for (const char c : a_s) {
                switch (c) {
                case '"':  o += "\\\""; break;
                case '\\': o += "\\\\"; break;
                case '\n': o += "\\n"; break;
                case '\r': break;
                case '\t': o += "\\t"; break;
                default:   o += c; break;
                }
            }
            return o;
        }

        // Structured status data for the PrismaUI status screen — the same numbers the
        // monospace ImGui ledger shows, handed over as JSON so the web view can lay them
        // out as a proper dashboard.
        [[nodiscard]] std::string BuildStatusJson() {
            const auto& st = GetState();

            const std::size_t earned = MilestonesEarned();

            std::map<RE::ActorValue, float> totals;
            for (const auto* p : EarnedPassives()) {
                totals[p->actorValue] += PassiveAmount(*p);
            }

            const auto boolStr = [](bool b) { return b ? "true" : "false"; };

            std::string j = "{";
            j += "\"tier\":\"" + JsonEsc(PowerName(st.power)) + "\",";
            j += "\"rank\":\"" + JsonEsc(SystemRank()) + "\",";
            j += "\"mode\":\"" +
                 std::string(st.custom     ? "CUSTOM"
                             : st.dormant   ? "DORMANT"
                             : st.shattered ? "SHATTERED"
                                            : "") + "\",";
            // The three axes, for a CUSTOM build's detail line (all equal on a preset).
            j += "\"rewardTier\":\"" + JsonEsc(PowerName(st.power)) + "\",";
            j += "\"treeTier\":\"" + JsonEsc(PowerName(st.treeTier)) + "\",";
            j += "\"grantTier\":\"" + JsonEsc(PowerName(st.grantTier)) + "\",";
            j += "\"nextAwakening\":" + std::to_string(NextAwakeningLevel()) + ",";
            j += "\"milestonesEarned\":" + std::to_string(earned) + ",";
            j += "\"milestonesTotal\":" + std::to_string(std::size(kMilestones)) + ",";
            j += "\"points\":" + std::to_string(st.systemPoints) + ",";
            j += "\"hasTree\":" + std::string(boolStr(st.reincarnated)) + ",";
            j += "\"hasStorage\":" + std::string(boolStr(Storage::Available())) + ",";
            j += "\"hasShop\":" + std::string(boolStr(Shop::Available())) + ",";
            j += "\"canReboot\":" + std::string(boolStr(st.reincarnated)) + ",";

            // The standing System objective. questText is "" when there is none, which is
            // what the view keys off to hide the row entirely.
            j += "\"questText\":\"" + JsonEsc(Quests::Text()) + "\",";
            j += "\"questProgress\":" + std::to_string(Quests::Progress()) + ",";
            j += "\"questTarget\":" + std::to_string(Quests::Target()) + ",";
            j += "\"questReward\":" + std::to_string(Quests::Reward()) + ",";
            // Hours until the next one, 0 while an objective is running. Lets the view say
            // "the System is quiet" instead of showing nothing at all.
            j += "\"questWaitHours\":" +
                 std::to_string(static_cast<int>(std::lround(Quests::DaysUntilNext() * 24.0f))) +
                 ",";
            // Whether the view should offer NEW TASK at all. A testing tool, off unless
            // the ini turns it on — see the button's own comment further down.
            j += "\"questCanReroll\":" +
                 std::string(boolStr(Quests::Active() && Config::QuestRerollButton())) + ",";

            j += "\"attunements\":[";
            bool first = true;
            for (const auto& [av, total] : totals) {
                j += (first ? "" : ",");
                first = false;
                j += "{\"stat\":\"" + JsonEsc(StatName(av)) + "\",\"amount\":" +
                     std::to_string(static_cast<int>(total)) + "}";
            }
            j += "],\"titles\":[";
            first = true;
            for (const auto& m : kMilestones) {
                if (!AlreadyGranted(m.key)) {
                    continue;
                }
                j += (first ? "" : ",");
                first = false;
                j += "{\"name\":\"" + JsonEsc(PassiveName(m)) + "\",\"effect\":\"" +
                     JsonEsc(PassiveEffectText(m.passive)) + "\",\"quest\":\"" +
                     JsonEsc(QuestName(m)) + "\"}";
            }
            j += "]}";
            return j;
        }

        void ShowStatusPanel() {
            // Diagnostic: confirms the hotkey actually reached the handler (it did not in
            // VR before the input fix). If this logs but nothing appears, the panel was
            // handed to the renderer and the problem is downstream (e.g. PrismaUI not
            // painting in VR without its VR build).
            logger::info("System hotkey fired — opening status (prisma={}, paused={})",
                         UI::Prisma::Active(),
                         RE::UI::GetSingleton() ? RE::UI::GetSingleton()->GameIsPaused() : false);

            // Never replace a live panel: opening over the blessing selection would
            // throw away its callback, and the reincarnation would hang half-finished.
            if (UI::IsSystemScreenOpen()) {
                return;
            }

            // Don't pop up over a game menu. The hotkey's 'S' collides with typing —
            // searching the inventory for "Salmon", entering a console command, naming an
            // enchantment — and every one of those menus pauses the game, so a paused game
            // means "a menu already has the keyboard; leave it alone". (A live PrismaUI
            // screen also pauses, so this same guard stops a re-open over the web UI.)
            if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
                return;
            }

            // Nothing to report before the System has bound itself — the ledger would be
            // an empty form. The key boots it instead, which is what AutoStart = 0 leans
            // on and a way back in if the automatic trigger never fired.
            if (!GetState().reincarnated) {
                TriggerReincarnation();
                return;
            }

            // The dedicated, richer status screen when the PrismaUI patch is present;
            // the monospace ImGui ledger below otherwise.
            if (UI::Prisma::Active()) {
                UI::Prisma::ShowStatus(BuildStatusJson());
                return;
            }

            std::string body;

            // The label columns are part of the translatable text, padding and all: this
            // is a monospace ledger, so a translator has to be able to re-align it. Every
            // one of these is measured, not wrapped.
            body += LF("status.powerLevel", "POWER LEVEL   {}{}\n", PowerName(GetState().power),
                       GetState().custom      ? L("status.mode.custom", "  (CUSTOM)")
                       : GetState().dormant   ? L("status.mode.dormant", "  (DORMANT)")
                       : GetState().shattered ? L("status.mode.shattered", "  (SHATTERED)")
                                              : "");
            body += LF("status.systemRank", "SYSTEM RANK   {}\n", SystemRank());
            if (GetState().custom) {
                body += LF("status.customAxes", "  rewards {} / tree {} / gift {}\n",
                           PowerName(GetState().power), PowerName(GetState().treeTier),
                           PowerName(GetState().grantTier));
            }
            // A dormant blessing's whole point is the promise of the next rung — show
            // it, otherwise the panel just reads as a permanently weaker NORMAL.
            if (const auto next = NextAwakeningLevel(); next > 0) {
                body += LF("status.nextAwakening", "NEXT AWAKENING at level {}\n", next);
            }

            const std::size_t earned = MilestonesEarned();
            body += LF("status.milestones", "MILESTONES    {} / {}\n", earned,
                       std::size(kMilestones));
            body += LF("status.points", "SYSTEM POINTS {}\n", GetState().systemPoints);

            if (Quests::Active()) {
                body += LF("status.objective", "OBJECTIVE     {}   {} / {}   (+{} SP)\n",
                           Quests::Text(), Quests::Progress(), Quests::Target(),
                           Quests::Reward());
            } else if (const float wait = Quests::DaysUntilNext(); wait > 0.0f) {
                // An idle System should read as waiting, not broken. Hours rather than a
                // fraction of a day, because "0.4 days" is not something anyone thinks in.
                const int hours = std::max(1, static_cast<int>(std::lround(wait * 24.0f)));
                body += LF("status.objectiveQuiet",
                           "OBJECTIVE     none — the System is quiet for another {}h\n", hours);
            }

            // The aggregated totals — the same numbers the abilities carry in the
            // magic menu, so the two views can be checked against each other.
            std::map<RE::ActorValue, float> totals;
            for (const auto* p : EarnedPassives()) {
                totals[p->actorValue] += PassiveAmount(*p);
            }
            if (!totals.empty()) {
                body += std::string("\n") + L("status.attunements", "ATTUNEMENTS") + "\n";
                for (const auto& [av, total] : totals) {
                    body += "  " + PadTo(StatName(av), 16) + "+" +
                            std::to_string(static_cast<int>(total)) + "\n";
                }
            }

            // The ledger: every earned passive with the deed it came from.
            if (earned > 0) {
                body += std::string("\n") + L("status.passives", "PASSIVES") + "\n";
                for (const auto& m : kMilestones) {
                    if (!AlreadyGranted(m.key)) {
                        continue;
                    }
                    body += "  " + PadTo(PassiveName(m), 20) +
                            PadTo(PassiveEffectText(m.passive), 22) + QuestName(m) + "\n";
                }
            } else {
                body += std::string("\n") + L("status.noDeeds", "No deeds recognised yet.");
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
                choices.push_back({ L("button.skillTree", "SKILL TREE"),
                                    "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\ui_skilltree.png",
                                    /*iconOnly=*/true });
                actions.emplace_back([]() {
                    // Web tree if the optional PrismaUI patch is installed, else ImGui.
                    if (UI::Prisma::Active()) {
                        UI::Prisma::OpenTree();
                    } else {
                        UI::ShowSkillTree();
                    }
                });
            }
            if (Storage::Available()) {
                choices.push_back({ L("button.storage", "STORAGE"),
                                    "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\ui_storage.png",
                                    /*iconOnly=*/true });
                actions.emplace_back([]() { Storage::Open(); });
            }
            if (Shop::Available()) {
                choices.push_back({ L("button.shop", "SHOP"),
                                    "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\ui_shop.png",
                                    /*iconOnly=*/true });
                actions.emplace_back([]() { Shop::Open(); });
            }
            // Throw the standing objective away and roll another one. A TESTING TOOL, and
            // hidden unless the ini asks for it (Config::QuestRerollButton): it is free
            // and instant, so it undoes the thing it is used to test — objectives are
            // supposed to arrive when the System decides, and re-rolling until an easy
            // quarry appears is the shortest way around that. docs/MANUAL_TESTS.md turns
            // it on, because walking the objective table otherwise means hunting whatever
            // the dice picked, one quarry at a time. Reopens the panel so the new
            // objective is visible immediately.
            if (Quests::Active() && Config::QuestRerollButton()) {
                choices.push_back({ L("button.newTask", "NEW TASK"), {} });
                actions.emplace_back([]() {
                    Quests::Reroll();
                    ShowStatusPanel();
                });
            }
            // Reboot: re-open the blessing choice on this character (e.g. HERO -> a
            // Shattered Dormant run) without the fragile uninstall/reinstall dance that
            // drops the co-save's milestone list. Guarded behind a confirm; earned
            // progress is kept (see RebootSystem).
            if (GetState().reincarnated) {
                choices.push_back({ L("button.reboot", "REBOOT"), {} });
                actions.emplace_back([]() {
                    UI::ShowSystemWindow(
                        L("window.system", "[ SYSTEM ]"),
                        L("reboot.confirm",
                          "REBOOT THE SYSTEM?\n"
                          "\n"
                          "Re-opens the blessing choice, so you can pick a different path\n"
                          "(tier, Full/Shattered, Dormant).\n"
                          "\n"
                          "KEPT:    milestones, System Points, storage. The skill\n"
                          "         tree is refunded, not lost — every point spent\n"
                          "         on it comes back to spend again.\n"
                          "UNDONE:  the skills and attributes a previous blessing\n"
                          "         granted — you return to the mortal you were\n"
                          "         when the System first bound itself. Your\n"
                          "         character level is left alone.\n"
                          "NOTE:    gold and perk points already spent stay spent.\n"
                          "         A character from before this update has no such\n"
                          "         record, and keeps what it was given."),
                        std::vector<std::string>{ L("button.cancel", "CANCEL"),
                                                  L("button.reboot", "REBOOT") },
                        [](int a_confirm) {
                            if (a_confirm == 1) {
                                RebootSystem();
                            }
                        });
                });
            }
            choices.push_back({ L("button.close", "CLOSE"), {} });

            auto onSelect = [actions = std::move(actions)](int a_idx) {
                if (a_idx >= 0 && static_cast<std::size_t>(a_idx) < actions.size()) {
                    actions[static_cast<std::size_t>(a_idx)]();
                }
            };

            // Instant reveal: this is a ledger, not a story beat — nobody wants to
            // watch half a minute of typewriter before they can read their own stats.
            // Wider than the story panels: the ledger rows run to ~70 monospace
            // characters, and at the default width the quest names wrapped mid-word.
            // The rank block rides in the header's left margin: crest, and the rank
            // spelled out beside it in the rank's own colour. It duplicates the SYSTEM
            // RANK line on purpose — the line is the precise readout, this is the thing
            // you register at a glance (E is dull, S blazes).
            const std::string rank = SystemRank();
            UI::ShowSystemWindow(L("window.status", "[ SYSTEM ] STATUS"), std::move(body),
                                 std::move(choices), std::move(onSelect), 100000.0f, 980.0f,
                                 UI::Emblem{ RankIcon(), LF("status.rankEmblem", "RANK {}", rank),
                                             UI::Style::RankColor(rank.empty() ? 'E' : rank[0]) });
        }

        // F11: pay out the next milestone still owed, exactly as a real quest would.
        // Tuning an animation by replaying a quest is not a workable loop. Disabled
        // for the modlist test runs; flip to re-arm.
        constexpr bool          kEnableDebugGrant = false;
        constexpr std::uint32_t kDebugGrantKey = 0x57;  // DIK_F11

        [[maybe_unused]] void DebugGrantNext() {
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
        text += StatName(a_passive.actorValue);
        return text;
    }

    void OpenStatusPanel() {
        ShowStatusPanel();
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
        // UnresolvedMilestones() exposes the same list to the self-test, where it becomes
        // a PASS/FAIL line rather than a warning buried in the load log.
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

        UI::RegisterHotkey(Config::SystemMenuKey(), ShowStatusPanel,
                           Config::SystemMenuModifier());
        UI::RegisterGamepadHotkey(Config::GamepadMenuButton(), ShowStatusPanel,
                                  Config::GamepadMenuModifier());
        UI::RegisterVRHotkey(Config::VRMenuButton(), ShowStatusPanel);
        if constexpr (kEnableDebugGrant) {
            UI::RegisterHotkey(kDebugGrantKey, DebugGrantNext);
        }
        logger::info("Progression: system-menu hotkey registered (key={:#x}, modifier={:#x})",
                     Config::SystemMenuKey(), Config::SystemMenuModifier());
    }

    void CatchUpOnLoad() {
        if (!GetState().reincarnated) {
            return;  // the System is not bound yet; ApplyReincarnation calls back in
        }

        std::vector<const Milestone*> caughtUp;
        std::int32_t                  perks = 0;
        std::int32_t                  souls = 0;
        std::int32_t                  sp = 0;

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
            sp += MilestoneSystemPoints(m.endpoint);
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
        if (sp > 0) {
            body += "\n  System Points +" + std::to_string(sp);
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

    std::pair<std::vector<std::string>, std::size_t> UnresolvedMilestones() {
        std::vector<std::string> missing;
        for (const auto& m : kMilestones) {
            if (!g_quests.contains(m.editorID)) {
                missing.emplace_back(std::string(m.questName) + " [" + m.editorID + "]");
            }
        }
        return { std::move(missing), std::size(kMilestones) };
    }

    std::vector<std::pair<const char*, RE::ActorValue>> PassiveActorValues() {
        std::vector<std::pair<const char*, RE::ActorValue>> out;
        out.reserve(std::size(kMilestones));
        for (const auto& m : kMilestones) {
            out.emplace_back(m.passive.name, m.passive.actorValue);
        }
        return out;
    }

    std::string SystemRank() {
        // A deliberately simple, transparent formula rather than a tuned curve — milestones
        // are capped at 79, so rank growth past "finished the available content" has to come
        // from levelling and tree investment, which is exactly the isekai "still grinding for
        // rank" trope. Thresholds are a first pass (see docs/IDEAS.md); they are pure
        // constants, free to retune once a real playthrough's numbers are seen.
        std::uint16_t level = 1;
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            level = player->GetLevel();
        }
        const std::int32_t score = static_cast<std::int32_t>(MilestonesEarned()) +
                                   static_cast<std::int32_t>(level) / 5 +
                                   SkillTree::TotalInvested() / 10;

        if (score < 10) return "E";
        if (score < 25) return "D";
        if (score < 50) return "C";
        if (score < 90) return "B";
        if (score < 150) return "A";
        return "S";
    }
}
