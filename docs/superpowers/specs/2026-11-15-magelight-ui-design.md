# Isekai Hero — Magelight UI rewrite (design)

Date: 2026-11-15 · Status: draft for approval · Scope: Isekai Hero only (SotN Backgrounds follows, see "Phase 2")

## 1. Intent

The user wants the whole Isekai Hero interface moved to **Magelight UI** and redesigned while at it.

In the user's words:
- PrismaUI "is not that performant".
- "We switch over completely and also drop the ImGui version."
- "Rework the whole UI once."

The look they picked is the **projection** style: a System Window that looks projected into the world. It stays cyan by default, and other colours mark special things.

Why this matters beyond the look:

- **Performance.** Prisma is the reason for the move. A rewrite that looks better but runs slower has failed.
- **One renderer instead of two.**
  - Today every screen exists twice: about 6000 lines of ImGui C++ in `src/UI/`, plus a 1840-line Prisma page.
  - The ImGui path is still the default renderer whenever the Prisma patch is not installed.
  - Each change has to be made in both places, and the two drift apart. The shop, for example, was missed at four call sites (see `SystemWindow.h`).
- **VR.** The ImGui overlay crashes on the VR renderer and is switched off there. The VR HUD depends on ImGuiVRHelper, which is off by default after a crash report. Magelight brings its own VR overlays, laser pointer and keyboard.

## 2. Goals

1. **Magelight is the only renderer.** It covers every screen the mod has today:
   - System dialog panel: reincarnation boot sequence, blessing choice, rank choice, generic messages and self-test results.
   - Status ledger.
   - Skill tree.
   - Shop.
   - Level-up flourish.
   - Toasts.
   - Threat labels.
2. **Projection look** throughout, built on the approved mockup (section 7).
3. **Colour has meaning.** Cyan is the default. Gold, red and violet are reserved for specific meanings (section 7.2).
4. **Nothing is lost.** Feature parity covers:
   - keyboard, gamepad and VR controller;
   - ESC rules, the typewriter, icons and translations (`lang/`);
   - `UiScale` and every existing hotkey and threat-label setting.
5. **No slower than today in play.** This is measured (section 10), not assumed.
6. **The test bridge can see the new UI**, through `[SELFTEST]` lines for the host, the views and the page version.

## 3. Non-goals

- No change to gameplay, progression, the save format or the skill-tree data.
- The data builders stay single-source in C++: tree, shop and status JSON (`BuildTreeJson`, `BuildShopJson`, `BuildStatusJson`).
- No fallback renderer. Neither Prisma nor ImGui stays as a backup.
- The MCM (MCM Helper) is not redesigned.
- No version bump. The user picks the release number.
- SotN Backgrounds is not ported in this project. It has its own short design afterwards.

## 4. Anti-goals

These would make the work a failure even if it technically runs:

- **Prettier but slower.** The move exists for performance.
- **A stuck character.** The UI must not fail to appear when the reincarnation fires, because then the blessing can never be picked. The existing VR guard in `BeginReincarnation` already handles this: without a UI the reincarnation is held and one message explains why. It becomes the rule on every runtime (section 6.5).
- **Effects that cost the player time.** Animation must never hide information or slow down repeated use:
  - The typewriter is always skippable.
  - The tree and the shop open instantly.
  - Opening flicker is short.
- **Losing VR.** It is allowed to look different in VR (section 8). It is not allowed to stop working.

## 5. Constraints

- **Magelight versions.** SotN has Magelight 1.8.0 installed and 1.9.0 is available.
  - Require host ≥ 1.7.0, the same floor as GrownUp's working wrapper.
  - Gate features by `QueryCapability`, not just the version: `gamepad` is 0 in VR and when the player switched the pad layer off; `prepaint` never paints in VR.
  - Pad without Magelight's pad layer (*my call*): VR uses the laser pointer; flat with the layer switched off by the player is mouse/keyboard only, logged once. The mod's own pad bridge (`Prisma::SendGamepad`) is not rebuilt.
