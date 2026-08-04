# UI art prompts

Everything the interface still borrows or renders as bare text. Shop card art lives in
`SHOP_ICON_PROMPTS.md`; this covers the rest of the UI plus the mod page.

Drop finished PNGs in `icons/` (flat — `build.bat` and both package scripts copy
`icons\*.png` **non-recursively**, so a subfolder ships nowhere). A missing file is never
fatal: the built-in UI draws no icon and the web view hides it, so these can arrive one at
a time.

## Shared style block

Prepend this to every icon prompt below, then append that icon's subject line.

```
Game UI icon, single centred subject on a fully transparent background, 512x512, square.
Painted digital illustration in a cold high-fantasy sci-fi register: deep navy and black
base, luminous cyan as the single accent light, sharp angular geometry, faint holographic
glow. Reads clearly at small size - bold silhouette, few internal details, strong contrast
against a dark panel. Key light from the top left. Subject occupies the central 80% with
clear margins. No text, no numbers, no watermark, no border, no drop shadow on the
background. Subject:
```

> **Why cyan and angular:** the whole interface is cold cyan light on deep navy with hard
> corner brackets. Icons in a warmer or rounder style read as borrowed — which is exactly
> the problem this set exists to fix.

> **The small-size rule is the real constraint.** These are drawn at 26-70px in game. An
> intricate illustration turns to mush; a bold silhouette survives. When a prompt fights
> that, favour the silhouette.

---

## 1. Status panel buttons (3)

The most-seen icons in the mod — the row down the right of the status panel, every time it
opens. They currently borrow arbitrary spell art.

| File | Button | Subject line |
|---|---|---|
| `ui_skilltree.png` | Skill Tree | `a branching constellation of linked nodes forming a stylised tree, glowing cyan lines between angular crystal points` |
| `ui_storage.png` | Storage | `an open dimensional portal shaped like a chest mouth, cyan void inside with faint geometric depth, angular frame` |
| `ui_shop.png` | Shop | `a floating merchant scale over a hexagonal cyan sigil, one pan holding a coin, the other a crystal` |

> Once these three land I swap the paths in `Progression.cpp` (they point at
> `spells_38/03/04_frame.png` today) and in `Overlay.cpp`'s texture preload.

## 2. Blessing choice (7)

The reincarnation prompt — the one decision a character makes once. Today it is plain text
buttons; **the code already points at these file names**, so dropping the files in is
enough.

These want to read as a *ladder* — Normal to Hero to Ascended should escalate visibly, and
the three modifiers should look like modifiers rather than tiers.

| File | Choice | Subject line |
|---|---|---|
| `blessing_normal.png` | NORMAL | `a single small cyan spark held in an open palm, plain and unadorned, the humblest possible offering` |
| `blessing_hero.png` | HERO | `a burning cyan star above a raised sword silhouette, radiant, confident` |
| `blessing_ascended.png` | ASCENDED | `a crowned figure silhouette dissolving upward into a pillar of white-cyan light, overwhelming radiance` |
| `blessing_full.png` | FULL AWAKENING | `an intact cyan crystal sphere at full brilliance, whole and sealed` |
| `blessing_shattered.png` | SHATTERED | `the same crystal sphere broken into drifting shards, light leaking from the cracks, still burning` |
| `blessing_dormant.png` | DORMANT | `a cyan seed crystal asleep inside dark stone, faint pulse of light within, not yet woken` |
| `blessing_custom.png` | CUSTOM | `three separate cyan dials or sliders on an angular panel, each set to a different height` |

> `blessing_full` and `blessing_shattered` are a **pair** — same sphere, one whole, one
> broken. Generate them together so they match, or the choice reads as two unrelated
> things rather than one thing in two states.

## 3. System Rank insignia (6)

Replaces the plain `RANK S` text badge in the status panel. **Drawn at roughly 24px**, so
these are the strictest of the set: an emblem, not an illustration.

| File | Rank | Subject line |
|---|---|---|
| `rank_e.png` | E | `a single plain angular chevron, dim slate grey, no glow` |
| `rank_d.png` | D | `two stacked angular chevrons, cool grey with a faint cyan edge` |
| `rank_c.png` | C | `three stacked chevrons inside a thin hexagonal frame, cyan` |
| `rank_b.png` | B | `a hexagonal emblem with a bright cyan core and two wing marks` |
| `rank_a.png` | A | `an ornate hexagonal emblem, radiant cyan, small crown notch at the top` |
| `rank_s.png` | S | `a golden-white emblem blazing with light, laurel or wing flourishes, unmistakably the top of the ladder` |

> Keep E deliberately dull and S deliberately blinding. The whole point is that the badge
> tells you at a glance how far along you are — a set of six similarly pretty emblems
> conveys nothing.

> These need a code change from me as well (the badge is text in both renderers), so send
> them and I will wire it.

---

## 4. Mod page art

Not in-game, and for a release this is the part that decides whether anyone clicks at all.

**`nexus_header.jpg`** — the banner across the top of the mod page. Wide, roughly 1920x480.
No transparency; this one is a scene, not an icon.

```
Wide cinematic key art banner. A lone armoured figure seen from behind, standing on a
Nordic mountain ridge under an aurora sky, facing a colossal translucent cyan interface
panel hanging in the air before them - glowing angular frames, faint holographic text
blocks, Solo-Leveling-style system window. Cold cyan light spills onto the snow and the
figure's armour. Skyrim's landscape language: pine, stone, distant peaks. Dramatic,
lonely, awed. Leave the left third relatively uncluttered for a title overlay. No text.
```

**`nexus_logo.png`** — square, 512x512, transparent. Used as the small tile.

```
Mod logo emblem on a transparent background, 512x512. An angular hexagonal system sigil in
luminous cyan over deep navy, with a subtle upward arrow or ascension motif inside it.
Sharp geometry, holographic glow, no text. Reads at small size.
```

**`nexus_key_01.jpg` … ** — optional supporting art, same 1920x1080 scene language as the
banner. Two or three is plenty. Suggested subjects:

- the moment of rebirth: a figure kneeling in snow as a cyan pillar of light descends
- the pocket dimension: an impossible cyan void holding neat rows of floating supplies
- a quiet one: the same interface panel glowing in a dark Nordic barrow, no figure

> **Screenshots beat key art for credibility.** Generated banners sell the fantasy; actual
> in-game shots of the skill tree, the shop and the status panel prove the mod does what it
> claims. Use both — art at the top, real screenshots below it.

> Nothing generated should show a **real** Skyrim UI element or imply endorsement by
> Bethesda, and the mod page should not present generated scenes as gameplay footage.
> Label them as key art if there is any doubt.

---

## When files arrive

1. Put them in `icons/` (the mod page art belongs in `docs/` or straight on Nexus — it
   must **not** go in `icons/`, which ships to every user inside the archive).
2. Run `node tools/check.mjs` — it verifies every icon a card or node names actually
   exists.
3. Tell me, and I wire whatever still needs code: the three status buttons and the rank
   badge. The blessing icons need nothing.
