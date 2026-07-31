#include "Plugin.h"

#include <vector>

namespace Isekai::Plugin {

    namespace {
        std::atomic<bool> g_loaded{ false };
    }

    bool IsLoaded() {
        return g_loaded.load(std::memory_order_acquire);
    }

    RE::FormID ResolveLocalID(RE::FormID a_localID) {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            return 0;
        }
        const auto* file = data->LookupModByName(kFileName);
        if (!file) {
            return 0;
        }
        // Same reconstruction DumpForms filters by, in reverse: partial index over the
        // local bits. Light = 12 local bits (0xFExxx already sits in the partial), a
        // regular plugin = 24.
        const std::uint32_t partial = file->GetPartialIndex();
        return file->IsLight() ? (partial << 12) | (a_localID & 0x00000FFF)
                               : (partial << 24) | (a_localID & 0x00FFFFFF);
    }

    void DumpForms() {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            return;
        }

        const auto* file = data->LookupModByName(kFileName);
        if (!file) {
            logger::warn("{} is not loaded — passives and dimensional storage are off",
                         kFileName);
            g_loaded.store(false, std::memory_order_release);
            return;
        }
        g_loaded.store(true, std::memory_order_release);

        // A light plugin (ESL / ESL-flagged) carries its index in the top 20 bits and
        // has only 12 bits of local ID; a normal one uses the top 8 and 24. Getting this
        // wrong would silently match nothing, so it is worth handling both.
        const bool          light = file->IsLight();
        const std::uint32_t partial = file->GetPartialIndex();

        const auto belongsToUs = [&](RE::FormID a_id) {
            return light ? (a_id >> 12) == partial : (a_id >> 24) == partial;
        };
        const auto localID = [&](RE::FormID a_id) -> RE::FormID {
            return light ? (a_id & 0x00000FFF) : (a_id & 0x00FFFFFF);
        };

        logger::info("=== {} loaded ({}, index {:#x}) ===", kFileName,
                     light ? "light" : "regular", partial);

        std::vector<std::pair<RE::FormID, RE::TESForm*>> mine;
        {
            const auto& [all, lock] = RE::TESForm::GetAllForms();
            const RE::BSReadLockGuard guard{ lock };
            for (const auto& [id, form] : *all) {
                if (form && belongsToUs(id)) {
                    mine.emplace_back(id, form);
                }
            }
        }

        std::ranges::sort(mine, {}, &std::pair<RE::FormID, RE::TESForm*>::first);

        for (const auto& [id, form] : mine) {
            const char* name = form->GetName();
            logger::info("  form: local={:#08x}  type={:<14}  \"{}\"", localID(id),
                         RE::FormTypeToString(form->GetFormType()), name ? name : "");
        }
        logger::info("=== {} forms from {} ===", mine.size(), kFileName);
    }

    bool IsOfficialMaster(const RE::TESForm* a_form) {
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
}
