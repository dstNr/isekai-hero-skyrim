#include "Analyze.h"

#include "Config.h"
#include "SkillTree.h"
#include "UI/Input.h"
#include "UI/SkillTreeWindow.h"
#include "UI/SystemWindow.h"

#include <string>
#include <vector>

namespace Isekai::Analyze {

    namespace {
        // a_diff = target level - player level. Four bands are enough to read as a
        // System verdict without pretending to be a precise combat calculator.
        [[nodiscard]] const char* ThreatTag(std::int32_t a_diff) {
            if (a_diff >= 20) {
                return "LETHAL";
            }
            if (a_diff >= 5) {
                return "DANGEROUS";
            }
            if (a_diff <= -15) {
                return "TRIVIAL";
            }
            return "MANAGEABLE";
        }

        void RunScan() {
            // Same guard ShowStatusPanel uses: never act while one of our own panels is
            // up, or while any game menu holds the keyboard (inventory search, console,
            // dialogue — the hotkey fires "whether or not a panel is open", so this is
            // the only thing stopping it from firing into one).
            if (UI::IsSystemWindowOpen() || UI::IsSkillTreeOpen()) {
                return;
            }
            auto* ui = RE::UI::GetSingleton();
            if (ui && ui->GameIsPaused()) {
                return;
            }

            if (!SkillTree::IsUnlocked(SkillTree::kAnalyzeNodeKey)) {
                RE::DebugNotification(
                    "[ SYSTEM ] Analysis not yet unlocked - buy System Analysis in the skill tree.");
                return;
            }

            // CrosshairPickData resolves through a 2-argument RELOCATION_ID (SE/AE only;
            // under VR the id() lookup falls back to the SE id in a database built for a
            // different binary, and may not resolve at all). Unverified in a real VR
            // session, so — same call we made for the ImGui overlay — stay out of it
            // until it can actually be tested there instead of guessing.
            if (REL::Module::IsVR()) {
                RE::DebugNotification("[ SYSTEM ] Analysis is not yet available in VR.");
                return;
            }

            auto* pick = RE::CrosshairPickData::GetSingleton();
            RE::TESObjectREFR* target = pick ? pick->target.get().get() : nullptr;
            if (!target) {
                RE::DebugNotification("[ SYSTEM ] Nothing in view to analyze.");
                return;
            }

            std::string body =
                "TARGET         " + std::string(target->GetDisplayFullName()) + "\n\n";

            if (auto* actor = target->As<RE::Actor>()) {
                auto*      player = RE::PlayerCharacter::GetSingleton();
                const auto targetLevel = actor->GetLevel();

                body += "Level          " + std::to_string(targetLevel) + "\n";
                if (auto* avOwner = actor->AsActorValueOwner()) {
                    const auto hp = avOwner->GetActorValue(RE::ActorValue::kHealth);
                    // Permanent (base) rather than current: an approximation of "full
                    // health" that ignores temporary fortify/drain effects, the same
                    // idiom other SKSE plugins use for this — there is no dedicated
                    // "max health" getter.
                    const auto hpMax = avOwner->GetPermanentActorValue(RE::ActorValue::kHealth);
                    body += "Health         " + std::to_string(static_cast<int>(hp)) + " / " +
                            std::to_string(static_cast<int>(hpMax)) + "\n";
                }
                if (player && actor->IsHostileToActor(player)) {
                    body += "Disposition    HOSTILE\n";
                }

                const auto playerLevel = player ? player->GetLevel() : 1;
                const auto diff = static_cast<std::int32_t>(targetLevel) -
                                  static_cast<std::int32_t>(playerLevel);
                body += "\nTHREAT: " + std::string(ThreatTag(diff));
            } else {
                body += "An inanimate object.\nNothing more for the System to glean.";
            }

            UI::ShowSystemWindow("[ SYSTEM ] ANALYSIS", body, std::vector<std::string>{ "CONTINUE" },
                                 [](int) {});
        }
    }

    void Install() {
        UI::RegisterHotkey(Config::AnalyzeKey(), RunScan);
        logger::info("Analyze: hotkey registered (key={:#x})", Config::AnalyzeKey());
    }
}
