#include "PCH.h"

#include "CraftHooks.h"
#include "Logger.h"
#include "System.h"

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);
    Isekai::InitLogger();

    logger::info("=== Isekai Hero SKSE loaded ===");
    logger::info("Runtime version: {}", a_skse->RuntimeVersion().string());

    // Which of the three editions we booted under. CommonLibSSE-NG builds one DLL for
    // all of them (SE/AE/VR), so this line is the first thing to check when bringing
    // the mod up on Skyrim VR — it confirms the VR code paths were selected at runtime
    // and that the VR Address Library resolved.
    if (REL::Module::IsVR()) {
        logger::info("Runtime edition: Skyrim VR (EXPERIMENTAL — requires VR Address Library)");
    } else if (REL::Module::IsAE()) {
        logger::info("Runtime edition: Skyrim AE");
    } else {
        logger::info("Runtime edition: Skyrim SE");
    }

    // Papyrus natives must be registered here, not at kDataLoaded: SKSE hands the VM to
    // its Papyrus callbacks while the VM is built. Only a getter for patched listeners.
    if (!SKSE::GetPapyrusInterface()->Register(Isekai::CraftHooks::RegisterPapyrus)) {
        logger::error("Papyrus interface refused the IsekaiHeroCraft natives");
    }

    Isekai::Install();

    return true;
}
