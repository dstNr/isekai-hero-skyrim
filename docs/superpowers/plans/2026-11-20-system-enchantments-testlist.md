# System Enchantments — test list

For one collected run in the SotN modlist, through the test bridge. Build under test: the
`feat/system-enchantments` head that the final review approved. The deploy into the modlist goes
through the SotN project lead.

Setup: a `SotNTest_` save with enough System Points (`isekai` console or a test save with ≥ 300 SP),
HERO and ASCENDED gates reachable, an Enchanting table nearby, filled grand soul gems, and one
follower. Test targets: a hostile group of 4+ (bandit camp), one high-resist target (a dragon
priest or an actor with 85 % shock/magic resist, set with `ModAV`).

| # | Check | How it is measured | Pass |
|---|---|---|---|
| E1 | The eight nodes in the ENCHANT band | Open the skill tree; `ui_read` the tree; photo | Eight nodes, keys 24–31, prices and HERO/ASCENDED gates as in `SkillTree.cpp`; band header uncovered; no link crosses a name; readable at 1280x720 |
| E2 | Buying teaches exactly that one | Buy each node; open the Enchanting table, `ui_lists` the enchantment list | The bought one appears; after Arcane Omniscience alone none of the eight appear |
| E3 | Old save without the nodes | Load a save made before any node was bought (fresh start after this build) | None of the eight at the table; self-test PASS |
| E4 | Each look | Enchant one weapon per weapon effect and one item per armor effect; hit / wear; photos into the Teststand artifact | Divine Judgment gold, Stormchain violet, Soul Rend dark red, Breach cyan glitch (animates and fades within ~1 s, not frozen), Bulwark blue, Thorncrown red, Phoenix embers, Voidstep black-violet at the feet |
| E5 | Stormchain targets | Hit one of 4+ hostiles with the player; `game_events` for the jump spell; then give the follower a Stormchain weapon | Up to 3 jumps to hostiles within ~800 units; never the follower, never a neutral; the follower's chain never hits the player |
| E5b | Stormchain on a killing blow | Kill a target with one Stormchain hit | The chain still jumps (No Death Dispel on the carrier) |
| E6 | Breach ignores armor and resists | Same Breach weapon against a low-armor and a high-armor/high-resist target; read Health before/after | The Breach part of the damage is the same against both |
| E6b | Breach kill credit | Kill a quest-counted target with the Breach part; read the System quest tally | Counts for the attacker exactly once; note whether the cyan art plays on the corpse |
| E7 | Phoenix rescue | Wear Phoenix Heart; drop below 25 % by a hit | One heal plus ember flash; a second drop within 60 s does not rescue; after 60 s of unpaused play it rescues again |
| E7b | Phoenix without a hit | Drop below 25 % by fall damage or a burn/poison effect | Rescue within about 1 s |
| E7c | Phoenix flash visible | Photo during the rescue | The flash reads over the constant ember aura |
| P1 | Cooldown survives reload | Rescue, save at once, quit to main menu via pause menu, reload | Log `Co-save: … version 17`; no rescue until the 60 s are up. A save from before this build loads with `version 16` and no cooldown |
| E8 | Thorncrown, Voidstep | Melee the wearer; read attacker Health. Voidstep: `SpeedMult`, stamina rate, jump from height | Reflects a share below 100 %; faster movement; cheaper stamina; no fall damage. Whatever the ESP does not achieve becomes a plugin follow-up task with its own test |
| E9 | Empty weapon | Drain a Stormchain and a Breach weapon to 0 charge; hit | No chain, no breach, no art |
| E10 | Self-test | Log after load | `[SELFTEST] isekai.enchantments PASS` |
| E11 | Followers and NPCs | Give the follower Bulwark and Phoenix Heart; let them take damage | Effects apply to them as to the player |

Known open points the run should settle: whether `DoDamage` skips armor (E6); whether `IdleOnLoad`
animates the Breach art under `ApplyArtObject` (E4); `IsHostileToActor` with the player as attacker
(E5); the Phoenix flash reusing the aura shader (E7c).
