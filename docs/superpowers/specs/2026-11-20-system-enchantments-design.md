# System Enchantments — design

Status: approved by the owner (2026-11-20).

## Intent

The System's gifts so far are numbers and knowledge. This adds eight enchantments of its
own, four for weapons and four for armor, that are clearly stronger than anything in the
vanilla game: the isekai "cheat" fantasy in something the player wears and swings. Each one
has its own visible particle effect, so a System enchantment is recognisable in a fight.
They are bought in the skill tree and then applied at the normal enchanting table.

## Goals

- Eight enchantments: Divine Judgment, Stormchain, Soul Rend, System Breach (weapons);
  Bulwark, Thorncrown, Phoenix Heart, Voidstep (armor).
- Each is a skill-tree node. Buying it makes the enchantment known at the enchanting table.
- Each has a distinct look: on the blade and the target for weapons, as an aura on the
  wearer for armor.
- "Pretty OP": magnitudes well above vanilla's best of the same kind, priced and gated so
  they read as a late reward.

## Non-goals

- No own enchanting UI. The vanilla table does the enchanting: soul gems, charges, the
  Enchanting skill and perk mods (Ordinator) all apply unchanged.
- No smithing integration.
- No dependency on another mod. No assets copied from other mods.
- No Magelight "Codex" page yet (a later, separate step: a System page that shows the
  unlocked enchantments with description and picture).
- No strength multiplier setting. NodeCostScale already scales their price; a magnitude
  knob comes only if players ask.

## Anti-goals

- An unlock that cannot be undone safely does not count as working: a respec must never
  leave the save in a state it cannot explain.
- Arcane Omniscience (node 7) must not teach these eight for free.
- A new hard requirement for players (another framework, a VFX mod) would be a failure.

## Approaches considered (for the particle effects)

| | Approach | Trade-off |
|---|---|---|
| **A** | Vanilla effect assets, recoloured and recombined in `IsekaiHero.esp` | No new files, no dependency, no permissions. Players with VFX overhauls (Flame/Frost/Drain VFX Edit, KittyVFX …) get the nicer version automatically. Looks familiar rather than unique. |
| B | Effects from other mods (e.g. Vibrant Weapons via Enchantment Art Extender) | Prettiest, but a hard dependency or a permission request for a public mod. Possible later as an optional patch. |
| C | Own particle meshes through the asset pipeline | Most distinctive, most time and test runs. |

