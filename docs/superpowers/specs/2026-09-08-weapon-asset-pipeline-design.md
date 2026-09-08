# A pipeline for System-granted weapons

Prove, on exactly one weapon, that this project can take a concept image and end up
with a usable Skyrim weapon in the System Shop. The weapon is the deliverable; the
chain that produced it is the point.

## Goals

- One one-handed sword, in game, drawable, droppable, correctly sized.
- Every stage documented well enough that the second weapon costs a fraction of the
  first.
- No new runtime code. Handing out a weapon is the call the shop already makes.

## Non-goals

- **No armour in this pass.** Armour is intended and will follow; it is not in scope
  here. It needs Skyrim's body mesh and skeleton to be fitted, weighted, split into
  dismemberment partitions and given `_0`/`_1` weight variants, and none of those
  files can reach the remote Blender worker. That makes it a different project with a
  different toolchain — Outfit Studio, locally, with the user doing the geometry. What
  this weapon establishes is how much of the *rest* of the chain (records, packaging,
  shop integration, the path checks) carries over unchanged, which is most of it.
- No custom magic effect or enchantment record. The weapon may carry a vanilla
  enchantment; a bespoke one is a separate piece of work.
- No world placement, no levelled lists, no crafting recipe.
- No second weapon until this one is in game.

## Why a one-handed sword

It is the cheapest weapon class to prove the chain with: a single mesh, a common
animation type, no skeleton, no body fitting, no weight morphs, no dismemberment
partitions. Everything that makes armour hard is absent, so a failure here is a
failure of the *pipeline* rather than of the asset class.

## Division of labour

| Stage | Tool | Owner |
|---|---|---|
| Concept images | the user's own AI image workflow | **user** |
| Geometry | remote Blender 5.2 worker, `bpy`, with render feedback | Claude |
| Handover | GLB | Claude |
| GLB → NIF | Blender + PyNifly, locally | **user** |
| Textures → DDS | texconv or Paint.NET | **user** |
| WEAP record | Creation Kit, from a written guide | **user** |
| Shop entry, checks, packaging | C++ and `tools/` | Claude |

The two stages that sit with the user are there for one reason: they need Skyrim's own
files and format plugins, and the remote worker is explicitly forbidden from fetching
external files. This is a boundary of the tooling, not a division of skill.

## What the concept images must show

The model is built from these, so their form decides the result's quality.

**Required — one image:**
- The blade **in profile, side on**, whole weapon in frame, nothing cropped.
- As close to orthographic as the generator will give: minimal perspective, the tip no
  smaller than the hilt.
- A plain, flat background. The silhouette has to read as a shape.
- Even lighting. Dramatic rim light and bloom hide the form that has to be rebuilt.

**Helpful — a second image:**
- The same weapon **edge on**, or the guard and pommel **from the front**. This is what
  fixes the cross-section, which a single profile view leaves ambiguous.

**Actively unhelpful:** three-quarter hero shots, motion blur, a hand holding it,
sparks and particles, a background that competes with the silhouette.

**Format:** PNG, at least 1024 px on the long edge. These are read for proportions and
edge detail, not displayed, so a small image costs accuracy in the model.

A geometric, faceted, artificial design — which suits a System-granted weapon — is
substantially easier to build than an organic, flowing one, and will look closer to its
concept. Worth knowing while generating.

## Geometry

Modelled directly in the Blender worker with `bpy`, iterating against rendered views.

**Not** generated with `generate_3d` and cleaned up. Image-to-3D output carries topology
unsuited to a game asset, and repairing it costs more than building clean geometry for a
shape this simple. `generate_3d` stays available as a fallback if a concept turns out too
organic to script, and `multi_image_to_3d` would then be the variant to use, since the
user is supplying more than one view.

Delivered as GLB, which is the worker's portable export.

## Scale and orientation, the silent failures

Neither of these produces an error. Both produce a weapon that is obviously wrong the
first time it is equipped, and both are cheap to get right if they are decided up front.

**Scale.** Blender works in metres; Skyrim uses its own units at roughly **70 units per
metre** (a human is about 128 units tall). A one-handed sword is **0.75 m to 0.9 m**
overall. The model is built at metre scale and the conversion applies the factor — so
the number that matters is the length in metres, stated above, and it is checked against
a vanilla sword in NifSkope before the record is made.

**Orientation.** The blade must run along the axis Skyrim expects, with the correct
handedness, or it sits sideways in the hand. The reference is a vanilla one-handed sword
NIF opened beside it; matching that is the check.

## The record goes through the Creation Kit

A WEAP record could be written into the ESP programmatically — this project has already
parsed and edited its own plugin at byte level. For the *first* weapon that would still
be the wrong call. A WEAP carries a dozen subrecords, and a wrong field does not raise an
error: it produces a weapon that deals no damage, or will not fit an animation, or is
silent. The Creation Kit guarantees a valid record.

Once the chain works and more weapons follow, automating the record is a sensible second
pass — with a known-good record to diff against, which is exactly what is missing now.

The guide follows the pattern of `docs/CREATION_KIT_ESP.md`, which has worked for this
project before.

## What changes in the repository

The mod ships **no meshes and no textures today** — there is no `meshes/` or `textures/`
directory. Both are new:

- `meshes/isekai/` and `textures/isekai/` in the repo, mirroring the `Data\` layout.
- `package.ps1` stages both.
- The FOMOD installs both as required files, beside the ESP and the DLL.
- `build.bat` does not need them: it deploys the plugin for development, and asset work
  is verified through the packaged archive in MO2, which is how this mod is tested
  anyway.

### The privacy gate already covers this

Exported NIF and DDS files sometimes carry their source path in a header —
`C:\Users\<name>\...`. `package.ps1`'s gate walks the entire staged tree, not just the
DLL, so it catches this before an upload does. This is not luck: walking the whole tree
is the reason the gate was widened after v0.5.0 shipped a 130 MB `.pdb`.

No change is needed. It is recorded here because a new asset type is exactly the case
the gate exists for, and because a future contributor might otherwise narrow it.

## Verification

There is nothing here that `check.mjs` can decide from source, with one exception worth
adding: **every mesh and texture path named in the ESP must exist in the repository.** A
weapon whose `MODL` points at a file that was never committed installs cleanly and then
shows an invisible weapon in game. That is the same class of failure the existing icon
check guards against, and it belongs in the same harness.

Everything else is in-game verification, and the pipeline is proven only when all of it
passes:

1. The weapon appears in the System Shop and can be bought.
2. Equipped, it sits in the hand at a believable size beside a vanilla sword.
3. It draws and sheathes with the one-handed animation.
4. Dropped, it lands on the ground rather than falling through it — this is the check
   that the collision survived the conversion.
5. Picked up again, it returns to the inventory with its name and stats intact.

## Open question, to settle during the work

Whether the GLB → NIF conversion needs a collision shape built by hand or whether one
copied from a vanilla sword in NifSkope is sufficient. Copying is the standard practice
and is assumed here; if it turns out that a copied `bhkCollisionObject` does not fit the
new blade's proportions, the fallback is generating a simple convex shape, which is more
work but well-trodden. This does not change the design, only the effort in one stage.
