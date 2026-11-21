# System Voice — test list

For one collected run in the SotN modlist, through the test bridge. Build under test: the
`feat/system-voice` head that the final review approved. The deploy into the modlist goes
through the SotN project lead. Playback is measured with `game_audio` / `game_audio_level`
(a voice line shows as a playing sound in category System Voice, `0xDCB`, with an
`fx\isekai\voice\` file);
an ear check is only needed for V9.

Setup: a fresh character (for V1 binding and blessing) and a `SotNTest_` save of a bound
character with System Points, threat labels on, one follower, a bandit camp nearby. MCM
System voice on unless a point says otherwise. Game System Voice, Voice and Effects sliders
at default.

| # | Check | How it is measured | Pass |
|---|---|---|---|
| V1 | Every moment speaks once | Trigger each: binding (new game), each blessing (NORMAL, HERO, ASCENDED, DORMANT, CUSTOM over separate starts or reboots), level up (`player.advlevel`), milestone and quest done (`isekai` / `IsekaiHeroCraft.DebugGrantNext()`), new System quest (automatic roll), rank rise, Dormant awakening, reboot, guild promotion (`AG_RankChanged`), lost co-save (load a save with its `.skse` removed in a copy) | Exactly one `IsekaiVOX_<moment>` play per trigger, the file from that moment's set; nothing before binding (a level up with AutoStart 0 before reincarnation is silent) |
| V2 | Level up that awakens DORMANT | DORMANT character at the awakening threshold, `player.advlevel` | One line: the awakening; no level-up line on top of it |
| V3 | Reload with a higher rank | Save at rank C, raise to B, save; load the C save, then load the B save from the main menu | No rank line on either load; a real rank rise afterwards speaks |
| V4 | Three LETHAL enemies | Spawn three enemies far above the player's level so the labels read LETHAL | One lethal line; none for the next 180 s; a new LETHAL enemy after 180 s speaks |
| V5 | MCM switch | Turn System voice off, trigger a level up; turn it on, trigger again | Off: no play; on: one play |
| V6 | Loudness follows the System Voice slider | Same line with the System Voice slider (Settings → Audio) at 100 % and at 30 %, then the Voice slider at 30 %, then the Effects slider at 30 % | `game_audio_level` drops with System Voice, not with Voice or Effects |
| V7 | Self-test | Log after load | `System voice descriptors 20/20 resolved` |
| V8 | Silent places | Open the System window, buy in the shop, buy a skill-tree node | No voice play |
| V9 | Queueing in menus | Reincarnation, then pick NORMAL within 2 s; reboot, then pick a blessing within 2 s | The first line finishes, then the blessing line follows; no overlap and no cut |
| V10 | Load drops pending lines | Trigger a quest done (900 ms delay) and quick-load within the delay | Nothing from the old session plays in the loaded save |
| V11 | Lines keep playing under a System window | Blessing, dormant awakening, milestone, reincarnation, reboot: watch `game_audio` / `game_audio_level` while the window stays open, plus one ear check | The line plays through to its end while the System window is open; it does not wait for the window to close |

Watch items: loudness next to normal dialogue (static attenuation 10.57 dB copied from the
existing System sound; too quiet → plugin follow-up); `quest_done_04` may start with a
clipped first syllable (listen once); a LETHAL warning dropped by the 4 s gap is lost for
that pack (known, deferred).

Known open points the run settles: whether the engine stops a playing voice line on load
(V10); whether the lines keep playing under an open System window now that their category
is outside the menu pause (V11); how the modlist's AudibleNPCDialogues override of the 2D
dialogue output model affects the lines (V6; the Voice category overrides no longer apply).
(V9's open point, whether a menu-paused line counts as playing, is gone with the own
category.)
