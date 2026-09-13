#include "UI/VROverlay.h"

#include "Config.h"       // VRInHeadsetLayer, the gate on registering at all
#include "Progression.h"  // OpenStatusPanel, what the VR controller chord opens
#include "UI/Style.h"
#include "UI/ThreatLabels.h"
#include "UI/Toast.h"

#include <d3d11.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>

#include "ImGuiVRHelperClientSDK.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <fstream>
#include <vector>

namespace Isekai::UI {

    namespace {
        namespace VRH = ImGuiVRHelperPluginAPI;

        // The helper hands every client the same panel size today (1920x1080), but it is
        // documented as changeable between frames, so it is always read back from the
        // panel rather than assumed. This is only the LOGICAL canvas we lay out against.
        constexpr ImVec2 kCanvas{ 1920.0f, 1080.0f };

        // One client per compositing mode — see the header. Both are file-static because
        // the SDK's Client is non-copyable and owns a private ImGui context whose lifetime
        // has to match the plugin's.
        VRH::Client g_world;  // threat labels, anchored in the world
        VRH::Client g_hud;    // toasts, locked to the head

        bool g_ready = false;

        // Each private context owns its own font atlas, so a single Style::g_body cannot
        // serve both — the pointer is only valid inside the context it was built in.
        // These are set once per context and swapped into the globals around each draw.
        ImFont* g_worldBody = nullptr;
        ImFont* g_worldTitle = nullptr;
        ImFont* g_hudBody = nullptr;
        ImFont* g_hudTitle = nullptr;

        // Reused across frames rather than rebuilt: this runs every frame and the list is
        // capped at a dozen entries, so the allocation is pure waste after the first one.
        std::vector<VRH::WorldQuad> g_quads;

        // ---- The way into the menu that needs no keyboard --------------------
        //
        // A headset player reaching for RShift+S is not an answer, and whatever is wrong
        // with key delivery in VR, a controller chord does not travel that road at all.
        // The helper does the matching, the timing and the edge detection; we register a
        // default and ask once a frame whether it fired.
        VRH::ComboId g_openCombo = 0;

        // Y + B held together, one on each controller. A two-hand gesture on purpose:
        // either button alone is a normal Skyrim VR action (ready weapon, jump), and both
        // at once is not something a hand does by accident. 1 is
        // RE::BSOpenVRControllerDevice::Keys::kBY, which is what the helper's own key
        // codes are.
        constexpr std::uint32_t kKeyBY = 1;

        [[nodiscard]] std::vector<VRH::InputCombo> DefaultOpenChord() {
            return { VRH::InputCombo::Both(kKeyBY) };
        }

        // Rebinding lives here rather than in IsekaiHero.ini: the helper's own controller
        // map can rebind it at runtime, and that has to survive a restart. The ini is read
        // once at load and never written, so a setting the game changes does not belong in
        // it — that would be two owners for one value.
        constexpr const char* kBindingsPath =
            "Data\\SKSE\\Plugins\\IsekaiHero\\vrbindings.json";

        [[nodiscard]] std::vector<VRH::InputCombo> LoadOpenChord() {
            std::ifstream in(kBindingsPath);
            if (!in) {
                return DefaultOpenChord();  // never bound: first run, or the file was removed
            }
            try {
                nlohmann::json j;
                in >> j;
                std::vector<VRH::InputCombo> keys = j.value("openMenu", nlohmann::json{});
                // An empty list is a DELIBERATE unbind (the helper's Clear button), not a
                // parse failure — honour it rather than resurrecting the default.
                if (j.contains("openMenu")) {
                    logger::info("VR: open-menu chord loaded from {} ({} key(s))", kBindingsPath,
                                 keys.size());
                    return keys;
                }
            } catch (const std::exception& e) {
                // A hand-edited or truncated file must not stop the mod loading.
                logger::warn("VR: {} could not be read ({}) — using the default chord",
                             kBindingsPath, e.what());
            }
            return DefaultOpenChord();
        }