- **One DLL for SE, AE and VR**, as today.
  - Address Library, no runtime-specific build.
  - SotN itself runs 1.6.1170.
- **API headers.** Magelight's C++ API is MIT and header-only (`MagelightUI_API.h`, `MagelightUI_Mod.h`). Vendor the headers as GrownUp does (`extern/magelight/`), with the licence file.
- **Page files.**
  - Pages ship under `Data\Magelight\IsekaiHero\`, following the GrownUp convention (`Data\Magelight\SotNGrownUp\`).
  - Pages may only read from the `Data\Magelight` tree, registered page folders and the host runtime dir, so the **icons move** to `Data\Magelight\IsekaiHero\icons\`. That touches packaging/FOMOD, the `interface icons` self-test and the icon paths in `Choice`/`Emblem` (today relative to the game folder, then relative to the page folder).
- **Game text** stays English and translatable through the `lang/` table. Chat and docs are German; everything persistent in the repo is English.
- **Privacy.** `package.ps1`'s privacy gate is never weakened. No user name in any shipped file.
- **Repo checks.** `tools/check.mjs` rules that name ImGui or Prisma files must be updated with the code, never deleted to make the check pass.
- **Nexus release.**
  - Magelight UI becomes a hard requirement.
  - Update the FOMOD (drop the Prisma patch option), the README, `docs/NEXUS_DESCRIPTION.md` and `docs/VR.md`.
  - Remove `package-prisma-patch.ps1`.
- **SotN install.** Installing into the SotN modlist needs a slot from the project lead session and the user's explicit yes for that step.

## 6. Architecture

### 6.1 Approaches considered

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| A | **Two views.** A *system* view (Panel layer, focus + pause) holds every interactive screen as a JS screen. A *hud* view (fullscreen, click-through) holds toasts, flourish and threat labels. | One texture for all menus, so switching screens is instant (status → tree → shop without closing). The HUD never steals input. Two pages to keep in one style; shared CSS solves that. | **Chosen.** The user handed over the how. |
| B | One view per window, about 7 views. | Clean isolation, but every view costs a texture, a DOM-ready and memory. Switching screens means switching views. | Rejected. More memory, no gain. |
| C | Menus in Magelight, threat labels stay native (ImGui or Scaleform). | Labels stay pinned to actors in VR. | Rejected. It contradicts "drop ImGui" and keeps the Present hook alive. |

### 6.2 What goes, what stays, what is new

**Removed:**
- `src/UI/Overlay.*` (Present hook), `VROverlay.*`, `Style.h`, `Textures.*`.
- The ImGui drawing in `SystemWindow`, `SkillTreeWindow`, `ShopWindow`, `Toast`, `LevelUpEffect` and `ThreatLabels`.
- `src/UI/Prisma.*`, `src/PrismaUI_API.h`, `prisma-patch/`, `package-prisma-patch.ps1`.
- `extern/imgui-vr-helper/`, the `imgui` vcpkg dependency, and `d3d11`/`dxgi` linking.
- MinHook stays because `CraftHooks.cpp` uses it.
- INI keys `VRInHeadsetLayer` and `VRThreatLabelHeight`. Reading an old INI that still has them must not fail.

**Moves:**
- `BuildTreeJson` and `BuildShopJson` live in the anonymous namespace of `src/UI/Prisma.cpp` (only `BuildStatusJson` is in `Progression.cpp`). They move to `src/UI/Payloads.*` before `Prisma.*` is deleted.
- The game clock: `Overlay.cpp`'s Present hook runs `PollGameClock()` → `Quests::Tick()` and `ModAPI::PublishIfChanged()`. Deleting the hook without a replacement would stop quests and the mod API (section 6.6).

**Stays, with logic unchanged:**
- The JSON builders (after the move).
- Threat-label *selection*: aggro / all filter, range, line of sight, aim radius, at most 12 labels, resources, numbers.
- Hotkey handling in `Input.cpp`: system menu key + modifier, gamepad Back+LB, `VRMenuButton`, self-test key, threat toggle F10.

**New:**
- `src/UI/Magelight.h/.cpp`, the host wrapper. It is modelled on GrownUp's `gu::magelight` (`H:\SotN-GrownUp\plugin\Magelight.*`):
  - At kDataLoaded: acquire the API, check the host version and `RegisterMod("isekai-hero")`.
  - Create both views hidden, then show the hud.
  - The hud view is click-through on the **Popup** layer, not `Layer::Hud` (*my call*). `Hud` is the lowest tier, under `Panel`, and toasts and the flourish must stay visible over our own open screens, as they are today (a purchase toast in the shop). If the host refuses a click-through Popup view, the system page mirrors toasts while it is open instead.
  - Register the listeners `isekai.action` (one channel, `{type, …}` payload) and `isekai.ready`.
  - Expose `RequestOpen` / `Close` / `Run` / `Eval`.
  - Build JS only from `nlohmann::json::dump()` output.
- `src/UI/Screens.*`, a facade that keeps today's call-site API (section 6.3). About 30 call sites in game logic stay untouched apart from include and type cleanups.
- `src/UI/Hud.*`, the producer for toasts, the flourish and threat-label frames. It runs the projection and the change detection for labels (section 6.4).
- `Data/Magelight/IsekaiHero/`: `system.html`, `hud.html`, a shared `theme.css`, shared JS helpers, and a `manifest.json` if Magelight wants one.

### 6.3 Call-site API (kept stable)

| Today | After |
|---|---|
| `ShowSystemWindow(title, body, choices, onSelect, reveal, width, emblem)` | same signature. `Emblem::captionColor` (ImVec4) becomes a **tone** (`cyan/gold/red/violet`). `width` becomes a size hint. |
| `ShowSkillTree()`, `ShowShopWindow()`, `Prisma::ShowStatus(json)`, `Prisma::OpenTree/OpenShop` | `ShowSkillTree()`, `ShowShop()`, `ShowStatus(json)`. The `Prisma::Active()` branches in `Progression.cpp` collapse to one path. |
| `PlayLevelUpEffect(title, subtitle)` | same |
| `ShowToast(text, key)`, `ShowToastBanner(text, key)` | same. Safe from any thread, as today. |
| `IsSystemWindowOpen()`, `IsSystemScreenOpen()`, `DismissSystemWindow()` | same meaning, now backed by the system view's state. `IsSystemScreenOpen` covers every screen. |
| `BuiltInUiCanDisplay()` | replaced by `UI::Available()` (Magelight host present and views created), on every runtime |

`onSelect` keeps its contract: invoked on the main thread with the 0-based index. Actions from the page arrive on Magelight's callback thread and are posted to the game thread before any game state is touched.

### 6.4 Data flow

- **Opening a screen.**
  1. C++ builds the JSON.
  2. `Run("isekai.show(" + json + ")")`, with screen name and payload.
  3. `RequestOpen()`, which is UI mode with pause.
  4. The page switches screen and plays the open animation.
- **Switching screens.** Status → tree → shop happens inside the page through an action (`{type:"nav", to:"tree"}`). C++ answers with the new payload and never closes the view in between.
- **Actions.**
  - Page to C++: `magelight.send("isekai.action", {...})`.
  - Types: `choose`, `buy`, `respec`, `shopBuy`, `status` (tree/storage/reboot/close), `close`, `nav`.
  - After `buy` / `shopBuy`, C++ re-pushes the screen so balances and affordability stay current, as Prisma does today.
- **Ready handshake.**
  - The page sends `isekai.ready` with its **page version**.
  - C++ holds every call until ready. The Prisma workaround of remembering a single pending call goes away: queue them all.
  - C++ compares the page version with the DLL's UI build id and logs a mismatch. The self-test reports it.
  - No build id exists today. CMake generates one (git short hash + dirty flag) into a header **and** into `version.js` next to the pages, so DLL and page are stamped by the same build; `package.ps1` ships both from that build.
- **Localisation and scale.** On ready, C++ pushes the `lang` table (`isekai.loc`) and `UiScale` (`isekai.scale`).
- **Toasts.**
  - Posting enqueues on the C++ side, any thread, and is drained to the page on the game thread.
  - Key-replace and banner duration live **in the page**, which owns timing. Ageing now runs on real time, so the VR 1/60 delta-time problem disappears.
- **Threat labels.**
  - On the frame tick (section 6.6) C++ selects and projects with `NiCamera::WorldPtToScreenPt3` to normalised screen coordinates.
  - It pushes **positions every frame while labels are on screen and moving**, and the set/content only when it changes (*my call*). ImGui draws them lag-free today; a 30 Hz push would make them visibly trail on fast camera turns, and 12 positions per frame is a tiny call. `InvokeJS` lands one frame later; that one frame is accepted.
  - The page moves labels with CSS transforms only, no layout. Nothing moving means no push.
  - If the FPS measurement (section 10) shows a cost, fall back to a throttled push with page-side extrapolation.
  - An empty set is pushed once, then nothing.
  - Labels off (INI or F10) means no work at all.

### 6.5 Without Magelight

There is no fallback renderer. If Magelight is missing, too old, or the overlay reports RenderDead:

- Reincarnation is **held**. The flag is not set and one native notification appears: "Isekai Hero needs Magelight UI".
- The hotkeys answer with the same notification instead of doing nothing.
- The self-test line `magelight host` is FAIL with the reason.

This is the existing VR rule from `System.cpp` (`IsVR && !Prisma::Active()`), generalised to every runtime.

**The window can also be refused later.** `BeginReincarnation` sets `reincarnated` and shows the blessing choice
4.5 s later; by then another Magelight mod may hold UI mode (`Busy`), the host may refuse (`UIModeRefused`, e.g. low
memory) or a load may end UI mode. Rules (*my call*):
- Every open of the system view uses `kUIModeFlagQueue`: wait for the holder instead of failing.
- A pending decision (blessing, rank choice — any panel with several choices) is remembered in memory until a
  choice arrives. If UI mode ends without one (refused, view lost), the system-menu hotkey offers it again, and the
  page re-opens it by itself once UI mode is free. The decision is never silently dropped within a session.
- The existing `reincarnated` flag stays as it is (fires once); the pending decision is its own in-memory state.
  A load clears it, because the loaded save has its own state. A save made in the 4.5 s gap between the flag and
  the choice is an **existing** gap of today's code too (`power` defaults to Normal, so "never chosen" is not
  derivable); closing it needs a cosave field, which is a save-format change and therefore out of scope — noted as
  a follow-up.

### 6.6 Frame tick

The Present hook was also the mod's clock. Its replacement is a main-thread per-frame hook on
`PlayerCharacter::Update` via its vtable (*my call*: CommonLibSSE-NG resolves the vtable per runtime incl. VR, so no
new Address Library offsets; main thread means RE:: reads are legal, unlike the render thread today). It runs
`Quests::Tick`, `ModAPI::PublishIfChanged` and the threat-label pass. It only runs in the world; anything that
polled during menus before must be checked by the builder against `PollGameClock`'s current behaviour.

## 7. Look

### 7.1 Projection language (from the approved mockup)

- **Window.**
  - Translucent cyan fill `rgba(60,200,255,.09)`, border `rgba(110,230,255,.8)`, outer glow `0 0 18px rgba(80,220,255,.35)`.
  - 4 px scanlines `repeating-linear-gradient(0deg, rgba(110,230,255,.07) 0 1px, transparent 1px 4px)`.
  - Slight tilt away from the viewer: `perspective(800px) rotateX(8deg)`, origin bottom.
  - Corner brackets top-left and bottom-right in `#6ee6ff`.
