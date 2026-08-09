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
   → **Addendum 2 (tester report on 0.6.1, Aug 2026):** the hotkey still did not open the
     menu. Verified that the input fix above IS in the `v0.6.1` tag, so the handler is
     reached. The remaining hole was that a built-in screen in VR failed **silently**: with
     PrismaUI inactive, `ShowStatusPanel` fell through to the ImGui path, which in VR is
     never rendered — the panel was "opened" into nothing, indistinguishable from a dead
     hotkey. `UI::BuiltInUiCanDisplay()` now gates the three built-in entry points and
     raises a native notification instead. **This is a diagnosis fix, not proof of the root
     cause** — whether that tester's PrismaUI view is actually active still has to come from
     their log (see "Reading a VR log" below).
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

## Addendum 3 — the 0.6.1 tester's log (Aug 2026)

The log settled two things and left one open.

**PrismaUI is fine.** `Prisma: web UI active (view …)` followed by `Prisma: view DOM ready`
— the framework loaded, our patch is installed, the view was created. The "missing patch"
theory, which was the most likely cause, is wrong.

**The hotkey never reaches the handler.** `System hotkey fired — opening status (…)` does
not appear anywhere in the log, so the problem is upstream of everything the UI does. The
input sinks *are* installed (`UI: hooked Skyrim's input event stream`,
`menu guard armed at the front of the chain (9 handlers)`), so it is not the ordering bug
Addendum 1 fixed.

**Still open: what the game actually sends.** `InputSink::ProcessEvent` only considers
`INPUT_DEVICE::kKeyboard`. CommonLibSSE-NG additionally defines `kVRRight = 5` and
`kVRLeft = 6` under `ENABLE_SKYRIM_VR`, and those are dropped before any hotkey is
matched. Whether that is the cause depends on what device the tester's key arrives under —
which nothing in the log said, because the key-press diagnostic was a compile-time
constant set to `false`.

It is `LogInputDiagnostics` in the ini now, so it can be switched on without a special
build. Ask the tester to set it, press the menu key, and send the log. Every button press
then logs as:

```
input: device=0 scanCode=0x2f (keyboard — hotkeys see this)
input: device=5 scanCode=0x…  (NOT keyboard — hotkeys ignore this)
```

- **Nothing logged at all** → no input event reaches the plugin; the sink is installed but
  the VR runtime is not routing through it.
- **device=0 with the configured scan code** → the event arrives and is accepted, so the
  fault is downstream, inside `FireHotkey` or the handler.
- **device=5 or 6** → confirmed: VR controller input, currently dropped. The fix is then to
  accept those devices, which needs care because their button IDs are a different
  namespace from keyboard scan codes and could collide.

> That tester's ini has `SystemMenuKey=0x2f, SystemMenuModifier=0x0` — V with no modifier,
> not the default RShift+S. Worth confirming they are pressing V on a physical keyboard,
> since that is the only thing the current code can act on.

> **On asking a user to enable input logging.** The mod reads Skyrim's own in-process
> event stream (`BSInputDeviceManager::AddEventSink`) — there is no `SetWindowsHookEx`, no
> `GetAsyncKeyState`, no raw input, and nothing anywhere converts a scan code to a
> character, so the API patterns antivirus keylogger heuristics look for are absent. It
> still records scan codes typed into game text fields, which is why it is off by default,
> named for what it is, documented in the ini, and stops after 200 presses per session.
> Worth saying plainly when asking someone to switch it on.

## Reading a VR log

Ask for `Documents\My Games\Skyrim VR\SKSE\IsekaiHeroSKSE.log`. **No special build is
needed** — every line below has existed since `v0.6.1` (verified against the tag). Exactly
one `Prisma:` line is written at load, and it alone separates the likely causes:

| Log line | Means |
|---|---|
| `Prisma: web UI active (view N)` | PrismaUI is fine — look further downstream |
| `Prisma: PrismaUI not loaded` | the framework is absent or its API request failed |
| `Prisma: PrismaUI present but the view patch is not installed` | **our** patch archive is missing (the most common case: the base mod ships two archives) |
| `Prisma: CreateView failed` | the framework is there but rejected our view |

Then check whether `System hotkey fired — opening status (prisma=…, paused=…)` appears on
a key press. Present ⇒ input works and the problem is rendering; absent ⇒ input.

## Phase 2 — getting the UI into the headset

Three routes, in the order they are currently worth considering.

