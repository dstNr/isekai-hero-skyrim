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

        RE::TESObjectCONT* g_base = nullptr;

        // What the crafting menu borrowed from storage, per base object. Never
        // persisted: the crafting menu pauses the game and cannot outlive a session.
        std::map<RE::TESBoundObject*, std::int32_t> g_craftLoan;

        // Every reincarnated soul gets the pocket dimension — the panel button, the
        // chest, the crafting-station lending. NORMAL chose the pure challenge, so its
        // chest simply starts EMPTY (see GrantStartingMaterials): a usable stash, but
        // no head start handed to it.
        [[nodiscard]] bool IsEligible() {
            return GetState().reincarnated;
        }

        // The starting stock follows the flat grant, so it keys off grantTier — a
        // pre-stocked chest for a HERO/ASCENDED starting gift, an empty one for NORMAL, a
        // SHATTERED/DORMANT start, or a CUSTOM build that took no starting gift.
        [[nodiscard]] bool GetsStartingStock() {
            const auto& state = GetState();
            return state.reincarnated && state.grantTier != PowerLevel::Normal;
        }

        // The official game masters. We freely stock and keep their ingredients; anything
        // else (Creation Club, mods) is held at arm's length for ingredients specifically,
        // because CC ingredients ship tracker scripts that flood the VM when handled in
        // bulk — the very stutter this feature was rebuilt to avoid. Misc materials carry
        // no such scripts, so those we take from any plugin (see GrantStartingMaterials).
        [[nodiscard]] bool IsOfficialMaster(const RE::TESForm* a_form) {
            if (!a_form) {
                return false;
            }
            const auto* file = a_form->GetFile(0);
            if (!file) {
                return false;
            }
            using namespace std::string_view_literals;
            const auto name = file->GetFilename();
            return name == "Skyrim.esm"sv || name == "Update.esm"sv ||
                   name == "Dawnguard.esm"sv || name == "HearthFires.esm"sv ||
                   name == "Dragonborn.esm"sv;
        }

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

    // Shared stocking core. topUpOnly (the load path) skips any type already present, so
    // an existing chest gains only what is NEW — add-on materials, DLC ingredients — and
    // never refills what the player has spent; it also leaves the gold alone. The grant
    // path (topUpOnly=false, from reincarnation) fills everything and seeds the gold.
    static void StockChest(RE::TESObjectREFR* chest, RE::TESDataHandler* data, bool topUpOnly) {
        // Every FormID below was read out of Skyrim.esm itself (MISC/SLGM groups),
        // not quoted from memory. Base counts are the NORMAL scale; the blessing
        // multiplies them (HERO x2 -> 1000 each, ASCENDED x4 -> 2000 each).
        struct Entry {
            RE::FormID   id;
            std::int32_t base;
        };
        constexpr Entry kMaterials[] = {
            { 0x0005ACE4, 500 },  // IngotIron
            { 0x0005ACE5, 500 },  // IngotSteel
            { 0x0005AD93, 500 },  // IngotCorundum
            { 0x0005AD99, 500 },  // IngotOrichalcum
            { 0x000DB8A2, 500 },  // IngotDwarven
            { 0x0005AD9F, 500 },  // IngotIMoonstone (refined moonstone)
            { 0x0005ADA0, 500 },  // IngotQuicksilver
            { 0x0005ADA1, 500 },  // IngotMalachite
            { 0x0005AD9D, 500 },  // IngotEbony
            { 0x0005AD9E, 500 },  // IngotGold
            { 0x0005ACE3, 500 },  // ingotSilver
            { 0x000DB5D2, 500 },  // Leather01
            { 0x000800E4, 500 },  // LeatherStrips
            { 0x0006F993, 500 },  // Firewood01
            { 0x00033760, 500 },  // Charcoal
            { 0x0003ADA4, 500 },  // DragonBone
            { 0x0003ADA3, 500 },  // DragonScales
            { 0x00063B46, 500 },  // GemAmethyst
            { 0x00063B47, 500 },  // GemDiamond
            { 0x00063B43, 500 },  // GemEmerald
            { 0x00063B45, 500 },  // GemGarnet
            { 0x00063B42, 500 },  // GemRuby
            { 0x00063B44, 500 },  // GemSapphire
            { 0x0006851E, 500 },  // GemAmethystFlawless
            { 0x0006851F, 500 },  // GemDiamondFlawless
            { 0x00068520, 500 },  // GemEmeraldFlawless
            { 0x00068521, 500 },  // GemGarnetFlawless
            { 0x00068522, 500 },  // gemRubyFlawless
            { 0x00068523, 500 },  // GemSapphireFlawless
            // Soul gems are stocked separately, enumerated below — one hand-picked
            // FormID missed the black gem, which is exactly the sort of gap to avoid.
        };
        constexpr RE::FormID   kGold = 0x0000000F;   // Gold001
        constexpr std::int32_t kGoldBase = 1'250'000;  // -> 2.5M HERO, 5M ASCENDED

        const float scale = RewardScale();
        constexpr std::int32_t kBase = 500;  // -> 1000 HERO, 2000 ASCENDED

        // One stock helper, deduplicated: a form reached by two paths (say a gem that is
        // both hand-picked and a recipe component) is added exactly once.
        std::set<RE::TESBoundObject*> seen;
        std::size_t                   stocked = 0;
        const auto stock = [&](RE::TESBoundObject* obj, std::int32_t base) {
            if (!obj || !seen.insert(obj).second) {
                return;
            }
            if (topUpOnly && ChestCount(obj) > 0) {
                return;  // load path: never pile onto or refill what's already there
            }
            chest->AddObjectToContainer(
                obj, nullptr, static_cast<std::int32_t>(static_cast<float>(base) * scale), nullptr);
            ++stocked;
        };

        // 1) Guaranteed vanilla core: the ingots/leather/gems (incl. FLAWLESS gems, which
        //    the enchant "empower" uses and no forge recipe requires, so the recipe sweep
        //    below would miss them) and dragon parts.
        for (const auto& entry : kMaterials) {
            // Look up under the concrete form types. TESBoundObject would be the natural
            // common base, but it has no FORMTYPE, so the typed lookup rejects everything.
            RE::TESBoundObject* obj = data->LookupForm<RE::TESObjectMISC>(entry.id, "Skyrim.esm");
            if (!obj) {
                obj = data->LookupForm<RE::TESSoulGem>(entry.id, "Skyrim.esm");
            }
            if (!obj) {
                logger::error("Storage: material {:#010x} missing from Skyrim.esm", entry.id);
                continue;
            }
            stock(obj, entry.base);
        }

        // 2) Every material an OFFICIAL-master recipe requires (Misc or ingredient),
        //    sourced from the recipe components rather than a blind "all Misc" so only
        //    things a recipe actually consumes are stocked — and so DLC materials come
        //    along (chitin plate, netch leather, corkbulb root). Restricted to official
        //    masters on purpose: a modded crafting material can be a scripted deployable
        //    (bear traps, campfire kit) whose OnContainerChanged handler spawns copies of
        //    itself and floods the VM. Vanilla + DLC materials carry no such scripts.
        std::size_t recipeMats = 0;
        for (auto* cobj : data->GetFormArray<RE::BGSConstructibleObject>()) {
            if (!cobj) {
                continue;
            }
            cobj->requiredItems.ForEachContainerObject([&](RE::ContainerObject& a_c) {
                auto* obj = a_c.obj;
                if (obj && IsOfficialMaster(obj) && !seen.contains(obj)) {
                    const auto type = obj->GetFormType();
                    const bool wanted =
                        type == RE::FormType::Misc || type == RE::FormType::Ingredient;
                    if (wanted) {
                        if (const char* name = obj->GetName(); name && *name) {
                            stock(obj, kBase);
                            ++recipeMats;
                        }
                    }
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }

        // 3) Alchemy: every ingredient from the official masters (Skyrim + DLC — corkbulb
        //    root, ash yam, ...), enumerated rather than hand-listed. CC/mod ingredients
        //    stay out for the tracker-script reason above.
        std::size_t ingredients = 0;
        for (auto* ingredient : data->GetFormArray<RE::IngredientItem>()) {
            if (!ingredient || !IsOfficialMaster(ingredient)) {
                continue;
            }
            if (const char* name = ingredient->GetName(); !name || !*name) {
                continue;  // nameless = internal/test records, not for players
            }
            if (!seen.contains(ingredient)) {
                ++ingredients;
            }
            stock(ingredient, kBase);
        }

        // 4) Enchanting: every FILLED soul gem base form from the official masters (petty
        //    .. grand + black). Empty gems can't power an enchantment, so they stay out.
        std::size_t soulGems = 0;
        for (auto* gem : data->GetFormArray<RE::TESSoulGem>()) {
            if (!gem || !IsOfficialMaster(gem)) {
                continue;
            }
            if (gem->GetContainedSoul() == RE::SOUL_LEVEL::kNone) {
                continue;  // empty base form — useless for enchanting
            }
            if (!seen.contains(gem)) {
                ++soulGems;
            }
            stock(gem, kBase);
        }

        if (!topUpOnly) {
            if (auto* gold = RE::TESForm::LookupByID<RE::TESBoundObject>(kGold)) {
                chest->AddObjectToContainer(
                    gold, nullptr,
                    static_cast<std::int32_t>(static_cast<float>(kGoldBase) * scale), nullptr);
            }
        }

        logger::info("Storage: {} {} stacks ({} recipe materials, {} ingredients, "
                     "{} soul gems){} (scale x{})",
                     topUpOnly ? "topped up" : "stocked", stocked, recipeMats, ingredients,
                     soulGems, topUpOnly ? "" : " + gold", scale);
    }

    // Reincarnation path: create the chest (HERO/ASCENDED only) and fill it completely.
    void GrantStartingMaterials() {
        if (!GetsStartingStock()) {
            return;  // NORMAL gets the chest, but empty — nothing to put in it
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* chest = player ? GetOrCreateChest(player) : nullptr;
        auto* data = RE::TESDataHandler::GetSingleton();
        if (chest && data) {
            StockChest(chest, data, /*topUpOnly=*/false);
        }
    }

    // Load path: bring an existing chest up to the current material set (add-on materials,
    // DLC ingredients, any missing soul gem) without refilling spent stacks.
    void TopUpStock() {
        if (!GetsStartingStock()) {
            return;  // NORMAL keeps its empty chest
        }
        auto* chest = ResolveChest();
        auto* data = RE::TESDataHandler::GetSingleton();
        if (chest && data) {
            StockChest(chest, data, /*topUpOnly=*/true);
        }
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

        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(CraftWatcher::GetSingleton());
        }
        logger::info("Storage: reachable via the System panel; crafting borrows its inventory");
    }
}