- **Light cone** under each window. Gradient `rgba(90,220,255,.26) → transparent`, clipped to a cone from a point below the window. Subtle, so the window reads as *projected* from below.
- **Titles.** `// TITLE`, letter-spacing 5px, `#6ee6ff`, chromatic fringe `text-shadow: -1px 0 rgba(255,60,140,.55), 1px 0 rgba(60,255,240,.55)`.
- **Numbers and codes** in a monospace face. **Body text** in a sans face.
- **Typewriter** with a block caret for story/system text. Click, Enter or pad A completes it at once.
- **Choices** are cards (tier label, big name, one-line description). The selected card glows. Buttons are thin outlined capsules with spaced capitals.
- **Open/close.** A short projector "flicker-on" (scale-y in plus brightness flicker, ≤ 200 ms) and a faster fade out.
- **Skill tree.** A projected node net (SVG): owned nodes filled, available nodes outlined and glowing, locked nodes dashed, the SP counter in the title bar.

### 7.2 Tones

| Tone | Colour (starting point) | Used for |
|---|---|---|
| cyan (default) | `#6ee6ff` | everything not listed below |
| gold | `#ffd27a` | blessings, rank-up, S rank, Ascended, level-up flourish, milestone payouts, rare/top-tier shop items |
| red | builder's call within the language (e.g. `#ff6b7a`) | warnings, Shattered, penalties, dangerous threat tiers, errors, FAIL in the self-test |
| violet | builder's call (e.g. `#c49bff`) | Dormant, Custom blessing |

