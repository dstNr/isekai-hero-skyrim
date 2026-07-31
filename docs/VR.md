# Skyrim VR — status & plan

**Status: EXPERIMENTAL / UNTESTED.** The mod is built VR-compatible, but with no SkyrimVR
install available nobody has verified it in a running game. Until that happens VR is
explicitly "might work", not "supported".

## Why most of it is already in place

CommonLibSSE-NG builds **one** DLL for SE, AE and VR; the runtime is detected at load.
The vcpkg port (v3.7.0) is built with `ENABLE_SKYRIM_VR=1`, and these defines propagate to
our plugin (`HAS_SKYRIM_MULTI_TARGETING=1`). The plugin declaration uses
`VersionIndependence::AddressLibrary` with no runtime restriction — which includes VR via
the VR Address Library.

The logic layer is runtime-neutral:
- Reincarnation, milestones, passives, storage, skill-tree data, serialization — pure
  game-object logic.
- Crafting hooks already use `REL::VariantID(se, ae, vr)` with real VR offsets
  (`src/CraftHooks.cpp`).
- VTables, event sources (`LevelIncrease`), `BSGraphics::Renderer` — NG resolves these per
  runtime. No bare `REL::ID` / 2-arg `RelocationID` that would break in VR.

`src/main.cpp` logs the detected edition (SE/AE/VR) at load — the first line to check on a
VR bring-up.

## Strategy: the VR UI goes entirely through PrismaUI

Decision (July 2026): in VR there is **no UI of our own**. The ImGui overlay is off in VR
(it crashes on the VR renderer, see below), so in VR **every** panel goes through the
PrismaUI patch. Quick to do, community-testable; a native in-HMD UI (Phase 2, `MessageBox`
fallback) stays as a fallback option in case PrismaUI VR proves too shaky.

So it doesn't wreck a character: `BeginReincarnation()` checks `UI::Prisma::Active()` in
VR. If PrismaUI is **not** active, the one-shot flag `reincarnated` is **not** set —
otherwise the character would be "reincarnated with no way to ever pick a blessing".
Instead a single native notification (the headset renders it) and it waits until PrismaUI
is there.

## Player requirements for VR (differing from SE/AE)

- **SKSEVR** instead of SKSE64
- **VR Address Library for SKSEVR** instead of "Address Library for SKSE Plugins"
- **PrismaUI 1.5.0 VR build** + our **PrismaUI patch** — mandatory in VR, because the
  built-in UI is off. The stable PrismaUI 1.4.x is **not** enough (no VR).
- The same mod archive as for SE/AE — the DLL is multi-runtime, no separate VR build
  needed.

## Open risks (what a VR test must check first)

1. **Does the plugin load at all?** Check the log for `Runtime edition: Skyrim VR`. If the
   line is missing or SKSEVR rejects the plugin → VR Address Library / SKSEVR version.
2. **Overlay/UI (the big one) — CONFIRMED: crashes in VR, now disabled.**
   The ImGui overlay hooked `IDXGISwapChain::Present` of the desktop **mirror** swap chain
   (`renderWindows[0]`, `src/UI/Overlay.cpp`). In VR the same struct read yields an
   **invalid but non-null** `swapChain` (the `RendererData` layout is only fixed for the
   flat-screen editions via `static_assert`) — dereferencing its vtable was an immediate
   `EXCEPTION_ACCESS_VIOLATION` on load (reported by a tester on 0.5.0, `Overlay.cpp:221`).
   **Since the fix the overlay hook is skipped in VR** via `REL::Module::IsVR()` — no more
   crash, but VR therefore has **no ImGui UI** (the blessing choice via the ImGui path does
   not appear).
   → **Addendum (tester report):** the VR return also skipped `InstallInput()`, which sat
     at the very end of the same function — so VR registered **no input at all** and the
     System hotkey did nothing ("menu not displaying on the keybind"). Fix: `InstallInput()`
     now runs BEFORE the VR return (event sinks only, renderer-free, VR-safe); only the
     swap-chain hook stays skipped. The log now shows
     `System hotkey fired — opening status (...)` for confirmation.
   → A real VR UI is **Phase 2** (in-HMD rendering via OpenVR overlay / stereo targets; a
     tester named the mod **"ImGui VR Helper"**, which can render ImGui in VR — possibly
     the way to get the ImGui overlay into the headset after all. Or — smaller — a fallback
     to the game's own `MessageBox` menus for the panels, which the headset renders itself).
3. **PrismaUI patch in VR — clarified upstream (as of July 2026):** VR is supported **only
   in the PrismaUI 1.5.0 VR alpha / 1.5.0-rc**, as an experimental alpha ("full VR support
   is coming", best on Meta headsets, alpha build via Discord/Dwemer Mods, not the stable
   Nexus file 148718). The **stable 1.4.x our patch is built against cannot do VR.** That
   means for us:
   - The overlay crash (bug 1) is in OUR ImGui overlay, not PrismaUI — it is fixed
     independently.
   - A VR player who wants to see our PrismaUI UI in the headset needs the **PrismaUI 1.5.0
     VR alpha**, not the stable 1.4.x. Our patch talks to the V1 API; 1.5.0 keeps V1 (V2 is
     additive only), so it should keep working — untested.
   - For the first pure load/logic test (bug 2) PrismaUI doesn't matter; all that counts is
     that passives/sounds/storage resolve.
4. **Crafting hooks.** The VR offsets in `CraftHooks.cpp` come from earlier work and were
   never checked in VR — test them at a workbench in VR (recipe visibility, material
   consumption).

5. **Form resolution (CONFIRMED broken in 0.5.0/0.5.1, now fixed).** In the VR log,
   `TESDataHandler::LookupForm(localID, kFileName)` resolved to null for ALL of our forms
   (passives 0/8, sounds 0/4, storage container missing), even though `DumpForms` found the
   same forms. Fix: `Plugin::LookupOurForm<T>()` reconstructs the FormID via
   `GetPartialIndex()` (the same math as DumpForms) and calls `LookupByID` — which
   demonstrably found the forms in the VR log. Identical result on SE/AE as before. **Not
   yet verified in VR**, but empirically grounded (DumpForms found the forms in the same
   log). On a VR test, watch for `Passives: 8 of 8` and `Sounds: 4 of 4` in the log.

## Phases

- **Phase 1 (done):** VR-loadable build, runtime logging, docs, requirements. The overlay
  crash is fixed (the hook is skipped in VR) — the mod loads and the logic runs, but VR has
  no UI for now.
- **Phase 2 (open):** get the UI into the headset. Two routes: (a) in-HMD rendering via
  OpenVR overlay / stereo targets (heavy), or (b) a fallback to the game's own `MessageBox`
  menus for the panels (the headset renders them natively; the skill tree stays the hard
  case). Only worthwhile with a VR test environment.
