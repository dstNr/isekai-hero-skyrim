# A pipeline for System-granted weapons

Prove, on exactly one weapon, that this project can take a concept image and end up
with a usable Skyrim weapon in the System Shop. The weapon is the deliverable; the
chain that produced it is the point.

## Goals

- One one-handed short sword, in game, drawable, droppable, correctly sized.
- Every stage documented well enough that the second weapon costs a fraction of the
  first.
- No new engine work. Handing out a weapon is the call the shop already makes
  (`Plugin::LookupOurForm<RE::TESBoundObject>`); the only C++ change is a catalog row
  and the shelf to put it on.

## Non-goals

- **No armour in this pass.** Armour is intended and will follow; it is not in scope
  here. It needs Skyrim's body mesh and skeleton to be fitted, weighted, split into
  dismemberment partitions and given `_0`/`_1` weight variants. The original reason for
  deferring it — that none of those files could reach a remote Blender worker — no longer
  holds: the chain now runs in local Blender, where PyNifly imports Skyrim's body meshes
  and skeletons directly. What still separates armour from a weapon is the work itself.
  Skinning and weight painting are judgement rather than script, and no amount of tooling
  access changes that; Outfit Studio remains the likely route. Armour is therefore closer
  than this spec first assumed, and still out of scope here. What this weapon establishes is how much of the *rest* of the chain (records, packaging,
  shop integration, the path checks) carries over unchanged, which is most of it.
- No custom magic effect or enchantment record. The weapon may carry a vanilla
  enchantment; a bespoke one is a separate piece of work.
- No world placement, no levelled lists, no crafting recipe.
- No second weapon until this one is in game.

## Why a one-handed short sword

A one-handed weapon is the cheapest class to prove the chain with: a single mesh, a common
animation type, no skeleton, no body fitting, no weight morphs, no dismemberment
partitions. Everything that makes armour hard is absent, so a failure here is a failure of
the *pipeline* rather than of the asset class.

**A short sword rather than a full-length one, because of what the mesh measures.** Concept
art draws heroic proportions and image-to-3D reproduces them faithfully. The generated
blade is **68% blade to 32% grip**, and that sits between the two obvious classes: a
one-handed sword wants roughly 78/22, a dagger roughly 63/37.

The class is therefore not a property of the mesh — it is decided by the length the mesh is
scaled to, and the ratio determines which lengths look right:

| scaled to | blade | grip | reads as |
|---|---|---|---|
| 0.38 m | 26 cm | 12 cm | dagger, cleanly proportioned |
| **0.70 m** | **48 cm** | **22 cm** | **short sword — a generous grip, but sound** |
| 0.97 m | 66 cm | 31 cm | hand-and-a-half; visibly wrong for one hand |

**0.70 m** is the choice: it keeps the sword class and its ordinary one-handed animations,
and the long grip reads as a stylistic flourish rather than an error. At full sword length
the same ratio becomes an obvious mistake.

## Division of labour

| Stage | Tool | Owner |
|---|---|---|
| Concept images | the user's own AI image workflow | **user** |
| Geometry, UVs, PBR maps | Meshy image-to-3D, Pro plan | **user** |
| Import, scale, orientation | local Blender 5.2, `bpy` over the Blender MCP | Claude |
| NIF export | PyNifly 28.2, from the same session | Claude |
| Collision | NifSkope, copied from a vanilla sword | **user** |
| Textures → DDS | texconv or Paint.NET | **user** |
| WEAP record | Creation Kit, from a written guide | **user** |
| Shop entry, checks, packaging | C++ and `tools/` | Claude |

This split was originally drawn around a remote Blender worker that cannot fetch external
files, which put NIF export on the user's side. That boundary is gone: Blender 5.2 is
installed locally, the official Blender MCP drives it, and PyNifly 28.2 supports 5.2, so
geometry, UVs and NIF export are one continuous scripted step with no handover in the
middle. A GLB is still produced, but as an intermediate rather than a delivery.

What remains with the user is what genuinely needs a human at a GUI: approving the
silhouette against the concept, the NifSkope collision work, and the Creation Kit.

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

Generated from the concept image with **Meshy** (image-to-3D), on a Pro plan. Local
Blender then becomes the conversion and verification stage rather than the modelling one.

An earlier version of this spec rejected image-to-3D on topology grounds. That objection
does not survive contact with the actual constraint. Topology matters enormously for
armour, which is skinned to a skeleton and deforms; a weapon is a rigid prop that is never
deformed, so it needs only valid geometry, sane density and a UV map. Meshy supplies all
three directly: **target polygon count**, **UV unwrapping**, and **PBR maps**.