A tone recolours the whole window: border, glow, scanlines, cone, title and caret, as in the gold mockup. Within a cyan window it can also mark a single element, such as one gold card or one red label. The tone of a panel is passed from C++ (`tone` field), not guessed by the page.

### 7.3 Per screen

- **System panel.**
  - Title, typewriter body and choices (cards for 2–4 decisions, buttons otherwise).
  - Icon-only actions in the top right, and an optional emblem (crest + caption) on the left.
  - ESC closes only panels with a single CLOSE/CONTINUE button. Decision panels ignore ESC, as today.
- **Status.**
  - Header: name, rank crest + rank letter in its tone (S is gold), level and blessing in its tone.
  - Below: tiles for milestones and points, the attunement grid and the titles ledger.
  - Actions: tree / storage / reboot / close.
- **Skill tree.** The node net with hover/focus details (cost, prerequisites, tier gate), buy and respec. Pad navigation moves between nodes.
- **Shop.**
  - Item cards with icon, price and affordability; rare items are gold.
  - Buying re-pushes the screen.
- **Level-up flourish.** Gold, centred, non-interactive, no pause, fades on its own.
- **Toasts.** Top centre, a small projected strip each. Banners last longer and may carry a tone.
- **Threat labels.**
  - A compact projected tag above the head: level, name and threat-tier tag in its tone, plus resources when enabled.
  - It shrinks with distance (the existing `1 − 0.32·t` curve).
  - Dangerous tiers are red. Content is unchanged from today.

