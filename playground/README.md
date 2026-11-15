# Playground

Develop and review the Magelight UI pages **without launching Skyrim**.

It loads the real, unmodified pages — the system page
(`magelight/IsekaiHero/system.html`) and, over it, the click-through hud page
(`magelight/IsekaiHero/hud.html`: toasts, the level-up flourish, threat labels)
— in two iframes over a painted scene and drives them with `mock.js`, which
plays both halves of the game:

- **The host.** `serve.mjs` injects a fake `window.magelight` into the page
  (only when asked with `?host=mock`), forwarding every `send()` to `mock.js`.
  `isekai.ready` releases the queue the way `src/UI/Magelight.cpp` does — the
  translation table and UiScale first, then everything shown before the page
  was ready. `isekai.action` is routed like `Screens::OnAction` (`choose`,
  `buy`, `respec`, `shopBuy`, `status`, `nav`, `close`). The hud has its own
  ready gate and prelude (loc, scale, and `isekai.vr(true)` when VR is ticked);
  it never sends anything but `isekai.ready`.
- **The game.** The tree data is the actual 17-node graph from
  `src/SkillTree.cpp`, the shop is `Shop::Catalog()` in order, and the panel
  scenarios use the real strings from `src/System.cpp` (blessing choice,
  custom builder, awakening path, reincarnation result).

Icons are served from the repo's top-level `icons/` at the page's `icons/`
path, and `version.js` from `build/generated/` when a build exists (otherwise
the page reports build `dev`, as it would in game). Nothing is copied.

## Run

```
node playground/serve.mjs
```

then open the printed URL: **http://localhost:5173/playground/**

> **Why a server and not just double-clicking the file?** Opening the harness as
> a `file://` page makes the browser treat the parent page and the iframe as
> different origins, which blocks the cross-document scripting the mock needs.
> Over `http://localhost` they share one origin and it works. Node standard
> library only — no install, no dependencies.
>
> Change the port with `PORT=8080 node playground/serve.mjs`.

## Controls (top-left dock)

| Control | What it does |
|---------|--------------|
| **Panel** buttons | Story (cyan, one button), Blessing ×5 (the real five-way choice, capsule row), Blessing ×3 (cards), Awakening (two cards), Dormant (violet), Warning (red self-test), Emblem (crest + icon actions). Choices continue the flow as in game. |
| **Status** | The ledger for each blessing kind: Custom (violet blessing, the three dials), S rank (rank and ASCENDED gold, all 79 titles), Hero (cyan), Shattered (red), Dormant (violet, awakening level, the System quiet). |
| **Screens** | Skill tree; Shop; Shop 12 SP (sets the balance so the dearest goods cannot be afforded). Buying, respec, Back and the status buttons continue as in game. |
| **Hud** | Toasts (a burst: objective banner, a kill tally that replaces itself under one key, a payout, the objective paid out in gold), Banners (gold, red and a line at once), Flourish and Grandmaster (gold), Labels (12 threat labels drifting, `labels.pos` at 60 Hz), Still (8 labels, only the 4 Hz `labels.keep()` heartbeat), Stall (no label call at all — free camera or a pause: the page hides them after 300 ms; Labels or Still brings them back), Clear (`labels.full([])`, what a pausing menu sends). |
| **VR** | The hud in VR: threat labels become the head-locked scan list, most dangerous first. |
| **Tone** | Re-shows the panel in another tone (in game C++ picks it). |
| **Scene** | Dusk, snow (bright — the legibility check), field (mid-grey with figures — the label check) or night behind the page. |
| **Text** | English or a partial German table: re-renders the open screen in place, as a settings reload does. |
| **UiScale** | Pushes `isekai.scale`. |
| **+10 / Rebirth / Dormant / Reset** | Game state. A change re-pushes the tree, shop or status on display, the way the plugin re-pushes after a purchase. |
| **☰** | Hides the dock for a clean screenshot. |

The **Host traffic** panel (bottom-right) logs every message the page sends.
Click the view first for keyboard input: arrows move focus, Enter confirms,
Esc closes a single-button panel; a click or Enter while text types only
completes it.

### Direct links

`?open=<id>&tone=<cyan|gold|red|violet>&scene=<dusk|snow|field|night>&dock=0&instant=1&vr=1`
opens a scenario straight away (`instant=1` skips the typewriter, `vr=1` puts
the hud in VR mode). Ids:
`story`, `blessing`, `cards`, `awaken`, `dormant`, `warning`, `emblem`,
`status`, `status-s`, `status-hero`, `status-shattered`, `status-dormant`, `tree`,
`shop`, `shop-low`, `hud-toasts`, `hud-banner`, `hud-flourish`, `hud-flourish-gm`,
`hud-labels`, `hud-labels-still`, `hud-stall`, `hud-clear` (`open=none` opens
nothing). Toasts and the flourish are over within seconds, so a fixed-budget
`--screenshot` misses them; drive headless Edge over CDP and capture at the
moment you want. A headless screenshot, for example:

```
msedge --headless=new --window-size=1600,900 --virtual-time-budget=6000 ^
  --screenshot=panel.png "http://localhost:5173/playground/?dock=0&instant=1&open=cards"
```

## What this is not

- It does **not** touch the shipped pages. The host shim is injected by the
  server at request time, and this folder never ends up in a release.
- The mock is a faithful stand-in, not the plugin. Anything that depends on real
  game state (actual perk pool, unlocked shouts, save data) is simulated.
- Chromium is not Ultralight. Layout and colour match closely; performance and
  some CSS edge cases are only proven in game.
