#include "FrameTick.h"

#include "ModAPI.h"
#include "Quests.h"
#include "TickThrottle.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>

namespace Isekai::FrameTick {

    namespace {

        // Room for every listener this mod adds, with slack. A fixed array so the hook
        // path never allocates; Add refuses (and logs) past the end.
        constexpr std::size_t kMaxListeners = 8;

        // A listener that keeps throwing would otherwise write a line every frame.
        constexpr std::uint32_t kMaxLoggedFailures = 3;

        struct Slot {
            Listener      fn = nullptr;
            std::uint32_t failures = 0;
        };

        std::array<Slot, kMaxListeners> g_listeners{};
        std::size_t                     g_count = 0;
        bool                            g_installed = false;

        using Update_t = void (*)(RE::PlayerCharacter*, float);
        Update_t g_originalUpdate = nullptr;

        void ReportFailure(std::size_t a_index, Slot& a_slot, const char* a_what) {
            ++a_slot.failures;
            if (a_slot.failures <= kMaxLoggedFailures) {
                logger::error("FrameTick: listener #{} threw ({}){}", a_index, a_what,
                              a_slot.failures == kMaxLoggedFailures
                                  ? " - further failures of this listener are not logged"
                                  : "");
            }
        }

        void RunListeners() {
            for (std::size_t i = 0; i < g_count; ++i) {
                Slot& slot = g_listeners[i];
                try {
                    slot.fn();
                } catch (const std::exception& e) {
                    ReportFailure(i, slot, e.what());
                } catch (...) {
                    ReportFailure(i, slot, "unknown exception");
                }
            }
        }

        void Hook_Update(RE::PlayerCharacter* a_this, float a_delta) {
            g_originalUpdate(a_this, a_delta);
            RunListeners();
        }

        // The System's own clock: once a second is far finer than a cadence measured in
        // game days. Already on the main thread, so the work runs here, not on a task.
        void GameClock() {
            static TickThrottle throttle{ std::chrono::seconds(1) };
            if (!throttle.Due(std::chrono::steady_clock::now())) {
                return;
            }
            Quests::Tick();
            // Same cadence, same thread, and the API's listeners want exactly the state
            // this tick may just have changed.
            ModAPI::PublishIfChanged();
        }
    }

    void Add(Listener a_listener) {
        if (!a_listener) {
            return;
        }
        if (g_count == kMaxListeners) {
            logger::error("FrameTick: listener table full ({}) - listener not added",
                          kMaxListeners);
            return;
        }
        g_listeners[g_count++] = Slot{ a_listener, 0 };
    }

    void Install() {
        if (g_installed) {
            return;
        }
        g_installed = true;

        Add(&GameClock);

        // Actor::Update(float) is not at the same slot on every runtime: VR inserts extra
        // Actor virtuals ahead of it. The indices are CommonLibSSE-NG's own, from
        // extern/CommonLibSSE-NG/src/RE/A/Actor.cpp:1776,
        //   RelocateVirtual<decltype(&Actor::Update)>(0x0AD, 0x0AF, this, a_delta);
        // (SE/AE first, VR second). The vtable base itself is runtime-resolved by
        // VTABLE_PlayerCharacter's VariantID.
        const std::size_t index = REL::Module::IsVR() ? 0x0AF : 0x0AD;
        try {
            REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
            g_originalUpdate = reinterpret_cast<Update_t>(vtbl.write_vfunc(index, &Hook_Update));
        } catch (const std::exception& e) {
            logger::error("FrameTick: PlayerCharacter::Update vtable hook failed ({})", e.what());
            return;
        }
        logger::info("FrameTick: hooked PlayerCharacter::Update at vtable index 0x{:X} ({})",
                     index, REL::Module::IsVR() ? "VR" : "SE/AE");
    }
}
