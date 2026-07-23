# Isekai Hero — Skyrim SE (SKSE Plugin)

An Isekai / "reincarnated hero" system for Skyrim Special Edition, built as a
native **SKSE C++ plugin** using [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG),
with its own ImGui-based UI drawn straight into the game's D3D11 swap chain.

> **The System has chosen you. Your new life begins.**

---

## Features

- **Reincarnation on first control.** Fires the first time the player is
  actually in the world — works with vanilla starts, `coc`, Alternate Start,
  Skyrim Unbound. A paced `[ SYSTEM ]` boot sequence leads into the blessing
  choice.
- **Three blessings** that keep mattering for the whole playthrough:

  | | NORMAL | HERO | ASCENDED |
  |---|---|---|---|
  | Skills | — | 50 | 100 |
  | Level (+ matching attributes) | — | 25 | 150 |
  | Perk points | — | 10 | 255 (engine max) |
  | Gold / Dragon souls / System Points | — | 2k / 3 / 5 | 25k / 20 / 500* |
  | Reward scale on everything below | ×1 | ×2 | ×4 |

  \*current test value — will be rebalanced.

  HERO and ASCENDED additionally choose **Full** or **Shattered**: Full grants
  the flat start above; **Shattered** keeps only the tier's reward scale and the
  deeper skill tree, starting you at the NORMAL floor — the higher ceiling, earned.
- **79 quest milestones** (main quest, Companions, College, Thieves Guild,
  Dark Brotherhood, Civil War beats, Dawnguard, Dragonborn). Completing one
  plays a level-up flourish (rings, title punch, custom sound) and pays out a
  passive stat bonus, System Points, and on the big beats dragon souls.
  Already-completed quests are recognised retroactively on load.
- **Passives as real abilities.** All milestone bonuses aggregate into eight
  `System:` abilities visible under Active Effects — recomputed from scratch
  on every load, so they can never double-apply.
- **System Skill Tree** (`RShift+S` → crystal icon): a hub and three branches,
  paid with **System Points**. Includes four knowledge unlocks that span
  *every loaded plugin* via semantic filters:
  - all shouts + words of power ("has a description", deduplicated by name)
  - all enchantments ("referenced as a base enchantment")
  - all ingredient effects
  - all spells ("has a spell tome")

  Plus **repeatable** nodes with no lock-out: **Perk Synthesis** (1 System Point
  → 5 perk points) and utility ranks — **Fleet of Foot** (move speed), **Beast of
  Burden** (carry weight), **Enduring Vigor** (Health/Magicka/Stamina). The tree is
  **gated by rebirth tier** (`Node::minPower`): NORMAL walks the self-made
  stat/utility half, HERO additionally unlocks the four Omniscience gifts, ASCENDED
  additionally unlocks the World Tree capstone. Sealed nodes render greyed with a
  "requires HERO/ASCENDED rebirth" hint (or hidden entirely — see
  `IsekaiHero.ini`) — one shared graph, one source of truth, no duplicate tables.

  An **optional [PrismaUI](https://www.prismaui.dev) patch** renders the **whole UI**
  — this tree plus the System dialog panels and the level-up flourish — as an HTML/CSS
  view instead of ImGui. Auto-detected at load, with a safe ImGui fallback whenever the
  patch or the framework is absent. The base mod depends on neither. See `prisma-patch/`,
  `src/UI/Prisma.cpp` and `package-prisma-patch.ps1`.
- **Dimensional Storage** (every reincarnated soul): one chest inventory
  reachable from anywhere via the System panel. Crafting stations **read and
  consume its contents in place** — the material count and the recipe both see
  the chest without anything being shuttled around. It holds up inside a full
  overhaul stack: recipes a mod hides behind "do you carry this?" still appear
  (a single unit of each stored material is lent while you craft, then returned),
  and other mods' pre-craft prompts — an enchanter's "empower with a flawless
  gem?" — see the stored gems too. Stock is drawn from what recipes require, so
  DLC materials come along (chitin plate, netch leather, corkbulb root).
  HERO/ASCENDED find it pre-stocked (blessing-scaled); NORMAL gets the same
  dimension empty, as a stash.
- **Custom UI & sound.** Solo-Leveling-inspired panels (glow frames, corner
  brackets, typewriter reveal, monospace terminal font), custom SFX routed
  through the game's audio system, icon buttons, ESC handled properly.

## Requirements

- **Skyrim Special Edition / Anniversary Edition** (developed against 1.6.1170)
- **[SKSE64](https://skse.silverlock.org/)**
- **[Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)**

## Installation

Install `dist/IsekaiHero-v*.7z` with your mod manager (data-relative layout)
and activate `IsekaiHero.esp`. The plugin is **ESL-flagged** — it takes no
load order slot, overrides no vanilla records, and its position in the load
order does not matter.

**Safe to install mid-playthrough:** on an existing save the System boots on
the next load, already-completed milestone quests are rewarded retroactively,
and blessings only ever raise stats — never demote an established character.

In-game: **RShift + S** (remappable in `IsekaiHero.ini`) opens the
`[ SYSTEM ] STATUS` panel (storage and skill tree live behind the icon buttons
in its top-right corner).

## Building

Toolchain: **Visual Studio 2022 Build Tools** (MSVC + Windows SDK + CMake +
Ninja) and **vcpkg**.

```powershell
./build.bat      # configure + build + deploy DLL/PDB/icons into the game folder
./package.ps1    # pack a mod-manager-ready 7z into dist/
```

`build.bat` refuses to deploy while Skyrim is running (a locked DLL used to
mean silently testing stale code). It builds `RelWithDebInfo`, so Crash
Logger can symbolicate our frames.

The plugin logs richly to
`Documents/My Games/Skyrim Special Edition/SKSE/IsekaiHeroSKSE.log` — form
resolution, milestone grants, storage traffic. It is the first place to look
when something misbehaves.

## Repository layout

| Path | Contents |
|---|---|
| `src/` | the SKSE plugin (System, Progression, SkillTree, Storage, Passives, Sounds, `UI/`) |
| `plugin/IsekaiHero.esp` | the ESL-flagged data plugin (abilities, container, sound descriptors) |
| `icons/`, `sounds/` | UI assets, deployed by the build/package scripts |
| `docs/` | Creation-Kit/xEdit guide for the ESP, ideas backlog, Nexus description |
| `papyrus/` | the archived original Papyrus version (git tag `papyrus-v1.0`) |

## Credits

- **UI sounds** by Cyrex Studios — [UI Sound Pack](https://cyrex-studios.itch.io/ui-sound-pack)
- **Spell icons** by The Higalina Vault — [40 Spell Icons (Fantasy Style)](https://higalina.itch.io/40-spell-icons-fantasy-style-png-512x512)
- **Crafting integration**: the Dimensional Storage reads/consumes the chest at a
  workbench without moving items, adapting the zero-transfer hooking technique from
  [SCIE — Skyrim Crafting Inventory Extender](https://github.com/ohfor/scie) by
  ohfor, used under the MIT License. SCIE is **not a runtime dependency** — its
  approach (which engine functions to intercept, the `RemoveItem` vtable slot) is
  reimplemented here, credited in the source.

## Status

🧪 **Pre-release** (v0.5.0). Feature-complete for full-modlist test runs;
balance values (starting System Points, node costs) are explicitly in a
testing configuration. Version history in [CHANGELOG.md](CHANGELOG.md).
