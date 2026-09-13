# Nexus Mods — Description

For pasting directly into the Nexus description field (BBCode). The short "Brief
overview" is separate, below.

## Brief overview (short field)

```
You died. The System chose you. Begin your new life in Skyrim as a reincarnated hero — blessing choice, quest milestone rewards, kill objectives on the System's own schedule, threat readings over your enemies, a skill tree with game-wide knowledge unlocks, a shop and dimensional storage. Pure SKSE, custom UI, ESL-flagged. Keyboard, controller and VR; translatable.
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
[*][b]HERO[/b] — the classic isekai protagonist: skills at 50, a body of 180 Health, Magicka and Stamina, a head start in gold, souls and perk points. All future rewards ×2.
[*][b]ASCENDED[/b] — the overpowered rebirth: all skills 100, a body of 600 Health, Magicka and Stamina, maximum perk points, a fortune. All future rewards ×4.
[*][b]DORMANT[/b] — the blessing sleeps instead of being taken. You begin as an ordinary mortal and the System wakes on its own: [b]HERO at level 25[/b], [b]ASCENDED at level 80[/b]. Nothing is handed to you early, and nothing stays sealed for good.
[*][b]CUSTOM[/b] — set the pieces yourself. The [b]starting gift[/b], the [b]reward pace[/b] and the [b]skill-tree depth[/b] are each chosen on their own (NORMAL/HERO/ASCENDED), so a build like "the whole tree open, but a normal start and normal rewards" is entirely valid. It self-balances — you still buy every node with System Points earned at your chosen pace.
[/list]

[b]No blessing raises your character level[/b], and that is deliberate: Skyrim's enemies scale with it, so a granted level drags the whole world up with you. Your skills, your body and your perks make you stronger while the world stays where it is. The accepted cost: an ASCENDED character has every skill at 100, and since character levels come from raising skills, that character's level stops advancing. You are meant to be finished growing — the System's own skill tree is where you grow from there.

Taking HERO or ASCENDED — or letting them come to you as DORMANT — then asks a second question: [b]how[/b] the power reaches you. [b]Full[/b] grants the flat start above. [b]Shattered[/b] keeps only what the tier is worth over time — the reward multiplier and the deeper skill tree — and drops you at the same mortal floor as NORMAL. The higher ceiling, earned rather than handed over.

Changed your mind later? The System status menu has a [b]Reboot[/b] button that re-opens this whole choice on an existing character — keeping your milestones, your storage and every System Point you have earned. It [b]hands your body back[/b] as well: the skills and attributes you had before your first blessing are restored, so rebooting from ASCENDED into a Shattered run really is a different character and not just a different label. The skill tree is [b]refunded[/b] rather than carried over — every point you spent on it returns to the pool, to spend again on the new run. Your [b]character level is left alone[/b]: the levels you lived are yours.

[size=4][b]The System watches your deeds[/b][/size]
[b]79 quest milestones[/b] across the main quest, the Companions, the College of Winterhold, the Thieves Guild, the Dark Brotherhood, the Civil War, Dawnguard and Dragonborn. Completing one triggers a level-up flourish — expanding rings, a title, custom sound — and pays out:

[list]
[*]a permanent [b]passive stat bonus[/b] with its own flavor title ("Tomb Raider", "Time Walker", "Listener", ...)
[*][b]System Points[/b], the System's own currency
[*][b]dragon souls[/b] on the dragon-flavored beats
[/list]

All passives aggregate into [b]System abilities visible under Active Effects[/b], and every earned title is listed in the System's status ledger. Quests you completed before installing are recognized retroactively.

[size=4][b]The System gives you work[/b][/size]
The System does not only watch — it assigns. On its own schedule it hands you a [b]kill objective[/b] ("Slay 25 Draugr"), announces it, counts your kills on screen as they happen, and pays [b]System Points[/b] when it is done. Then it goes quiet for a while and offers the next one.

[list]
[*]Targets are matched by actor [b]keyword[/b], so creatures added by other mods count — a mod's draugr is a draugr.
[*]Objectives scale with your level and are never gated above it: no level-4 character is sent after dragons.
[*]A kill counts when it is yours [b]or a teammate's[/b] — followers, summons, thralls, poisons and runes all land.
[*]No quest markers, no journal entries, no busywork. It ticks over in the background of however you were already playing, and it is what keeps System Points coming in once the 79 milestones run out.
[/list]

[size=4][b]Reading the enemy[/b][/size]
The signature isekai [b]"Observation"[/b] move, and it costs nothing to unlock. A target frame floats over an enemy with its [b]level[/b], a [b]health bar with the figure on it[/b], a thin [b]magicka and stamina[/b] strip beneath, and a colour-coded verdict — [b]TRIVIAL[/b], [b]MANAGEABLE[/b], [b]DANGEROUS[/b], [b]LETHAL[/b] — measured against your own level. You see it [b]before[/b] you commit to a fight, which is the only moment it is worth anything.

By default it appears on what you are aiming at and on whatever is actually fighting you; the ini offers three other policies, and a key switches the whole display off. It reads what you can [b]see[/b] — a bandit behind a wall or a hillside stays unread — and aiming anywhere on an enemy is enough, not just at their head.

[size=4][b]The System Shop[/b][/size]
System Points buy more than skill nodes. A shop screen with item cards on four shelves: [b]material packs[/b] (smithing, alchemy, soul gems), [b]gold[/b], and the ten [b]System potions[/b] — four instant restoratives and six one-hour elixirs. Everything is delivered straight into your Dimensional Storage.

Hover a card and it lists [b]exactly what that potion does[/b] — the effects are read off the potion itself, so the card cannot promise something the bottle does not deliver.

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

Alongside them sits a [b]mastery rail[/b] of eight repeatable utility stats — Fleet of Foot (move speed), Beast of Burden (carry weight), Enduring Vigor (H/M/S), Storm Ward, Warded Mind, Arcane Absorption, Iron Skin and Rapid Recovery. Each climbs [b]10 ranks across five named tiers[/b] (Novice through Grandmaster), the price rising per tier, with a level-up flourish when you reach Grandmaster — a real, celebrated ceiling instead of an open-ended grind.

Changed your mind? A [b]Respec[/b] button refunds the points spent on stat nodes and reverts their effects. The knowledge unlocks and Perk Synthesis stay: the System cannot un-teach a shout you already know, and those perk points are long since spent.

Your standing in all of it is summed up as a [b]System Rank[/b], E through S, with its own insignia in the status panel — derived from the deeds you have earned, your level and everything you have invested in the tree.

[size=4][b]Progression scales with your rebirth[/b][/size]
The blessing you choose keeps mattering — everything the System pays out is multiplied for the rest of the run:

[list]
[*][b]Reward scale[/b] on every milestone (passive stat bonuses, System Points, dragon souls): NORMAL [b]×1[/b], HERO [b]×2[/b], ASCENDED [b]×4[/b].
[*][b]Perk points[/b] per milestone endpoint: NORMAL none, HERO +10, ASCENDED a full pool.
[*][b]The skill tree itself deepens with the blessing.[/b] NORMAL walks the self-made half — stats, resistances, Perk Synthesis, faster Thu'um. HERO additionally unlocks the four [b]Omniscience[/b] gifts (shouts, enchantments, ingredients, spells). ASCENDED additionally unlocks the [b]World Tree[/b] capstone. Sealed nodes are visible but greyed, so you always see what a deeper rebirth would have granted — and on a DORMANT blessing they name the level at which they awaken.
[/list]

[size=4][b]Dimensional Storage[/b][/size]
Every reincarnated soul receives a private pocket dimension: one chest inventory, reachable from anywhere through the System panel. Its contents are [b]automatically available at crafting stations[/b] — forge, alchemy table, enchanter — without moving a single item yourself. It [b]holds up inside a full crafting-overhaul stack[/b]: recipes a mod hides behind "do you carry this material?" still show, and pre-craft prompts like an enchanter's "empower with a flawless gem?" see the stored gems. Stock is drawn from what recipes require, so DLC materials (chitin plate, netch leather, corkbulb root…) come along.

It [b]starts empty for every blessing[/b] — you stock it from the System Shop's material packs, so what it holds is what you chose to spend your points on. HERO and ASCENDED still get there sooner: their reward scale multiplies point income.

[size=4][b]Keyboard, controller or headset[/b][/size]
[list]
[*][b]Keyboard[/b] — Right Shift + S opens the System panel, F10 toggles the threat display. Both remappable.
[*][b]Controller[/b] — hold [b]LB[/b] and tap [b]Back[/b] for the System menu; the left stick moves the cursor, [b]A[/b] clicks, [b]B[/b] closes. A combination rather than a single button because a controller has none spare — every face button, both shoulders and both sticks are already bound in vanilla. Both bindings are in the ini, and `None` switches the controller off.
[*][b]Skyrim VR[/b] — the menus come into the headset through the PrismaUI patch, and the keyboard and controller bindings above work there. The in-headset layer that would add threat labels standing in the world, notifications on the HUD plane and a [b]B + Y[/b] controller chord is [b]switched off by default[/b]: a VR player found that it sends the game to the desktop as the mods finish loading, and the cause is not yet found. Set [font=Courier New]VRInHeadsetLayer = 1[/font] in the ini to try it, and please report what happens.
[/list]

[size=4][b]Translations[/b][/size]
Every line of text the mod shows goes through a language file. Drop a `lang/<code>.txt` next to the plugin, set [b]Language[/b] in the ini, and anything a translation does not cover falls back to English rather than showing a blank or a key. The PrismaUI view translates itself the same way. A full English template ships with the mod as the starting point.

[size=4][b]Two interfaces, one download[/b][/size]
The installer asks which interface you want. [b]Built-in[/b] is the one the mod draws itself and needs nothing else. [b]PrismaUI[/b] renders the [b]whole interface[/b] — skill tree, System panels, level-up flourish — as a modern HTML/CSS view instead. Same mod, same logic, different skin.

This used to be a second download; it is now an option inside this one. The mod detects PrismaUI at load and falls back safely on its own UI when it is absent, so the option is safe to pick either way and needs no new save.

Only that option requires [url=https://www.nexusmods.com/skyrimspecialedition/mods/148718]PrismaUI[/url] (and its own Media Keys Fix dependency). The base mod requires neither.

[size=4][b]Settings[/b][/size]
An optional ini at [i]Data/SKSE/Plugins/IsekaiHero.ini[/i] — ships with sane defaults, safe to delete:
[list]
[*][b]Remappable hotkeys[/b] — the System menu (default Right Shift + S) and the threat-display toggle (default F10). Key names or scan codes, modifier optional. The controller and VR bindings sit next to them.
[*][b]Interface size[/b] — one multiplier over everything the mod draws, on top of the resolution scaling that is already there. Those are different questions: scaling with resolution keeps the interface the same [i]apparent[/i] size on a 4K screen, which is not the same as whether you can read it.
[*][b]Language[/b] — which translation file to load.
[*][b]Threat labels[/b] — off/on, how far they reach, how close to the crosshair an enemy must be, whether the figures and the magicka/stamina strip are shown, and which actors get one: what you aim at plus whatever is fighting you (default), every enemy in range, everyone alive, or aim only.
[*][b]Objective schedule[/b] — how long the System waits before the first kill objective of a character's life, and between each one after.
[*][b]Hide sealed skill-tree nodes[/b] instead of showing them greyed
[*][b]DORMANT thresholds[/b] — the levels at which the sleeping blessing wakes
[*][b]Self-test[/b] — a diagnostic that checks the mod against your load order and writes a PASS/FAIL report to the log. It runs by itself after every load, so a bug report never needs more than the log file.
[/list]

[size=4][b]Requirements[/b][/size]
[list]
[*]Skyrim Special Edition / Anniversary Edition (built against 1.6.1170)
[*][url=https://skse.silverlock.org/]SKSE64[/url]
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url]
[/list]

[size=4][b]Installation & Compatibility[/b][/size]
Install with your mod manager. The archive is a [b]FOMOD[/b] and asks three things — which interface, whether the System boots itself or waits for your hotkey, and whether you want the threat readings. Everything it sets is one line in the ini and can be changed afterwards.

The plugin is [b]ESL-flagged[/b] (no load order slot), adds only new records and [b]overrides nothing[/b] — load order position does not matter. No patches needed.

[b]Safe to install mid-playthrough.[/b] Add it to an existing save and the System boots on your next load: you pick your blessing, and every milestone quest you have already completed is recognized and rewarded retroactively. Blessings only ever raise your stats — an established character never loses levels, skills or perks.

[b]Hotkey:[/b] Right Shift + S opens the System status panel; storage, shop and skill tree sit behind the icon buttons in its corner. F10 switches the threat display off and on. Both are remappable in the ini.

[b]Skyrim VR:[/b] the mod loads and runs, and the menus work — a VR player confirmed that much. They go through the PrismaUI option (VR build), which is the one to tick in the installer.

The [b]in-headset layer[/b] — threat labels as billboards in the world, notifications on a head-locked plane, and a controller chord that opens the menu — needs the optional [url=https://www.nexusmods.com/skyrimspecialedition/mods/149180]ImGui VR Helper[/url] and is [b]off by default[/b]. The same player reported that with the helper installed the game goes to the desktop as the mods finish loading: every time, on a five-mod load order, and without writing a crash log. Turning the layer off restores the working setup and leaves the helper installed for other mods that use it.

[i]The cause is not known. There is no VR install on the author's machine, so this one is being chased through player reports — if you turn [font=Courier New]VRInHeadsetLayer = 1[/font] on and it works, or crashes, saying so is genuinely the fastest way to get it fixed.[/i]

[size=4][b]Status[/b][/size]
Early release. Feedback is very welcome, especially from heavily modded setups. If something misbehaves, attach [i]Documents\My Games\Skyrim Special Edition\SKSE\IsekaiHeroSKSE.log[/i] to your report — the mod logs everything it does.

[size=4][b]Credits[/b][/size]
[list]
[*][b]UI sounds[/b] by Nathan Gibson (Cyrex Studios) — [url=https://cyrex-studios.itch.io/ui-sound-pack]UI Sound Pack[/url], used under [url=https://creativecommons.org/licenses/by/4.0/]CC BY 4.0[/url]
[*][b]Skill-tree node icons[/b] by The Higalina Vault — [url=https://higalina.itch.io/40-spell-icons-fantasy-style-png-512x512]40 Spell Icons (Fantasy Style)[/url]. Every other icon is AI-generated artwork made for this mod.
[*][b]Crafting integration[/b]: the Dimensional Storage reads and consumes the chest at a workbench without moving items, adapting the zero-transfer hooking technique from [url=https://github.com/ohfor/scie]SCIE — Skyrim Crafting Inventory Extender[/url] by ohfor (MIT License). SCIE itself is [b]not required[/b] — its approach is reimplemented here, not depended upon.
[*][b]VR interface[/b]: drawn through [url=https://www.nexusmods.com/skyrimspecialedition/mods/149180]ImGui VR Helper[/url] (LGPL-3.0-or-later). Its client API ships with this mod unmodified; the helper itself is an optional separate install.
[/list]

[size=4][b]AI Disclaimer[/b][/size]
This mod was developed with AI assistance: the C++ code was written together with an AI coding assistant (Claude), and the skill/UI icons are AI-generated artwork. All content was reviewed, integrated and tested by hand in real playthroughs. The sound effects are licensed third-party assets, not AI-generated.
```
