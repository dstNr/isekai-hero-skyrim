# Magelight UI Rewrite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the ImGui and PrismaUI renderers of Isekai Hero with one Magelight UI renderer: two views, every screen redesigned in the projection style.

**Architecture:** There are two Magelight views:
- a focused *system* view (Panel layer) that holds every interactive screen;
- a click-through *hud* view (Popup layer) for toasts, the level-up flourish and threat labels.

Game logic keeps its call sites through a facade (`UI/Screens.h`, `UI/Hud.h`). Data payloads are built once in C++ and pushed as JSON. A main-thread `PlayerCharacter::Update` hook replaces the Present hook as the mod's frame clock.

**Tech Stack:** C++23, CommonLibSSE-NG, Magelight UI C++ API v4 (vendored headers, MIT), nlohmann-json, HTML/CSS/vanilla JS (Ultralight), Node for `tools/check.mjs` and the playground, PowerShell 7 for build and packaging.

**Spec:** `docs/superpowers/specs/2026-11-15-magelight-ui-design.md`. Read it before any task; section numbers below refer to it.

## Global Constraints

- Magelight host floor `1.7.0` (`ml::PackVersion(1, 7, 0)`). Newer features are gated by `QueryCapability`: `gamepad`, `prepaint`, `escapeCapture`, `vrGaze`.
- One DLL for SE/AE/VR. No new Address Library IDs: hooks go through CommonLibSSE-NG vtables.
- Pages ship under `Data\Magelight\IsekaiHero\`, and icons under `Data\Magelight\IsekaiHero\icons\`.
- Game text is English and goes through `L()`/`LF()` and `Loc::Json()`. Repo text is English.
- Tones: cyan `#6ee6ff` (default), gold `#ffd27a`. Red and violet are the builder's call within the projection language (spec 7.2).
- Projection tokens are copied verbatim from spec 7.1: fill `rgba(60,200,255,.09)`, border `rgba(110,230,255,.8)`, glow `0 0 18px rgba(80,220,255,.35)`, scanlines `repeating-linear-gradient(0deg, rgba(110,230,255,.07) 0 1px, transparent 1px 4px)`, `perspective(800px) rotateX(8deg)` with origin bottom, title fringe `-1px 0 rgba(255,60,140,.55), 1px 0 rgba(60,255,240,.55)`.
- Timings carried over from the ImGui code: toast line `3.2 s`, banner `5.5 s`, fade `0.8 s`, rise `0.22 s`; flourish `3.2 s` with a `0.7 s` fade; open flicker `≤ 200 ms`.
- Threat labels: at most `12`, distance scale `1 − 0.32·t`. Verdicts are LETHAL (diff ≥ 20), DANGEROUS (≥ 5), TRIVIAL (≤ −15), else MANAGEABLE.
- Never bump the version. Never weaken the `package.ps1` privacy gate. No user name in any file.
- `tools/check.mjs` rules are updated with the code, never deleted to go green.
- Commits are English, on branch `feat/magelight-ui`, cut from the current `feat/rebirth-modevent` HEAD. Commit only the files of the task.
- Builds: `cmake --build build` from the build.bat environment (vcvars64). Never deploy into a game folder from a task.
- Unit tests: `pwsh -NoProfile -File tools/run-unit-tests.ps1`.
- Repo checks: `node tools/check.mjs`.

## Review Focus

1. **The blessing choice fires while another Magelight mod (GrownUp) holds UI mode, or the host refuses.** Expected: the choice waits its turn and appears; if it is lost it is offered again; never silently dropped. Pinned by `PendingDecision` tests (Task 1) and its use in Task 9.
2. **The page sends garbage, an unknown action, an out-of-range index or an oversized message.** Expected: it is ignored with one log line, with no crash and no game action. Pinned by `ParseAction` tests (Task 1) and the 16 KB drop in Task 5.
3. **Screens are opened in the first seconds after load, before the page is DOM-ready.** Expected: every call is delivered, in order, once ready; nothing is lost (Prisma kept only one). Pinned by `ReadyGate` tests (Task 1).
4. **Labels are off, nothing moves, or VR has no screen projection.** Expected: no per-frame JS calls; one empty push when the last label goes. Pinned by `LabelPushGate` tests (Task 1).
5. **A leftover old install.** Either an old `IsekaiHero.ini` still has `VRInHeadsetLayer` / `VRThreatLabelHeight`, or a stale page is on disk from an older build. Expected: the INI loads silently; a page/DLL mismatch shows as a self-test FAIL with both ids. Pinned by `PageMatchesBuild` tests (Task 1) and the self-test check in Task 12.

---

### Task 1: Engine-free UI logic

**Files:**
- Create: `src/UI/UiLogic.h`
- Create: `tests/ui_logic_tests.cpp`
- Modify: `tests/CMakeLists.txt` (add `ui_logic` to the `foreach` list)
- Modify: `CMakeLists.txt` (list `src/UI/UiLogic.h`)