The scripted route was built and does work — it produced a 1784-triangle sword with a
correct fuller, swept quillons and a faceted pommel, and it remains the fallback if a
generated mesh has unusable proportions. Its `bpy` code is in the git history rather than
repeated here. Its ceiling is the reason it is not the primary route: scripted geometry is
good at geometric, faceted forms and poor at the organic ornament that concept art tends
to carry.

**Density target: about 8,000 triangles.** Counted rather than quoted: the vanilla iron
sword's visible blade mesh is **425 triangles**, and the whole file is 795 including its
scabbard and blood-effect meshes. The iron dagger's blade is 416. Earlier drafts of this
spec said 1,000–2,000, which was too high. That is a 2011 console budget either way, not a
design goal — modern weapon mods run 5,000–20,000. A
weapon is the cheapest place in the game to spend triangles: one or two are on screen at a
time, and they are held closest to the camera. The scripted blade's 12-sided grip and
8-sided pommel were visibly faceted at that range, which is what the higher target fixes.

**Every mesh must still carry a UV map.** PyNifly reads `uv_layers.active.data`
unconditionally and raises `AttributeError: 'NoneType' object has no attribute 'data'`
when there is none. Meshy's unwrap satisfies this; it is asserted after import rather than
assumed, because the failure is otherwise reported only as "see console window for
details".

## Scale and orientation, the silent failures

Neither of these produces an error. Both produce a weapon that is obviously wrong the
first time it is equipped, and both are cheap to get right if they are decided up front.

**Scale.** Blender works in metres; Skyrim uses its own units at roughly **70 units per
metre** (a human is about 128 units tall). The short sword is **0.70 m** overall, so about
**49 units**. That target is confirmed against the vanilla `longsword.nif` rather than taken
from this figure alone.

**A generated mesh does not arrive at metre scale**, so the factor is not 70. It is derived
from the intended real-world length: the current generation measures **1.9098** in its own
units, so 49 Skyrim units needs a factor of about **25.6**. Measure first, then divide —
never assume the source is in metres.

**PyNifly does not apply that conversion.** It writes Blender units into the NIF one for
one, and `blender_xf` does not change this — that flag governs bone orientation and
armature scale, not asset scale. Measured, not assumed: an early export of a 0.857 m blade
produced a NIF measuring **0.86 units**, which beside a 128-unit human is about 1.2 cm, and
PyNifly reported "Export successful".

So the factor is applied explicitly as the last operation before export, and checked by
reading the vertex extents back out of the written file rather than by trusting the
exporter. The rule is the same whatever the class: scale explicitly, then read the written
file back and check the number against the intended length.

**Orientation — measured, no longer assumed.** Three vanilla weapons were extracted from
`Skyrim - Meshes1.bsa` and read directly:

| | axis | length | Y range | blade / hilt |
|---|---|---|---|---|
| iron sword (`longsword.nif`) | **Y** | 77.33 u | −13.17 … +64.16 | 83 / 17 |
| iron dagger (`irondagger.nif`) | **Y** | 35.38 u | −11.68 … +23.70 | 67 / 33 |

The blade runs along **+Y**, the pommel sits at negative Y, and the origin falls inside the
grip. That is exactly the convention this pipeline already targets, so the conversion step
is a rotation from the generated mesh's **Z** onto **Y** and nothing more.

**The vanilla iron sword's mesh is `meshes\weapons\iron\longsword.nif`.** There is no
`ironsword.nif`; the only files by that name are the broken-sword clutter props
`ironswordbottombroken.nif` and `ironswordtopbroken.nif`.

## Textures: Meshy's PBR is the source, not the shipped form

Skyrim SE's standard shader is not PBR. Meshy's maps carry more information than the game
can consume directly, so they are converted rather than passed through:

| Meshy produces | Skyrim SE consumes |
|---|---|
| Base colour | diffuse, `systemblade.dds` |
| Normal | normal RGB, **with specular in the alpha channel** |
| Roughness | inverted into that alpha |
| Metallic | selects the environment-map shader and its cubemap |

This conversion is the real work of the texture stage. Skipped, the blade reads matte and
lifeless, with no error anywhere.

**Check that a normal map actually came through.** Meshy's first remesh download shipped a
base colour and a packed metallic/roughness map only, with the Principled node's Normal
input unconnected. Re-running the texturing stage produced a full set — base colour, ORM
and a 4096² normal — on identical geometry. So a missing normal map is a property of one
texturing run rather than of the remesh, and the fix is to texture again, not to work
around it.

This is worth checking every time, because Skyrim's shader expects a `_n.dds` and takes its
specular mask from that file's alpha. Without one the blade is uniformly matte, with no
error to say so.

