#include "Shop.h"

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
            kPack,  // stock every material of a category, `amount` of each
            kGold,  // hand over `amount` septims
        };

        // The catalog as one table: what Catalog() renders and what Buy() spends against,
        // so a price can never differ between the card the player reads and the purchase
        // they actually get.
        //
        // Two sizes per category on purpose: the small pack is what you can afford early,
        // the large one is worth saving for (5x the materials for ~3.3x the price). The
        // quantities are per material TYPE, not per pack — the alchemy packs alone cover
        // every official ingredient in the load order.
        struct Entry {
            const char*              name;
            const char*              qty;
            std::int32_t             cost;
            const char*              icon;
            Kind                     kind;
            Storage::MaterialCategory category;  // kPack only
            std::int32_t             amount;
        };

        using Cat = Storage::MaterialCategory;

        // First-pass pricing, deliberately round: the whole point-economy is still a
        // testing configuration (see README), and these are the numbers most likely to
        // need tuning once the empty-chest start has actually been played.
        constexpr Entry kCatalog[] = {
            { "Smithing Materials", "20 of each", 15, "shop_smithing_small.png",
              Kind::kPack, Cat::kSmithing, 20 },
            { "Smithing Crate", "100 of each", 50, "shop_smithing_large.png",
              Kind::kPack, Cat::kSmithing, 100 },
            { "Alchemy Ingredients", "20 of each", 15, "shop_alchemy_small.png",
              Kind::kPack, Cat::kAlchemy, 20 },
            { "Alchemy Crate", "100 of each", 50, "shop_alchemy_large.png",
              Kind::kPack, Cat::kAlchemy, 100 },
            { "Soul Gems", "10 of each", 20, "shop_souls_small.png",
              Kind::kPack, Cat::kEnchanting, 10 },
            { "Soul Gem Crate", "50 of each", 65, "shop_souls_large.png",
              Kind::kPack, Cat::kEnchanting, 50 },
            { "Gold", "x1000", 10, "shop_gold_small.png", Kind::kGold, Cat::kSmithing, 1000 },
            { "Gold Hoard", "x10000", 75, "shop_gold_large.png", Kind::kGold, Cat::kSmithing,
              10000 },
        };

        // Gold is the only entry with a form that can be missing; the material packs are
        // sweeps and always "resolve". Both Catalog() and Buy() filter through this, so
        // an index means the same thing to each of them.
        [[nodiscard]] bool EntryAvailable(const Entry& a_entry) {
            if (a_entry.kind != Kind::kGold) {
                return true;
            }
            return RE::TESForm::LookupByID<RE::TESBoundObject>(kGold) != nullptr;
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

    std::vector<Item> Catalog() {
        std::vector<Item> out;
        for (const auto* e : LiveEntries()) {
            out.push_back({ e->name, e->qty, e->cost, e->icon });
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
        if (entry->kind == Kind::kGold) {
            delivered = Storage::Deliver(RE::TESForm::LookupByID<RE::TESBoundObject>(kGold),
                                         entry->amount);
        } else {
            delivered = Storage::StockCategory(entry->category, entry->amount) > 0;
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
