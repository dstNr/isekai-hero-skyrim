# Nexus Mods — Beschreibung

Zum direkten Einfügen in das Nexus-Beschreibungsfeld (BBCode). Kurzbeschreibung
("Brief overview") darunter separat.

## Brief overview (Kurzfeld)

```
You died. The System chose you. Begin your new life in Skyrim as a reincarnated hero — blessing choice, quest milestone rewards, a skill tree with game-wide knowledge unlocks, and dimensional storage. Pure SKSE, custom UI, ESL-flagged.
```

## Description (BBCode)

```
[size=5][b]The System has chosen you. Your new life begins.[/b][/size]

Isekai Hero turns your next playthrough into a reincarnation story: you arrive in Skyrim as a soul from another world, the [b]System[/b] binds itself to you, and from that moment on it rewards everything you achieve — with a UI and feel inspired by Solo Leveling and classic isekai progression fantasies.

Built as a [b]native SKSE plugin[/b] with its own interface rendered inside the game. No Papyrus scripts, no MCM dependency, ESL-flagged.

[size=4][b]The Awakening[/b][/size]
The first time you gain control of your character — regardless of how you start the game (vanilla, Alternate Start, Skyrim Unbound, ...) — the System boots up and offers a choice:

[list]
[*][b]NORMAL[/b] — no blessing. The pure challenge. The System still tracks your deeds, and its skill tree becomes your own progression path.
[*][b]HERO[/b] — the classic isekai protagonist: skills at 50, level 25, a head start in gold, souls and perk points. All future rewards ×2.
[*][b]ASCENDED[/b] — the overpowered rebirth: all skills 100, level 150 with matching attributes, maximum perk points, a fortune. All future rewards ×4.
[/list]

[size=4][b]The System watches your deeds[/b][/size]
[b]79 quest milestones[/b] across the main quest, the Companions, the College of Winterhold, the Thieves Guild, the Dark Brotherhood, the Civil War, Dawnguard and Dragonborn. Completing one triggers a level-up flourish — expanding rings, a title, custom sound — and pays out:

[list]
[*]a permanent [b]passive stat bonus[/b] with its own flavor title ("Tomb Raider", "Time Walker", "Listener", ...)
[*][b]System Points[/b], the System's own currency
[*][b]dragon souls[/b] on the dragon-flavored beats
[/list]

All passives aggregate into [b]System abilities visible under Active Effects[/b], and every earned title is listed in the System's status ledger. Quests you completed before installing are recognized retroactively.

[size=4][b]The Skill Tree[/b][/size]
Open the System panel ([b]Right Shift + S[/b]) and enter the skill tree: a hub and three branches — might, arcana, shadow — paid with System Points. Besides stat nodes it holds the System's signature unlocks:

[list]
[*][b]Thu'um Omniscience[/b] — every shout and every word of power
[*][b]Arcane Omniscience[/b] — every enchantment known, no disenchanting needed
[*][b]Alchemical Insight[/b] — every ingredient effect revealed
[*][b]Spell Omniscience[/b] — every spell that has a spell tome
[*][b]Perk Synthesis[/b] — repeatable: convert System Points into perk points
[/list]

The knowledge unlocks are [b]mod-aware[/b]: they cover everything your load order contains, filtered semantically (a mod spell with a tome qualifies exactly like a vanilla one).

[size=4][b]Progression scales with your rebirth[/b][/size]
The blessing you choose keeps mattering — everything the System pays out is multiplied for the rest of the run:

[list]
[*][b]Reward scale[/b] on every milestone (passive stat bonuses, System Points, dragon souls): NORMAL [b]×1[/b], HERO [b]×2[/b], ASCENDED [b]×4[/b].
[*][b]Perk points[/b] per milestone endpoint: NORMAL none, HERO +10, ASCENDED a full pool.
[*][b]The skill tree itself deepens with the blessing.[/b] NORMAL walks the self-made half — stats, resistances, Perk Synthesis, faster Thu'um. HERO additionally unlocks the four [b]Omniscience[/b] gifts (shouts, enchantments, ingredients, spells). ASCENDED additionally unlocks the [b]World Tree[/b] capstone. Sealed nodes are visible but greyed, so you always see what a deeper rebirth would have granted.
[/list]

[size=4][b]Dimensional Storage[/b][/size]
Every reincarnated soul receives a private pocket dimension: one chest inventory, reachable from anywhere through the System panel. Its contents are [b]automatically available at crafting stations[/b] — forge, alchemy table, enchanter — without moving a single item yourself. HERO and ASCENDED find it [b]pre-stocked[/b] with crafting materials, ingredients and gold (scaled by the blessing); NORMAL receives the same dimension [b]empty[/b], to fill as a personal stash.

[size=4][b]Requirements[/b][/size]
[list]
[*]Skyrim Special Edition / Anniversary Edition (built against 1.6.1170)
[*][url=https://skse.silverlock.org/]SKSE64[/url]
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url]
[/list]

[size=4][b]Installation & Compatibility[/b][/size]
Install with your mod manager. The plugin is [b]ESL-flagged[/b] (no load order slot), adds only new records and [b]overrides nothing[/b] — load order position does not matter. No patches needed.

[b]Safe to install mid-playthrough.[/b] Add it to an existing save and the System boots on your next load: you pick your blessing, and every milestone quest you have already completed is recognized and rewarded retroactively. Blessings only ever raise your stats — an established character never loses levels, skills or perks.

[b]Hotkey:[/b] Right Shift + S opens the System status panel; storage and skill tree sit behind the icon buttons in its corner.

[size=4][b]Status[/b][/size]
Early release. Feedback is very welcome, especially from heavily modded setups. If something misbehaves, attach [i]Documents\My Games\Skyrim Special Edition\SKSE\IsekaiHeroSKSE.log[/i] to your report — the mod logs everything it does.

[size=4][b]Credits[/b][/size]
The Dimensional Storage's crafting integration (reading and consuming the chest at a workbench without moving items) adapts the zero-transfer hooking technique from [url=https://github.com/ohfor/scie]SCIE — Skyrim Crafting Inventory Extender[/url] by ohfor, used under the MIT License. SCIE itself is [b]not required[/b] — its approach is reimplemented here, not depended upon.

[size=4][b]AI Disclaimer[/b][/size]
This mod was developed with AI assistance: the C++ code was written together with an AI coding assistant (Claude), and the skill/UI icons are AI-generated artwork. All content was reviewed, integrated and tested by hand in real playthroughs. The sound effects are licensed third-party assets, not AI-generated.
```
