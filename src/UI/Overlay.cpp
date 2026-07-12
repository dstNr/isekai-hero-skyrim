#include "UI/Overlay.h"

#include "UI/SystemWindow.h"

#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND a_hWnd, UINT a_msg, WPARAM a_wParam,
                                                             LPARAM a_lParam);

namespace Isekai::UI {

    namespace {
        // Slot 8 of the IDXGISwapChain COM vtable: IUnknown (0-2), IDXGIObject (3-6),
        // IDXGIDeviceSubObject (7), then Present. Hooking the vtable rather than a
        // hard-coded address keeps this working across SE/AE without Address Library IDs.
        constexpr std::size_t kPresentVTableIndex = 8;

        using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
        using WndProcFn = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM);

        PresentFn g_originalPresent = nullptr;
        WndProcFn g_originalWndProc = nullptr;

        ID3D11RenderTargetView* g_backBufferView = nullptr;
        ID3D11DeviceContext*    g_context = nullptr;
        HWND                    g_window = nullptr;

        std::atomic<bool> g_ready{ false };

        LRESULT WINAPI HookedWndProc(HWND a_hWnd, UINT a_msg, WPARAM a_wParam, LPARAM a_lParam) {
            if (g_ready.load(std::memory_order_acquire) && IsCapturingInput()) {
                ImGui_ImplWin32_WndProcHandler(a_hWnd, a_msg, a_wParam, a_lParam);
            }
            // The game still gets the message, but its controls are switched off while
            // a window of ours is open (see SystemWindow), so nothing acts on it.
            return g_originalWndProc(a_hWnd, a_msg, a_wParam, a_lParam);
        }

        // Runs on the render thread, on the first Present after the hook is in place.
        bool InitImGui(IDXGISwapChain* a_swapChain) {
            auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
            if (!renderer) {
                return false;
            }

            auto* device = reinterpret_cast<ID3D11Device*>(renderer->data.forwarder);
            g_context = reinterpret_cast<ID3D11DeviceContext*>(renderer->data.context);
            g_window = reinterpret_cast<HWND>(renderer->data.renderWindows[0].hWnd);
            if (!device || !g_context || !g_window) {
                logger::error("UI: renderer is missing device/context/window");
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
                logger::error("UI: CreateRenderTargetView failed ({:#x})", static_cast<std::uint32_t>(hr));
                return false;
            }

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();

            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;  // don't litter the game folder with imgui.ini
            io.LogFilename = nullptr;
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;  // we draw our own

            if (!ImGui_ImplWin32_Init(g_window) || !ImGui_ImplDX11_Init(device, g_context)) {
                logger::error("UI: ImGui backend init failed");
                return false;
            }

            g_originalWndProc = reinterpret_cast<WndProcFn>(
                SetWindowLongPtrA(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));

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
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                ImGui::GetIO().MouseDrawCursor = IsCapturingInput();
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

        logger::info("UI: swap chain Present hooked — overlay armed");
    }
}
