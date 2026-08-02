#include "Storage.h"

#include "CraftHooks.h"
#include "Plugin.h"
#include "Sounds.h"
#include "System.h"
#include "UI/SystemWindow.h"

#include <map>
#include <set>
#include <string_view>

namespace Isekai::Storage {

    namespace {
        // The container base object in IsekaiHero.esp ("Dimensional Storage",
        // read out of the plugin file itself).
        constexpr RE::FormID kContainerBase = 0x000D7A;

        // The storage codex — "IsekaiStorageToken", an ALCH item that already sat unused
        // in the ESP (docs/CREATION_KIT_ESP.md Part G was written, the C++ side of it
        // never was). A physical fallback: "drink" it like a potion and the chest opens,
        // no System panel needed.
        constexpr RE::FormID kCodexToken = 0x000D7F;

        RE::TESObjectCONT* g_base = nullptr;
        RE::TESBoundObject* g_codexToken = nullptr;

        // What the crafting menu borrowed from storage, per base object. Never
        // persisted: the crafting menu pauses the game and cannot outlive a session.
        std::map<RE::TESBoundObject*, std::int32_t> g_craftLoan;

        // Every reincarnated soul gets the pocket dimension — the panel button, the
        // chest, the crafting-station lending. It starts EMPTY for every blessing now;
        // materials are bought from the System Shop (see StockCategory), so no tier is
        // handed a crate it did not spend points on.
        [[nodiscard]] bool IsEligible() {
            return GetState().reincarnated;
        }

        // The official game masters check moved to Plugin::IsOfficialMaster (shared with
        // CraftHooks/Shop — used to be duplicated identically). We freely stock and keep
        // their ingredients; anything else (Creation Club, mods) is held at arm's length
        // for ingredients specifically, because CC ingredients ship tracker scripts that
        // flood the VM when handled in bulk — the very stutter this feature was rebuilt to
        // avoid. Misc materials carry no such scripts, but StockCategory keeps them to
        // the official masters too, so a pack's contents stay predictable.
        using Plugin::IsOfficialMaster;

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
            if (!chest || chest->IsDeleted()) {
                // The save lost it (mangled by a save cleaner, most likely). Recreate
                // rather than dangle — the contents are gone either way, but the
                // feature keeps working. (A merely disabled/orphaned ref is NOT lost —
                // that is repaired in Open, keeping the contents.)
                logger::warn("Storage: chest {:#x} vanished from the save — starting a new one",
                             state.storageChest);
                state.storageChest = 0;
                return nullptr;
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

            // Bury it right away. PlaceObjectAtMe drops it at the player's feet, and
            // since the stocking happens during reincarnation — not on first open —
            // it would otherwise stand there in plain sight until first used.
            const auto pos = a_player->GetPosition();
            chest->SetPosition(pos.x, pos.y, pos.z - 3000.0f);

            GetState().storageChest = chest->GetFormID();
            logger::info("Storage: chest created ({:#x})", chest->GetFormID());
            return chest.get();
        }

