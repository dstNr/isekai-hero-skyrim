#include "UI/Overlay.h"

#include "UI/Input.h"
#include "UI/SystemWindow.h"

#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace Isekai::UI {

    namespace {
        // Slot 8 of the IDXGISwapChain COM vtable: IUnknown (0-2), IDXGIObject (3-6),
        // IDXGIDeviceSubObject (7), then Present. Hooking the vtable rather than a
        // hard-coded address keeps this working across SE/AE without Address Library IDs.
        constexpr std::size_t kPresentVTableIndex = 8;

        using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);

        PresentFn               g_originalPresent = nullptr;
        ID3D11RenderTargetView* g_backBufferView = nullptr;
        ID3D11DeviceContext*    g_context = nullptr;

        std::atomic<bool> g_ready{ false };

        // We deliberately do not use ImGui's Win32 backend. It reads the mouse from
        // the window message queue, which Skyrim never fills (it takes the mouse via
        // DirectInput), and it fights the game over the OS cursor. Input comes from
        // the game's own event bus instead (see Input.cpp), and the two values the
        // backend would otherwise provide — display size and frame time — we set here.
        void UpdateDisplayAndTime(IDXGISwapChain* a_swapChain, ImGuiIO& a_io) {
            DXGI_SWAP_CHAIN_DESC desc{};
            if (SUCCEEDED(a_swapChain->GetDesc(&desc))) {
                a_io.DisplaySize = ImVec2{ static_cast<float>(desc.BufferDesc.Width),
                                           static_cast<float>(desc.BufferDesc.Height) };
            }

            using clock = std::chrono::steady_clock;
            static auto last = clock::now();
            const auto  now = clock::now();
            const auto  delta = std::chrono::duration<float>(now - last).count();
            last = now;

            // A stalled frame (loading screen, alt-tab) must not hand ImGui a huge or
            // zero dt — that would make animations jump or divide by zero.
            a_io.DeltaTime = std::clamp(delta, 1.0f / 1000.0f, 1.0f / 15.0f);
        }

        // Runs on the render thread, on the first Present after the hook is in place.
        bool InitImGui(IDXGISwapChain* a_swapChain) {
            auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
            if (!renderer) {
                return false;
            }

            auto* device = reinterpret_cast<ID3D11Device*>(renderer->data.forwarder);
            g_context = reinterpret_cast<ID3D11DeviceContext*>(renderer->data.context);
            if (!device || !g_context) {
                logger::error("UI: renderer is missing device/context");
                return false;
            }

            // Bind our own view of the back buffer rather than trusting whatever the
            // game happened to leave bound when it called Present.
            ID3D11Texture2D* backBuffer = nullptr;
            if (FAILED(a_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) || !backBuffer) {
                logger::error("UI: could not get the swap chain back buffer");
                return false;
            }
            const HRESULT hr = device->CreateRenderTargetView(backBuffer, nullptr, &g_backBufferView);
            backBuffer->Release();
            if (FAILED(hr)) {
                logger::error("UI: CreateRenderTargetView failed ({:#x})",
                              static_cast<std::uint32_t>(hr));
                return false;
            }

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();

            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;  // don't litter the game folder with imgui.ini
            io.LogFilename = nullptr;
            io.MouseDrawCursor = true;  // the only cursor on screen while a panel is up

            if (!ImGui_ImplDX11_Init(device, g_context)) {
                logger::error("UI: ImGui DX11 backend init failed");
                return false;
            }

            logger::info("UI: ImGui overlay initialised");
            return true;
        }

        HRESULT WINAPI HookedPresent(IDXGISwapChain* a_swapChain, UINT a_syncInterval, UINT a_flags) {
            static bool initTried = false;
            if (!initTried) {
                initTried = true;
                g_ready.store(InitImGui(a_swapChain), std::memory_order_release);
            }

            if (g_ready.load(std::memory_order_acquire)) {
                ImGuiIO& io = ImGui::GetIO();
                UpdateDisplayAndTime(a_swapChain, io);
                FeedImGui(io);

                ImGui_ImplDX11_NewFrame();
                ImGui::NewFrame();

                DrawSystemWindow();

                ImGui::Render();
                g_context->OMSetRenderTargets(1, &g_backBufferView, nullptr);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            }

            return g_originalPresent(a_swapChain, a_syncInterval, a_flags);
        }
    }

    bool IsCapturingInput() {
        return IsSystemWindowOpen();
    }

    void Install() {
        auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
        if (!renderer) {
            logger::error("UI: no renderer — overlay not installed");
            return;
        }

        auto* swapChain = reinterpret_cast<IDXGISwapChain*>(renderer->data.renderWindows[0].swapChain);
        if (!swapChain) {
            logger::error("UI: no swap chain — overlay not installed");
            return;
        }

        auto** vtable = *reinterpret_cast<void***>(swapChain);
        g_originalPresent = reinterpret_cast<PresentFn>(vtable[kPresentVTableIndex]);

        REL::safe_write(reinterpret_cast<std::uintptr_t>(&vtable[kPresentVTableIndex]),
                        reinterpret_cast<std::uintptr_t>(&HookedPresent));

        InstallInput();

        logger::info("UI: swap chain Present hooked — overlay armed");
    }
}
