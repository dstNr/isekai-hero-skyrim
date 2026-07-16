#include "Storage.h"

#include "Plugin.h"
#include "Sounds.h"
#include "System.h"
#include "UI/Input.h"
#include "UI/SystemWindow.h"

namespace Isekai::Storage {

    namespace {
        // The container base object in IsekaiHero.esp ("Dimensional Storage",
        // read out of the form dump like everything else in that file).
        constexpr RE::FormID kContainerBase = 0x000D7A;

        constexpr std::uint32_t kStorageKey = 0x43;  // DIK_F9

        RE::TESObjectCONT* g_base = nullptr;

        // The one chest reference, created on first use.
        //
        // There is no reference in the ESP on purpose: Skyrim's CK cannot mark a
        // reference persistent, and a non-persistent one only exists while its cell
        // is loaded — unreachable from anywhere else. PlaceObjectAtMe with
        // forcePersist creates a reference the save system tracks properly; its
        // FormID lives in our co-save (State::storageChest).
        [[nodiscard]] RE::TESObjectREFR* ResolveChest() {
            auto& state = GetState();
            if (state.storageChest == 0) {
                return nullptr;
            }
            auto* chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(state.storageChest);
            if (!chest) {
                // The save lost it (mangled by a save cleaner, most likely). Recreate
                // rather than dangle — the contents are gone either way, but the
                // feature keeps working.
                logger::warn("Storage: chest {:#x} vanished from the save — starting a new one",
                             state.storageChest);
                state.storageChest = 0;
            }
            return chest;
        }

        [[nodiscard]] RE::TESObjectREFR* GetOrCreateChest(RE::PlayerCharacter* a_player) {
            if (auto* chest = ResolveChest()) {
                return chest;
            }
            if (!g_base) {
                return nullptr;
            }

            const auto chest = a_player->PlaceObjectAtMe(g_base, /*forcePersist=*/true);
            if (!chest) {
                logger::error("Storage: PlaceObjectAtMe failed");
                return nullptr;
            }

            GetState().storageChest = chest->GetFormID();
            logger::info("Storage: chest created ({:#x})", chest->GetFormID());
            return chest.get();
        }
    }

    void Open() {
        // Not while a System panel holds the game, and not while the game is paused
        // in some other menu — opening a container on top of either is asking for
        // stuck input states.
        auto* ui = RE::UI::GetSingleton();
        if (UI::IsSystemWindowOpen() || !ui || ui->GameIsPaused()) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) {
            return;
        }

        auto* chest = GetOrCreateChest(player);
        if (!chest) {
            return;
        }

        // Keep the chest in the player's cell (so it is loaded and activatable), but
        // far below the floor, where its model can never be seen. The activation is a
        // direct call, not a look-at, so where it sits makes no difference.
        chest->MoveTo(player);
        const auto pos = player->GetPosition();
        chest->SetPosition(pos.x, pos.y, pos.z - 3000.0f);

        Sounds::Play(Sounds::Sfx::WindowOpen);
        chest->ActivateRef(player, 0, nullptr, 1, false);
    }

    void Install() {
        if (!Plugin::IsLoaded()) {
            logger::warn("Storage: {} not loaded — dimensional storage is off",
                         Plugin::kFileName);
            return;
        }

        auto* data = RE::TESDataHandler::GetSingleton();
        g_base = data ? data->LookupForm<RE::TESObjectCONT>(kContainerBase, Plugin::kFileName)
                      : nullptr;
        if (!g_base) {
            logger::error("Storage: no container {:#08x} in {}", kContainerBase,
                          Plugin::kFileName);
            return;
        }

        UI::RegisterHotkey(kStorageKey, []() { Open(); });
        logger::info("Storage: F9 opens the dimensional storage");
    }
}
