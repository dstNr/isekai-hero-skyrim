#pragma once

// The in-headset layer, through ImGuiVRHelper.
//
// WHY THIS EXISTS AT ALL
//
// The flat overlay draws straight onto the game's back buffer. In VR that buffer is the
// desktop mirror, which nobody in a headset is looking at — so the whole ImGui layer was
// simply switched off there, and threat labels and toasts did not exist in VR. The
// PrismaUI patch covers the menus, but it cannot carry a per-frame HUD: a web view
// repainting nameplates every frame is exactly the frame-rate collapse this avoids.
//
// ImGuiVRHelper solves the missing half. It hands each registered client a render target,
// composites it into the headset itself at the OpenVR submit hook, and — for world quads —
// projects sub-rects of that texture as billboards at world positions, occluded by scene
// geometry. We draw the same frames we always drew; the helper puts them in the world.
//
// TWO CLIENTS, NOT ONE
//
// Threat labels belong to actors, so they are world quads: anchored in the world, they
// stay on the enemy while the head turns. Toasts belong to the player, so they are a HUD
// plane: head-locked, always facing, no anchor. Those are two different compositing modes
// in the helper, one flag each, and a client picks exactly one — hence two.
//
// WITHOUT THE HELPER INSTALLED
//
// Everything here no-ops. The helper is a soft dependency: a VR player without it is
// exactly where they were before this file existed, and the log says so once.

namespace Isekai::UI {

    // Perform the handshake and register both clients. Call at kPostPostLoad — the
    // helper's SKSE listener is up by then regardless of load order. No-op outside VR.
    void InstallVROverlay();

    // True once at least one client is registered, i.e. we have somewhere to draw.
    [[nodiscard]] bool VROverlayReady();

    // Draw one frame of both layers. Called from the Present hook, on the render thread,
    // because that is the only place a D3D11 immediate context may be touched — the
    // helper's own per-frame callback runs on its frame thread and is not that place.
    void DrawVRFrame();

    // Unregister and tear down the private ImGui contexts. Safe if nothing was ever
    // registered.
    void ShutdownVROverlay();
}
