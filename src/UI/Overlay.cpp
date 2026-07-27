#include "UI/Overlay.h"

#include "SkillTree.h"
#include "UI/Input.h"
#include "UI/LevelUpEffect.h"
#include "UI/SkillTreeWindow.h"
#include "UI/Style.h"
#include "UI/SystemWindow.h"
#include "UI/Textures.h"

#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

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

        // Consolas ships with Windows, so we can use it without redistributing a font
        // file. If it is somehow missing, ImGui's built-in font takes over — the panel
        // still works, it just looks plainer. Sizes are irrelevant here: ImGui 1.92
        // rasterises on demand at whatever size PushFont asks for.
        void LoadFonts(ImGuiIO& a_io) {
            Style::g_body = a_io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 20.0f);
            Style::g_title = a_io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consolab.ttf", 30.0f);

            if (!Style::g_body || !Style::g_title) {
                logger::warn("UI: Consolas not found — falling back to the built-in font");
            }
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
            io.MouseDrawCursor = false;  // we draw our own, see Style::DrawSystemCursor

            LoadFonts(io);

            if (!ImGui_ImplDX11_Init(device, g_context)) {
                logger::error("UI: ImGui DX11 backend init failed");
                return false;
            }

            // Warm the texture cache while the player is still in the main menu.
            // Decoding half-megabyte PNGs synchronously in a panel's first frame is
            // exactly the "hangs for a moment when I first open the menu" the tester
            // felt — here the same cost hides behind the loading screen.
            {
                constexpr const char* kIconDir = "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\";
                GetTexture(std::string(kIconDir) + "spells_03_frame.png");  // storage
                GetTexture(std::string(kIconDir) + "spells_38_frame.png");  // skill tree
                std::size_t count = 0;
                const auto* nodes = SkillTree::Nodes(count);
                for (std::size_t i = 0; i < count; ++i) {
                    GetTexture(std::string(kIconDir) + nodes[i].icon);
                }
                logger::info("UI: panel icons preloaded");
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
                Style::UpdateScale(io.DisplaySize.y);
                FeedImGui(io);

                ImGui_ImplDX11_NewFrame();
                ImGui::NewFrame();

                // The flourish never blocks: it plays over normal gameplay, and it is
                // drawn before the cursor so it can never sit on top of it.
                DrawLevelUpEffect();
                DrawSystemWindow();
                DrawSkillTree();

                // Only while we own the input, and above everything else — otherwise
                // it would sit on screen next to Skyrim's own cursor whenever the game
                // opens a menu of its own (a Survival Mode prompt, say).
                if (IsCapturingInput()) {
                    Style::DrawSystemCursor(ImGui::GetForegroundDrawList(), io.MousePos,
                                            Style::g_scale, Style::kAccent);
                }

                ImGui::Render();
                g_context->OMSetRenderTargets(1, &g_backBufferView, nullptr);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            }

            return g_originalPresent(a_swapChain, a_syncInterval, a_flags);
        }
    }

    bool IsCapturingInput() {
        return IsSystemWindowOpen() || IsSkillTreeOpen();
    }

    void SetGameHold(bool a_hold) {
        // Deliberately NOT ControlMap::ToggleControls: that corrupted the input
        // system (see the git history around the post-panel crash). These two are
        // plain scalars — the pause counter a vanilla menu bumps, and the flag that
        // stops PlayerControls from feeding its handlers.
        static bool held = false;
        if (a_hold == held) {
            return;
        }
        held = a_hold;

        if (auto* controls = RE::PlayerControls::GetSingleton()) {
            controls->blockPlayerInput = held;
        }
        if (auto* ui = RE::UI::GetSingleton()) {
            if (held) {
                ++ui->numPausesGame;
            } else if (ui->numPausesGame > 0) {
                --ui->numPausesGame;
            }
        }

        logger::info("Game {} for System panel", held ? "paused" : "resumed");
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

        // VR note: renderWindows[0] is the desktop MIRROR swap chain, so in VR this
        // overlay draws on the monitor, not inside the headset (in-HMD rendering is a
        // separate, later effort). We still hook it — the mirror UI is Phase-1 usable —
        // but this is the first thing a VR tester should confirm doesn't misbehave, as
        // the RendererData layout is only asserted for the flat-screen editions.
        if (REL::Module::IsVR()) {
            logger::warn("UI: VR detected — overlay will render to the desktop mirror only "
                         "(in-headset UI not implemented yet)");
        }

        auto** vtable = *reinterpret_cast<void***>(swapChain);
        g_originalPresent = reinterpret_cast<PresentFn>(vtable[kPresentVTableIndex]);

        REL::safe_write(reinterpret_cast<std::uintptr_t>(&vtable[kPresentVTableIndex]),
                        reinterpret_cast<std::uintptr_t>(&HookedPresent));

        InstallInput();

        logger::info("UI: swap chain Present hooked — overlay armed");
    }
}