### (a) ImGui VR Helper — most promising, but not the "four easy steps" it looks like

[alandtse/imgui-vr-helper](https://github.com/alandtse/imgui-vr-helper) renders an
existing ImGui menu onto a flat panel in VR. Its README documents four integration steps:
pull `api/` via CMake FetchContent, `Connect()` in `kPostPostLoad`, `Update(menuOpen)` per
frame, and `RenderFrame()` in place of `ImGui_ImplDX11_RenderDrawData`. It is a runtime
dependency, but `Connect()` returns false when absent, so the soft-dependency pattern we
already use for PrismaUI and SkyrimNet applies. API headers are LGPL-3.0-or-later; the
helper itself GPL-3.0 with modding exceptions. **This repo is MIT (see LICENSE), which can link an LGPL library without becoming LGPL — but read the helper's own terms before depending on it.**

**CORRECTED 2026-08-09, by reading the actual API instead of this document.** An earlier
version of this section said the helper exposes no per-frame render callback and that we
would therefore still need our own frame tick — the very thing that is broken in VR. That
was quoted from an older SDK and **is no longer true**. It was also the entire basis for
calling this route expensive, so the estimate below is much smaller than it used to be.

What the API (LGPL-3.0, `alandtse/imgui-vr-helper`, `api/`) actually offers:

- `RegisterClient(name, version, OnFrameFn on_frame, void* user, flags)` — **the helper
  calls us.** `OnFrameFn` is `void(*)(const Frame*, void*)`, delivered per frame with the
  HMD pose, both hands, focus flags and `dt`.
- `GetPanel(client_id, PanelHandle*)` hands back `{ width, height, ID3D11RenderTargetView* }`
  — a render target we draw into.
- The client SDK resolves the D3D device **from that RTV** (`ResolveImmediateContext`,
  which notes there is only one `ID3D11Device` in the game), and provides `RenderToPanel`,
  `RenderHud` and `BlitDrawData`.

**That removes the blocker rather than working around it.** We never read
`RE::BSGraphics::Renderer` in VR, never hook Present, never touch
`renderWindows[0].swapChain`. The garbage read that crashes today — and that also backs
`UI/Textures.cpp`'s icon loader — is simply not on this path. The device arrives from the
helper.

- `SubmitWorldQuads(client_id, const WorldQuad*, …)` (interface 004) takes billboards
  positioned in **Skyrim world space, in game units**, and converts them itself from a
  fresh pose at submit time, explicitly so the client does not introduce jitter. That is
  purpose-built for the threat labels: we already compute exactly that point
  (`HeadPoint` in `UI/ThreatLabels.cpp`), and it is the one part of the interface PrismaUI
  could never carry, because a per-frame world-anchored HUD through a web view is what
  costs framerate.
- `RegisterCombo` / `ComboFired` give VR controller bindings, which `RShift+S` cannot.

Sizing: our DirectX surface is two files (`UI/Overlay.cpp`, `UI/Textures.cpp`); everything
else is renderer-agnostic ImGui and carries over untouched. The open work is the soft
connection, an `on_frame` that draws our existing UI into the panel, world quads for the
labels, and combos for input.

**Still nobody on this side can test it.** That stays the real cost, and it is why the
integration should be written to disable itself and log at every check rather than assume:
the failure mode must be "VR still has no overlay, and the log says which check failed",
never a crash on someone else's machine.

**Licensing, before any of this is vendored:** the API headers are LGPL-3.0. This repo is
MIT, which may link them, but the headers keep their own licence and the carve-out section
in `LICENSE` has to name them. That is a deliberate decision, not a detail to slip in with
a commit.

### (b) PrismaUI (current strategy)

Already implemented and free if it works; depends on PrismaUI's own 1.5.0 VR alpha. Other
PrismaUI mods demonstrably open in VR, so the framework is viable — establish whether our
patch is actually installed before investing in (a).

### (c) `MessageBox` fallback

Route the dialog panels through the game's own menus, which the headset renders natively.
Smallest of the three, but the skill tree and the shop have no sensible `MessageBox` form,
so it only ever covers part of the UI.

## Phases

- **Phase 1 (done):** VR-loadable build, runtime logging, docs, requirements. The overlay
  crash is fixed (the hook is skipped in VR) — the mod loads and the logic runs, but VR has
  no UI of its own.
- **Phase 2 (open):** see the three routes above. Still gated on a VR test environment, or
  at least on a responsive tester.