**Interfaces:**
- Produces, all in `namespace Isekai::UI::Logic`, header-only, standard library only:
  - `enum class Tone : std::uint8_t { Cyan, Gold, Red, Violet };` and `constexpr std::string_view ToneName(Tone)` returning `"cyan"|"gold"|"red"|"violet"`.
  - `enum class Blessing : std::uint8_t { Normal, Hero, Ascended, Full, Shattered, Dormant, Custom };` with `constexpr Tone ToneFor(Blessing)`. Ascended→Gold, Shattered→Red, Dormant/Custom→Violet, rest→Cyan.
  - `constexpr Tone ToneForRank(char rank)`: `'S'`→Gold, else Cyan.
  - `enum class Verdict : std::uint8_t { Trivial, Manageable, Dangerous, Lethal };` with `constexpr Verdict VerdictFor(std::int32_t levelDiff)` (thresholds in Global Constraints) and `constexpr Tone ToneFor(Verdict)`: Dangerous/Lethal→Red, rest→Cyan.
  - `struct Action { enum class Type : std::uint8_t { Choose, Buy, Respec, ShopBuy, Status, Close, Nav }; Type type; std::uint32_t number = 0; std::string word; };` and `std::optional<Action> ParseAction(std::string_view)`.
    - Wire format `type` or `type:arg`. `choose:<int ≥ 0>`, `shopBuy:<int ≥ 0>`, `buy:<uint32 > 0>`.
    - `status:<tree|storage|shop|reroll|reboot|close>` and `nav:<status|tree|shop>` put the arg in `word`. `close` and `respec` take no arg.
    - Anything else, or input longer than 256 chars, gives `nullopt`.
  - `class ReadyGate { public: std::optional<std::string> Submit(std::string call); std::vector<std::string> MarkReady(); void Reset(); bool Ready() const; static constexpr std::size_t kMaxPending = 64; };`
    - Before ready, `Submit` queues and returns `nullopt`. When full, the oldest is dropped.
    - After ready, `Submit` returns the call to run now.
    - `MarkReady` returns the queue in order and clears it.
    - `Reset` is for a page reload and clears both the queue and the ready flag.
  - `class PendingDecision { public: void Offer(std::uint32_t id); void Resolve(std::uint32_t id); void OnUiModeLost(); bool NeedsReoffer() const; std::optional<std::uint32_t> Current() const; void Reset(); };`
    - `Offer` replaces any current decision.
    - `Resolve` with the wrong id is a no-op.
    - `OnUiModeLost` sets `NeedsReoffer` only if a decision is pending.
    - `Offer` of the same id clears `NeedsReoffer`.
  - `struct LabelFrame { std::uint32_t id; float x, y, scale; std::uint64_t content; };` (x/y normalised 0..1) plus `enum class LabelPush : std::uint8_t { None, Positions, Full };` and `class LabelPushGate { public: LabelPush Update(std::span<const LabelFrame> frames, float screenW, float screenH, float minMovePx = 0.5f); void Reset(); };`
  - `constexpr bool PageMatchesBuild(std::string_view page, std::string_view build)`: both non-empty and equal.

- [ ] **Step 1: Write the failing tests** in `tests/ui_logic_tests.cpp`, using the same `Expect` harness as `tests/readiness_tests.cpp`. At minimum:
  - `ToneFor(Blessing::Ascended)==Tone::Gold`, `ToneFor(Blessing::Shattered)==Tone::Red`, `ToneFor(Blessing::Dormant)==Tone::Violet`, `ToneFor(Blessing::Hero)==Tone::Cyan`, `ToneForRank('S')==Tone::Gold`, `ToneForRank('A')==Tone::Cyan`.
  - `VerdictFor(20)==Lethal`, `VerdictFor(19)==Dangerous`, `VerdictFor(5)==Dangerous`, `VerdictFor(4)==Manageable`, `VerdictFor(-15)==Trivial`, `VerdictFor(-14)==Manageable`, and `ToneFor(Verdict::Dangerous)==Tone::Red`.
  - `ParseAction("choose:2")` → Choose/2; `"buy:123456"` → Buy/123456; `"status:storage"` → Status/"storage"; `"nav:tree"` → Nav/"tree"; `"close"` → Close; `"respec"` → Respec.
  - `nullopt` for: `""`, `"choose:-1"`, `"choose:x"`, `"buy:0"`, `"status:format"`, `"nav:inventory"`, `"close:1"`, `"launch"`, and a 257-char string.
  - `ReadyGate`:
    - three submits before ready give `nullopt` each; `MarkReady` returns all three in order;
    - the next `Submit` returns its call;
    - 65 submits keep the newest 64, so the first entry is call #2;
    - `Reset` makes `Ready()` false.
  - `PendingDecision`:
    - `Offer(7); OnUiModeLost()` → `NeedsReoffer()` and `Current()==7`;
    - `Resolve(8)` leaves it; `Resolve(7)` clears everything;
    - `OnUiModeLost()` with nothing pending → `!NeedsReoffer()`.
  - `LabelPushGate`:
    - empty→empty gives None; first non-empty gives Full; same frames gives None;
    - one label moved 0.4 px at 1920×1080 gives None; moved 0.6 px gives Positions;
    - changed `content` gives Full; a different id set gives Full;
    - non-empty→empty gives Full once, then None.
  - `PageMatchesBuild("a1b2c3","a1b2c3")`; `!PageMatchesBuild("","")`; `!PageMatchesBuild("dev","a1b2c3")`.
