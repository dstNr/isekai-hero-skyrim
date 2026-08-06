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

  A fourth choice, **Dormant**, does not pick a tier at all: you start at NORMAL
  and the blessing wakes on its own — HERO at level 25, ASCENDED at level 80
  (configurable, `[Dormant]` in the ini). Nothing arrives early, nothing stays
  sealed for good. It is orthogonal to the question above, so Dormant + Shattered
  grows only the System's reach and never hands over a grant.

  And a fifth, **Custom**, unbundles the tier: you set its three pieces
  independently — the **starting gift** (flat skills/level/gold), the **reward pace**
  (×1/×2/×4) and the **skill-tree depth** — each NORMAL/HERO/ASCENDED. So "the whole
  tree open, but a normal start and normal rewards" is a valid build; it self-balances,
  because you still buy every node with System Points earned at your chosen pace.

  Changed your mind later? The System status panel has a **Reboot** button (with a
  confirm) that re-opens this whole blessing choice on an existing character — keeping
  your milestones, skill tree and System Points. (Stats a previous *Full* blessing
  already granted stay; only a new game truly starts from zero.)

  The status panel also shows a derived **System Rank** (E through S) next to your
  tier — a single readout of milestones earned, character level and System Points
  invested in the tree, the "how far along am I" number every isekai protagonist gets.
- **79 quest milestones** (main quest, Companions, College, Thieves Guild,
  Dark Brotherhood, Civil War beats, Dawnguard, Dragonborn). Completing one
  plays a level-up flourish (rings, title punch, custom sound) and pays out a
  passive stat bonus, System Points, and on the big beats dragon souls.
  Already-completed quests are recognised retroactively on load.
- **Passives as real abilities.** All milestone bonuses aggregate into eight
  `System:` abilities visible under Active Effects — recomputed from scratch
  on every load, so they can never double-apply.
