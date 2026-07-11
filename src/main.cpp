#include "PCH.h"

#include "Logger.h"
#include "System.h"

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);
    Isekai::InitLogger();

    logger::info("=== Isekai Hero SKSE loaded ===");
    logger::info("Runtime version: {}", a_skse->RuntimeVersion().string());

    Isekai::RegisterMessageListener();

    return true;
}