## 8. VR

- **System view.**
  - Magelight's default panel placement (LazyFollow), with its laser pointer and VR keyboard.
  - Opened by the existing `VRMenuButton` path, or by Magelight `BindVRHotkey`.
  - The old Y+B chord came from ImGuiVRHelper's controller map and goes with it. Its *intent* stays: two buttons, so it cannot fire by accident (see section 12).
- **Hud view.** HeadLocked. Toasts and the flourish sit in the upper field of view.
- **Threat labels in VR become a scan list.** Magelight can place a view head-locked, world-locked once, or lazily following. It cannot pin one element over each moving actor.
  - VR therefore shows a head-locked list of the nearest labelled actors, sorted by danger, with the same content and tones.
  - Flat screens keep labels above heads.
- `docs/VR.md` is rewritten for the new state. VR stays "experimental / untested" until someone with a headset confirms it.

## 9. Performance

- **Hud view.**
  - Stays alive (no hibernate), because toasts must appear instantly.
  - When nothing is on it, the page renders an empty transparent layer and C++ pushes nothing.
  - Labels are diffed as described in section 6.4: positions per frame only while moving, content only on change.
  - The page animates only `transform`/`opacity`, never layout properties.
- **System view.** Prepaint on, if the capability exists, so the first open is not a long frame. Hibernate after a long idle time is optional (section 12).
- **No idle JS calls.** The label position push is the only per-frame call, and only while labels are visible and moving. Everything else is event-driven.