**Meshy's base colour arrives oversized.** The accepted generation ships an 8192² base
colour, more than most whole-body textures in a load order. A weapon wants 2048² or 4096²;
`texconv` resizes during the DDS conversion.

**The normal map cannot be BC5.** BC5 stores two channels and has no alpha, so it cannot
carry the specular mask that gives a metal blade its shine. Use BC7. (An earlier draft of
the plan specified BC5, copied from the general advice for normal maps without checking it
against Skyrim's shader.)

### A PBR set is planned as an optional component

Community Shaders' True PBR is common in modern load orders, and Meshy's output is already
PBR. Shipping it would be a real gain for the users who have it — but requiring Community
Shaders for a reward weapon would not be acceptable, so it is not an either/or.

The pattern is one this mod already runs: a base that works for everyone plus an optional
component detected or selected at install time, exactly as the ImGui overlay and the
PrismaUI view are shipped together in one FOMOD.

**Not on the first weapon.** The mod currently ships no meshes and no textures at all, so
the entire asset chain is unproven; two texture sets on the first run would make a failure
ambiguous. What matters now is only the consequence for storage: **Meshy's original PBR
maps must be kept**, because the conversion to the vanilla shader is lossy and generative
output is not reproducible. Where they are kept — in the repository, outside it, or in Git
LFS — is an open decision, and the repository is already 32 MB with icons accounting for
28.6 MB of that.

## Licensing and the backup obligation

The assets are generated under a **Meshy Pro** plan, which matters because the terms treat
the tiers differently. Free-plan output is owned by Meshy and licensed back under CC BY
4.0 — redistributable, but only with credit. That assignment is written to apply to the
free plan alone; paid customers instead *grant Meshy* a licence in their User Content,
which is defined to include generated output. Granting a licence presupposes holding the
rights, so on a paid plan the rights stay with the author and redistribution in a public
mod is the author's to make.

The terms never use the word "own" for paid customers, so that reading rests on the
structure rather than on an explicit sentence.

**Settled by taking the stricter branch:** the assets are credited and licensed as CC BY
4.0, the licence the terms attach to free-plan output. Meshy is named in the README and
carved out of the MIT grant in `LICENSE`, exactly as the sound effects are. This is correct
whichever plan produced a given generation — under free it is required, under a paid plan
it costs nothing and removes the ambiguity. The practical effect matches the sounds: a fork
may keep the files, and may not drop the credit.

Two operational consequences, both stated in the terms rather than inferred:

- **Meshy may delete generated output**, including from inactive accounts, and says
  explicitly that backing up anything worth keeping is the customer's responsibility. The
  local archive of source meshes and maps is therefore required, not prudent.
- **Nothing may be published to the Meshy Community page**, which dedicates the output
  under CC0 irrevocably.

The `LICENSE` SCOPE section lists what the MIT grant covers. Meshes and textures need a
line there either way, and whether a fork may reuse them is the author's decision, as it
was for the sounds and the icons.

## What a vanilla weapon file contains that ours does not

Reading `longsword.nif` turned up three node groups that the generated mesh has no
equivalent for:

- **`Scb` — the scabbard.** This is what renders on the character when the weapon is
  sheathed. A weapon shipped without it has nothing to show when put away.
- **`BloodLighting` and `BloodEffects`** — the overlay meshes Skyrim uses to make a weapon
  look bloodied. Cosmetic, and the weapon works without them.
- **A separate first-person mesh.** `1stpersonlongsword.nif` exists alongside the world
  model and is denser — 1,019 triangles for the blade against 425. A WEAP record carries a
  first-person model field pointing at its own record, so a duplicated vanilla sword keeps
  pointing at the **vanilla** first-person mesh unless that is changed too. Left alone, the
  weapon would look correct in third person and be an iron sword in first person.

None of this is hard, but none of it was in the plan, and the first two are invisible
failures of exactly the kind this project keeps finding: the weapon works, looks right in
the hand, and is wrong the moment it is sheathed.

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

Whether the exported NIF needs a collision shape built by hand or whether one
copied from a vanilla sword in NifSkope is sufficient. The vanilla iron sword's collision
is not a simple box: it is a `bhkCollisionObject` → `bhkRigidBody` → **`bhkListShape`**
holding several `bhkConvexTransformShape` children. Copying the whole branch is still the
approach, but it is more than one primitive to refit. Copying is the standard practice
and is assumed here; if it turns out that a copied `bhkCollisionObject` does not fit the
new blade's proportions, the fallback is generating a simple convex shape, which is more
work but well-trodden. This does not change the design, only the effort in one stage.
