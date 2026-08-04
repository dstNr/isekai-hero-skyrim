# Shop icon generation prompts

> Shop card art only. The rest of the interface — the status panel buttons, the blessing
> choice, the rank badge — plus the mod page art is in
> [UI_ART_PROMPTS.md](UI_ART_PROMPTS.md).

The System Shop's eight cards each want an icon. The plugin already references them by
file name — generate the PNGs, drop them in `icons/`, and both renderers pick them up
with no code change (the build and package scripts copy the whole folder).

If a file is missing, the card simply renders without an image — nothing breaks, so
these can be filled in one at a time.

## Target files

| File name | Card | What it shows |
|---|---|---|
| `shop_smithing_small.png` | Smithing Materials | a few metal ingots |
| `shop_smithing_large.png` | Smithing Crate | a crate overflowing with ingots |
| `shop_alchemy_small.png` | Alchemy Ingredients | a few herbs / a small vial |
| `shop_alchemy_large.png` | Alchemy Crate | a crate of herbs and vials |
| `shop_souls_small.png` | Soul Gems | a few glowing soul gems |
| `shop_souls_large.png` | Soul Gem Crate | a crate of glowing soul gems |
| `shop_gold_small.png` | Gold | a small stack of septims |
| `shop_gold_large.png` | Gold Hoard | a large pile / chest of septims |

## Requirements (apply to all eight)

- **512 x 512 px, PNG, transparent background.**
- The subject fills roughly the middle 80%, centred, with breathing room at the edges —
  the UI draws them inside a bordered tile and crops nothing.
- No text, no numbers, no lettering of any kind.
- No drop shadow onto the background (it would show as a grey smear on the dark tile).
- Consistent light direction across all eight (top-left key light).
- The pairs must read as **the same item at two scales** — same palette, same rendering,
  the "Crate" version simply being the bigger haul.

## Style prompt (prepend to every one)

Paste this block, then append the per-icon line below it:

```
Painted fantasy game inventory icon, single centred object on a fully transparent
background, 512x512, square composition. Rich painterly digital illustration in the
style of high-fantasy RPG item art: warm rim lighting, deep shadows, visible brush
texture, slightly desaturated earthy palette with one glowing accent colour. Key light
from the top left. Subject occupies the central 80% of the frame with clear margins.
No text, no numbers, no watermark, no border, no drop shadow on the background.
Subject:
```

## Per-icon subject lines

**`shop_smithing_small.png`**
```
three stacked metal ingots — iron, steel and a golden one — resting together, cool
grey and warm gold metal with hammered facets and faint forge-glow on the edges.
```

**`shop_smithing_large.png`**
```
an open wooden crate packed with metal ingots spilling over the rim, iron and steel bars
with a few gold and ebony ones catching the light, iron-banded planks, faint forge-glow.
```

**`shop_alchemy_small.png`**
```
a small bundle of alchemical herbs — dried leaves, a blue mountain flower, a pale
mushroom — tied with twine beside a corked glass vial of green liquid.
```

**`shop_alchemy_large.png`**
```
an open wooden crate filled with alchemical supplies: bundles of dried herbs, mushrooms,
and several corked glass vials of coloured liquids, straw packing, iron-banded planks.
```

**`shop_souls_small.png`**
```
three cut crystal soul gems clustered together, deep violet and cyan, glowing from
within with swirling captured light, faceted surfaces refracting the glow.
```

**`shop_souls_large.png`**
```
an open wooden crate filled with glowing cut soul gems in violet and cyan, their inner
light spilling out and lighting the iron-banded planks from inside.
```

**`shop_gold_small.png`**
```
a small neat stack of golden septim coins, a few loose ones leaning against the pile,
warm reflective gold with worn edges.
```

**`shop_gold_large.png`**
```
an overflowing heap of golden septim coins spilling from a tipped iron-bound chest,
warm reflective gold, a few gemstones mixed into the pile.
```

## Potion icons (Part H)

Ten more, for the System potions. Same requirements and same style block as above — only
the subject line changes. These cards stay hidden until their ESP record exists, so the
icons can wait until after the Creation Kit work.

| File name | Card | Subject line |
|---|---|---|
| `shop_potion_vigor.png` | Restorative: Vigor | `a round glass flask of glowing crimson liquid, cork stopper, warm red inner light` |
| `shop_potion_focus.png` | Restorative: Focus | `a slender glass vial of luminous deep-blue liquid, cork stopper, cool blue inner glow` |
| `shop_potion_vitality.png` | Restorative: Vitality | `a squat glass bottle of glowing emerald-green liquid, cork stopper, soft green inner light` |
| `shop_potion_panacea.png` | Panacea | `a clear crystal vial of pale luminous liquid with drifting golden motes, silver filigree collar` |
| `shop_elixir_system.png` | Elixir of the System | `an ornate hexagonal glass decanter of swirling cyan and white light, faceted stopper, radiant` |
| `shop_elixir_ascended.png` | Draught of the Ascended | `a heavy iron-banded flask of molten orange liquid, embers rising inside, brutal and warlike` |
| `shop_elixir_aegis.png` | Aegis Elixir | `a shield-shaped glass flask of pale silver-blue liquid, faint hexagonal barrier shimmer around it` |
| `shop_elixir_phantom.png` | Phantom Draught | `a smoky dark-violet vial, its contents half-transparent and wisping away at the edges` |
| `shop_elixir_casting.png` | Elixir of Endless Casting | `a tall spiral glass flask of violently glowing purple liquid, arcane sparks arcing off it` |
| `shop_elixir_titan.png` | Titan's Draught | `a massive stone-and-bronze flagon of thick amber liquid, oversized and heavy` |

Keep the four **Restoratives** visually plainer than the six **Elixirs** — they are the
cheap spammable ones, and the elixirs should read as the prize.

## After generating

1. Save all eight into `icons/` with exactly the file names above.
2. Run `./build.bat` (deploys the icon folder) or `./package.ps1` for an archive.
3. Check them in the playground: `node playground/serve.mjs`, then the **Shop** button.

If you want different names or a different split of goods, the single place to change is
`kCatalog` in [src/Shop.cpp](../src/Shop.cpp) — the renderers read it, they do not
hardcode anything.
