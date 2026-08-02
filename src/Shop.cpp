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

        // One representative filled soul gem per grade from the official masters — same
        // dedup idiom SkillTree's UnlockAllEnchantments uses (lowest FormID wins, so the
        // pick is deterministic across loads). Resolved once at Install(), not on every
        // catalog open.
        RE::TESSoulGem* g_grand = nullptr;
        RE::TESSoulGem* g_common = nullptr;

        [[nodiscard]] RE::TESSoulGem* FindSoulGem(RE::SOUL_LEVEL a_grade) {
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!data) {
                return nullptr;
            }
            RE::TESSoulGem* best = nullptr;
            for (auto* gem : data->GetFormArray<RE::TESSoulGem>()) {
                if (!gem || !Plugin::IsOfficialMaster(gem) || gem->GetContainedSoul() != a_grade) {
                    continue;
                }
                if (!best || gem->GetFormID() < best->GetFormID()) {
                    best = gem;
                }
            }
            return best;
        }

        // The catalog as one table: what Catalog() renders and what Buy() spends against,
        // so a price can never differ between the card the player reads and the purchase
        // they get. `obj` is resolved lazily because Gold001 is a plain lookup while the
        // soul gems come from Install()'s scan.
        struct Entry {
            const char*         name;
            const char*         qty;
            std::int32_t        cost;
            std::int32_t        count;
            const char*         icon;
            RE::TESBoundObject* (*resolve)();
        };

        constexpr Entry kCatalog[] = {
            { "Grand Soul Gem", "x1", 25, 1, "spells_20_frame.png",
              []() -> RE::TESBoundObject* { return g_grand; } },
            { "Common Soul Gem", "x5", 15, 5, "spells_19_frame.png",
              []() -> RE::TESBoundObject* { return g_common; } },
            { "Gold", "x1000", 10, 1000, "spells_21_frame.png",
              []() -> RE::TESBoundObject* {
                  return RE::TESForm::LookupByID<RE::TESBoundObject>(kGold);
              } },
        };

        // The entries whose form actually resolved, in catalog order. Both Catalog() and
        // Buy() go through this, so an index means the same thing to each of them even
        // when a form is missing from the load order.
        [[nodiscard]] std::vector<const Entry*> LiveEntries() {
            std::vector<const Entry*> out;
            for (const auto& e : kCatalog) {
                if (e.resolve()) {
                    out.push_back(&e);
                }
            }
            return out;
        }
    }

    void Install() {
        g_grand = FindSoulGem(RE::SOUL_LEVEL::kGrand);
        g_common = FindSoulGem(RE::SOUL_LEVEL::kCommon);
        logger::info("Shop: catalog resolved (grand={}, common={})", g_grand != nullptr,
                     g_common != nullptr);
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
        auto* chest = Storage::ChestRef();
        auto* obj = entry->resolve();
        if (!chest || !obj) {
            RE::DebugNotification("[ SYSTEM ] The Dimensional Storage is not ready yet.");
            return false;
        }

        state.systemPoints -= entry->cost;
        chest->AddObjectToContainer(obj, nullptr, entry->count, nullptr);
        Sounds::Play(Sounds::Sfx::ButtonClick);
        logger::info("Shop: bought {}x {} for {} System Point(s)", entry->count, entry->name,
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
