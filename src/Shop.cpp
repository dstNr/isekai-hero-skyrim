#include "Shop.h"

#include "Plugin.h"
#include "Sounds.h"
#include "Storage.h"
#include "System.h"
#include "UI/Prisma.h"
#include "UI/ShopWindow.h"
#include "UI/SystemWindow.h"

namespace Isekai::Shop {

    namespace {
        constexpr RE::FormID kGold = 0x0000000F;

        // What a card does when bought.
        enum class Kind {
            kPack,     // stock every material of a category, `amount` of each
            kGold,     // hand over `amount` septims
            kOurItem,  // hand over `amount` of a form from IsekaiHero.esp
        };

        // How the shelves are laid out. Purely a display grouping — Storage's
        // MaterialCategory decides where a pack is DELIVERED, which is a different
        // question (gold and every potion deliver as kSmithing and would all land on one
        // meaningless shelf). Order here is the order the groups appear.
        enum class Shelf {
            kMaterials,     // the crafting packs
            kWealth,        // septims
            kRestoratives,  // the cheap, spammable potions
            kElixirs,       // the expensive ones
            kCount
        };

        constexpr const char* kShelfNames[] = { "MATERIALS", "WEALTH", "RESTORATIVES",
                                                "ELIXIRS" };
        static_assert(std::size(kShelfNames) == static_cast<std::size_t>(Shelf::kCount),
                      "every shelf needs a name");

        // The catalog as one table: what Catalog() renders and what Buy() spends against,
        // so a price can never differ between the card the player reads and the purchase
        // they actually get.
        //
        // Two sizes per category on purpose: the small pack is what you can afford early,
        // the large one is worth saving for (5x the materials for ~3.3x the price). The
        // quantities are per material TYPE, not per pack — the alchemy packs alone cover
        // every official ingredient in the load order.
        struct Entry {
            const char*               name;
            const char*               qty;
            std::int32_t              cost;
            const char*               icon;
            Kind                      kind;
            Storage::MaterialCategory category;  // kPack only
            std::int32_t              amount;
            Shelf                     shelf;
            RE::FormID                localID = 0;  // kOurItem only, ESP-local
        };

        using Cat = Storage::MaterialCategory;