        void SaveOpenChord(const VRH::InputCombo* a_keys, std::size_t a_count) {
            try {
                nlohmann::json j;
                j["openMenu"] = std::vector<VRH::InputCombo>(a_keys, a_keys + a_count);
                std::ofstream out(kBindingsPath, std::ios::trunc);
                if (!out) {
                    logger::warn("VR: could not write {} — the rebind holds for this session "
                                 "only", kBindingsPath);
                    return;
                }
                out << j.dump(2) << '\n';
                logger::info("VR: open-menu chord rebound to {} key(s), saved", a_count);
            } catch (const std::exception& e) {
                logger::warn("VR: saving the rebound chord failed ({})", e.what());
            }
        }

        // Real elapsed time, measured here rather than read out of ImGui: the SDK's
        // RenderHud pins its private context's DeltaTime to a flat 1/60, so anything that
        // ages on it would run at 60 Hz's pace on a 90 Hz headset.
        [[nodiscard]] float FrameDelta() {
            using clock = std::chrono::steady_clock;
            static auto last = clock::now();
            const auto  now = clock::now();
            const auto  delta = std::chrono::duration<float>(now - last).count();
            last = now;
            // A stalled frame (loading screen, headset put down) must not expire the whole
            // queue at once, and a zero must not stall it forever.
            return std::clamp(delta, 1.0f / 1000.0f, 1.0f / 15.0f);
        }

        void LoadFonts(ImFont*& a_body, ImFont*& a_title) {
            ImGuiIO& io = ImGui::GetIO();
            a_body = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 20.0f);
            a_title = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consolab.ttf", 30.0f);
            if (!a_body || !a_title) {
                logger::warn("VR: Consolas not found — falling back to ImGui's built-in font");
            }
        }

        // Bind a client's fonts and scale into the Style globals for the duration of one
        // draw. The drawing code reads those globals, and there are now three contexts
        // that could be current (the flat overlay's, and one per VR client) — whichever
        // one drew last must not leave its fonts behind for the next.
        struct ScopedStyle {
            ImFont* body;
            ImFont* title;
            float   scale;

            ScopedStyle(ImFont* a_body, ImFont* a_title, float a_scale) :
                body(Style::g_body), title(Style::g_title), scale(Style::g_scale) {
                Style::g_body = a_body;
                Style::g_title = a_title;
                Style::g_scale = a_scale;
            }
            ~ScopedStyle() {
                Style::g_body = body;
                Style::g_title = title;
                Style::g_scale = scale;
            }
            ScopedStyle(const ScopedStyle&) = delete;
            ScopedStyle& operator=(const ScopedStyle&) = delete;
        };

        // The helper's panel, as it stands this frame. Returns false before the first one
        // is issued, which is normal for the first few frames after registration.
        [[nodiscard]] bool PanelSize(VRH::Client& a_client, ImVec2& a_out) {
            VRH::PanelHandle panel{};
            auto*            helper = a_client.Helper();
            if (!helper || !helper->GetPanel(a_client.Id(), &panel) || !panel.width ||
                !panel.height) {
                return false;
            }
            a_out = ImVec2{ static_cast<float>(panel.width), static_cast<float>(panel.height) };
            return true;
        }

