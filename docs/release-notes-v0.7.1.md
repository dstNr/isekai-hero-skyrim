What was uploaded to Nexus as **0.7.1**.

A player-feedback release. Every item in it comes from one report: a tester running the
mod through a modlist reads the same boot sequence on every restart, an alternate start
with a modern-world prologue puts that sequence somewhere it does not belong, and the
threat labels danced over anything that moved.

**No save-format change.** 0.7.0 characters load untouched, and nothing here needs a new
game.

Archives: `IsekaiHero-v0.7.1.7z` (main mod) and `IsekaiHero-PrismaUI-Patch-v0.7.1.7z` (optional UI patch).

### Fixed

- **Threat labels no longer hop about over a moving enemy.** The label was anchored to the
  actor's *animated* bounds, which swell when a bandit swings, when a wolf leaps, when
  anything draws a weapon — and whose centre bobs with every stride. So the frame danced
  around the head instead of sitting over it. It is now anchored to the actor's placement
  and its own height, neither of which moves when the actor animates. What you aim at is
  picked from the same point, so that steadies too.

### Added

- **`AutoStart`** (new `[System]` section in the ini). Set it to 0 and the System does not
  bind itself when you first take control — it waits for the System hotkey and boots
  wherever you are standing when you press it.

  This is for starts that open somewhere the boot sequence does not belong: a
  modern-world prologue above all, but any dream, cell or cutscene start counts. The mod
  can tell a character-creation room from the world, but it cannot tell an intended
  prologue from a start gone wrong — you can. It is also the way back in if the automatic
  trigger ever fails to fire, since the hotkey now boots the System on a character it has
  not run for yet.

- **`TextSpeed`** (`[System]`). A multiplier on the System's typewriter: 1 is as before,
  2 is twice as fast, **0 shows the whole text at once with no typing at all**.

- **Click a panel to skip the rest of its reveal.** Works at any speed setting and in both
  renderers. The buttons stay hidden until the reveal finishes, so the skip click can
  never press one by accident.

  The two together are for the second playthrough and for anyone testing a modlist: the
  boot sequence is a moment exactly once, and reading it again on every restart is not
  one.

### Notes

- Controller support is **not** in this release. It is the other half of the same report
  and it is the next thing on the list — the built-in menus can be driven by a gamepad
  with reasonable effort, the PrismaUI patch is a separate problem.
- If you already have an `IsekaiHero.ini`, your mod manager will ask whether to overwrite
  it. Keeping yours is safe — the two new settings simply use their defaults, which is
  exactly 0.7.0's behaviour. Take the new file (or add a `[System]` section by hand) if
  you want them.