**Chosen by the owner: A for seven, C for System Breach** (its cyan glitch shards are the
System's signature and have no vanilla equivalent). B is left for a possible optional patch.

## The eight

In-game text is English. Magnitudes below are the starting point for the first test run,
not final; "vanilla best" is the strongest vanilla enchantment of the same kind at 100
Enchanting with perks.

| # | Name | Slot | Effect | Look (approach) | Logic |
|---|---|---|---|---|---|
| 1 | **Divine Judgment** | weapon | Sun damage, about 3× vanilla fire; double against undead; target burns on | Golden holy light and motes, Dawnguard sun-magic visuals (A) | ESP |
| 2 | **Stormchain** | weapon | Shock damage that jumps to up to 3 more hostile actors nearby | Violet lightning threads, recoloured shock (A) | plugin |
| 3 | **Soul Rend** | weapon | Absorbs health and magicka in one hit | Dark-red mist drawn to the wielder, recoloured absorb (A) | ESP (two absorb effects) |
| 4 | **System Breach** | weapon | Extra damage that ignores armor and resistances | Cyan glitch shards in the System's projection look (C) | plugin |
| 5 | **Bulwark** | armor | Large armor rating and damage reduction | Blue shield shimmer (A) | ESP |
| 6 | **Thorncrown** | armor | Reflects part of melee damage taken | Red thorn tendrils (A) | ESP, plugin fallback |
| 7 | **Phoenix Heart** | armor | Strong health regeneration; once a minute a rescue heal when health drops low | Ember feathers (A) | ESP + plugin |
| 8 | **Voidstep** | armor | Faster movement, cheaper stamina, no fall damage | Black-violet streaks at the feet (A) | ESP, plugin fallback |

Armor auras use the magic effect's own persistent hit shader/art on the wearer, the way
vanilla constant effects show (Oakflesh). Weapon looks use the effect's enchant shader and
enchant art on the blade and the hit shader/art on the target.

## In the skill tree

- A new zone **Enchant** (`Zone::kEnchant`), drawn as its own band, labelled like the others.
- Eight one-shot nodes with new, never-used keys. Each node's effect is a new
  `Effect::kTeachEnchantment` that marks one specific enchantment known.
- The band hangs off **Arcane Omniscience** (key 7). Weapon and armor are two short lines of
  four, but no chain: every one of the eight needs only key 7, so each can be bought on its
  own and the HERO/ASCENDED gates below hold as written. *(Builder's call, confirmed in review.)*
- Price 30–40 SP each. Gates: HERO for six, ASCENDED for System Breach and Phoenix Heart.
  NodeCostScale applies as to every node.
- Not refundable, like the other knowledge nodes: a respec cannot un-enchant items already
  made. REBOOT refunds nothing for them, the same as for Omniscience today.

## How it works

- **Records** in `IsekaiHero.esp`: per enchantment one ENCH (base enchantment, not
  referenced by any variant) and its MGEFs; effect shaders and art objects for the looks.
  Built the established way (a new part in `docs/CREATION_KIT_ESP.md`, owner sends the
  FormIDs, code looks them up by local FormID).
- **Teaching:** set the kKnown flag on the one ENCH, the same mechanism
  `UnlockAllEnchantments` uses. Re-applied on every load by `SkillTree::ApplyOnLoad`, like
  the other knowledge nodes.
- **Omniscience stays out:** `UnlockAllEnchantments` skips the eight explicitly, in both its
  marking and its name-dedup loop. Relying on "no variant points to them" is not enough:
  an item the player enchants may create a form that does. A load also clears the flag on
  any of the eight whose node the save did not buy.
- **Where they go, and stacking** *(builder's call)*: weapons on any weapon. Armor by
  Worn Restrictions: Bulwark and Thorncrown on body and shield, Phoenix Heart on body and
  amulet, Voidstep on feet. Effects done in the ESP stack like vanilla enchantments.
  Effects done in the plugin count once per actor however many items carry them;
  Thorncrown's reflection is capped below 100 %.
- **Strength comes from the table** *(builder's call)*: the plugin effects read their
  magnitude from the enchantment on the actual item, so soul gem, Enchanting skill and perks
  scale them like the ESP effects. A weapon effect fires only while the weapon still has
  charge.
- **Anyone who wears or wields it** *(owner's decision)*: the enchantment belongs to the
  item, not to the player. Followers and NPCs with such an item get every effect, the
  plugin ones included. "Hostile" always means hostile to the attacker.
- **Plugin logic** (one new source file) through the existing event sinks:
  - Stormchain: on a hit with that enchantment, find up to 3 hostile actors within range of
    the target and cast a hidden ESP spell from target to each (the visual is the spell's).
  - System Breach: on a hit, apply the extra damage directly to health so armor and
    resistances do not apply; play the C art on the target.
  - Phoenix Heart: watch each wearer's health; below a threshold, heal and start that
    actor's 60-second (game-time-independent, real seconds of unpaused play) cooldown, kept
    per actor in the co-save so a reload does not reset it.
  - Thorncrown and Voidstep first try vanilla actor values (ReflectDamage; SpeedMult,
    stamina rate, a fall-damage perk entry). Whatever does not work in game moves into the
    plugin.
- **Self-test:** a `[SELFTEST] isekai.enchantments` line: all eight forms resolve, and
  their known flag matches the unlocked nodes.

## Constraints

- One DLL for SE, AE and VR, 1.6.1170–1.7.104, as now. VR untested is acceptable.
- The co-save version rises by one for Phoenix Heart's per-actor cooldowns; older saves load
  with no cooldowns running.
- New text goes through the language table (`lang/template.txt`, `tools/extract-strings.mjs`).
- `tools/check.mjs` must stay green; package and privacy gate unchanged.

## Left to the builder

- Node coordinates, as long as the tree's layout rule holds: nothing on a line between two
  other nodes.
- Exact magnitudes, ranges, durations and colours within the table above; the first test
  run settles them.
- Icons for the eight nodes (existing icon set or new art through the asset pipeline).
- The System Breach mesh through the asset pipeline (approach C).
