#include "System.h"

#include "Config.h"
#include "Loc.h"
#include "CraftHooks.h"
#include "Passives.h"
#include "Plugin.h"
#include "Progression.h"
#include "Quests.h"
#include "SelfTest.h"
#include "SkillTree.h"
#include "Shop.h"
#include "SkyrimNet.h"
#include "Sounds.h"
#include "Storage.h"
#include "UI/Input.h"
#include "UI/LevelUpEffect.h"
#include "UI/Overlay.h"
#include "UI/Prisma.h"
#include "UI/SkillTreeWindow.h"
#include "UI/SystemWindow.h"
#include "UI/ThreatLabels.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace Isekai {

    namespace {
        State g_state;

        // --- Co-save serialization IDs ---
        constexpr std::uint32_t kSerID = 'ISKA';    // unique plugin id
        constexpr std::uint32_t kRecState = 'STAT';  // record tag
        // 7: shattered flag; 8: repeatable node ranks; 9: dormant flag;
        // 10: grantTier / treeTier / custom (the decoupled CUSTOM axes);
        // 11: the standing System objective (questKey / questProgress);
        // 12: its snapshotted target/reward, and when the next one is due
        constexpr std::uint32_t kVersion = 13;

        void SystemMsg(const char* a_text) {
            RE::DebugNotification(a_text);
        }

        // Icon paths for the reincarnation choices. Files may not exist yet: a missing
        // icon renders as no icon, exactly as these buttons looked before, so wiring the
        // names ahead of the art costs nothing. See docs/UI_ART_PROMPTS.md.
        [[nodiscard]] std::string BlessingIcon(const char* a_name) {
            return std::string("Data\\SKSE\\Plugins\\IsekaiHero\\icons\\") + a_name;
        }

        // ---- Reincarnation flow (choice captured into g_state) ----

        // kMaxPerkPoints now lives in System.h — and is 255, not 127: the field is
        // unsigned in the engine's eyes, whatever CommonLibSSE's declaration says.

        // What each blessing grants. 0 = leave that stat untouched.
        struct Blessing {
            std::uint16_t skillLevel;    // set all 18 skills to this
            std::uint16_t playerLevel;   // set character level
            std::int32_t  perkPoints;    // add to available perk points
            std::int32_t  gold;          // add to inventory
            std::int32_t  dragonSouls;   // add to the unspent soul pool
            std::int32_t  systemPoints;  // seed for the skill tree
        };

        Blessing BlessingFor(PowerLevel a_power) {
            switch (a_power) {
            case PowerLevel::Hero:
                return { 50, 25, 10, 2000, 3, 5 };
            case PowerLevel::Ascended:
                // systemPoints 500 is a TEST value for the modlist runs — enough to
                // buy the whole tree (260) outright. Rebalance before any release
                // (the earned-through-milestones figure for ASCENDED is ~380).
                return { 100, 150, kMaxPerkPoints, 25000, 20, 500 };
            default:  // Normal — pure challenge, no boosts
                return { 0, 0, 0, 0, 0, 0 };
            }
        }

        // Health/Magicka/Stamina are not a separate knob: they follow the granted
        // level. A character who levelled to N by hand would have banked (N-1) * 10
        // attribute points; we spread them evenly across the big three. A flat +300
        // used to make ASCENDED a paper giant — level 150 with level-30 stats.
        // Takes the level actually gained: an existing-save character already banked
        // their own level-ups, so only the granted difference pays out.
        [[nodiscard]] float AttributeBonusPerStat(std::uint16_t a_fromLevel,
                                                  std::uint16_t a_toLevel) {
            if (a_toLevel <= a_fromLevel) {
                return 0.0f;
            }
            return std::round(static_cast<float>(a_toLevel - a_fromLevel) * 10.0f / 3.0f);
        }

        // Hand over one blessing's flat grant, and report the per-stat attribute bonus
        // that came with it (for the log line). Split out of ApplyReincarnation because
        // Raise the player's character level to a_target, never lower.
        //
        // For the PLAYER the level is stored in the ActorBase's actorData.level, but that
        // does NOT survive a save/load on its own — the player's base reverts, so the
        // level dropped back to 1 on load (reported bug). Everything else the blessing
        // grants (skills, attributes, perks, gold) goes through channels that DO persist,
        // so only the number was affected. We therefore re-assert the level on every load
        // in ReapplyLevelOnLoad, the same derive-on-load approach the passives use.
        //
        // The before/after GetLevel is logged on purpose: if the engine does not report
        // the new level here, actorData.level is not the field the player's level reads
        // from and we need the XP/AdvanceLevel path instead — the log tells a tester which.
        // The level this character had before we ever raised it, for THIS SESSION.
        // 0 = we have not raised anything yet.
        //
        // It cannot live in the co-save, which is the whole point: the save it has to be
        // restored into is one that predates the System and therefore carries no record of
        // ours at all. It has to be remembered process-side, because the thing that leaks
        // is process-side (see RestoreLevelBeforeGrant).
        std::uint16_t g_levelBeforeGrant = 0;

        void SetPlayerLevelAtLeast(std::uint16_t a_target) {
            if (a_target == 0) {
                return;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return;
            }
            const std::uint16_t before = player->GetLevel();
            if (a_target <= before) {
                return;  // already at or past it — never demote
            }
            if (auto* base = player->GetActorBase()) {
                if (g_levelBeforeGrant == 0) {
                    g_levelBeforeGrant = before;
                }
                base->actorData.level = a_target;
                logger::info("Player level: {} -> requested {} (GetLevel reads back {})", before,
                             a_target, player->GetLevel());
            }
        }

        // Put the level back where we found it, before anything else is loaded.
        //
        // REPORTED BUG. actorData.level lives on the player's ActorBase, and a save load
        // does not restore it — that is the same fact ReapplyLevelOnLoad exists for, seen
        // from the other side. Once ASCENDED has written 150 there, loading a save from
        // BEFORE the System ever ran leaves the character at 150: the save has no record
        // of us, nothing lowers it, and SetPlayerLevelAtLeast deliberately never demotes.
        // The reporter watched level-gated quest mods fire in a game the System had not
        // touched yet. Restarting Skyrim fixed it, because that reloads the form.
        //
        // So the level is reverted on every load, and ReapplyLevelOnLoad then raises it
        // again to whatever the save being loaded actually earned. Down first, up second.
        void RestoreLevelBeforeGrant() {
            if (g_levelBeforeGrant == 0) {
                return;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* base = player ? player->GetActorBase() : nullptr;
            if (!base) {
                return;
            }
            if (base->actorData.level != g_levelBeforeGrant) {
                logger::info("Player level: {} -> {} (reverting our grant before load)",
                             base->actorData.level, g_levelBeforeGrant);
                base->actorData.level = g_levelBeforeGrant;
            }
            g_levelBeforeGrant = 0;
        }

        // a DORMANT awakening pays out the very same grant, just years of play later.
        //
        // Blessings only ever RAISE — on an existing save the character may already be
        // past parts of the blessing, and neither a rebirth nor an awakening may demote.
        float ApplyBlessing(const Blessing& b) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return 0.0f;
            }
            auto* avOwner = player->AsActorValueOwner();

            // Skills: the 18 skill actor values are contiguous (kOneHanded..kEnchanting).
            if (b.skillLevel > 0 && avOwner) {
                for (int av = static_cast<int>(RE::ActorValue::kOneHanded);
                     av <= static_cast<int>(RE::ActorValue::kEnchanting); ++av) {
                    const auto avEnum = static_cast<RE::ActorValue>(av);
                    if (avOwner->GetBaseActorValue(avEnum) < static_cast<float>(b.skillLevel)) {
                        avOwner->SetBaseActorValue(avEnum, static_cast<float>(b.skillLevel));
                    }
                }
            }

            // Character level lives on the ActorBase (TESNPC); attributes follow the
            // levels actually gained, spread evenly across the big three.
            const std::uint16_t currentLevel = player->GetLevel();
            const float attrBonus = AttributeBonusPerStat(std::max<std::uint16_t>(currentLevel, 1),
                                                          b.playerLevel);
            if (attrBonus > 0.0f && avOwner) {
                for (auto av : { RE::ActorValue::kHealth, RE::ActorValue::kMagicka,
                                 RE::ActorValue::kStamina }) {
                    avOwner->SetBaseActorValue(av, avOwner->GetBaseActorValue(av) + attrBonus);
                }
            }
            SetPlayerLevelAtLeast(b.playerLevel);

            GrantPerkPoints(b.perkPoints);
            GrantDragonSouls(b.dragonSouls);
            GrantSystemPoints(b.systemPoints);

            // Gold (Gold001 = 0x0000000F).
            if (b.gold > 0) {
                if (auto* gold = RE::TESForm::LookupByID<RE::TESObjectMISC>(0x0000000F)) {
                    player->AddObjectToContainer(gold, nullptr, b.gold, nullptr);
                }
            }

            return attrBonus;
        }

        void ApplyReincarnation() {
            if (!RE::PlayerCharacter::GetSingleton()) {
                return;
            }

            // The flat starting grant follows grantTier, the dedicated field for it —
            // NORMAL (a SHATTERED or DORMANT start, or a CUSTOM "no floor") gives an empty
            // blessing, HERO/ASCENDED the corresponding table. reward pace (power) and tree
            // depth (treeTier) are separate; on a preset all three agree, on CUSTOM they may
            // not. A DORMANT start is NORMAL here and its grant arrives later via AwakenTo.
            const Blessing b = BlessingFor(g_state.grantTier);
            const float attrBonus = ApplyBlessing(b);
            // No crafting stock here any more: the Dimensional Storage starts empty for
            // every blessing and is filled through the System Shop's material packs, so
            // what it holds is something the player chose to spend points on. HERO and
            // ASCENDED still fill it sooner — their reward scale multiplies point income.
            Storage::GrantCodexIfMissing();  // physical fallback access — every blessing
            Quests::EnsureObjective();       // the System's first standing objective

            logger::info(
                "Reincarnation applied: power={} skills={} level={} perks=+{} attr=+{} gold={} souls=+{}",
                PowerName(g_state.power), b.skillLevel, b.playerLevel, b.perkPoints, attrBonus,
                b.gold, b.dragonSouls);

            // Tell SkyrimNet (if present) who the player now is, so AI NPCs can react to
            // the reincarnation. Persistent world-knowledge, third person. No-op without
            // SkyrimNet.
            SkyrimNet::PushEvent(
                "isekai_awakening",
                "This person has been reincarnated into this world as a hero bound to a "
                "mysterious progression System" +
                    std::string(g_state.dormant ? ", though its power still lies dormant and "
                                                  "will awaken as they grow stronger. "
                                                : ". ") +
                    "An otherworldly power (System tier: " + PowerName(g_state.power) +
                    ") clings to them, sensed faintly by those around them.");

            std::string body =
                "REINCARNATION COMPLETE\n"
                "\n"
                "  Power level   " + PowerName(g_state.power) +
                (g_state.custom     ? "  (CUSTOM)"
                 : g_state.dormant  ? "  (DORMANT)"
                 : g_state.shattered ? "  (SHATTERED)"
                                     : "") + "\n";
            if (g_state.dormant) {
                body += "  Awakens at    level " +
                        std::to_string(Config::DormantHeroLevel()) + "\n";
            }
            // A CUSTOM build's three dials rarely agree, so spell them out.
            if (g_state.custom) {
                body += "  Rewards       " + PowerName(g_state.power) + "\n";
                body += "  Skill tree    " + PowerName(g_state.treeTier) + "\n";
                body += "  Starting gift " + PowerName(g_state.grantTier) + "\n";
            }
            body +=
                "\n"
                "The System is now bound to your soul.\n"
                "Your new life begins.";

            // The biggest moment the mod has: let the flourish land before the panel.
            // (The sting comes with the flourish — PlayLevelUpEffect owns it.)
            UI::PlayLevelUpEffect("AWAKENED", PowerName(g_state.power));

            DelayedMainThread(1600, [body]() {
                UI::ShowSystemWindow("[ SYSTEM ]", body, { "CONTINUE" }, [](int) {
                    // Now that the System is bound, sweep up whatever was completed
                    // before it existed — alternate starts finish the early main
                    // quests to skip the intro, and those deeds still count.
                    Progression::CatchUpOnLoad();
                });
            });
        }

        // Second step: take the power now, or earn it. FULL is the blessing as designed
        // (flat skills/level/fortune); SHATTERED keeps the tier's reward pace and deep
        // tree but starts you at the mortal floor. The question is orthogonal to DORMANT
        // — there it decides not *whether* the tiers arrive but whether each one still
        // arrives carrying its grant.
        void ShowPathSelection() {
            const std::string intro =
                g_state.dormant
                    ? "The dormant blessing stirs.\n"
                      "How will each awakening reach you?\n"
                      "\n"
                      "  FULL       Every awakening arrives whole —\n"
                      "             skills, level and fortune granted\n"
                      "             the moment it comes.\n"
                      "  SHATTERED  Only the System's reach grows: faster\n"
                      "             rewards and deeper gifts unlock, but\n"
                      "             nothing is ever handed to you.\n"
                      "\n"
                      "Choose how you rise:"
                    : "The " + PowerName(g_state.power) + " blessing resonates.\n"
                      "How will you receive it?\n"
                      "\n"
                      "  FULL       Awaken at once — skills, level and\n"
                      "             fortune granted now.\n"
                      "  SHATTERED  The System is fractured. Begin as any\n"
                      "             mortal, but its rewards still flow faster\n"
                      "             and its deepest gifts stay open to you.\n"
                      "             Higher ceiling, same floor — earn it.\n"
                      "\n"
                      "Choose how you rise:";

            UI::ShowSystemWindow(
                "[ SYSTEM ]", intro,
                std::vector<UI::Choice>{
                    { "FULL AWAKENING", BlessingIcon("blessing_full.png"), false },
                    { "SHATTERED", BlessingIcon("blessing_shattered.png"), false },
                },
                [](int a_idx) {
                    g_state.shattered = (a_idx == 1);
                    // Full hands over the tier's flat grant; Shattered takes none. (For a
                    // dormant blessing grantTier stays NORMAL now and each awakening sets
                    // it in AwakenTo.) treeTier was already set to the tier in
                    // ShowPowerSelection, so it is unaffected either way.
                    g_state.grantTier = (g_state.shattered || g_state.dormant)
                                            ? PowerLevel::Normal
                                            : g_state.power;
                    logger::info("Awakening path: {}{}", g_state.shattered ? "SHATTERED" : "FULL",
                                 g_state.dormant ? " (DORMANT)" : "");
                    ApplyReincarnation();
                });
        }

        // ---- CUSTOM build: the three axes, chosen one by one ----

        // A single reusable tier question (NORMAL/HERO/ASCENDED) that stores the pick via
        // a_set and then runs a_next.
        void AskTier(std::string a_prompt, std::function<void(PowerLevel)> a_set,
                     std::function<void()> a_next) {
            UI::ShowSystemWindow(
                "[ SYSTEM ]", std::move(a_prompt),
                std::vector<UI::Choice>{
                    { "NORMAL", BlessingIcon("blessing_normal.png"), false },
                    { "HERO", BlessingIcon("blessing_hero.png"), false },
                    { "ASCENDED", BlessingIcon("blessing_ascended.png"), false },
                },
                [a_set = std::move(a_set), a_next = std::move(a_next)](int a_idx) {
                    a_set(static_cast<PowerLevel>(std::clamp(a_idx, 0, 2)));
                    a_next();
                });
        }

        void ShowCustomBuilder() {
            // Three independent dials, asked in turn: starting grant, reward pace, tree
            // ceiling. Each is a plain tier pick; the last applies the build.
            AskTier(
                "CUSTOM — 1 of 3: STARTING GIFT\n"
                "\n"
                "The flat head start handed over at birth — skills, level,\n"
                "gold and seed points.\n"
                "\n"
                "  NORMAL    Nothing. Begin as any mortal.\n"
                "  HERO      A hero's start.\n"
                "  ASCENDED  A transcendent's fortune.",
                [](PowerLevel t) { g_state.grantTier = t; },
                []() {
                    AskTier(
                        "CUSTOM — 2 of 3: REWARD PACE\n"
                        "\n"
                        "How fast the System keeps paying out — milestone\n"
                        "passives, System Points, dragon souls, perk points.\n"
                        "\n"
                        "  NORMAL    x1. The honest grind.\n"
                        "  HERO      x2.\n"
                        "  ASCENDED  x4.",
                        [](PowerLevel t) { g_state.power = t; },
                        []() {
                            AskTier(
                                "CUSTOM — 3 of 3: SKILL TREE\n"
                                "\n"
                                "How deep the tree opens. You still buy each node\n"
                                "with System Points, so a deep tree on a NORMAL\n"
                                "pace is access, not a free ride.\n"
                                "\n"
                                "  NORMAL    The self-made half.\n"
                                "  HERO      + the Omniscience gifts.\n"
                                "  ASCENDED  + the World Tree capstone.",
                                [](PowerLevel t) { g_state.treeTier = t; },
                                []() {
                                    g_state.custom = true;
                                    g_state.dormant = false;
                                    g_state.shattered = false;
                                    logger::info("Custom build: reward={} tree={} grant={}",
                                                 PowerName(g_state.power),
                                                 PowerName(g_state.treeTier),
                                                 PowerName(g_state.grantTier));
                                    ApplyReincarnation();
                                });
                        });
                });
        }

        void ShowPowerSelection() {
            UI::ShowSystemWindow(
                "[ SYSTEM ]",
                "You have been reincarnated.\n"
                "The System offers you a blessing.\n"
                "\n"
                "  NORMAL    No blessing. Pure challenge.\n"
                "  HERO      Awakened power.\n"
                "  ASCENDED  Transcend mortal limits.\n"
                "  DORMANT   The blessing sleeps. Begin as a mortal;\n"
                "            it wakes to HERO at level " +
                    std::to_string(Config::DormantHeroLevel()) + ", then to\n"
                "            ASCENDED at level " +
                    std::to_string(Config::DormantAscendedLevel()) + ".\n"
                "  CUSTOM    Set the pieces yourself — starting gift,\n"
                "            reward pace and skill-tree depth, each on\n"
                "            its own.\n"
                "\n"
                "Choose your path:",
                std::vector<UI::Choice>{
                    { "NORMAL", BlessingIcon("blessing_normal.png"), false },
                    { "HERO", BlessingIcon("blessing_hero.png"), false },
                    { "ASCENDED", BlessingIcon("blessing_ascended.png"), false },
                    { "DORMANT", BlessingIcon("blessing_dormant.png"), false },
                    { "CUSTOM", BlessingIcon("blessing_custom.png"), false },
                },
                [](int a_idx) {
                    // CUSTOM sets the three axes apart in its own builder.
                    if (a_idx == 4) {
                        ShowCustomBuilder();
                        return;
                    }
                    g_state.shattered = false;
                    g_state.custom = false;
                    // DORMANT is not a fourth tier: it starts at NORMAL and climbs the
                    // same ladder on its own, so g_state.power stays the tier held now.
                    g_state.dormant = (a_idx == 3);
                    g_state.power = g_state.dormant ? PowerLevel::Normal
                                                    : static_cast<PowerLevel>(std::clamp(a_idx, 0, 2));
                    // For a preset the reward tier, the tree ceiling and (via Full) the
                    // grant all agree: tree opens to the tier now; for DORMANT the tree
                    // grows with each awakening (AwakenTo raises treeTier too).
                    g_state.treeTier = g_state.power;
                    g_state.grantTier = PowerLevel::Normal;  // set for real in ShowPathSelection
                    logger::info("Power level selected: {} ({}{})", a_idx, PowerName(g_state.power),
                                 g_state.dormant ? ", DORMANT" : "");
                    // NORMAL has no floor to skip, so it never asks; HERO/ASCENDED and
                    // DORMANT choose full-vs-shattered next.
                    if (g_state.power == PowerLevel::Normal && !g_state.dormant) {
                        ApplyReincarnation();
                    } else {
                        ShowPathSelection();
                    }
                });
        }

        // ---- Dormant awakening ----

        // The tier a dormant character's level has earned. Read straight from the level
        // rather than stepped one rung at a time, so an existing save that takes the
        // dormant path at level 90 lands on ASCENDED at once instead of walking a ladder
        // it is long past.
        [[nodiscard]] PowerLevel EarnedTier(std::uint16_t a_level) {
            if (a_level >= Config::DormantAscendedLevel()) {
                return PowerLevel::Ascended;
            }
            if (a_level >= Config::DormantHeroLevel()) {
                return PowerLevel::Hero;
            }
            return PowerLevel::Normal;
        }

        void AwakenTo(PowerLevel a_tier) {
            const PowerLevel from = g_state.power;
            g_state.power = a_tier;      // reward tier catches up to the awakening
            g_state.treeTier = a_tier;   // and the tree opens to it

            // SHATTERED means the tier arrives bare — the faster reward pace and the
            // deeper tree open up, nothing is handed over.
            if (!g_state.shattered) {
                g_state.grantTier = a_tier;  // this awakening did hand over its grant
                ApplyBlessing(BlessingFor(a_tier));
                // No crafting stock is handed over here either — see ApplyReincarnation.
                // The awakening raises the reward scale, which is what pays for the
                // storage now.
            }

            const auto* player = RE::PlayerCharacter::GetSingleton();
            logger::info("Dormant awakening: {} -> {} at level {}{}", PowerName(from),
                         PowerName(a_tier), player ? player->GetLevel() : 0,
                         g_state.shattered ? " (SHATTERED)" : "");

            SkyrimNet::PushEvent(
                "isekai_awakening",
                "The dormant System within this hero has awakened further, rising to tier " +
                    PowerName(a_tier) + ". Their otherworldly power grows more palpable.");

            const bool  ascended = (a_tier == PowerLevel::Ascended);
            std::string body =
                (ascended ? "TRANSCENDENCE\n" : "AWAKENING\n") +
                std::string("\n"
                            "  Power level   ") + PowerName(a_tier) +
                (g_state.shattered ? "  (SHATTERED)" : "") + "\n";
            if (!ascended) {
                body += "  Transcends at level " +
                        std::to_string(Config::DormantAscendedLevel()) + "\n";
            }
            body +=
                "\n" +
                std::string(ascended ? "The last seal breaks. The System withholds\n"
                                       "nothing from you now."
                                     : "The blessing that slept in your soul opens\n"
                                       "its eyes. The System reaches further.");

            UI::PlayLevelUpEffect(ascended ? "TRANSCENDED" : "AWAKENED", PowerName(a_tier));
            DelayedMainThread(1600, [body]() {
                UI::ShowSystemWindow("[ SYSTEM ]", body, { "CONTINUE" }, [](int) {});
            });
        }

        void BeginReincarnation() {
            if (g_state.reincarnated) {
                return;
            }

            // In VR the ImGui overlay is disabled (it crashes on the VR renderer), so the
            // blessing menu can ONLY come from PrismaUI. If it is not active yet, do not
            // consume the one-shot flag below — otherwise the character would be marked
            // reincarnated with no way ever to pick a blessing (a soft-brick of the core
            // feature). Bail instead; the menu-close watcher calls back in, and once the
            // player has the PrismaUI VR build the flow starts normally. A single note
            // (native notification — those DO render in the headset) says what is needed.
            if (REL::Module::IsVR() && !UI::Prisma::Active()) {
                static bool warned = false;
                if (!warned) {
                    warned = true;
                    logger::warn("Reincarnation held: VR without an active PrismaUI view — the "
                                 "ImGui UI is disabled in VR, so the menu needs PrismaUI.");
                    SystemMsg(L("vr.needsPrisma",
                                "[ SYSTEM ] Isekai Hero needs PrismaUI (1.5.0 VR build) to "
                                "show its menu in VR."));
                }
                return;
            }

            g_state.reincarnated = true;  // fire exactly once per character

            logger::info("Reincarnation triggered — System boot sequence");

            // Solo-Leveling style boot: paced [SYSTEM] messages, then the rank menu.
            SystemMsg(L("boot.1", "[ SYSTEM ] Soul signature detected..."));
            DelayedMainThread(1500, []() {
                SystemMsg(L("boot.2", "[ SYSTEM ] Analyzing dimensional residue..."));
            });
            DelayedMainThread(3000, []() {
                SystemMsg(L("boot.3", "[ SYSTEM ] Awakening protocol ready."));
            });
            DelayedMainThread(4500, []() { ShowPowerSelection(); });
        }

        // True once the player is really playing: 3D loaded, not paused, past
        // character creation, not on a loading screen, and in control (so we do
        // not fire during the Helgen cart ride or any forced-walk cutscene).
        [[nodiscard]] bool IsPlayerReady() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->Is3DLoaded()) {
                return false;
            }

            auto* ui = RE::UI::GetSingleton();
            if (!ui || ui->GameIsPaused()) {
                return false;
            }
            if (ui->IsMenuOpen("RaceSex Menu"sv) || ui->IsMenuOpen("Loading Menu"sv)) {
                return false;
            }

            auto* controls = RE::ControlMap::GetSingleton();
            if (!controls || !controls->IsMovementControlsEnabled()) {
                return false;
            }

            return true;
        }

        // The cell the player was FIRST ready in. 0 = not seen yet.
        RE::FormID g_firstReadyCell = 0;

        void TryTrigger() {
            if (g_state.reincarnated || !IsPlayerReady()) {
                return;
            }

            // AutoStart = 0: the player boots the System with the menu hotkey instead.
            // Alternate starts that open with a prologue elsewhere (a modern-world intro
            // above all) put the boot sequence somewhere it does not belong, and no cell
            // heuristic can tell that apart from a normal start — the player can.
            if (!Config::AutoStart()) {
                static bool logged = false;
                if (!logged) {
                    logged = true;
                    logger::info("Reincarnation held: AutoStart = 0 — waiting for the System "
                                 "hotkey (key={})",
                                 Config::KeyName(Config::SystemMenuKey()));
                }
                return;
            }

            // "Ready" alone is not "in the game world". Character-creation start
            // rooms (NYA, Alternate Start, ...) hand the player full control while
            // they are still picking a face — the System fired the moment the room
            // spawned. The tell: those rooms are interiors, and the real start
            // arrives via a teleport out of them. So: if the first cell the player
            // is ready in is an exterior, fire right away (vanilla starts, coc);
            // if it is an interior, hold until the player reaches a DIFFERENT cell.
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* cell = player ? player->GetParentCell() : nullptr;
            if (!cell) {
                return;
            }

            if (g_firstReadyCell == 0) {
                g_firstReadyCell = cell->GetFormID();
                if (cell->IsInteriorCell()) {
                    logger::info(
                        "Reincarnation armed, holding: first ready in interior {:#x} "
                        "(likely a chargen room) — waiting for a cell change",
                        g_firstReadyCell);
                    return;
                }
            } else if (cell->GetFormID() == g_firstReadyCell && cell->IsInteriorCell()) {
                return;  // still in the start room
            }

            BeginReincarnation();
        }

        // ------------------------------------------------------------------
        // Trigger: watch menu open/close (fires on the main thread). Whenever a
        // menu closes we re-check readiness; the persisted flag guarantees the
        // reincarnation runs exactly once per character.
        // ------------------------------------------------------------------

        class MenuWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
                            public RE::BSTEventSink<RE::LevelIncrease::Event> {
        public:
            static MenuWatcher* GetSingleton() {
                static MenuWatcher singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (a_event && !a_event->opening) {
                    TryTrigger();
                    // The level-up event below is the real trigger; this is the retry
                    // for the case where it landed while a menu still held the screen.
                    CheckDormantAwakening();
                }
                return RE::BSEventNotifyControl::kContinue;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::LevelIncrease::Event*,
                RE::BSTEventSource<RE::LevelIncrease::Event>*) override {
                CheckDormantAwakening();
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            MenuWatcher() = default;
        };

        // ------------------------------------------------------------------
        // Serialization callbacks
        // ------------------------------------------------------------------

        // Written field by field, not as one memcpy of the struct: State now holds a
        // vector, whose bytes are a heap pointer, not the data. Blitting the struct
        // would write a pointer into the save and read it back as garbage.
        void SaveCallback(SKSE::SerializationInterface* a_intf) {
            if (!a_intf->OpenRecord(kRecState, kVersion)) {
                logger::error("Could not open the save record — state not written");
                return;
            }

            a_intf->WriteRecordData(g_state.reincarnated);
            a_intf->WriteRecordData(g_state.power);
            a_intf->WriteRecordData(g_state.skills);
            a_intf->WriteRecordData(g_state.storageChest);

            const auto count = static_cast<std::uint32_t>(g_state.grantedMilestones.size());
            a_intf->WriteRecordData(count);
            for (const auto key : g_state.grantedMilestones) {
                a_intf->WriteRecordData(key);
            }

            const auto nodes = static_cast<std::uint32_t>(g_state.unlockedNodes.size());
            a_intf->WriteRecordData(nodes);
            for (const auto key : g_state.unlockedNodes) {
                a_intf->WriteRecordData(key);
            }

            a_intf->WriteRecordData(g_state.systemPoints);

            a_intf->WriteRecordData(g_state.shattered);  // v7

            const auto ranks = static_cast<std::uint32_t>(g_state.nodeRanks.size());  // v8
            a_intf->WriteRecordData(ranks);
            for (const auto& [key, rank] : g_state.nodeRanks) {
                a_intf->WriteRecordData(key);
                a_intf->WriteRecordData(rank);
            }

            a_intf->WriteRecordData(g_state.dormant);  // v9

            a_intf->WriteRecordData(g_state.grantTier);  // v10
            a_intf->WriteRecordData(g_state.treeTier);
            a_intf->WriteRecordData(g_state.custom);

            a_intf->WriteRecordData(g_state.questKey);  // v11
            a_intf->WriteRecordData(g_state.questProgress);

            a_intf->WriteRecordData(g_state.questTarget);  // v12
            a_intf->WriteRecordData(g_state.questReward);
            a_intf->WriteRecordData(g_state.questNextDue);

            a_intf->WriteRecordData(g_state.questsGiven);  // v13

            logger::info(
                "State saved (reincarnated={}, milestones={}, nodes={}, sp={}, shattered={}, "
                "dormant={}, custom={}, grant={}, tree={})",
                g_state.reincarnated, count, nodes, g_state.systemPoints, g_state.shattered,
                g_state.dormant, g_state.custom, PowerName(g_state.grantTier),
                PowerName(g_state.treeTier));
        }

        void LoadCallback(SKSE::SerializationInterface* a_intf) {
            std::uint32_t type = 0;
            std::uint32_t version = 0;
            std::uint32_t length = 0;

            while (a_intf->GetNextRecordInfo(type, version, length)) {
                if (type != kRecState) {
                    continue;
                }
                // Older layouts stay readable (each version only appended fields);
                // anything unknown is skipped rather than misread. The player then
                // loses the System's memory, not their character.
                if (version < 3 || version > kVersion) {
                    logger::warn("Save holds state version {} but we speak {} — ignoring it",
                                 version, kVersion);
                    continue;
                }

                a_intf->ReadRecordData(g_state.reincarnated);
                a_intf->ReadRecordData(g_state.power);
                a_intf->ReadRecordData(g_state.skills);

                if (version >= 4) {
                    a_intf->ReadRecordData(g_state.storageChest);
                    // A created reference keeps its FormID inside one save, but SKSE
                    // may still remap across load-order changes — resolve, don't assume.
                    RE::FormID resolved = 0;
                    if (g_state.storageChest != 0 &&
                        a_intf->ResolveFormID(g_state.storageChest, resolved)) {
                        g_state.storageChest = resolved;
                    }
                }

                std::uint32_t count = 0;
                a_intf->ReadRecordData(count);
                g_state.grantedMilestones.clear();
                g_state.grantedMilestones.reserve(count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    std::uint32_t key = 0;
                    a_intf->ReadRecordData(key);
                    g_state.grantedMilestones.push_back(key);
                }

                g_state.unlockedNodes.clear();
                if (version >= 5) {
                    std::uint32_t nodes = 0;
                    a_intf->ReadRecordData(nodes);
                    g_state.unlockedNodes.reserve(nodes);
                    for (std::uint32_t i = 0; i < nodes; ++i) {
                        std::uint32_t key = 0;
                        a_intf->ReadRecordData(key);
                        g_state.unlockedNodes.push_back(key);
                    }
                }

                g_state.systemPoints = 0;
                if (version >= 6) {
                    a_intf->ReadRecordData(g_state.systemPoints);
                }

                g_state.shattered = false;
                if (version >= 7) {
                    a_intf->ReadRecordData(g_state.shattered);
                }

                g_state.nodeRanks.clear();
                if (version >= 8) {
                    std::uint32_t ranks = 0;
                    a_intf->ReadRecordData(ranks);
                    g_state.nodeRanks.reserve(ranks);
                    for (std::uint32_t i = 0; i < ranks; ++i) {
                        std::uint32_t key = 0;
                        std::int32_t  rank = 0;
                        a_intf->ReadRecordData(key);
                        a_intf->ReadRecordData(rank);
                        g_state.nodeRanks.emplace_back(key, rank);
                    }
                }

                g_state.dormant = false;
                if (version >= 9) {
                    a_intf->ReadRecordData(g_state.dormant);
                }

                g_state.custom = false;
                if (version >= 10) {
                    a_intf->ReadRecordData(g_state.grantTier);
                    a_intf->ReadRecordData(g_state.treeTier);
                    a_intf->ReadRecordData(g_state.custom);
                } else {
                    // Before the axes were split, the tier drove all three: the tree
                    // opened to `power`, and the flat grant was `power` unless shattered.
                    // Reproduce that so an existing save behaves exactly as before.
                    g_state.treeTier = g_state.power;
                    g_state.grantTier = g_state.shattered ? PowerLevel::Normal : g_state.power;
                }

                // v11: the standing System objective. An older save has none, and
                // Quests::EnsureObjective (run on load) rolls the first one — so an
                // existing character picks up the feature without a reboot.
                g_state.questKey = 0;
                g_state.questProgress = 0;
                if (version >= 11) {
                    a_intf->ReadRecordData(g_state.questKey);
                    a_intf->ReadRecordData(g_state.questProgress);
                }

                // v12: the objective's own target and payout, and the clock for the next
                // one. A v11 save carries an objective whose numbers were read live from
                // the quarry table; leaving them at 0 makes Quests treat it as unsized and
                // re-size it once, which is exactly what should happen when the table's
                // numbers have just become level-dependent.
                g_state.questTarget = 0;
                g_state.questReward = 0;
                g_state.questNextDue = 0.0f;
                if (version >= 12) {
                    a_intf->ReadRecordData(g_state.questTarget);
                    a_intf->ReadRecordData(g_state.questReward);
                    a_intf->ReadRecordData(g_state.questNextDue);
                }

                // v13: how many objectives this character has been given. An older save
                // that already carries one counts as having had it — otherwise the next
                // load would treat a veteran as a beginner and hand out the starter hunt.
                g_state.questsGiven = 0;
                if (version >= 13) {
                    a_intf->ReadRecordData(g_state.questsGiven);
                } else if (g_state.questKey != 0) {
                    g_state.questsGiven = 1;
                }
            }

            logger::info(
                "State loaded (reincarnated={}, milestones={}, nodes={}, sp={}, shattered={}, "
                "dormant={}, custom={}, grant={}, tree={})",
                g_state.reincarnated, g_state.grantedMilestones.size(),
                g_state.unlockedNodes.size(), g_state.systemPoints, g_state.shattered,
                g_state.dormant, g_state.custom, PowerName(g_state.grantTier),
                PowerName(g_state.treeTier));
        }

        void RevertCallback(SKSE::SerializationInterface*) {
            // Before g_state is cleared: this is the one moment we still know what we
            // granted, and the last moment before the incoming save decides what the
            // character should be.
            RestoreLevelBeforeGrant();
            g_state = State{};
            g_firstReadyCell = 0;  // the next game gets a fresh chargen-room detection
            logger::info("State reverted to defaults (new game / pre-load)");
        }

        // Re-assert the blessing's character level after a load. The player's level does
        // not persist when set through actorData.level (it reverted to 1 on load), so we
        // rebuild it from the save's memory here — the flat start's level for the tier
        // whose grant was handed over (grantTier). NORMAL / SHATTERED / a gift-less CUSTOM
        // build granted no level, so this is a no-op there.
        void ReapplyLevelOnLoad() {
            if (!g_state.reincarnated) {
                return;
            }
            SetPlayerLevelAtLeast(BlessingFor(g_state.grantTier).playerLevel);
        }

        // ------------------------------------------------------------------
        // SKSE lifecycle
        // ------------------------------------------------------------------

        void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg) {
            switch (a_msg->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                Config::Load();
                Loc::Load();  // after Config: it reads Config::Language()
                Plugin::DumpForms();
                Passives::Install();
                Sounds::Install();
                Storage::Install();
                Shop::Install();         // resolves the System Shop's soul-gem catalog
                CraftHooks::Install();  // zero-transfer crafting (validation build for now)
                UI::Install();
                UI::Prisma::Install();  // optional web UI; no-op without the patch
                SkyrimNet::Install();   // optional AI-NPC context; no-op without SkyrimNet
                Progression::Install();
                Quests::Install();      // watches kills for the standing objective
                UI::InstallThreatLabels();  // the on/off key for the floating verdicts
                SelfTest::Install();    // diagnostic hotkey, off unless the ini sets one
                UI::LogHotkeys();       // the table the input handler actually consults

                if (auto* ui = RE::UI::GetSingleton()) {
                    ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuWatcher::GetSingleton());
                    logger::info("Menu watcher installed — reincarnation trigger armed");
                }
                if (auto* levels = RE::LevelIncrease::GetEventSource()) {
                    levels->AddEventSink(
                        static_cast<RE::BSTEventSink<RE::LevelIncrease::Event>*>(
                            MenuWatcher::GetSingleton()));
                    logger::info("Level watcher installed — dormant awakening armed");
                }
                break;

            // Both fire after the co-save has been read back, so grantedMilestones is
            // populated and the catch-up knows what it already owes.
            case SKSE::MessagingInterface::kPostLoadGame:
            case SKSE::MessagingInterface::kNewGame:
                Progression::CatchUpOnLoad();
                // A dormant blessing that came due while the mod was off (or whose
                // threshold the ini has just lowered) is settled on load.
                CheckDormantAwakening();
                // Before anything reads the unlocked list: drop nodes this build no longer
                // has and hand their cost back, so a save that bought one is not left
                // holding points spent on nothing.
                SkillTree::RetireRemovedNodes();
                // Knowledge unlocks and the shout-cooldown value are re-derived from
                // the unlocked node list, same reasoning as the ability magnitudes.
                SkillTree::ApplyOnLoad();
                // Ability magnitudes live in the plugin, not the save, so they come back
                // as whatever the ESP says (zero) on every load. Rebuild them from the
                // milestones the save *does* remember.
                Passives::Refresh();
                // The character level doesn't persist for the player either — re-assert
                // the blessing's level so it stops dropping to 1 on load.
                ReapplyLevelOnLoad();
                // Chests stocked by earlier builds still hold the Creation Club
                // ingredients that made the crafting shuttle stutter — clean them out.
                Storage::PruneForeignStock();
                // NOTE: no top-up here any more. It used to add every material type the
                // chest was missing on each load — which, now that the chest starts
                // empty, would quietly refill it for free and defeat buying materials
                // with System Points. Older saves keep what they already hold.
                // Sweep out orphaned chest husks earlier rebuilds left behind (save bloat).
                Storage::PruneOrphanChests();
                // Backfill the codex token for saves reincarnated before it existed.
                Storage::GrantCodexIfMissing();
                // Hand out a standing objective — including to characters from before
                // System Quests existed, whose save simply carries none.
                Quests::EnsureObjective();
                // Last, once everything above has settled: write the diagnostic report.
                // It runs on a task, so it does not sit inside the engine's post-load pass.
                SelfTest::RunOnLoad();
                break;

            default:
                break;
            }
        }
    }

    void TriggerReincarnation() {
        if (g_state.reincarnated) {
            return;
        }
        // No cell heuristic here, unlike TryTrigger: a player who presses the key has
        // told us they are where they want the System to find them, which is the whole
        // point of AutoStart = 0. Readiness is still checked — a boot sequence during
        // character creation would be no more welcome for having been asked for.
        if (!IsPlayerReady()) {
            logger::info("Reincarnation requested by hotkey, but the player is not ready yet");
            return;
        }
        BeginReincarnation();
    }

    std::string PowerName(PowerLevel a_power) {
        switch (a_power) {
        case PowerLevel::Hero:
            return "HERO";
        case PowerLevel::Ascended:
            return "ASCENDED";
        default:
            return "NORMAL";
        }
    }

    void CheckDormantAwakening() {
        if (!g_state.dormant || !g_state.reincarnated) {
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }
        const auto earned = EarnedTier(player->GetLevel());
        if (static_cast<int>(earned) <= static_cast<int>(g_state.power)) {
            return;
        }
        // Never awaken over a live panel — the reincarnation dialog owns a callback
        // that a replacement would throw away, leaving the rebirth half-finished. The
        // menu-close watcher brings us straight back here.
        if (UI::IsSystemScreenOpen()) {
            return;
        }
        AwakenTo(earned);
    }

    void RebootSystem() {
        // Just re-run the blessing selection. ShowPowerSelection overwrites the blessing
        // fields (power / shattered / dormant) from the new choice and re-applies, while
        // leaving reincarnated, milestones, the skill tree and System Points untouched —
        // so nothing earned is lost and the milestone catch-up cannot double-grant. No
        // need to reset the one-shot flag: we deliberately bypass the boot trigger and
        // open the menu directly.
        logger::info("System reboot — re-opening blessing choice (earned progress kept)");
        ShowPowerSelection();
    }

    std::uint16_t AwakeningLevelFor(PowerLevel a_tier) {
        if (!g_state.dormant) {
            return 0;
        }
        switch (a_tier) {
        case PowerLevel::Hero:
            return Config::DormantHeroLevel();
        case PowerLevel::Ascended:
            return Config::DormantAscendedLevel();
        default:  // NORMAL is where everyone starts — nothing to wait for
            return 0;
        }
    }

    std::uint16_t NextAwakeningLevel() {
        switch (g_state.power) {
        case PowerLevel::Normal:
            return AwakeningLevelFor(PowerLevel::Hero);
        case PowerLevel::Hero:
            return AwakeningLevelFor(PowerLevel::Ascended);
        default:  // Ascended — the ladder is done
            return 0;
        }
    }

    void DelayedMainThread(std::uint32_t a_ms, std::function<void()> a_fn) {
        std::thread([a_ms, fn = std::move(a_fn)]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(a_ms));
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([fn = std::move(fn)]() { fn(); });
            }
        }).detach();
    }

    State& GetState() {
        return g_state;
    }

    float RewardScale() {
        switch (g_state.power) {
        case PowerLevel::Hero:
            return 2.0f;
        case PowerLevel::Ascended:
            return 4.0f;
        default:  // Normal is the baseline the table is written against
            return 1.0f;
        }
    }

    std::int32_t MilestonePerkPoints() {
        // The same amount the blessing itself paid: the choice made at the start keeps
        // paying out at every endpoint. NORMAL took no blessing, so it earns no perk
        // points here either — it grows through passives alone.
        return BlessingFor(g_state.power).perkPoints;
    }

    void GrantPerkPoints(std::int32_t a_points) {
        if (a_points <= 0) {
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }
        auto& stats = player->GetGameStatsData();
        // Unsigned read: the declared int8 turns 200+ points into negative numbers.
        const auto held = static_cast<std::int32_t>(static_cast<std::uint8_t>(stats.perkCount));
        const auto total = std::min(held + a_points, kMaxPerkPoints);
        stats.perkCount = static_cast<std::int8_t>(static_cast<std::uint8_t>(total));
    }

    void GrantSystemPoints(std::int32_t a_points) {
        if (a_points > 0) {
            g_state.systemPoints += a_points;
        }
    }

    std::int32_t MilestoneSystemPoints(bool a_endpoint) {
        const auto base = a_endpoint ? 5.0f : 1.0f;
        return static_cast<std::int32_t>(std::lround(base * RewardScale()));
    }

    void GrantDragonSouls(std::int32_t a_souls) {
        if (a_souls <= 0) {
            return;
        }
        // The unspent soul pool is just an actor value — no special API, and no cap
        // to fight, unlike perk points.
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* avOwner = player ? player->AsActorValueOwner() : nullptr;
        if (!avOwner) {
            return;
        }
        const float have = avOwner->GetBaseActorValue(RE::ActorValue::kDragonSouls);
        avOwner->SetBaseActorValue(RE::ActorValue::kDragonSouls,
                                   have + static_cast<float>(a_souls));
    }

    void Install() {
        auto* serial = SKSE::GetSerializationInterface();
        serial->SetUniqueID(kSerID);
        serial->SetSaveCallback(SaveCallback);
        serial->SetLoadCallback(LoadCallback);
        serial->SetRevertCallback(RevertCallback);

        if (auto* messaging = SKSE::GetMessagingInterface()) {
            messaging->RegisterListener(OnSKSEMessage);
        }

        logger::info("Isekai System installed (serialization + trigger)");
    }
}