## 10. Testing and acceptance

- **Unit tests** (engine-free, in `tests/`, run by `tools/run-unit-tests.ps1`) cover the new pure logic:
  - label diff (when to push, what changed);
  - tone mapping (blessing/rank/tier → tone);
  - action parsing (page JSON → typed action, unknown types rejected);
  - the ready-gate queue.
- **Playground.** `playground/` is updated to load the new pages with mock data for **every** screen, both views and all tones. This is where the look is iterated without starting Skyrim. The user reviews the look there before the in-game run.
- **Self-test** (`[SELFTEST]` lines, ending in ENDE):
  - `magelight host` (found, version);
  - `system view` / `hud view` (created);
  - `page ready` (ready received, page version = DLL build);
  - `interface icons` (kept).
  - The Prisma/overlay lines in `CheckEnvironment` go.
- **In-game run** through the test bridge, with test points handed to the project lead:
  - every screen opens and closes;
  - pad navigation works and the ESC rules hold;
  - status → tree → shop works without closing;
  - toasts replace by key;
  - labels follow actors while the camera turns.
- **FPS acceptance.** `measure_fps` on the same scene (labels on, several enemies) before (current build) and after. The result must not be worse.
- `tools/check.mjs` passes with updated rules.

## 11. Delivery

- The work lands on a feature branch in the Isekai repo. Commits are in English with the repo's identity.
- Release notes go in `CHANGELOG.md`, the FOMOD and the Nexus texts. The user picks the version number.
- SotN integration:
  - The package is built with `package.ps1`.
  - It is installed into `mods\Isekai Hero` only after the project lead assigns a slot and the user says yes.
  - The old Prisma patch folder in the modlist, if any, is removed after a deletion list the user approves.
  - The Magelight 1.9.0 update is a separate, optional step under the download rules.

## 12. Left to the builder

These are minor calls that do not change what gets built:

- File split of the pages (one HTML per view + shared CSS/JS is the starting point).
- Exact red and violet values, animation timings and sizes within the projection language.
- Fonts: system fonts only (Segoe UI / Bahnschrift for text, Consolas for numbers), with a glyph fallback chain. The INI already notes Consolas lacks some glyphs. No bundled font files unless their licence allows redistribution.
- The VR open binding (`BindVRHotkey` with a modifier vs. the existing `VRMenuButton` sink), as long as it needs two buttons or one deliberate button.
- Whether the system view hibernates after a long idle time.
- Gamepad style: navigation style is the starting point (`kGamepadStyleNavigation`, focusable elements), cursor mode as a fallback for the tree.
- Whether `MenuGuard` in `Input.cpp` is still needed once Magelight's UI mode owns input. Remove it only if the run shows the game no longer receives input behind an open screen.

## 13. Phase 2 — SotN Backgrounds (outline only)

`H:\SotN-Backgrounds` has an 86-line Prisma page (`view/index.html`), a view wrapper (`plugin/View.*`) with a UIExtensions list-menu fallback, and a Prisma focus check in `plugin/Events.*`.

After Isekai ships, it moves to Magelight with:
- the same `theme.css` (copied, not linked across mods);
- the same projection look.

It gets its own short design: whether the list-menu fallback stays, and which tone the background choice uses.