        // Pricing is calibrated against two things, not guessed:
        //
        //  * What the chest USED to hold for free. It was stocked with 500 x the reward
        //    scale of every material — 2000 of each at ASCENDED — plus millions in gold.
        //    A pack has to land in that league or moving materials behind a paywall reads
        //    as a nerf rather than as a choice. A crate is 500 of each, so four of them
        //    match what ASCENDED was simply given.
        //  * What System Points are actually worth. Milestones pay 1 (5 at an endpoint)
        //    times the reward scale, so a full ASCENDED run earns roughly 550 on top of
        //    its 500 starting points — while the skill tree alone can absorb ~1170. Points
        //    are the scarce thing; the shop must not compete with the tree for them.
        //
        // Hence: single-digit entry prices, and a crate at 5x the contents for 3x the
        // price. Kitting out all three categories costs 45 points, well inside a starting
        // blessing, which is the "the System just hands you the world" feeling this is
        // supposed to produce. Still a testing configuration (see README).
        constexpr Entry kCatalog[] = {
            { "Smithing Materials", "100 of each", 5, "shop_smithing_small.png",
              Kind::kPack, Cat::kSmithing, 100, Shelf::kMaterials },
            { "Smithing Crate", "500 of each", 15, "shop_smithing_large.png",
              Kind::kPack, Cat::kSmithing, 500, Shelf::kMaterials },
            { "Alchemy Ingredients", "100 of each", 5, "shop_alchemy_small.png",
              Kind::kPack, Cat::kAlchemy, 100, Shelf::kMaterials },
            { "Alchemy Crate", "500 of each", 15, "shop_alchemy_large.png",
              Kind::kPack, Cat::kAlchemy, 500, Shelf::kMaterials },
            { "Soul Gems", "50 of each", 5, "shop_souls_small.png",
              Kind::kPack, Cat::kEnchanting, 50, Shelf::kMaterials },
            { "Soul Gem Crate", "250 of each", 15, "shop_souls_large.png",
              Kind::kPack, Cat::kEnchanting, 250, Shelf::kMaterials },
            { "Gold", "x100,000", 5, "shop_gold_small.png", Kind::kGold, Cat::kSmithing,
              100'000, Shelf::kWealth },
            { "Gold Hoard", "x1,000,000", 20, "shop_gold_large.png", Kind::kGold,
              Cat::kSmithing, 1'000'000, Shelf::kWealth },

            // --- System potions (docs/CREATION_KIT_ESP.md, Part H) ---
            // localID is the record's ESP-local FormID. It is 0 until the record exists:
            // the entry then fails to resolve and is left out of the catalog entirely, so
            // an ESP without these potions simply shows the eight cards above. Fill an ID
            // in and its card appears — no other change needed.
            //
            // Restoratives are instant and meant to be spammed, hence the low price; the
            // hour-long elixirs cost a little more but are still inside a starting
            // blessing, matching the material packs' "the System hands you the world"
            // pricing.
            { "Restorative: Vigor", "x10", 3, "shop_potion_vigor.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kRestoratives, 0x000D81 },
            { "Restorative: Focus", "x10", 3, "shop_potion_focus.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kRestoratives, 0x000D82 },
            { "Restorative: Vitality", "x10", 3, "shop_potion_vitality.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kRestoratives, 0x000D83 },
            { "Panacea", "x10", 3, "shop_potion_panacea.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kRestoratives, 0x000D84 },
            { "Elixir of the System", "x10", 6, "shop_elixir_system.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D85 },
            { "Draught of the Ascended", "x10", 6, "shop_elixir_ascended.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D86 },
            { "Aegis Elixir", "x10", 6, "shop_elixir_aegis.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D87 },
            { "Phantom Draught", "x10", 6, "shop_elixir_phantom.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D88 },
            { "Elixir of Endless Casting", "x10", 6, "shop_elixir_casting.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D89 },
            { "Titan's Draught", "x10", 6, "shop_elixir_titan.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D8A },
        };

        // The form an entry hands over, or nullptr for the material packs (which are
        // sweeps, not a single form).
        [[nodiscard]] RE::TESBoundObject* EntryForm(const Entry& a_entry) {
            switch (a_entry.kind) {
            case Kind::kGold:
                return RE::TESForm::LookupByID<RE::TESBoundObject>(kGold);
            case Kind::kOurItem:
                return a_entry.localID == 0
                           ? nullptr  // record not created yet — card stays hidden
                           : Plugin::LookupOurForm<RE::TESBoundObject>(a_entry.localID);
            default:
                return nullptr;
            }
        }

        // Both Catalog() and Buy() filter through this, so an index means the same thing
        // to each of them. A potion whose ESP record does not exist drops out here, which
        // is what lets the catalog ship ahead of the Creation Kit work.
        [[nodiscard]] bool EntryAvailable(const Entry& a_entry) {
            return a_entry.kind == Kind::kPack || EntryForm(a_entry) != nullptr;
        }

        [[nodiscard]] std::vector<const Entry*> LiveEntries() {
            std::vector<const Entry*> out;
            for (const auto& e : kCatalog) {
                if (EntryAvailable(e)) {
                    out.push_back(&e);
                }
            }
            return out;
        }
    }

    void Install() {
        // Nothing to resolve up front any more: the material packs are sweeps run at
        // purchase time (so a mid-playthrough load-order change is picked up), and gold
        // is a single well-known form. Kept as an install hook because System.cpp's
        // kDataLoaded sequence calls it and a future catalog may need it again.
        logger::info("Shop: catalog ready ({} entries)", std::size(kCatalog));
    }

    bool Available() {
        return Storage::Available();
    }

    std::vector<std::string> Shelves() {
        return { std::begin(kShelfNames), std::end(kShelfNames) };
    }

    std::vector<Item> Catalog() {
        std::vector<Item> out;
        for (const auto* e : LiveEntries()) {
            out.push_back({ e->name, e->qty, e->cost, e->icon,
                            kShelfNames[static_cast<std::size_t>(e->shelf)] });
        }
        return out;
    }

    bool Buy(int a_index) {
        const auto live = LiveEntries();
        if (a_index < 0 || static_cast<std::size_t>(a_index) >= live.size()) {
            return false;  // a stale card from a catalog built before a form went away
        }
        const auto* entry = live[static_cast<std::size_t>(a_index)];

        auto& state = GetState();
        if (state.systemPoints < entry->cost) {
            RE::DebugNotification("[ SYSTEM ] Not enough System Points.");
            return false;
        }

        // Deliver FIRST, and only charge if it landed: the chest is created on demand
        // now, so "could not deliver" is a real outcome and must not cost the player
        // their points.
        bool delivered = false;
        if (entry->kind == Kind::kPack) {
            delivered = Storage::StockCategory(entry->category, entry->amount) > 0;
        } else {
            delivered = Storage::Deliver(EntryForm(*entry), entry->amount);
        }
        if (!delivered) {
            RE::DebugNotification("[ SYSTEM ] The Dimensional Storage is not ready yet.");
            return false;
        }

        state.systemPoints -= entry->cost;
        Sounds::Play(Sounds::Sfx::ButtonClick);
        logger::info("Shop: bought '{}' ({}) for {} System Point(s)", entry->name, entry->qty,
                     entry->cost);
        return true;
    }

    void Open() {
        // Same guard Storage::Open() uses: reached as a status-panel action, so the
        // panel that led here has already dismissed itself by the time this runs.
        if (UI::IsSystemWindowOpen()) {
            return;
        }
        if (!Available()) {
            RE::DebugNotification("[ SYSTEM ] ACCESS DENIED — the System is not yet bound to you.");
            return;
        }

        // Same split every other screen uses: the web patch when it is installed, the
        // built-in window otherwise.
        if (UI::Prisma::Active()) {
            UI::Prisma::OpenShop();
        } else {
            UI::ShowShopWindow();
        }
    }
}