        // Rebuild the chest when its reference has been orphaned — disabled or stripped of
        // its 3D by a cell reset. Reported after finishing the Dragonborn questline: many
        // in-game days pass without opening the storage, the cell it last sat in resets,
        // and ActivateRef then opens a container menu that instantly closes ("throws me
        // back to the game"). CommonLibSSE exposes no Enable(), so instead of resurrecting
        // the husk we move its inventory — which lives in the reference data, not the 3D,
        // so it survives — into a fresh reference and repoint the co-save at it.
        //
        // The old reference is then disabled AND marked for deletion. Earlier builds only
        // disabled it, so every rebuild left a dormant husk behind; over a long save these
        // pile up as orphaned created references (save bloat, and the kind of "unattached"
        // entries a save cleaner like ReSaver flags). Disable() + SetDelete() is the proper
        // removal, the same as Papyrus Disable()/Delete().
        [[nodiscard]] RE::TESObjectREFR* RebuildChest(RE::PlayerCharacter* a_player,
                                                      RE::TESObjectREFR* a_old) {
            if (!g_base) {
                return nullptr;
            }
            const auto fresh = a_player->PlaceObjectAtMe(g_base, /*forcePersist=*/true);
            if (!fresh) {
                logger::error("Storage: rebuild PlaceObjectAtMe failed");
                return nullptr;
            }

            std::size_t moved = 0;
            if (a_old) {
                // a_old->RemoveItem re-enters the CraftHooks RemoveItem hook with the OLD
                // chest as `this` (not the player) — it passes straight through.
                for (const auto& [obj, count] : a_old->GetInventoryCounts()) {
                    if (obj && count > 0) {
                        a_old->RemoveItem(obj, count, RE::ITEM_REMOVE_REASON::kStoreInContainer,
                                          nullptr, fresh.get());
                        ++moved;
                    }
                }
                a_old->Disable();
                a_old->SetDelete(true);  // actually remove it — don't leave a dormant husk
            }

            const auto pos = a_player->GetPosition();
            fresh->SetPosition(pos.x, pos.y, pos.z - 3000.0f);
            GetState().storageChest = fresh->GetFormID();
            logger::warn("Storage: rebuilt orphaned chest -> {:#x}, migrated {} stack(s)",
                         fresh->GetFormID(), moved);
            return fresh.get();
        }

        // ------------------------------------------------------------------
        // Crafting: lend the storage to the player while a crafting menu is open.
        // Vanilla crafting only ever looks at the player's inventory, so the
        // materials walk over for the duration and the leftovers walk back.
        // ------------------------------------------------------------------

        using BenchType = RE::TESFurniture::WorkBenchData::BenchType;

