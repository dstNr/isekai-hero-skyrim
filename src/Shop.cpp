#include "Shop.h"

#include "Loc.h"
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
            kArmaments,     // weapons the System hands over
            kCount
        };

        constexpr const char* kShelfNames[] = { "MATERIALS", "WEALTH", "RESTORATIVES",
                                                "ELIXIRS", "ARMAMENTS" };
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
            // Translation key, spelled out rather than derived from the name. The other
            // tables in this mod key their strings off a stable number they already had;
            // these rows have none, and a slug of the English name would mean the same
            // transformation implemented twice - here and in tools/extract-strings.mjs -
            // with a silent dead translation the day the two disagree. A literal cannot
            // disagree with itself. ".qty" is appended for the quantity line.
            const char*               key;
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
            { "shop.smithingMaterials", "Smithing Materials", "100 of each", 5,
              "shop_smithing_small.png",
              Kind::kPack, Cat::kSmithing, 100, Shelf::kMaterials },
            { "shop.smithingCrate", "Smithing Crate", "500 of each", 15,
              "shop_smithing_large.png",
              Kind::kPack, Cat::kSmithing, 500, Shelf::kMaterials },
            { "shop.alchemyIngredients", "Alchemy Ingredients", "100 of each", 5,
              "shop_alchemy_small.png",
              Kind::kPack, Cat::kAlchemy, 100, Shelf::kMaterials },
            { "shop.alchemyCrate", "Alchemy Crate", "500 of each", 15,
              "shop_alchemy_large.png",
              Kind::kPack, Cat::kAlchemy, 500, Shelf::kMaterials },
            { "shop.soulGems", "Soul Gems", "50 of each", 5, "shop_souls_small.png",
              Kind::kPack, Cat::kEnchanting, 50, Shelf::kMaterials },
            { "shop.soulGemCrate", "Soul Gem Crate", "250 of each", 15,
              "shop_souls_large.png",
              Kind::kPack, Cat::kEnchanting, 250, Shelf::kMaterials },
            { "shop.gold", "Gold", "x100,000", 5, "shop_gold_small.png", Kind::kGold,
              Cat::kSmithing, 100'000, Shelf::kWealth },
            { "shop.goldHoard", "Gold Hoard", "x1,000,000", 20, "shop_gold_large.png",
              Kind::kGold, Cat::kSmithing, 1'000'000, Shelf::kWealth },

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
            { "shop.restorativeVigor", "Restorative: Vigor", "x10", 3,
              "shop_potion_vigor.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kRestoratives, 0x000D81 },
            { "shop.restorativeFocus", "Restorative: Focus", "x10", 3,
              "shop_potion_focus.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kRestoratives, 0x000D82 },
            { "shop.restorativeVitality", "Restorative: Vitality", "x10", 3,
              "shop_potion_vitality.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kRestoratives, 0x000D83 },
            { "shop.panacea", "Panacea", "x10", 3, "shop_potion_panacea.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kRestoratives, 0x000D84 },
            { "shop.elixirSystem", "Elixir of the System", "x10", 6,
              "shop_elixir_system.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D85 },
            { "shop.draughtAscended", "Draught of the Ascended", "x10", 6,
              "shop_elixir_ascended.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D86 },
            { "shop.aegisElixir", "Aegis Elixir", "x10", 6, "shop_elixir_aegis.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D87 },
            { "shop.phantomDraught", "Phantom Draught", "x10", 6,
              "shop_elixir_phantom.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D88 },
            { "shop.elixirEndlessCasting", "Elixir of Endless Casting", "x10", 6,
              "shop_elixir_casting.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D89 },
            { "shop.titansDraught", "Titan's Draught", "x10", 6, "shop_elixir_titan.png",
              Kind::kOurItem, Cat::kSmithing, 10, Shelf::kElixirs, 0x000D8A },
            { "shop.systemBlade", "Flameforged Oathblade", "x1", 40, "shop_blade.png",
              Kind::kOurItem, Cat::kSmithing, 1, Shelf::kArmaments, 0x000D8F },
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

        // The effect list a card shows, straight off the resolved form. Writing it into
        // kCatalog beside the name would be a second copy of what the ESP already states,
        // free to drift from it — and Part H of docs/CREATION_KIT_ESP.md is *the* place
        // that has drifted before. Empty for the material packs and for gold, which have
        // no magic effects and whose quantity line already says everything.
        [[nodiscard]] std::string DescribeEffects(const Entry& a_entry) {
            auto* form = EntryForm(a_entry);
            if (!form) {
                return {};
            }
            // A weapon carries no effect list, so without this its card is a name and a
            // price with nothing between them — the damage is the whole pitch.
            if (auto* weapon = form->As<RE::TESObjectWEAP>()) {
                return LF("shop.weaponDamage", "Damage {}", weapon->GetAttackDamage()) +
                       '\n' +
                       LF("shop.weaponCritical", "Critical {}", weapon->GetCritDamage());
            }
            auto* potion = form->As<RE::AlchemyItem>();
            if (!potion) {
                return {};
            }
            std::string out;
            for (const auto* effect : potion->effects) {
                if (!effect || !effect->baseEffect) {
                    continue;
                }
                const char* name = effect->baseEffect->GetFullName();
                if (!name || !*name) {
                    continue;  // a nameless effect would render as a bare number
                }
                if (!out.empty()) {
                    out += '\n';
                }
                out += name;
                // Cure Disease and the like carry no magnitude, and Invisibility's is 0 —
                // printing "Invisibility 0" would read as "does nothing".
                if (const float mag = effect->GetMagnitude(); mag > 0.0f) {
                    out += ' ' + std::to_string(static_cast<int>(mag));
                }
                // Minutes, because every elixir here runs an hour and "3600 s" is a number
                // the player has to convert. Instant effects (duration 0) say nothing.
                if (const std::uint32_t dur = effect->GetDuration(); dur > 0) {
                    out += LF("shop.effectMinutes", "  ({} min)", (dur + 59) / 60);
                }
            }
            return out;
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

    // A shelf needs no key field: its English name is a single word, which is already a
    // valid key segment. "shelf." + "WEALTH" is a rule that cannot be implemented two
    // different ways. The catalog entries could not do that — their names have spaces and
    // colons in them — which is why those carry a written-out key instead.
    [[nodiscard]] const char* ShelfName(const char* a_english) {
        return Loc::Get(std::string("shelf.") + a_english, a_english);
    }

    std::vector<std::string> Shelves() {
        std::vector<std::string> out;
        for (const char* name : kShelfNames) {
            out.emplace_back(ShelfName(name));
        }
        return out;
    }

    std::vector<std::pair<std::string, RE::TESBoundObject*>> OurGoods() {
        std::vector<std::pair<std::string, RE::TESBoundObject*>> out;
        for (const auto& e : kCatalog) {
            if (e.kind == Kind::kOurItem && e.localID != 0) {
                out.emplace_back(e.name, EntryForm(e));
            }
        }
        return out;
    }

    std::vector<Item> Catalog() {
        std::vector<Item> out;
        for (const auto* e : LiveEntries()) {
            out.push_back({ Loc::Get(e->key, e->name),
                            Loc::Get(std::string(e->key) + ".qty", e->qty), e->cost, e->icon,
                            ShelfName(kShelfNames[static_cast<std::size_t>(e->shelf)]),
                            DescribeEffects(*e) });
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
            RE::DebugNotification(L("shop.notEnoughPoints", "[ SYSTEM ] Not enough System Points."));
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
            RE::DebugNotification(
                L("shop.storageNotReady", "[ SYSTEM ] The Dimensional Storage is not ready yet."));
            return false;
        }

        state.systemPoints -= entry->cost;

        // The reward sting, not the panel click. Buying is the payoff of every milestone
        // earned so far, and it used to sound exactly like pressing "Cancel".
        Sounds::Play(Sounds::Sfx::LevelUp);

        // Say so on screen. Nothing confirmed a purchase before: the card stayed put, the
        // goods land in a chest that is not open, and the only trace was a log line — so
        // a successful buy was indistinguishable from a click that never registered.
        RE::DebugNotification(LF("shop.delivered", "[ SYSTEM ] {} {} -> Dimensional Storage",
                                 Loc::Get(entry->key, entry->name),
                                 Loc::Get(std::string(entry->key) + ".qty", entry->qty))
                                  .c_str());

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
