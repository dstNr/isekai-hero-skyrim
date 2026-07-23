# Playground

Develop and demo the PrismaUI skill-tree view **without launching Skyrim**.

It loads the real, unmodified view
(`prisma-patch/PrismaUI/views/IsekaiHero/index.html`) in an iframe over a fake
scene and drives it with `mock.js`, which reproduces what the SKSE plugin
(`src/UI/Prisma.cpp`) does: it installs the `isekaiBuy` / `isekaiChoose` /
`isekaiCloseTree` callbacks on the view and calls `window.isekaiShowTree`,
`isekaiShowPanel` and `isekaiFlourish` with the same JSON shapes.

The tree data in `mock.js` is the actual 17-node graph from
`src/SkillTree.cpp` — same keys, coordinates, icons, costs, prerequisites and
rebirth-tier gates — so buying nodes, sealed/locked states and repeatable ranks
all behave as they do in-game.

## Run

```
node playground/serve.mjs
```

then open the printed URL: **http://localhost:5173/playground/**

> **Why a server and not just double-clicking the file?** Opening the harness as
> a `file://` page makes the browser treat the parent page and the iframe as
> different origins, which blocks the cross-document scripting the mock needs.
> Over `http://localhost` they share one origin and it works. The server is
> ~70 lines of Node standard library — no install, no dependencies.
>
> Change the port with `PORT=8080 node playground/serve.mjs`.

## Controls (top-left dock)

| Control | What it does |
|---------|--------------|
| **Skill Tree** | Push the tree screen (opens automatically on load too) |
| **Dialog** | Sample single-button narrative panel (typewriter reveal) |
| **Choice** | Sample multi-button choice panel |
| **Level Up** | Fire the flourish animation |
| **+10 / +1** | Grant System Points, so you can actually buy nodes |
| **Rebirth** | Set the reached tier (Normal / Hero / Ascended) — watch the gated "omniscience" and capstone nodes unseal |
| **Reset** | Back to a fresh save |

Click a glowing node to buy it — points drop, it lights up, connected nodes
become available, exactly as in-game. The **Callbacks** panel (bottom-right)
logs every round trip to the mock plugin. `Esc` closes the tree / confirms a
single-button panel (click the view first so it has keyboard focus).

## What this is not

- It does **not** touch the shipped view. The packaging script
  (`package-prisma-patch.ps1`) copies only `index.html` and the icons, so this
  whole folder never ends up in a release.
- The mock is a faithful stand-in, not the plugin. Anything that depends on real
  game state (actual perk pool, unlocked shouts, save data) is simulated.
