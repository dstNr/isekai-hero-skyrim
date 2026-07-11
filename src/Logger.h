#pragma once

namespace Isekai {
    // Route CommonLibSSE-NG's logger to
    // Documents\My Games\Skyrim Special Edition\SKSE\IsekaiHeroSKSE.log
    inline void InitLogger() {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }
        *path /= "IsekaiHeroSKSE.log";

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
    }
}
