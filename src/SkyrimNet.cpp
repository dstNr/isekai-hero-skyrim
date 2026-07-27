#include "SkyrimNet.h"

#include "Config.h"

#include <Windows.h>  // GetModuleHandleA — the soft-dependency probe

#include <atomic>
#include <functional>
#include <utility>

namespace Isekai::SkyrimNet {

    namespace {
        // SkyrimNet's Papyrus API lives on this script as global native functions
        // (SkyrimNetApi.DirectNarration / .RegisterEvent, both "Global Native"). We call
        // them straight through the VM — no Papyrus of our own, keeping this mod native.
        constexpr const char* kApiScript = "SkyrimNetApi";

        // SkyrimNet ships as a single SKSE plugin; this is its DLL. If it is ever renamed
        // the probe just fails and the whole integration stays dormant (logged), which is
        // exactly the safe default — it never affects players who don't run SkyrimNet.
        constexpr const char* kModuleName = "SkyrimNet.dll";

        std::atomic<bool> g_available{ false };

        // Run fn on the main thread. Papyrus VM calls belong on the game thread; our push
        // sites are already there, but marshalling keeps it true no matter who calls.
        void OnMainThread(std::function<void()> a_fn) {
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([fn = std::move(a_fn)]() { fn(); });
            }
        }

        // The one place that actually talks to the VM. a_args is consumed by the call.
        void Dispatch(const char* a_fn, RE::BSScript::IFunctionArguments* a_args) {
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm) {
                return;
            }
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;  // fire-and-forget
            if (!vm->DispatchStaticCall(kApiScript, a_fn, a_args, callback)) {
                // Most likely SkyrimNet's API script/function is not what we expect on
                // this build — disable so we stop trying, and leave a breadcrumb.
                logger::warn("SkyrimNet: {}.{} dispatch failed — disabling integration",
                             kApiScript, a_fn);
                g_available.store(false, std::memory_order_release);
            }
        }
    }

    void Install() {
        g_available.store(false, std::memory_order_release);

        if (!Config::SkyrimNetIntegration()) {
            logger::info("SkyrimNet: integration disabled in ini");
            return;
        }
        if (!GetModuleHandleA(kModuleName)) {
            logger::info("SkyrimNet: {} not loaded — integration off", kModuleName);
            return;
        }

        g_available.store(true, std::memory_order_release);
        logger::info("SkyrimNet: detected — System context will be pushed to it");
    }

    bool Available() {
        return g_available.load(std::memory_order_acquire);
    }

    void PushEvent(std::string a_eventType, std::string a_content) {
        if (!Available()) {
            return;
        }
        OnMainThread([type = std::move(a_eventType), content = std::move(a_content)]() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            // int RegisterEvent(String eventType, String content, Actor orig, Actor target)
            Dispatch("RegisterEvent",
                     RE::MakeFunctionArguments(RE::BSFixedString{ type.c_str() },
                                               RE::BSFixedString{ content.c_str() },
                                               static_cast<RE::Actor*>(player),
                                               static_cast<RE::Actor*>(nullptr)));
        });
    }

    void Narrate(std::string a_content) {
        if (!Available()) {
            return;
        }
        OnMainThread([content = std::move(a_content)]() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            // int DirectNarration(String content, Actor originatorActor, Actor targetActor)
            Dispatch("DirectNarration",
                     RE::MakeFunctionArguments(RE::BSFixedString{ content.c_str() },
                                               static_cast<RE::Actor*>(player),
                                               static_cast<RE::Actor*>(nullptr)));
        });
    }
}