        // Device and immediate context, taken from the panel's own render target view.
        //
        // Deliberately NOT from RE::BSGraphics::Renderer: that struct's layout is the
        // flat-screen one, and reading it under Skyrim VR is what produced the reported
        // access-violation crash on load. The RTV knows which device made it, and the
        // game has exactly one, so this is both correct and layout-independent.
        [[nodiscard]] bool ResolveDevice(VRH::Client& a_client, ID3D11Device*& a_device,
                                         ID3D11DeviceContext*& a_context) {
            static ID3D11Device*        cachedDevice = nullptr;
            static ID3D11DeviceContext* cachedContext = nullptr;
            if (cachedDevice && cachedContext) {
                a_device = cachedDevice;
                a_context = cachedContext;
                return true;
            }

            VRH::PanelHandle panel{};
            auto*            helper = a_client.Helper();
            if (!helper || !helper->GetPanel(a_client.Id(), &panel) || !panel.rtv) {
                return false;
            }
            ID3D11Device* device = nullptr;
            panel.rtv->GetDevice(&device);
            if (!device) {
                return false;
            }
            ID3D11DeviceContext* context = nullptr;
            device->GetImmediateContext(&context);
            // Both are owned by the device for its whole life, and the device outlives us,
            // so the references are dropped and the raw pointers kept — the same reasoning
            // the helper's own SDK uses for the immediate context.
            if (context) {
                context->Release();
            }
            device->Release();
            if (!context) {
                return false;
            }
            cachedDevice = device;
            cachedContext = context;
            a_device = device;
            a_context = context;
            logger::info("VR: D3D device resolved from the helper's panel target");
            return true;
        }
    }

    void InstallVROverlay() {
        if (!REL::Module::IsVR()) {
            return;
        }
        // Off by default, and the ini says why at length. Short version: a VR player
        // reported that this mod alongside ImGuiVRHelper sends the game to the desktop as
        // the mods finish loading, every time, with no crash log written. Without the
        // helper the mod runs and the menus work — and this layer is the one thing that
        // does any work only when the helper is present, which is what points at it.
        //
        // The gate sits HERE, before Connect(), rather than in DrawVRFrame: not connecting
        // is the state the working log shows, and it also leaves the helper free for the
        // other mods that use it. The cause is still unknown; this is a way around it, not
        // a repair.
        if (!Config::VRInHeadsetLayer()) {
            logger::info("VR: in-headset layer is off (VRInHeadsetLayer = 0) — no labels or "
                         "toasts in the headset, and no controller chord. Menus still run "
                         "through the PrismaUI patch. It ships off because it crashed for "
                         "the one player who tried it; turn it on to help find out why.");
            return;
        }

        if (!VRH::IsHelperInstalled()) {
            logger::info("VR: ImGuiVRHelper is not installed — no in-headset labels or "
                         "toasts. Menus still run through the PrismaUI patch. Install "
                         "ImGuiVRHelper to get the HUD layer.");
            return;
        }

        // Surfaced verbatim in the helper's "Registered Clients" table, so a tester's
        // screenshot of it says which build they are on.
        constexpr const char* version = ISEKAI_VERSION;

        // World quads for the threat labels: billboards at the actors' heads, projected by
        // the helper with the scene depth bound, so a label behind a wall is occluded by
        // it rather than drawn over it. That occlusion is free here and is the thing the
        // flat path had to add a line-of-sight check for.
        if (g_world.Connect("Isekai Hero", version, VRH::kClientFlag_WorldQuad)) {
            g_world.SetHudStyleCallback([]() { LoadFonts(g_worldBody, g_worldTitle); });
            logger::info("VR: world-quad client registered (id {})", g_world.Id());
        } else {
            logger::error("VR: the world-quad client was rejected — no threat labels");
        }

        // A second client, and a second NAME: the helper keys clients by name and rejects
        // a duplicate, so these cannot both be "Isekai Hero".
        if (g_hud.Connect("Isekai Hero HUD", version, VRH::kClientFlag_HUDMode)) {
            g_hud.SetHudStyleCallback([]() { LoadFonts(g_hudBody, g_hudTitle); });
            logger::info("VR: HUD client registered (id {})", g_hud.Id());
        } else {
            logger::error("VR: the HUD client was rejected — no toasts in the headset");
        }

        // The chord goes on whichever client registered — it is a global binding, not a
        // panel one, and neither of ours ever takes focus, so it fires whenever the player
        // presses it. offPanel is left false for exactly that reason: an off-panel combo
        // is gated on a wand that is not pointing at us, and we have nothing to point at.
        VRH::Client& owner = g_world.IsConnected() ? g_world : g_hud;
        if (owner.IsConnected()) {
            g_openCombo = owner.AddCombo(
                "Open the System menu", LoadOpenChord(),
                [](const VRH::InputCombo* keys, std::size_t n) { SaveOpenChord(keys, n); },
                DefaultOpenChord());
            if (g_openCombo != 0) {
                logger::info("VR: open-menu chord registered (combo {})", g_openCombo);
            } else {
                logger::warn("VR: the open-menu chord was not registered — the menu still "
                             "needs the keyboard hotkey in VR");
            }
        }

        g_ready = g_world.IsConnected() || g_hud.IsConnected();
        if (g_ready) {
            logger::info("VR: in-headset layer armed (world quads: {}, HUD: {})",
                         g_world.HasWorldQuads() ? "yes" : "helper too old", g_hud.IsConnected());
        }
    }

    bool VROverlayReady() {
        return g_ready;
    }

    void DrawVRFrame() {
        if (!g_ready) {
            return;
        }

        // Asked before anything is drawn, and asked every frame whether or not a panel
        // exists yet: Fired() is edge-triggered and consumes the activation, so skipping
        // the poll on a frame does not defer the press — it loses it.
        //
        // The work itself goes to the main thread. This is the render thread, and opening
        // the System panel touches the UI singleton, the player and the co-save state,
        // none of which are ours to read from here.
        if (g_openCombo != 0) {
            VRH::Client& owner = g_world.IsConnected() ? g_world : g_hud;
            if (owner.Fired(g_openCombo)) {
                logger::info("VR: open-menu chord fired");
                if (auto* task = SKSE::GetTaskInterface()) {
                    task->AddTask([]() { Progression::OpenStatusPanel(); });
                }
            }
        }

        ID3D11Device*        device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        VRH::Client&         anchor = g_world.IsConnected() ? g_world : g_hud;
        if (!ResolveDevice(anchor, device, context)) {
            return;  // no panel issued yet; normal for the first frames after registration
        }

        if (g_world.IsConnected()) {
            ImVec2 panel = kCanvas;
            static_cast<void>(PanelSize(g_world, panel));

            // Scale 1.0, not the player's UiScale: the billboard's size in the world comes
            // from VRThreatLabelHeight, and scaling the pixels as well would only change
            // how much of the panel each label eats — sharper or blurrier, never bigger.
            g_world.RenderHud(device, context, panel, [&panel]() {
                ScopedStyle style(g_worldBody, g_worldTitle, 1.0f);
                DrawThreatLabelsVR(panel, g_quads);
            });

            // Submitted even when empty, and OUTSIDE the draw callback so it still runs on
            // a frame where the panel was not issued: the helper keeps the previous list
            // until it is replaced, so "no labels this frame" has to be said explicitly or
            // the last set hangs in the world forever.
            g_world.SubmitWorldQuads(g_quads.empty() ? nullptr : g_quads.data(), g_quads.size());
        }

        if (g_hud.IsConnected()) {
            ImVec2 panel = kCanvas;
            static_cast<void>(PanelSize(g_hud, panel));

            // Here the player's UiScale DOES apply: this is a flat plane at a fixed
            // distance, so bigger pixels are the only way to make the text bigger.
            const float dt = FrameDelta();
            g_hud.RenderHud(device, context, panel, [dt]() {
                ScopedStyle style(g_hudBody, g_hudTitle, Style::g_userScale);
                DrawToasts(dt);
            });
        }
    }

    void ShutdownVROverlay() {
        if (!g_ready) {
            return;
        }
        // Quads first: unregistering releases the panel, and leaving a list pointing at a
        // texture that is going away is not something to find out about later.
        g_world.SubmitWorldQuads(nullptr, 0);
        g_world.ShutdownHud();
        g_hud.ShutdownHud();
        g_world.Disconnect();
        g_hud.Disconnect();
        g_ready = false;
        logger::info("VR: in-headset layer shut down");
    }
}
