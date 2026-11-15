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
- VTables and event sources (`LevelIncrease`) — NG resolves these per runtime. No bare
  `REL::ID` / 2-arg `RelocationID` that would break in VR.
- The per-frame hook of the interface is a vtable hook on `PlayerCharacter::Update`, which
  NG resolves per runtime too. The interface added no Address Library IDs.

`src/main.cpp` logs the detected edition (SE/AE/VR) at load — the first line to check on a
VR bring-up.

## The interface in VR: Magelight UI

Every screen of the mod is an HTML page that
[Magelight UI](https://www.nexusmods.com/skyrimspecialedition/mods/193303) (1.7.0 or newer)
draws. The mod has no renderer of its own any more: the old ImGui overlay (which hooked the
desktop mirror swap chain and crashed on the VR renderer), the PrismaUI view and the
ImGui VR Helper layer are gone. Magelight brings its own VR overlays, laser pointer and VR
keyboard, so the same pages serve every runtime.

What that means in a headset:

- **System panel** (status, skill tree, storage, shop, the blessing choice). Magelight's
  default VR panel placement, operated with its laser pointer and VR keyboard. Opened with
  the `VRMenuButton` binding in the ini (a scan code, bound on both controllers; **off by
  default** — the comment above the key in `IsekaiHero.ini` says how to find the code of
  a button).
- **HUD** (toasts, banners, the level-up flourish). A head-locked view in the upper field
  of view.
- **Threat labels are a scan list in VR.** Magelight can place a view head-locked, lock it
  into the world once, or lazily follow the head. It cannot pin one element over each
  moving actor, so a label floating over every enemy is not possible. VR shows a
  head-locked list of the nearest labelled actors instead, most dangerous first, with the
  same verdicts (TRIVIAL / MANAGEABLE / DANGEROUS / LETHAL), content and tones as the flat
  game. Flat screens keep the labels above heads.
- **No Magelight, no stranded character.** If Magelight UI is missing or older than 1.7.0
  the mod holds off the reincarnation prompt (the one-shot flag is not set), writes one
  note and waits — on every runtime, not only VR. Install Magelight and the flow starts
  normally on the next load.
- Features newer than the 1.7.0 floor are gated by Magelight's capability query, not by
  its version. `gamepad` is 0 in VR (the laser pointer replaces it) and `prepaint` never
  paints in VR; the mod simply does without them there.

### Retired

The ini keys `VRInHeadsetLayer` and `VRThreatLabelHeight` are ignored; an old ini that
still carries them loads without complaint. The two-button controller chord that opened the
menu came from the ImGui VR Helper and went with it.

## Player requirements for VR (differing from SE/AE)

- **SKSEVR** and the **VR Address Library for SKSEVR** (the flat Address Library does not
  carry VR offsets).
- **Magelight UI 1.7.0 or newer**, in a build that runs in VR. Whether a given Magelight
  build works in a given VR setup is that mod's own matter; see its page.
- Nothing else. PrismaUI, ImGui VR Helper and Media Keys Fix are no longer used.

## What a first VR test must check

1. **Loads at all.** `IsekaiHeroSKSE.log` reports the VR edition near the top, and there
   is no crash. Everything else depends on this.
2. **Magelight is found.** The log has `Magelight: requesting the API`, then
   `Magelight: creating view system (...)` and `... view hud ...`, then a
   `Magelight: system page ready` and a `Magelight: hud page ready` line. Every failure
   line ends in `no Isekai Hero UI this session` and says why:
   - `Magelight UI not loaded` — the DLL is absent or did not load;
   - `Magelight UI x.y.z is too old (1.7.0 or newer needed)`;
   - `page ... not found` — the `Data\Magelight\IsekaiHero\` folder did not install;
   - `view ... not created` or `listener ... refused` — Magelight rejected the view.
3. **The panel opens.** `VRMenuButton` is off by default: set it first, then check that
   the System panel appears, the laser pointer clicks and the VR keyboard types.
4. **The blessing choice appears** on a fresh character — the screen a stuck character
   would be stranded without.
5. **The scan list.** With enemies near, a head-locked list of up to 12 actors appears,
   most dangerous first, and stays in view as the head turns. `F10` toggles it, as in the
   flat game.
6. **Toasts and the flourish** appear in the upper field of view and fade on their own.
7. **Crafting with the storage chest.** The VR offsets are already in `CraftHooks.cpp` and
   have never been exercised.

The log is `Documents\My Games\<Skyrim folder>\SKSE\IsekaiHeroSKSE.log` (in a modlist that
redirects the profile, under that profile's `My Games`). Please attach it to the
[VR issue](https://github.com/dstNr/isekai-hero-skyrim/issues/35).

## History

Earlier revisions of this file weighed three routes into the headset — the ImGui VR Helper
layer, PrismaUI and a native `MessageBox` fallback — and kept a bring-up diary around
them. All three went away with the renderers they depended on; the diary stays in the git
history of this file. The one open question now is whether the Magelight build a VR player
has installed shows the pages and takes the laser pointer.
