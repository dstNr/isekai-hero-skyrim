#include "PCH.h"

namespace {
    // Route CommonLibSSE-NG's logger to
    // Documents\My Games\Skyrim Special Edition\SKSE\IsekaiHeroSKSE.log
    void InitLogger() {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }
        *path /= "IsekaiHeroSKSE.log";

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto logger = std::make_shared<spdlog::logger>("global", std::move(sink));
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(logger));
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);
    InitLogger();

    SKSE::log::info("=== Isekai Hero SKSE plugin loaded successfully ===");
    SKSE::log::info("Runtime version: {}", a_skse->RuntimeVersion().string());

    return true;
}