        // Which workbench the player is sitting at when the crafting menu opens.
        [[nodiscard]] BenchType CurrentBenchType() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return BenchType::kNone;
            }
            const auto furniture = player->GetOccupiedFurniture().get();
            if (!furniture || !furniture->GetBaseObject()) {
                return BenchType::kNone;
            }
            const auto* base = furniture->GetBaseObject()->As<RE::TESFurniture>();
            return base ? *base->workBenchData.benchType : BenchType::kNone;
        }

        // Only what THIS station can actually consume. Every stack moved is a
        // container event fanned out to every listening script, and the flat
        // "lend everything" version fed the forge 191 ingredient stacks it could
        // not use — enough VM traffic to stutter for minutes. Gold, gear, potions
        // and whatever else the player parked in the chest never move at all.
        [[nodiscard]] bool StationWantsItem(BenchType a_bench, const RE::TESBoundObject* a_obj) {
            if (a_obj->GetFormID() == 0x0000000F) {
                return false;  // Gold001 is technically Misc, but no recipe eats coins
            }
            switch (a_obj->GetFormType()) {
            case RE::FormType::Ingredient:
                return a_bench == BenchType::kAlchemy ||
                       a_bench == BenchType::kAlchemyExperiment || a_bench == BenchType::kNone;
            case RE::FormType::SoulGem:
                return a_bench == BenchType::kEnchanting ||
                       a_bench == BenchType::kEnchantingExperiment || a_bench == BenchType::kNone;
            case RE::FormType::Misc:
                // Smithing, tempering, smelting, tanning — everything item-shaped.
                return a_bench != BenchType::kAlchemy && a_bench != BenchType::kAlchemyExperiment &&
                       a_bench != BenchType::kEnchanting &&
                       a_bench != BenchType::kEnchantingExperiment;
            default:
                return false;
            }
        }

        // The item-crafting stations (forge/smelter/tanning/grindstone/armour bench)
        // are handled by the zero-transfer CraftHooks now — they read and consume the
        // chest in place. Only alchemy and enchanting still shuttle, because their
        // menus iterate the inventory (a hook path we have not built yet).
        [[nodiscard]] bool ShuttleStation(BenchType a_bench) {
            // The in-place CraftHooks cover every crafting menu now. The shuttle only
            // survives as a fallback for the (unlikely) case the core hooks failed to
            // install — then it lends at any crafting bench.
            if (CraftHooks::ItemCraftingHooksActive()) {
                return false;
            }
            switch (a_bench) {
            case BenchType::kAlchemy:
            case BenchType::kAlchemyExperiment:
            case BenchType::kEnchanting:
            case BenchType::kEnchantingExperiment:
            case BenchType::kCreateObject:
            case BenchType::kSmithingWeapon:
            case BenchType::kSmithingArmor:
                return true;
            default:
                return false;
            }
        }

        void LendToPlayer() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* chest = ResolveChest();
            if (!player || !chest || !IsEligible()) {
                return;
            }

            const auto bench = CurrentBenchType();
            if (!ShuttleStation(bench)) {
                return;  // CraftHooks covers item crafting; nothing to shuttle here
            }

            g_craftLoan.clear();
            for (const auto& [obj, count] : chest->GetInventoryCounts()) {
                if (!obj || count <= 0 || !StationWantsItem(bench, obj)) {
                    continue;
                }
                g_craftLoan[obj] = count;
                chest->RemoveItem(obj, count, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr,
                                  player);
            }

            if (!g_craftLoan.empty()) {
                logger::info("Storage: lent {} stack(s) to bench type {}", g_craftLoan.size(),
                             static_cast<int>(bench));
            }
        }

        // Return min(borrowed, still held): whatever crafting consumed stays
        // consumed, whatever it produced stays with the player, and the player's
        // own pre-existing materials never get swept into the chest.
        void TakeBack() {
            if (g_craftLoan.empty()) {
                return;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* chest = ResolveChest();
            if (!player || !chest) {
                g_craftLoan.clear();
                return;
            }

            const auto held = player->GetInventoryCounts();
            for (const auto& [obj, borrowed] : g_craftLoan) {
                const auto it = held.find(obj);
                const auto stillHeld = it != held.end() ? it->second : 0;
                const auto giveBack = std::min(borrowed, stillHeld);
                if (giveBack > 0) {
                    player->RemoveItem(obj, giveBack, RE::ITEM_REMOVE_REASON::kStoreInContainer,
                                       nullptr, chest);
                }
            }
            logger::info("Storage: took back {} stack(s) from the crafting menu",
                         g_craftLoan.size());
            g_craftLoan.clear();
        }

        class CraftWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        public:
            static CraftWatcher* GetSingleton() {
                static CraftWatcher singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (a_event && a_event->menuName == RE::CraftingMenu::MENU_NAME) {
                    if (a_event->opening) {
                        LendToPlayer();
                    } else {
                        TakeBack();
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            CraftWatcher() = default;
        };

        // Ingestibles have no "activate" of their own — "using" one from the inventory
        // menu is, in the engine's own model, momentarily equipping it, so this is the
        // native equivalent of the well-known Papyrus OnObjectEquipped quirk (it fires
        // for potions too, and by the time it does the item is already consumed). We
        // undo that consumption immediately: hand back the same token, so from the
        // player's side it reads as "drink it, the chest opens, the token never runs
        // out" rather than a one-time-use item.
        class CodexWatcher : public RE::BSTEventSink<RE::TESEquipEvent> {
        public:
            static CodexWatcher* GetSingleton() {
                static CodexWatcher singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESEquipEvent* a_event,
                RE::BSTEventSource<RE::TESEquipEvent>*) override {
                if (a_event && a_event->equipped && g_codexToken &&
                    a_event->baseObject == g_codexToken->GetFormID()) {
                    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                        player->AddObjectToContainer(g_codexToken, nullptr, 1, nullptr);
                    }
                    Open();
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            CodexWatcher() = default;
        };
    }

    void GrantCodexIfMissing() {
        if (!g_codexToken || !GetState().reincarnated) {
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }
        const auto counts = player->GetInventoryCounts();
        if (counts.contains(g_codexToken)) {
            return;  // already carries one — nothing to do
        }
        player->AddObjectToContainer(g_codexToken, nullptr, 1, nullptr);
        logger::info("Storage: granted the storage codex token");
    }

    bool Available() {
        return g_base != nullptr && IsEligible();
    }

    void PruneForeignStock() {
        auto* chest = ResolveChest();
        if (!chest) {
            return;
        }

        // Drops every ingredient AND misc material that isn't from an official master —
        // the Creation Club / mod ones whose scripts misbehave in bulk (CC ingredient
        // trackers; modded deployables like bear traps that spawn copies of themselves).
        // Earlier builds stocked modded recipe materials; this sweeps them back out of an
        // existing chest on load. DLC content (corkbulb root, chitin plate, ...) is an
        // official master and stays. Soul gems/gold are left untouched.
        //
        // NOTE before any public release: this also deletes non-official items a player
        // might have stored deliberately. Fine while the only chests in existence are our
        // own dev saves; needs a one-time migration flag instead if strangers' savegames
        // ever enter the picture.
        std::size_t pruned = 0;
        for (const auto& [obj, count] : chest->GetInventoryCounts()) {
            if (!obj || count <= 0) {
                continue;
            }
            const auto type = obj->GetFormType();
            if (type != RE::FormType::Ingredient && type != RE::FormType::Misc) {
                continue;  // only materials; gear/gold/soul gems are the player's to keep
            }
            if (obj->GetFormID() == 0x0000000F || IsOfficialMaster(obj)) {
                continue;  // gold and all Skyrim + DLC materials stay
            }
            chest->RemoveItem(obj, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            ++pruned;
        }

        if (pruned > 0) {
            logger::info("Storage: pruned {} foreign material stack(s) from the chest", pruned);
        }
    }

    void PruneOrphanChests() {
        if (!g_base) {
            return;
        }
        const RE::FormID keep = GetState().storageChest;  // the one still in use

        // Collect first, delete after: Disable()/SetDelete() mutate reference state, which
        // must not happen while the global form list is read-locked.
        std::vector<RE::TESObjectREFR*> orphans;
        {
            const auto& [all, lock] = RE::TESForm::GetAllForms();
            const RE::BSReadLockGuard guard{ lock };
            for (const auto& [id, form] : *all) {
                if (!form || id == keep) {
                    continue;
                }
                auto* ref = form->As<RE::TESObjectREFR>();
                if (ref && ref->GetBaseObject() == g_base) {
                    orphans.push_back(ref);
                }
            }
        }

        for (auto* ref : orphans) {
            ref->Disable();
            ref->SetDelete(true);
        }
        if (!orphans.empty()) {
            logger::info("Storage: removed {} orphaned chest husk(s) left by earlier rebuilds",
                         orphans.size());
        }
    }

    std::int32_t ChestCount(RE::TESBoundObject* a_obj) {
        if (!a_obj) {
            return 0;
        }
        auto* chest = ResolveChest();
        if (!chest) {
            return 0;
        }
        const auto counts = chest->GetInventoryCounts();
        const auto it = counts.find(a_obj);
        return it != counts.end() ? it->second : 0;
    }

    std::int32_t RemoveFromChest(RE::TESBoundObject* a_obj, std::int32_t a_count) {
        if (!a_obj || a_count <= 0) {
            return 0;
        }
        auto* chest = ResolveChest();
        if (!chest) {
            return 0;
        }
        const auto have = ChestCount(a_obj);
        const auto toRemove = std::min(a_count, have);
        if (toRemove <= 0) {
            return 0;
        }
        // Straight destroy: the crafting recipe consumes these. No moveTo, no extra
        // list (crafting materials carry none). chest->RemoveItem re-enters the
        // RemoveItem vtable hook with the chest as `this`, which passes through — see
        // CraftHooks.
        chest->RemoveItem(a_obj, toRemove, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        return toRemove;
    }

    RE::TESObjectREFR* ChestRef() {
        return ResolveChest();
    }

    // The chest, created on demand. The shop needs this: the chest is no longer made at
    // reincarnation, so a player who buys a material pack before ever opening the storage
    // would otherwise be told the storage "is not ready".
    static RE::TESObjectREFR* EnsureChest() {
        if (auto* chest = ResolveChest()) {
            return chest;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        return player ? GetOrCreateChest(player) : nullptr;
    }

    bool Deliver(RE::TESBoundObject* a_obj, std::int32_t a_count) {
        if (!a_obj || a_count <= 0) {
            return false;
        }
        auto* chest = EnsureChest();
        if (!chest) {
            return false;
        }
        chest->AddObjectToContainer(a_obj, nullptr, a_count, nullptr);
        return true;
    }

    std::size_t StockCategory(MaterialCategory a_category, std::int32_t a_perItem) {
        if (a_perItem <= 0) {
            return 0;
        }
        auto* chest = EnsureChest();
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!chest || !data) {
            return 0;
        }

        // Deduplicated: a form reached by two paths (a gem that is both hand-picked and a
        // recipe component) is added exactly once per purchase.
        std::set<RE::TESBoundObject*> seen;
        std::size_t                   stocked = 0;
        const auto stock = [&](RE::TESBoundObject* obj) {
            if (!obj || !seen.insert(obj).second) {
                return;
            }
            chest->AddObjectToContainer(obj, nullptr, a_perItem, nullptr);
            ++stocked;
        };

        switch (a_category) {
        case MaterialCategory::kSmithing: {
            // Every FormID below was read out of Skyrim.esm itself (MISC group), not
            // quoted from memory. This hand-picked core exists because the recipe sweep
            // that follows would miss the FLAWLESS gems — the enchanting table's
            // "empower" uses them and no forge recipe requires them.
            constexpr RE::FormID kCore[] = {
                0x0005ACE4,  // IngotIron
                0x0005ACE5,  // IngotSteel
                0x0005AD93,  // IngotCorundum
                0x0005AD99,  // IngotOrichalcum
                0x000DB8A2,  // IngotDwarven
                0x0005AD9F,  // IngotIMoonstone (refined moonstone)
                0x0005ADA0,  // IngotQuicksilver
                0x0005ADA1,  // IngotMalachite
                0x0005AD9D,  // IngotEbony
                0x0005AD9E,  // IngotGold
                0x0005ACE3,  // ingotSilver
                0x000DB5D2,  // Leather01
                0x000800E4,  // LeatherStrips
                0x0006F993,  // Firewood01
                0x00033760,  // Charcoal
                0x0003ADA4,  // DragonBone
                0x0003ADA3,  // DragonScales
                0x00063B46,  // GemAmethyst
                0x00063B47,  // GemDiamond
                0x00063B43,  // GemEmerald
                0x00063B45,  // GemGarnet
                0x00063B42,  // GemRuby
                0x00063B44,  // GemSapphire
                0x0006851E,  // GemAmethystFlawless
                0x0006851F,  // GemDiamondFlawless
                0x00068520,  // GemEmeraldFlawless
                0x00068521,  // GemGarnetFlawless
                0x00068522,  // gemRubyFlawless
                0x00068523,  // GemSapphireFlawless
            };
            for (const auto id : kCore) {
                // Look up under the concrete form type. TESBoundObject would be the
                // natural common base, but it has no FORMTYPE, so a typed lookup on it
                // rejects everything.
                stock(data->LookupForm<RE::TESObjectMISC>(id, "Skyrim.esm"));
            }

            // Every MISC material an OFFICIAL-master recipe requires, sourced from the
            // recipe components rather than a blind "all Misc", so only things a recipe
            // actually consumes are stocked — and so DLC materials come along (chitin
            // plate, netch leather). Restricted to official masters on purpose: a modded
            // crafting material can be a scripted deployable (bear traps, campfire kit)
            // whose OnContainerChanged handler spawns copies of itself and floods the VM.
            for (auto* cobj : data->GetFormArray<RE::BGSConstructibleObject>()) {
                if (!cobj) {
                    continue;
                }
                cobj->requiredItems.ForEachContainerObject([&](RE::ContainerObject& a_c) {
                    auto* obj = a_c.obj;
                    if (obj && IsOfficialMaster(obj) &&
                        obj->GetFormType() == RE::FormType::Misc) {
                        if (const char* name = obj->GetName(); name && *name) {
                            stock(obj);
                        }
                    }
                    return RE::BSContainer::ForEachResult::kContinue;
                });
            }
            break;
        }

        case MaterialCategory::kAlchemy:
            // Every ingredient from the official masters (Skyrim + DLC — corkbulb root,
            // ash yam, ...), enumerated rather than hand-listed. CC/mod ingredients stay
            // out for the tracker-script reason above.
            for (auto* ingredient : data->GetFormArray<RE::IngredientItem>()) {
                if (!ingredient || !IsOfficialMaster(ingredient)) {
                    continue;
                }
                if (const char* name = ingredient->GetName(); !name || !*name) {
                    continue;  // nameless = internal/test records, not for players
                }
                stock(ingredient);
            }
            break;

        case MaterialCategory::kEnchanting:
            // Every FILLED soul gem base form from the official masters (petty .. grand
            // + black). Empty gems can't power an enchantment, so they stay out.
            for (auto* gem : data->GetFormArray<RE::TESSoulGem>()) {
                if (!gem || !IsOfficialMaster(gem)) {
                    continue;
                }
                if (gem->GetContainedSoul() == RE::SOUL_LEVEL::kNone) {
                    continue;
                }
                stock(gem);
            }
            break;
        }

        logger::info("Storage: stocked {} stack(s) x{} for category {}", stocked, a_perItem,
                     static_cast<int>(a_category));
        return stocked;
    }

    void Open() {
        if (UI::IsSystemWindowOpen()) {
            return;
        }
        if (!IsEligible()) {
            RE::DebugNotification("[ SYSTEM ] ACCESS DENIED — the System is not yet bound to you.");
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

        // A ref a cell reset left disabled can't host a container menu — repair it (moves
        // the contents to a fresh ref) before trying to move/activate the husk.
        if (chest->IsDisabled()) {
            logger::warn("Storage: chest {:#x} disabled at open — rebuilding", chest->GetFormID());
            chest = RebuildChest(player, chest);
            if (!chest) {
                RE::DebugNotification("[ SYSTEM ] storage is re-anchoring — try again in a moment.");
                return;
            }
        }

        // Keep the chest in the player's cell (so it is loaded and activatable), but
        // far below the floor, where its model can never be seen. The activation is a
        // direct call, not a look-at, so where it sits makes no difference.
        chest->MoveTo(player);
        auto pos = player->GetPosition();
        chest->SetPosition(pos.x, pos.y, pos.z - 3000.0f);

        // If the move still didn't attach 3D, the ref is orphaned some other way — rebuild
        // and place the fresh one, which PlaceObjectAtMe drops into the loaded cell.
        if (!chest->Is3DLoaded()) {
            logger::warn("Storage: chest {:#x} has no 3D after move — rebuilding",
                         chest->GetFormID());
            chest = RebuildChest(player, chest);
            if (!chest) {
                RE::DebugNotification("[ SYSTEM ] storage is re-anchoring — try again in a moment.");
                return;
            }
            chest->MoveTo(player);
            pos = player->GetPosition();
            chest->SetPosition(pos.x, pos.y, pos.z - 3000.0f);
        }

        Sounds::Play(Sounds::Sfx::WindowOpen);
        chest->ActivateRef(player, 0, nullptr, 1, false);
    }

    void Install() {
        if (!Plugin::IsLoaded()) {
            logger::warn("Storage: {} not loaded — dimensional storage is off",
                         Plugin::kFileName);
            return;
        }

        g_base = Plugin::LookupOurForm<RE::TESObjectCONT>(kContainerBase);
        if (!g_base) {
            logger::error("Storage: no container {:#08x} in {}", kContainerBase,
                          Plugin::kFileName);
            return;
        }
        g_codexToken = Plugin::LookupOurForm<RE::TESBoundObject>(kCodexToken);
        if (!g_codexToken) {
            logger::error("Storage: no codex token {:#08x} in {}", kCodexToken,
                          Plugin::kFileName);
        }

        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(CraftWatcher::GetSingleton());
        }
        if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
            holder->AddEventSink<RE::TESEquipEvent>(CodexWatcher::GetSingleton());
        }
        logger::info("Storage: reachable via the System panel; crafting borrows its inventory");
    }
}
