#include "UI/Overlay.h"

#include "ModAPI.h"
#include "Quests.h"
#include "SkillTree.h"
#include "UI/Input.h"
#include "UI/LevelUpEffect.h"
#include "UI/ShopWindow.h"
#include "UI/SkillTreeWindow.h"
#include "UI/Style.h"
#include "UI/SystemWindow.h"
#include "UI/Textures.h"
#include "UI/ThreatLabels.h"
#include "UI/Toast.h"
#include "UI/VROverlay.h"

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
                // The handle is deliberately dropped — the cache keeps it, and warming
                // it is the whole point of the call here.
                const auto warm = [&](const std::string& a_name) {
                    static_cast<void>(GetTexture(kIconDir + a_name));
                };

                // Status panel chrome: the three action buttons and every rank insignia
                // (the badge changes as the character grows, so all six are warmed).
                for (const char* icon : { "ui_skilltree.png", "ui_storage.png", "ui_shop.png",
                                          "rank_e.png", "rank_d.png", "rank_c.png", "rank_b.png",
                                          "rank_a.png", "rank_s.png",
                                          // The blessing choice is shown once per character,
                                          // so warming it is not about the hitch — it is so a
                                          // missing file is reported at load instead of only
                                          // at the one moment it would have been drawn.
                                          "blessing_normal.png", "blessing_hero.png",
                                          "blessing_ascended.png", "blessing_full.png",
                                          "blessing_shattered.png", "blessing_dormant.png",
                                          "blessing_custom.png" }) {
                    warm(icon);
                }
                std::size_t count = 0;
                const auto* nodes = SkillTree::Nodes(count);
                for (std::size_t i = 0; i < count; ++i) {
                    warm(nodes[i].icon);
                }
                logger::info("UI: panel icons preloaded");
            }

            logger::info("UI: ImGui overlay initialised");
            return true;
        }

        // Ask the quest system whether anything is due, once a second, from the main
        // thread. Everything it might touch is game state, so the render thread only ever
        // decides WHEN — never what.
        void PollGameClock() {
            using namespace std::chrono;
            static steady_clock::time_point last{};
            const auto                      now = steady_clock::now();
            if (now - last < seconds(1)) {
                return;
            }
            last = now;
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([]() {
                    Quests::Tick();
                    // Same cadence, same thread, and the API's listeners want exactly
                    // the state this tick may just have changed.
                    ModAPI::PublishIfChanged();
                });
            }
        }

        // Set once at Install. In VR the same hook fires (on the desktop mirror's
        // swap chain) but nothing is drawn into that back buffer: the mirror is not what
        // the player is looking at. It is used purely as a per-frame tick on the render
        // thread, which is the one thread a D3D11 immediate context may be touched from —
        // and the helper's own frame callback is explicitly not it.
        bool g_vrMode = false;

        HRESULT WINAPI HookedPresent(IDXGISwapChain* a_swapChain, UINT a_syncInterval, UINT a_flags) {
            if (g_vrMode) {
                DrawVRFrame();
                // Quests never ticked in VR before, because the only caller sat inside the
                // flat overlay's frame. The System handing out work on its own schedule is
                // not a flat-screen feature.
                PollGameClock();
                return g_originalPresent(a_swapChain, a_syncInterval, a_flags);
            }

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
                DrawShopWindow();

                // HUD layers: they draw over gameplay and over our own panels, take no
                // input, and pause nothing.
                DrawThreatLabels();
                DrawToasts();

                // An SKSE plugin has no per-frame MAIN-thread hook, and "the System offers
                // work on its own schedule" needs one. This is the render thread, so the
                // poll only reads a clock here and does the work on a task. Throttled hard:
                // once a second is far finer than a cadence measured in game days.
                PollGameClock();

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

    namespace {
        // Read a pointer we are NOT certain points at an object, without dying if it does
        // not. Both live alone in their own functions on purpose: MSVC will not compile
        // __try/__except into a function that also needs C++ unwinding, and keeping them
        // free of objects is what makes that safe.
        //
        // This is deliberately not IsBadReadPtr, which lies on guard pages and is
        // discouraged for exactly that reason. SEH catches the access violation that
        // actually happens, which is the thing we care about.
        [[nodiscard]] void** SafeVTable(void* a_maybeObject) {
            if (!a_maybeObject) {
                return nullptr;
            }
            __try {
                return *reinterpret_cast<void***>(a_maybeObject);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return nullptr;
            }
        }

        [[nodiscard]] void* SafeVTableEntry(void** a_vtable, std::size_t a_index) {
            if (!a_vtable) {
                return nullptr;
            }
            __try {
                return a_vtable[a_index];
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return nullptr;
            }
        }
    }

    namespace {
        // The swap-chain vtable, obtained WITHOUT asking Skyrim for its swap chain.
        //
        // This exists because the normal route does not work in VR. RE::BSGraphics::
        // Renderer's layout is the flat-screen one; under Skyrim VR the same read yields a
        // non-null but meaningless renderWindows[0].swapChain, and dereferencing it is the
        // access violation a player reported as a crash on load.
        //
        // So: create a throwaway swap chain of our own on a hidden 1x1 window and read the
        // vtable off THAT. Every IDXGISwapChain made by the same dxgi.dll shares one
        // vtable, so patching Present here patches the game's swap chain too, whichever
        // one it is and wherever it lives in a struct we cannot read. Nothing about the
        // game's memory layout is involved, which is precisely the point.
        //
        // The device and window are released immediately; the vtable belongs to dxgi.dll
        // and outlives them both.
        [[nodiscard]] void** ProbeSwapChainVTable() {
            WNDCLASSEXA wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = DefWindowProcA;
            wc.hInstance = GetModuleHandleA(nullptr);
            wc.lpszClassName = "IsekaiHeroSwapChainProbe";
            if (!RegisterClassExA(&wc)) {
                logger::error("UI: could not register the probe window class");
                return nullptr;
            }

            HWND window = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1,
                                          nullptr, nullptr, wc.hInstance, nullptr);
            if (!window) {
                UnregisterClassA(wc.lpszClassName, wc.hInstance);
                logger::error("UI: could not create the probe window");
                return nullptr;
            }

            DXGI_SWAP_CHAIN_DESC desc{};
            desc.BufferCount = 1;
            desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.OutputWindow = window;
            desc.SampleDesc.Count = 1;
            desc.Windowed = TRUE;
            desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

            IDXGISwapChain*      swapChain = nullptr;
            ID3D11Device*        device = nullptr;
            ID3D11DeviceContext* context = nullptr;
            const HRESULT        hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                &desc, &swapChain, &device, nullptr, &context);

            void** vtable = nullptr;
            if (SUCCEEDED(hr) && swapChain) {
                vtable = SafeVTable(swapChain);
            } else {
                logger::error("UI: the probe swap chain could not be created ({:#x})",
                              static_cast<std::uint32_t>(hr));
            }

            if (swapChain) {
                swapChain->Release();
            }
            if (context) {
                context->Release();
            }
            if (device) {
                device->Release();
            }
            DestroyWindow(window);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return vtable;
        }
    }

    bool OverlayReady() {
        return g_ready.load(std::memory_order_acquire);
    }

    bool IsCapturingInput() {
        return IsSystemWindowOpen() || IsSkillTreeOpen() || IsShopWindowOpen();
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
        // Input first, and on EVERY runtime. InstallInput only registers event sinks
        // (keyboard/mouse, menu guard) — nothing touches the renderer, so it is safe in
        // VR, and it is what makes the System hotkey fire. It used to sit after the VR
        // early-return below, which meant VR registered no input at all and the hotkey
        // did nothing (reported: "menu not displaying on the keybind" in VR).
        InstallInput();

        // Two ways to the same vtable, because the flat-screen route does not exist in VR.
        //
        // SE/AE: read the game's own swap chain out of RE::BSGraphics::Renderer. That
        // struct's layout is the flat-screen one and this has shipped for months.
        //
        // VR: the same read yields a non-null but meaningless pointer, and dereferencing
        // it is the access violation a player reported as a crash on load. So VR does not
        // ask the game at all — it makes a throwaway swap chain of its own and reads the
        // vtable off that (see ProbeSwapChainVTable). Both editions end up patching the
        // same shared dxgi.dll vtable, so the hook is identical; only the way to find it
        // differs. The flat path is deliberately left exactly as it was rather than
        // switched over: it works, and nobody here can test either of them.
        void** vtable = nullptr;

        if (REL::Module::IsVR()) {
            g_vrMode = true;
            vtable = ProbeSwapChainVTable();
            if (!vtable) {
                logger::error("UI: no swap-chain vtable in VR — the in-headset layer has no "
                              "per-frame tick and will not draw. Menus still run through "
                              "PrismaUI.");
                return;
            }
            logger::info("UI: Skyrim VR — swap-chain vtable found by probe, drawing goes to "
                         "ImGuiVRHelper rather than the desktop mirror");
        } else {
            auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
            if (!renderer) {
                logger::error("UI: no renderer — overlay not installed");
                return;
            }

            auto* swapChain =
                reinterpret_cast<IDXGISwapChain*>(renderer->data.renderWindows[0].swapChain);
            if (!swapChain) {
                logger::error("UI: no swap chain — overlay not installed");
                return;
            }

            // The null check above is not enough, and a crash report proved it: a player
            // on an old build reached this line under VR, where the struct read yields
            // garbage rather than null, and dereferencing it crashed before a line of ours
            // had run (rax = 0x0000042700000410). VR no longer comes through here at all,
            // but "no longer" is not a guarantee on someone else's machine — a runtime
            // IsVR() cannot identify would land here again. Probing under SEH turns the
            // worst case into "no overlay, and the log says why".
            vtable = SafeVTable(swapChain);
            if (!vtable) {
                logger::error("UI: swap chain {:p} is not a readable object — overlay not "
                              "installed",
                              static_cast<void*>(swapChain));
                return;
            }
        }
        void* present = SafeVTableEntry(vtable, kPresentVTableIndex);
        if (!present) {
            logger::error("UI: swap chain vtable {:p} has no readable Present at index {} — "
                          "overlay not installed",
                          static_cast<void*>(vtable), kPresentVTableIndex);
            return;
        }
        g_originalPresent = reinterpret_cast<PresentFn>(present);

        REL::safe_write(reinterpret_cast<std::uintptr_t>(&vtable[kPresentVTableIndex]),
                        reinterpret_cast<std::uintptr_t>(&HookedPresent));

        // NOTE: no InstallInput() here. It ran a second time on the flat runtime after the
        // VR fix moved the call to the top of this function, and it is not idempotent:
        // MenuControls::AddHandler appends without a duplicate check, so the menu guard
        // ended up in the chain twice (and the rotation below then put one copy at the
        // front while the other stayed in the middle).
        logger::info("UI: swap chain Present hooked — overlay armed");
    }
}