- **System Skill Tree** (`RShift+S` → crystal icon): a hub and three named,
  visually framed branches — **MIGHT**, **SHADOW**, **ARCANA** — plus a **MASTERY**
  rail, all paid with **System Points**. Every node shows its name, and the tree's
  landmarks (the hub, the World Tree capstone, the Omniscience gifts) are drawn larger
  than the leaves. Includes four knowledge unlocks that span
  *every loaded plugin* via semantic filters:
  - all shouts + words of power ("has a description", deduplicated by name)
  - all enchantments ("referenced as a base enchantment")
  - all ingredient effects
  - all spells ("has a spell tome")

  Plus **repeatable** nodes with no lock-out: **Perk Synthesis** (1 System Point
  → 5 perk points, uncapped) and eight **mastery** utility stats in their own
  scrolling rail — **Fleet of Foot** (move speed), **Beast of Burden** (carry weight),
  **Enduring Vigor** (Health/Magicka/Stamina), **Storm Ward** (Shock Resist), **Warded
  Mind** (Magic Resist), **Arcane Absorption** (Spell Absorption), **Iron Skin** (Armor
  Rating) and **Rapid Recovery** (H/M/S regen). Each climbs 10 ranks across 5 named
  tiers (Novice through Grandmaster, price rising per tier), with a level-up flourish
  on reaching Grandmaster — a real, celebrated ceiling instead of an open-ended grind.
  The tree is
  **gated by rebirth tier** (`Node::minPower`): NORMAL walks the self-made
  stat/utility half, HERO additionally unlocks the four Omniscience gifts, ASCENDED
  additionally unlocks the World Tree capstone. Sealed nodes render greyed with a
  "requires HERO/ASCENDED rebirth" hint — or, on a dormant blessing, the level they
  awaken at (or hidden entirely — see `IsekaiHero.ini`) — one shared graph, one
  source of truth, no duplicate tables. A **Respec** button refunds the points spent
  on stat nodes and reverts their effects; the knowledge unlocks and Perk Synthesis
  are excluded, since neither can honestly be taken back.

  The System's read on an enemy — the "Observation"/"Appraisal" move every isekai
  protagonist gets — is no longer a node here. It is simply **on**: a colour-coded
  TRIVIAL/MANAGEABLE/DANGEROUS/LETHAL floating over what you are aiming at and over
  anything fighting you, shrinking and dimming with distance. `V` switches it off and
  on, and `IsekaiHero.ini` decides who gets a label. *(SE/AE only — it is drawn by the
  same overlay as the panels, which Skyrim VR does not get.)*

  An **optional [PrismaUI](https://www.nexusmods.com/skyrimspecialedition/mods/148718)
  patch** renders the **whole UI**
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
  It **starts empty for every blessing** — you stock it from the System Shop's
  material packs, so what it holds is what you chose to spend System Points on.
  A **storage codex** (granted on reincarnation) is a table-free physical shortcut:
  use it from the inventory and the chest opens directly, no System panel needed —
  and it never runs out.
- **System Quests.** The System hands you a kill objective — "Slay 25 Draugr" — announces
  it, counts your kills top-centre as they happen, and pays System Points when it is done.
  Then it goes quiet for a day and offers the next one on its own; it is never waiting for
  you to open a menu, and never sends a low-level character after dragons. Targets are
  matched by actor *keyword*, so creatures added by other mods count too. No quest markers
  and no busywork: it ticks over in the background of however you were already playing, and
  it is what keeps System Points coming in once the 79 milestones run out.
- **System Shop** — a third button beside Skill Tree and Storage in the status panel,
  opening its own screen of item cards. Spend System Points on **material packs**
  (smithing, alchemy, soul gems — each in a small and a large size) or on gold, all
  delivered straight into the Dimensional Storage. This is how the storage gets filled
  at all, and it means points keep mattering long after the skill tree is bought out.
- **Custom UI & sound.** Solo-Leveling-inspired panels (glow frames, corner
  brackets, typewriter reveal, monospace terminal font), custom SFX routed
  through the game's audio system, icon buttons, ESC handled properly.
- **Optional [SkyrimNet](https://github.com/MinLL/SkyrimNet-GamePlugin) integration
  (experimental, untested).** When SkyrimNet (AI-driven NPCs) is installed, the mod
  pushes the player's System status to it — the reincarnation and tier, and each
  milestone earned — as persistent world-knowledge, so AI NPCs can react to the isekai
  premise. One-way and soft-detected: no build- or load-time dependency, does nothing
  without SkyrimNet, and can be switched off in `IsekaiHero.ini`. **Not yet verified in
  a running game with SkyrimNet** — it has no effect at all unless SkyrimNet is present,
  so it is safe for everyone else. See `src/SkyrimNet.cpp`.

## Requirements

- **Skyrim Special Edition / Anniversary Edition** (developed against 1.6.1170)
- **[SKSE64](https://skse.silverlock.org/)**
- **[Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)**

### Skyrim VR (experimental)

The plugin is built for all three runtimes at once (CommonLibSSE-NG) and loads under
**SKSEVR** with the **VR Address Library for SKSEVR** — same archive, no separate VR
build. The mod's logic (blessings, milestones, passives, storage, crafting) is
runtime-neutral.

The built-in **ImGui overlay is disabled in VR** (it hooks the desktop swap chain,
which crashes on the VR renderer and would never appear in the headset anyway), so in
VR **all UI goes through the PrismaUI patch** — which needs PrismaUI's **1.5.0 VR
build** (the stable 1.4.x has no VR). Without it, the System menu cannot be shown in
VR and the mod holds off the reincarnation prompt rather than stranding the character.
Still community-testing; see [docs/VR.md](docs/VR.md) for status, requirements and the
test checklist.

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
./build.bat            # configure + build + deploy DLL/PDB/icons into the game folder
./package.ps1          # pack a mod-manager-ready 7z into dist/
node tools/check.mjs   # consistency checks — no game, no compiler, ~1s
```

### Testing

Almost every line here orchestrates game API calls and cannot run outside Skyrim, so
there is no unit-test suite — it would have caught very little. What actually went wrong
during development was different: **the same data described in two places drifting apart**,
and **geometry whose terms did not all scale together**. Two things cover that:

- **`node tools/check.mjs`** — runs without the game. Verifies the skill-tree node table
  against the playground mock, the shop catalog against its mock, that every advertised
  icon exists, that the status JSON's fields are emitted/read/mocked consistently, that no
  two zone frames or node tiles overlap and no node name can reach its neighbour, that
  `kVersion` matches the highest co-save read gate, that every saved field is also loaded,
  and that no source file is missing from `CMakeLists.txt`. Every check exists because its
  bug happened at least once.
- **The in-game self-test** — set `SelfTestKey` in `IsekaiHero.ini` (off by default) and
  press it. Checks what is only decidable in a running game: that the ESP's forms resolve,
  that **every quest target keyword actually exists in the load order** (a mistyped one
  fails silently — the objective simply never completes), that all 79 milestone quests
  resolve, that each material pack's sweep finds anything, and that the crafting hooks
  installed. Writes a PASS/FAIL block to the log. This is the thing to ask a bug reporter
  for.

Neither can test anything that **changes state** — whether a purchase really arrives,
whether respec reverts correctly, whether a kill counts. That is what
[docs/MANUAL_TESTS.md](docs/MANUAL_TESTS.md) is: the in-game test plan, ordered by risk,
covering exactly the gaps the two layers above leave and nothing they already prove.

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

🧪 **Pre-release** (v0.6.1). Feature-complete for full-modlist test runs;
balance values (starting System Points, node costs) are explicitly in a
testing configuration. Version history in [CHANGELOG.md](CHANGELOG.md).