- [ ] **Step 2: Run** `pwsh -NoProfile -File tools/run-unit-tests.ps1`. Expected: build FAIL, `UiLogic.h` not found.
- [ ] **Step 3: Implement `src/UI/UiLogic.h`** with the interfaces above. `ParseAction` uses `std::from_chars`, never exceptions.
- [ ] **Step 4: Run the unit tests.** Expected: `ui_logic`, `readiness` and `selftest_format` all PASS.
- [ ] **Step 5: Run `node tools/check.mjs`.** Expected: the CMakeLists rule passes.
- [ ] **Step 6: Commit** `feat(ui): engine-free UI logic for the Magelight renderer` with `src/UI/UiLogic.h tests/ui_logic_tests.cpp tests/CMakeLists.txt CMakeLists.txt`.

### Task 2: Branch, vendored Magelight headers, build id

**Files:**
- Create: `extern/magelight/MagelightUI_API.h`, `extern/magelight/MagelightUI_Mod.h`, `extern/magelight/LICENSE-MagelightUI.txt` (copied unchanged from `H:\SotN-GrownUp\extern\magelight\`)
- Create: `cmake/BuildId.cmake`
- Create: `magelight/IsekaiHero/` (empty except `.gitkeep`). This is the page source folder for Tasks 6–8.
- Modify: `CMakeLists.txt`, `.gitignore` (if needed)

**Interfaces:**
- Produces `build/generated/IsekaiBuildId.h` with `namespace Isekai { inline constexpr const char* kUiBuildId = "<git short hash>[-dirty]"; }`. `extern/magelight` and `build/generated` are on the include path.
- Produces `build/generated/version.js` with exactly `window.ISEKAI_UI_BUILD = "<same id>";`. Tasks 6 and 13 consume it.

- [ ] **Step 1:** `git switch -c feat/magelight-ui`.
- [ ] **Step 2:** Copy the three files. Add `extern/magelight` to `target_include_directories`.
- [ ] **Step 3:** Write `cmake/BuildId.cmake`. It runs `git rev-parse --short HEAD` and `git status --porcelain` at **build** time (a custom target the plugin depends on, so a new commit re-stamps) and writes both files with `file(WRITE …)` only when the content changed. With no git, the id is `nogit`.
- [ ] **Step 4:** `cmake --build build`. Expected: success, and both generated files exist with identical ids (`Select-String` both).
- [ ] **Step 5:** `node tools/check.mjs`. Expected: all pass.
- [ ] **Step 6: Commit** `build: vendor Magelight UI API headers and stamp a UI build id`.

### Task 3: Move the payload builders out of Prisma.cpp

**Files:**
- Create: `src/UI/Payloads.h`, `src/UI/Payloads.cpp`
- Modify: `src/UI/Prisma.cpp` (delete the moved code; call the new functions), `CMakeLists.txt`

**Interfaces:**
- Produces, in `namespace Isekai::UI::Payloads`, all main-thread:
  - `std::string Tree()`: today's `BuildTreeJson` body, unchanged.
  - `std::string Shop()`: today's `BuildShopJson`, unchanged.
  - `struct PanelSpec { std::string title, body; std::vector<Choice> choices; float revealCharsPerSec; float widthHint; Logic::Tone tone; std::string emblemIcon, emblemCaption; Logic::Tone emblemTone; };` and `std::string Panel(const PanelSpec&)`.
    - It produces today's `BuildPanelJson` fields plus `"tone"`, and `"emblem":{"icon","caption","tone"}` when an icon or caption is set.
    - Icons are emitted as `icons/<file>` (today's `IconFile` rule).
- `Choice` still comes from `UI/SystemWindow.h` in this task. Task 9 moves it to `UI/Screens.h`.

- [ ] **Step 1:** Move the code; keep the Prisma path working by calling `Payloads::Tree()` / `Shop()` / `Panel({...tone=Cyan})`.
- [ ] **Step 2:** `cmake --build build`. Expected: success.
- [ ] **Step 3:** `node tools/check.mjs`. Expected: all pass. The skill-tree icon rule parses whichever file holds the table; update its path if it names `Prisma.cpp`.
- [ ] **Step 4: Commit** `refactor(ui): payload builders out of the Prisma bridge`.

### Task 4: Main-thread frame tick

**Files:**
- Create: `src/FrameTick.h`, `src/FrameTick.cpp`, `src/TickThrottle.h`, `tests/tick_throttle_tests.cpp`
- Modify: `src/UI/Overlay.cpp` (remove `PollGameClock` and both calls), `src/System.cpp` (install at kDataLoaded), `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- `src/TickThrottle.h`, engine-free: `class TickThrottle { public: explicit TickThrottle(std::chrono::milliseconds period); bool Due(std::chrono::steady_clock::time_point now); };`. True on the first call, then at most once per period.
- `namespace Isekai::FrameTick { void Install(); using Listener = void (*)(); void Add(Listener); }`
  - The hook is on `RE::VTABLE_PlayerCharacter[0]`, the `Update(float)` vfunc. Confirm the index with the `housecarl:skse-plugin-authoring` skill before writing; `0xAD` is the expected SE/AE/VR index.
  - It calls the original first, then every listener, in the order added.
  - Built-in listener: `TickThrottle(1s)`, which then runs `Quests::Tick(); ModAPI::PublishIfChanged();` directly (already on the main thread, no AddTask).
  - `Add` is called only at kDataLoaded.

- [ ] **Step 1: Failing test** `tests/tick_throttle_tests.cpp`:
  - `Due(t0)` true; `Due(t0+999ms)` false; `Due(t0+1000ms)` true; `Due(t0+1500ms)` false.
  - A clock that goes backwards (`Due(t0-5s)` after `t0`) returns false and does not wedge: `Due(t0+1000ms)` is still true.
- [ ] **Step 2:** Run the unit tests. Expected: FAIL (missing header).
- [ ] **Step 3:** Implement `TickThrottle.h` and `FrameTick.*`. Install it in `System.cpp` kDataLoaded right after `ModAPI::Install()`. Delete `PollGameClock` from `Overlay.cpp`.
- [ ] **Step 4:** Unit tests PASS. `cmake --build build` succeeds.
- [ ] **Step 5:** Add a check.mjs rule `"tick: quests and the mod API run from FrameTick, not the Present hook"`: `FrameTick.cpp` names `Quests::Tick` and `ModAPI::PublishIfChanged`; `Overlay.cpp` names neither. Run `node tools/check.mjs` and expect all pass.
- [ ] **Step 6: Commit** `feat(core): main-thread frame tick replaces the Present-hook clock`.

### Task 5: Magelight host wrapper

**Files:**
- Create: `src/UI/Magelight.h`, `src/UI/Magelight.cpp`, stub `src/UI/Screens.h` + `src/UI/Screens.cpp` (the `detail` callbacks only)
- Modify: `src/System.cpp` (call `UI::Magelight::Install()` at kDataLoaded after `Loc::Load()`), `CMakeLists.txt`

**Interfaces:**
- `namespace Isekai::UI::Magelight`:
  - `enum class View : std::uint8_t { System, Hud };`
  - `bool Install();`
    - Requires host ≥ 1.7.0, then `RegisterMod("isekai-hero", "Isekai Hero")` with `CallbackThread::GameThread`.
    - **system** view: `system.html`, Panel, fullscreen, hidden, not click-through. Prepaint if the `prepaint` capability is 1. `SetViewGamepad` in navigation style if `gamepad` is 1, otherwise log once that the pad layer is off.
    - **hud** view: `hud.html`, Popup, fullscreen, click-through, visible. If CreateViewEx refuses that, retry with `Layer::Hud` and set `HudBelowPanels()`. In VR, `SetViewVRPlacement` HeadLocked.
    - Listeners `"isekai.action"` and `"isekai.ready"` on the system view, `"isekai.ready"` on the hud.
    - Page paths come from the exe directory as in GrownUp's `PagePath()`: `Data\Magelight\IsekaiHero\<file>`.
  - `bool Available();` Install succeeded and no RenderDead.
  - `std::string_view HostVersion();`, `bool ViewCreated(View);`, `bool PageReady(View);`, `std::string PageVersion(View);`, `bool HudBelowPanels();`
  - `void Run(View, std::string js);` Any thread. Goes through that view's `Logic::ReadyGate`, mutex-guarded. On ready, the queue drains in order. The js is built only from `nlohmann::json::dump()` output.
  - `bool RequestOpen();` `RequestUIMode(system, kUIModeFlagPause | kUIModeFlagQueue)`.
  - `void Close();` As GrownUp's: CancelUIModeRequest if supported, else ReleaseUIMode.
  - `bool HoldsUiMode();`
- Callbacks it calls, declared in `src/UI/Screens.h` under `namespace Isekai::UI::Screens::detail`, game thread:
  - `void OnAction(std::string_view raw);`
  - `void OnEntered();`
  - `void OnExited(bool reopened);`
  - `void OnRefused(std::string detail);`
  - `void OnPageReady(Magelight::View, std::string version);`
  - Until Task 9 exists, stub them in `Screens.cpp` with log lines only.
- Rules carried from GrownUp `plugin/Magelight.cpp`:
  - Page messages over 16 KB are dropped with a warning.
  - Every page message goes through `PostGameTask`.
  - Exited hides the system view unless `GetUIModeOwner()==our mod`.
  - `ViewReloaded` calls `ReadyGate::Reset()` for that view.
  - Every API call logs `ResultName` + `GetLastErrorMessage` on failure.
  - Log text is English and carries no paths with user names (log the page path relative to `Data`).

- [ ] **Step 1:** Implement the wrapper and the stub `Screens.cpp` (stubs only).
- [ ] **Step 2:** `cmake --build build`. Expected: success.
- [ ] **Step 3:** Add a check.mjs rule `"magelight: Install logs before RequestApi4 and each CreateViewEx, and page paths are under Data\\Magelight\\IsekaiHero"`, in the style of the existing `"vr startup"` rule. Run `node tools/check.mjs` and expect all pass.
- [ ] **Step 4: Commit** `feat(ui): Magelight host wrapper with system and hud views`.

### Task 6: System page shell, theme and the panel screen, with playground

**Files:**
- Create: `magelight/IsekaiHero/theme.css`, `magelight/IsekaiHero/common.js`, `magelight/IsekaiHero/system.html`
- Copy: `icons/*.png` → served from `magelight/IsekaiHero/icons/` in the playground via `serve.mjs` path mapping. The repo keeps one `icons/` folder; packaging copies it in Task 13.
- Modify: `playground/index.html`, `playground/mock.js`, `playground/serve.mjs`, `playground/README.md`
- Modify: `tools/check.mjs` (point the rules at lines 583–913 that read `prisma-patch/.../index.html` at `magelight/IsekaiHero/system.html`, keeping what each asserts)

**Interfaces:**
- `theme.css`:
  - All spec 7.1 tokens as custom properties on `:root`.
  - `[data-tone="gold|red|violet"]` overrides recolour border, glow, scanlines, title and caret.
  - Classes: `.proj` (window), `.proj-title`, `.caret`, `.card`, `.card.on`, `.btn`, `.mono`.
  - Animations use `transform`/`opacity` only.
- `common.js` (`window.isekaiUi`):
  - `send(type, arg?)` → `magelight.send('isekai.action', arg === undefined ? type : type + ':' + arg)`.
  - `ready()` → `magelight.send('isekai.ready', window.ISEKAI_UI_BUILD || 'dev')`.
  - `escapeCapture(on)` → `magelight.send('__escapecapture', on ? '1' : '0')`.
  - `t(key, english)` translation from the pushed table.
  - `typewriter(el, text, charsPerSec)` returns `{ finish() }`.
  - `setScale(s)`.
- `system.html` globals, called by C++:
  - `isekai.loc(table)`, `isekai.scale(s)`.
  - `isekai.show(screen, payload)` with `screen ∈ "panel"|"status"|"tree"|"shop"`. The payload shapes are today's Prisma shapes plus `tone`/`emblem` (Task 3). It sets `data-tone` on the window, plays the flicker-on and switches screens without reopening.
  - It loads `version.js` with a failure-tolerant `<script>`.
- Panel screen:
  - Title, typewriter body, choices.
  - Cards for 2–4 choices with icon/label, buttons otherwise.
  - Icon-only actions top-right, emblem on the left.
  - Click, Enter or pad A finishes the typewriter first, then acts.
  - Single-button panels: Escape closes (`send('choose', 0)`).
  - Multi-button panels call `escapeCapture(true)` and ignore Escape.
  - Pad focus ring on every card and button.

- [ ] **Step 1:** Write the check.mjs path updates first. Run `node tools/check.mjs` and expect the moved UI rules to FAIL (file missing).
- [ ] **Step 2:** Implement `theme.css`, `common.js`, `system.html` with the panel screen.
- [ ] **Step 3:** Update the playground:
  - `mock.js` provides a fake `window.magelight.send` that logs and routes actions.
  - It shows a screen picker: panel (story, blessing choice with 3 cards, gold tone, red warning, violet Dormant), status, tree, shop and hud.
- [ ] **Step 4:** `node tools/check.mjs`. Expected: all pass. Open `node playground/serve.mjs` in the browser pane and screenshot the panel in cyan, gold and red. Expected: it matches the spec 7.1 look.
- [ ] **Step 5: Commit** `feat(ui): projection theme and the System panel for Magelight`.

### Task 7: Status, skill tree and shop screens

**Files:**
- Modify: `magelight/IsekaiHero/system.html` (or split into `screens/*.js` if it passes ~1500 lines), `magelight/IsekaiHero/theme.css`, `playground/mock.js`, `tools/check.mjs`

**Interfaces:**
- **Status** (`isekai.show("status", json)`, `BuildStatusJson` shape):
  - The header shows the rank crest + letter (tone from `ToneForRank`, S gold), the level and the blessing name in its tone (`ToneFor(Blessing)`).
  - Body: tiles, attunement grid, titles ledger.
  - Footer: `send('status', 'tree'|'storage'|'shop'|'reroll'|'reboot'|'close')`.
  - The C++ side passes the tones as `"rankTone"` / `"blessingTone"` fields (Task 9 adds them to `BuildStatusJson`).
- **Tree** (`Payloads::Tree()` shape):
  - An SVG node net in the projection style: owned filled, available glowing, locked dashed, SP in the title.
  - Focus or hover shows cost, prerequisites and the tier gate.
  - `send('buy', key)`, `send('respec')`, `send('nav', 'status')` to go back; `send('close')`.
  - Pad: arrows move to the nearest node in that direction.
- **Shop** (`Payloads::Shop()` shape): cards with icon, price and affordability; top-tier items gold. `send('shopBuy', index)`, `send('nav','status')`, `send('close')`.
- Keep the existing check.mjs assertions working against the new file: rank insignia on screen, status buttons name real icons, the focus ring reaches every target kind, every translated label carries its English.

- [ ] **Step 1:** Extend `mock.js` with the real 17-node tree (already there), a status payload for each blessing kind and a shop payload with affordable, unaffordable and rare items.
- [ ] **Step 2:** Implement the three screens.
- [ ] **Step 3:** `node tools/check.mjs`. Expected: all pass. Playground screenshots of status (S rank gold, Shattered red, Dormant violet), tree and shop.
- [ ] **Step 4: Commit** `feat(ui): status, skill tree and shop screens in the projection style`.

### Task 8: Hud page

**Files:**
- Create: `magelight/IsekaiHero/hud.html`
- Modify: `playground/mock.js`, `tools/check.mjs`

**Interfaces** (C++ calls):
- `isekai.loc(table)`, `isekai.scale(s)`, `isekai.vr(on)`.
- `isekai.toast({text, key, banner, tone})`: top centre. A same `key` replaces and restarts. Line `3.2 s` / banner `5.5 s`, fade `0.8 s`, rise `0.22 s`.
- `isekai.flourish({title, subtitle})`: gold, centred, `3.2 s`, `0.7 s` fade.
- `isekai.labels.full([{id, x, y, s, level, name, tag, verdict, tone, hp:[now,max], mp:[now,max], sp:[now,max]}])`
  - The frame is name and level over one bar carrying the verdict tone, with HP figures when present.
  - The mp/sp strip appears only when max > 0.
  - Elements are keyed by `id` and reused.
- `isekai.labels.pos([[id, x, y, s], ...])`: transforms only.
- In VR mode, `labels.full` renders a head-locked **scan list** (top-right column) sorted Lethal > Dangerous > Manageable > Trivial, then by the order received (C++ sends nearest first). `labels.pos` is ignored.
- The page sends `isekai.ready` on load. It never sends actions.

- [ ] **Step 1:** Implement `hud.html`, reusing `theme.css`/`common.js`.
- [ ] **Step 2:** Playground hud demo:
  - a toast burst with key replace;
  - a banner;
  - a flourish;
  - 12 labels drifting with `pos` updates at 60 Hz;
  - the VR scan-list toggle.
- [ ] **Step 3:** Add a check.mjs rule `"hud: the page animates only transform and opacity"` that scans `hud.html` CSS transitions/animations for other properties. Run `node tools/check.mjs` and expect all pass.
- [ ] **Step 4: Commit** `feat(ui): hud page for toasts, flourish and threat labels`.
- [ ] **Step 5: Look review checkpoint.**
  - Show the user the playground (browser pane screenshots of every screen and tone, sent with SendUserFile).
  - Apply their look feedback before Task 9.
  - This is the "user reviews the look before the in-game run" gate from spec section 10.

### Task 9: Screens facade and call-site switch

**Files:**
- Create: `src/UI/Screens.h` (replaces the stub), `src/UI/Screens.cpp`
- Delete: `src/UI/SystemWindow.*`, `src/UI/SkillTreeWindow.*`, `src/UI/ShopWindow.*`
- Modify: callers.
  - `src/Progression.cpp`: `ShowStatusPanel` always uses `Screens::ShowStatus(BuildStatusJson())`; delete the ImGui ledger branch; add `rankTone`/`blessingTone`.
  - `src/Shop.cpp`, `src/System.cpp`.
  - Every file including the deleted headers (grep `SystemWindow.h|SkillTreeWindow.h|ShopWindow.h`).

**Interfaces:**
- `namespace Isekai::UI`, unchanged call-site names from spec 6.3:
  - `struct Choice { std::string label; std::string icon; bool iconOnly = false; };` The icon is a file name under `icons/`.
  - `struct Emblem { std::string icon; std::string caption; Logic::Tone tone = Logic::Tone::Cyan; };`
  - `void ShowSystemWindow(std::string title, std::string body, std::vector<Choice>, std::function<void(int)> onSelect, float revealCharsPerSec = 45.0f, float widthHint = 720.0f, Emblem = {}, Logic::Tone tone = Logic::Tone::Cyan);` plus the `std::vector<std::string>` convenience overload.
  - `void ShowSkillTree(); void ShowShop(); void ShowStatus(std::string json);`
  - `bool IsSystemWindowOpen(); bool IsSystemScreenOpen(); void DismissSystemWindow(); bool Available();`
- Behaviour, ported from `Prisma.cpp` `OnChoose`/`OnBuy`/`OnRespec`/`OnStatusAction`/`OnShopBuy`/`OnShopClose` (read them):
  - `detail::OnAction` → `Logic::ParseAction`; nullopt logs once per type and returns.
  - The sounds are the same: WindowOpen on open; ButtonClick vs WindowClose on choose, delayed `Sounds::kResumeGraceMs`.
  - `storage` closes first, then `Storage::Open()`.
  - `buy`/`shopBuy` re-push the same screen.
  - `nav` pushes the target screen without closing.
- Pending decisions: every panel with more than one choice calls `PendingDecision::Offer(panelSerial)`.
  - `OnRefused` and an `OnExited` without a choice call `OnUiModeLost()`.
  - The next `Progression::ShowStatusPanel` hotkey press re-shows the pending panel instead of status.
  - `OnExited(false)` with `NeedsReoffer()` re-requests UI mode once with the Queue flag.
  - A load (`kPostLoadGame`/`kNewGame`) calls `Reset()`.
- `System.cpp` `BeginReincarnation`: the guard `REL::Module::IsVR() && !UI::Prisma::Active()` becomes `!UI::Available()` on every runtime. Message key `ui.needsMagelight`, English `"[ SYSTEM ] Isekai Hero needs Magelight UI (1.7.0 or newer). The System will wait."`. Hotkeys while `!Available()` show the same notification.
- The self-test result panel (`SelfTest::Run(true)`'s `ShowSystemWindow`) passes `Logic::Tone::Red` when any line FAILs, else Cyan (spec 7.2).
- The Prisma code path stays compiled but unused until Task 11 deletes it. `Prisma::Install()` is no longer called.

- [ ] **Step 1:** Add a unit test to `tests/ui_logic_tests.cpp` for the re-offer sequence used here: `Offer(1)` → `OnUiModeLost()` → `NeedsReoffer()` → `Offer(1)` → `!NeedsReoffer()` → `Resolve(1)` → `!Current()`. Run it and expect PASS. This is logic from Task 1, pinned for this use.
- [ ] **Step 2:** Implement `Screens.*` and switch every caller. `grep -rn "SystemWindow.h\|SkillTreeWindow.h\|ShopWindow.h\|Prisma::" src` must only hit `src/UI/Prisma.*` and `src/UI/Overlay.cpp`.
- [ ] **Step 3:** `cmake --build build` succeeds. Unit tests PASS. `node tools/check.mjs`: update `"ui: both renderers colour a System Rank the same"` to `"ui: the status payload's rank tone comes from ToneForRank"` and `"ui: UiScale reaches both renderers"` to `"ui: UiScale reaches both pages"`. Expect all pass.
- [ ] **Step 4: Commit** `feat(ui): Screens facade on Magelight; game logic off ImGui and Prisma`.

### Task 10: Hud producer (toasts, flourish, threat labels)

**Files:**
- Create: `src/UI/Hud.h`, `src/UI/Hud.cpp`
- Modify: `src/UI/ThreatLabels.cpp/.h`. Keep the selection code (`Eligible`, `FightingUs`, `Visible`, `HeadPoint`, `PoolOf`, range, crosshair cone, cap 12). Replace `ImVec2` with `struct ScreenPt { float x, y; }`. Replace `Verdict{tag,col}` with `Logic::Verdict` + `L()` tag. Delete every drawing function and the VR billboard path.
- Delete: `src/UI/Toast.*`, `src/UI/LevelUpEffect.*`
- Modify: callers of `Toast.h`/`LevelUpEffect.h` include `UI/Hud.h`

**Interfaces:**
- `namespace Isekai::UI`:
  - `void ShowToast(std::string text, std::string key = {});` and `void ShowToastBanner(std::string text, std::string key = {});`
    - Any thread. They queue under a mutex.
    - The frame tick drains to `Run(View::Hud, "isekai.toast(" + json + ")")`.
    - If `!Available()`, fall back to `RE::SendHUDMessage::ShowHUDMessage` as today.
  - `void PlayLevelUpEffect(std::string title, std::string subtitle);`
  - `ToggleThreatLabels`, `ThreatLabelsVisible`, `BindThreatLabelHotkeys` stay declared in `UI/ThreatLabels.h` with today's behaviour. `ThreatLabels.h` also declares `std::vector<LabelData> SelectLabels(bool project)` for `Hud.cpp` (`LabelData` = today's `Label` minus `ImVec2`, plus `std::uint32_t id` = actor FormID).
  - `namespace Hud { void Install(); }` registers `OnFrame` with `FrameTick::Add`. It pushes `isekai.vr(true)` on ready when `REL::Module::IsVR()`.
- `OnFrame`, main thread:
  1. Drain toasts and flourishes.
  2. If labels are visible and the player is in the world, select labels and project them with `NiCamera::WorldPtToScreenPt3`. Normalise to 0..1; y is flipped as today.
  3. Build `Logic::LabelFrame`s: `content` hashes level, name, verdict and the hp/mp/sp now/max.
  4. Run them through a `LabelPushGate`: Full → `labels.full`; Positions → `labels.pos`; None → nothing.
  - In VR: no projection. Send `labels.full` when the set or content changes, nearest first. The `content` hash covers it, with x/y fixed at 0.
  - Labels hidden: one empty Full via the gate, then no work.
  - While `HudBelowPanels()` and the system view holds UI mode, toasts are also sent to the system view (`isekai.toast` is defined in `system.html` too; add it in this task).
- The INI keys `VRInHeadsetLayer` and `VRThreatLabelHeight` are no longer read. Delete their `Config` accessors and the `IsekaiHero.ini` blocks. `Config::Load` must not warn about unknown keys; check that SimpleIni ignores them.

- [ ] **Step 1:** Implement `Hud.*` and the ThreatLabels refactor, and switch the includes.
- [ ] **Step 2:** `cmake --build build` succeeds. Unit tests PASS. `node tools/check.mjs` passes. Update the hotkey rule only if its parser named `ThreatLabels.cpp` paths.
- [ ] **Step 3: Commit** `feat(ui): hud producer on the frame tick, threat labels without ImGui`.

### Task 11: Remove ImGui, PrismaUI and the VR helper

**Files:**
- Delete:
  - `src/UI/Overlay.*`, `src/UI/VROverlay.*`, `src/UI/Style.h`, `src/UI/Textures.*`;
  - `src/UI/Prisma.*`, `src/PrismaUI_API.h`;
  - `extern/imgui-vr-helper/`, `prisma-patch/`, `package-prisma-patch.ps1`, `fomod/images/prismaui.png`.
- Modify:
  - `src/UI/Input.*`: delete `FeedImGui`, the ImGui pending-input queue, the `Prisma::SendGamepad` forwarding and the mouse-cursor feed. Keep every hotkey path and `MenuGuard` (spec 12 leaves its removal to a later run).
  - `src/System.cpp`: remove `InstallVROverlay` from kPostPostLoad and the `UI::Install()` (overlay) call.
  - `CMakeLists.txt`: sources, `imgui`, `d3d11.lib`, `dxgi.lib`.
  - `vcpkg.json`: drop `imgui`, and `stb`/`directxmath`/`directxtk` only if `grep -rn` finds no remaining include.

- [ ] **Step 1:** Delete and edit. `grep -rni "imgui\|prisma\|VROverlay\|Overlay::" src CMakeLists.txt vcpkg.json` must be empty.
- [ ] **Step 2:** Reconfigure and build (`cmake --build build`; a vcpkg change may need the build.bat configure step). Expected: success, and the DLL is smaller than `aa22f5a`'s.
- [ ] **Step 3:** `node tools/check.mjs`. The `"vr startup … PrismaUI call are logged"` rule becomes the Task 5 Magelight rule (delete the duplicate). The `"shop: the built-in window still fits a 1080p screen"` rule goes, because that window no longer exists. Note it in the commit body. Expected: all pass. Unit tests PASS.
- [ ] **Step 4: Commit** `refactor(ui)!: remove the ImGui overlay, the PrismaUI bridge and ImGuiVRHelper`.

### Task 12: Self-test lines for the new UI

**Files:**
- Modify: `src/SelfTest.cpp`

**Interfaces:**
- New checks in the `Add(out, pass, warn, name, detail)` style:
  - `magelight host`: pass if `Available()`. Detail `"Magelight UI <HostVersion()>"` or the reason.
  - `system view` / `hud view`: `ViewCreated`; the hud detail adds `"(below panels)"` when `HudBelowPanels()`.
  - `page ready`: both `PageReady` and `PageMatchesBuild(PageVersion(v), kUiBuildId)` for both views. On a mismatch the detail is `"page <id> != dll <id>"`.
- `interface icons` checks `Data\Magelight\IsekaiHero\icons\`.
- `CheckEnvironment` drops the Prisma line.

- [ ] **Step 1:** Implement. `cmake --build build` succeeds.
- [ ] **Step 2:** Add a check.mjs rule `"selftest: the Magelight checks are wired"` asserting the four check names appear in `SelfTest.cpp`. Run `node tools/check.mjs` and expect all pass.
- [ ] **Step 3: Commit** `feat(selftest): Magelight host, views and page version checks`.

### Task 13: Packaging, FOMOD and docs

**Files:**
- Modify: `package.ps1`, `fomod/ModuleConfig.xml`, `fomod/info.xml`, `README.md`, `docs/NEXUS_DESCRIPTION.md`, `docs/VR.md`, `CHANGELOG.md`, `IsekaiHero.ini` (comments that mention PrismaUI/ImGui/ImGuiVRHelper), `lang/template.txt` (new key `ui.needsMagelight`)

**Steps:**
- [ ] **Step 1:** `package.ps1` stages:
  - `Data\Magelight\IsekaiHero\` with `system.html`, `hud.html`, `theme.css`, `common.js`;
  - `version.js` from `build/generated/`;
  - `icons\*.png`.
  - Drop the PrismaUI staging and the old `SKSE\Plugins\IsekaiHero\icons` copy.
  - The privacy gate is untouched.
- [ ] **Step 2:** FOMOD: remove the PrismaUI option. The description names Magelight UI (Nexus 193303, ≥ 1.7.0) as required. The `"fomod: the installer offers files that exist"` rule must pass.
- [ ] **Step 3:** Docs:
  - README/Nexus: requirement swap, VR scan-list note.
  - `VR.md`: rewrite the status section for Magelight, still "experimental / untested".
  - CHANGELOG `Unreleased` entry. No version number.
- [ ] **Step 4:** `node tools/check.mjs` passes. `pwsh -NoProfile -File package.ps1 -Version magelight-ui-dev` succeeds (the privacy gate passes). List the archive and confirm the `Data\Magelight\IsekaiHero\` layout and that no `PrismaUI` path remains.
- [ ] **Step 5: Commit** `chore(release): package and document the Magelight UI build`.

### Task 14: In-game verification in SotN (gated)

Each step needs its own consent. This is not a code task.

- [ ] **Step 1:** Ask the project-lead session for an install slot. Send it the test points below.
- [ ] **Step 2:** Before installing, and with the user's yes for that run, measure the FPS baseline on the current build (scene and route fixed in the test points). Then, with the user's explicit yes for the install:
  1. Back up `mods\Isekai Hero` to `H:\SotN-Rollback\IsekaiHero\<hash>\`.
  2. Install the package.
  3. Remove the old `PrismaUI\views\IsekaiHero` from that mod folder by a user-approved deletion list.
  4. Add the protocol entry, run `inventar.ps1` and commit in SotN with `git commit -- <own paths>`.
- [ ] **Step 3:** Vorprüfung file `H:\SotN-Testbruecke\checks\vorpruefung\lauf<N>\isekai-magelight.json`: DLL md5, page files present, `version.js` id equals the DLL's `kUiBuildId`, the mod is enabled.
- [ ] **Step 4:** Test points for the run, via the test bridge:
  - Self-test: all `[SELFTEST]` lines PASS, ending in `ENDE`.
  - Open status, then tree, then shop without closing; buy re-pushes.
  - The blessing panel ignores Escape; a single-button panel closes on Escape.
  - Pad navigation reaches every card and node.
  - A toast with the same key replaces; a purchase toast is visible over the shop.
  - Labels stay on actors during a fast camera turn.
  - `measure_fps` on the same scene as the Step 2 baseline. Must not be worse.
- [ ] **Step 5:** Write the report in `berichte/` and send the results to the project lead.
