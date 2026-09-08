# Weapon Asset Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Get one one-handed short sword from a concept image into the System Shop, and leave behind a written route so the second weapon costs a fraction of the first.

**Architecture:** Meshy generates the geometry, UVs and PBR maps from the concept image. Local Blender, driven with `bpy` through the official Blender MCP, is the conversion and verification stage — orientation, the Skyrim scale factor, and the PyNifly export flags — and the mesh lands in the repository at the end of Task 1. What stays with the user is the work that needs a human at a GUI — collision in NifSkope, textures, and the WEAP record in the Creation Kit. Only after the asset is proven in game does the C++ side change: a shop entry, packaging, and a path check.

**Tech Stack:** Meshy (Pro, image-to-3D), Blender 5.2.1 LTS driven by the Blender Lab MCP server (`blender-mcp`, socket on `localhost:9876`), PyNifly 28.2, NifSkope, texconv, Creation Kit, C++23 / CommonLibSSE-NG, `tools/check.mjs`, `package.ps1`.

**Spec:** `docs/superpowers/specs/2026-09-08-weapon-asset-pipeline-design.md`

## Global Constraints

- **The short sword is 0.70 m overall**, so about **49 Skyrim units** at 70 units per metre (a human is about 128 units tall). The mesh measures 68% blade to 32% grip, which is between a sword's 78/22 and a dagger's 63/37; 0.70 m is the length at which that ratio still reads as a sword.
- **Nothing applies the scale factor for you.** PyNifly writes Blender units into the NIF one for one. A generated mesh is not at metre scale either, so the factor is derived from the intended length, not assumed to be 70: the current generation measures **1.9098** in its own units, so 49 units needs about **×25.6**. Scale last, then read the written file back and check.
- **PyNifly must be called with `intuit_defaults=False`.** Left at its default it discards every setting passed to the operator and guesses instead, silently producing a Legacy Edition NIF. `export_modifiers=True` is needed too, or bevels and mirrors are dropped.
- **Every mesh needs a UV map before export.** PyNifly raises on `uv_layers.active` being `None` and reports only "see console window for details".
- **Nothing shipped may carry the author's real name.** `package.ps1`'s gate walks the whole staged tree. Exported NIF and DDS files sometimes embed their source path — if the gate trips, fix the export, never the gate.
- **The mod is tested from the packaged archive in MO2**, not from the base-game folder. `build.bat` deploys the plugin for development only and does not carry assets.
- **The plugin is ESL-flagged**: every local FormID must be `<= 0xFFF`. Current use is 35 records, so there is room.
- **All repository artifacts in English** — code, comments, commit messages, docs.
- **Never bump the project version.** `CMakeLists.txt` VERSION and `package.ps1`'s default `-Version` stay where they are.
- **Geometry comes from Meshy** on a Pro plan, at a target of about **8,000 triangles**. Vanilla's 1,000–2,000 is a 2011 console budget, not a design goal.
- **Meshy's original mesh and PBR maps must be archived locally.** Their terms reserve the right to delete generated output and place backup responsibility on the customer; the conversion to Skyrim's shader is lossy and generative output is not reproducible.
- **Nothing goes on the Meshy Community page** — publishing there dedicates the output under CC0 irrevocably.
- **No armour in this plan.** See the spec's non-goals; armour is #34.
- **`build.bat` needs `SKYRIM_DATA` set** and refuses to deploy while Skyrim is running.

## Two kinds of task in this plan

Tasks 1 and 4–5 are Claude's: code and repository changes, with the project's real verification (`node tools/check.mjs`, `build.bat`). Task 1 opens with two user steps — the Meshy generation and its archival — because they happen outside any tool reachable from here.

Tasks 2 and 3 are the user's: NifSkope, texture work and the Creation Kit. Those steps are written as precise instructions with a stated done-condition rather than as a test-first cycle, because there is no test to run — the verification is looking at the thing. Dressing them up as red-green-refactor would be theatre.

---

## File Structure

| File | Responsibility |
|---|---|
| `meshes/isekai/weapons/systemblade.nif` *(create, Task 1; collision and texture paths in Task 2)* | The weapon mesh, mirroring the `Data\` layout. |
| `textures/isekai/weapons/systemblade.dds` *(create, Task 2)* | Diffuse texture. |
| `textures/isekai/weapons/systemblade_n.dds` *(create, Task 2)* | Normal map. |
| `docs/WEAPON_ASSET_GUIDE.md` *(create, Task 2)* | The route from concept image to a working NIF and from the NIF to a WEAP record. Written once, followed for every later weapon. |
| `plugin/IsekaiHero.esp` *(modify, Task 3)* | Gains one WEAP record, made in the Creation Kit. |
| `src/Shop.cpp` *(modify, Task 4)* | A new `Shelf::kArmaments`, its display name, and the catalog row. |
| `lang/template.txt` *(regenerated, Task 4)* | Picks up the new translation keys. |
| `package.ps1` *(modify, Task 5)* | Stages `meshes\` and `textures\`. |
| `tools/make-fomod.mjs` *(modify, Task 5)* | Installs both folders as required files. |
| `tools/check.mjs` *(modify, Task 5)* | Every mesh path the ESP names must exist in the repo. |

---

### Task 1: Generate the blade with Meshy, then convert and export it

**Files:**
- Create: `meshes/isekai/weapons/systemblade.nif`

**Interfaces:**
- Consumes: the user's concept image, and a Meshy generation made from it.
- Produces: `meshes/isekai/weapons/systemblade.nif`, Skyrim SE format (BS version 100),
  blade along the axis a vanilla sword uses, **about 49 Skyrim units** overall, origin at the
  grip. Task 2 adds collision and texture paths to the same file; Task 3's WEAP record
  names it.

Meshy supplies the geometry, the UVs and the PBR maps. Local Blender is the conversion and
verification stage — it is where scale, orientation and the export flags are enforced, and
none of that changed when the geometry stopped being scripted.

The scripted route still works and stays the fallback for a generation with unusable
proportions. Its `bpy` code is in the git history rather than duplicated here.

- [ ] **Step 1: Generate in Meshy (user)**

Image-to-3D from the concept image, on the Pro plan. Settings that matter:

- **Target polygon count: about 8,000 triangles.** Check whether the field counts polygons
  or triangles — with quads that is a factor of two. Vanilla's 1,000–2,000 is a 2011
  console budget, not a target. The accepted generation came in at **10,159**, which is
  over the target and fine; what matters is that the raw texturing stage delivers well over
  a million and must not be shipped.
- **Run the remesh, and download the remeshed result.** A download whose name ends in
  `_texture` is the raw texturing stage. The remeshed object arrives named
  `output_unwrapped`, which is the quickest way to tell the two apart after import.
- **UV unwrap: on.** The export cannot proceed without it.
- **PBR textures: on.**

Do not publish the result to the Meshy Community page — that dedicates it under CC0
irrevocably.

- [ ] **Step 2: Download and archive before anything else (user)**

Save the mesh and **every** PBR map to a local archive outside the repository.

This is an obligation, not housekeeping. Meshy's terms reserve the right to delete
generated output, including from inactive accounts, and state that backing up anything
worth keeping is the customer's responsibility. The conversion to Skyrim's shader is lossy
and generative output is not reproducible, so these files cannot be recovered by
regenerating.

- [ ] **Step 3: Import into Blender and see what actually arrived**

```python
import bpy
from mathutils import Vector

bpy.ops.import_scene.gltf(filepath=r"<downloaded>.glb")
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
dg = bpy.context.evaluated_depsgraph_get()

pts = [(o.matrix_world @ Vector(c)) for o in meshes for c in o.bound_box]
result = {
    "objects": sorted(o.name for o in meshes),
    "tris": sum(len(o.evaluated_get(dg).to_mesh().polygons) for o in meshes),
    "extent_m": [round(max(p[i] for p in pts) - min(p[i] for p in pts), 4) for i in range(3)],
    "uvs": {o.name: bool(o.data.uv_layers.active) for o in meshes},
    "materials": sorted({m.name for o in meshes for m in o.data.materials if m}),
}
```

Read it against expectations: triangle count near the target, one or a few objects, a UV
layer on every mesh. Generated meshes sometimes arrive as a single merged object — that is
fine for a weapon, which needs no per-part separation.

- [ ] **Step 4: Assert the UV maps exist**

```python
missing = [o.name for o in meshes if not o.data.uv_layers.active]
assert not missing, f"no UV map on: {missing}"

# A texturing run can come back without a normal map, on identical geometry.
for o in meshes:
    for mat in [m for m in o.data.materials if m]:
        bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
        assert bsdf and bsdf.inputs["Normal"].is_linked, f"no normal map on {mat.name}"
```

If the normal map is missing, **re-run Meshy's texturing stage** rather than working around
it — that produced a full set on identical geometry the first time it happened. Skyrim's
shader expects a `_n.dds` and reads its specular mask from that file's alpha.

If Meshy's unwrap did not come through the GLB, unwrap in Blender before going further —
`bpy.ops.uv.smart_project` will do, though it is a poor layout to paint on.

- [ ] **Step 5: Fix orientation, then scale**

Orientation first, because scaling is about the origin and a rotation afterwards would
move the pivot. The blade must run along the axis a vanilla sword uses and point the same
way; the origin belongs in the grip, where the hand closes. Generated meshes arrive with
arbitrary orientation and an origin that is usually the bounding-box centre.

```python
import bpy

SKYRIM_UNITS_PER_METRE = 70.0     # a 128-unit human is about 1.83 m

meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]

# ... rotate here so the blade runs along +Y, and move the grip to the world origin ...

bpy.ops.transform.resize(value=(SKYRIM_UNITS_PER_METRE,) * 3,
                         center_override=(0.0, 0.0, 0.0))
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
```

**PyNifly does not convert metres to Skyrim units.** It writes Blender units into the NIF
one for one, and `blender_xf` governs bone orientation rather than asset scale. An
unscaled 0.857 m blade exports as a 0.86-unit NIF — about 1.2 cm beside a 128-unit human —
and reports "Export successful".

**A generated mesh does not arrive at metre scale**, so `SKYRIM_UNITS_PER_METRE` is the
wrong constant to reach for. Measure the mesh in Step 3 and derive the factor from the
intended real-world length: the current generation is **1.9098** long in its own units, and
a 0.70 m short sword is about 49 Skyrim units, so the factor is `49 / 1.9098` — roughly
**25.6**.

- [ ] **Step 6: Stage the source images so the texture paths come out right**

**PyNifly writes each image's filesystem path into the NIF's texture slots.** Images that
are still embedded in the imported GLB have no path, so the slots come out empty and the
weapon renders untextured — with only a `WindowsPath('.') has an empty name` warning to say
so.

Save the maps to a folder whose path *contains* `textures\isekai\weapons\`, and PyNifly
truncates at the `textures\` segment on its own:

```python
stage = r"<outside the repo>\stage\textures\isekai\weapons"
os.makedirs(stage, exist_ok=True)
for inp, fname in (("Base Color", "systemblade.png"),
                   ("Normal", "systemblade_n.png"),
                   ("Metallic", "systemblade_orm.png")):
    img = <the image feeding that Principled input>
    img.filepath_raw = os.path.join(stage, fname)
    img.file_format = "PNG"
    img.save()
```

Verified result in the NIF: `textures\isekai\weapons\systemblade.dds` and `..._n.dds`,
written automatically. That removes the manual NifSkope step this plan used to carry.

**Stage outside a user folder.** Whatever path the images sit at is what PyNifly writes, so
staging under `C:\Users\<name>\...` would put the author's real name into a shipped mesh.
Exporting from a work folder without the `textures\` segment produced an absolute
`e:\_isekai_assets\...\systemblade_basecolor.dds` in the slots — harmless there, fatal
under a home directory.

Give the shape a meaningful name before exporting, too: the glTF import calls it `Mesh_0`,
which says nothing in NifSkope. Vanilla uses the weapon's own name.

- [ ] **Step 7: Export the NIF**

```python
import bpy, logging, traceback

out = r"<repo>\meshes\isekai\weapons\systemblade.nif"

# "see console window for details" is a dead end over MCP; capture the real traceback.
caught = []
class Grab(logging.Handler):
    def emit(self, r):
        if r.exc_info:
            caught.append("".join(traceback.format_exception(*r.exc_info)))
log = logging.getLogger("pynifly"); h = Grab(); log.addHandler(h)

bpy.ops.export_scene.pynifly(
    filepath=out,
    target_game="SKYRIMSE",
    export_modifiers=True,      # or bevels are dropped and a mirrored part exports halved
    check_existing=False,
    intuit_defaults=False)      # REQUIRED - see below

log.removeHandler(h)
result = {"traceback": caught[-1] if caught else None}
```

**`intuit_defaults` defaults to `True`, and that silently discards every setting passed
in.** The operator then guesses from the objects and falls back to the enum default
`SKYRIM`, producing a Legacy Edition NIF while reporting success:

```python
if self.intuit_defaults:                                    # export_nif.py
    self.target_game = self._discover_game(self.objects_to_export)
```

- [ ] **Step 8: Verify the written file, not the success message**

PyNifly reports "Export successful" for files that are the wrong edition and the wrong
size. Read the NIF back and check the numbers.

```python
import importlib
pynifly = importlib.import_module("io_scene_nifly.pyn.pynifly")
nif = pynifly.NifFile(out)
allv = [v for sh in nif.shapes for v in sh.verts]
ext = [round(max(v[i] for v in allv) - min(v[i] for v in allv), 2) for i in range(3)]
result = {"game": str(nif.game),
          "extent_units": ext,
          "tris": sum(len(sh.tris) for sh in nif.shapes),
          "shapes": sorted(sh.name for sh in nif.shapes)}
```

Expected: `game == "SKYRIMSE"`, longest extent about **49 units**, triangle count matching
what went in (10,159 for the current generation). The raw header carries the same answer — BS version **100** is Skyrim SE,
**83** is Legacy Edition.

- [ ] **Step 9: Commit the mesh**

```bash
git add meshes/isekai/weapons/systemblade.nif
git commit -m "feat(assets): the System Blade mesh"
```

The mesh has no collision and no texture paths yet; Task 2 adds both to this file.

---

### Task 2: Collision, textures, and the guide

**Files:**
- Modify: `meshes/isekai/weapons/systemblade.nif`
- Create: `textures/isekai/weapons/systemblade.dds`, `textures/isekai/weapons/systemblade_n.dds`
- Create: `docs/WEAPON_ASSET_GUIDE.md`

**Interfaces:**
- Consumes: the NIF from Task 1.
- Produces: the same mesh with a `bhkCollisionObject` and `BSShaderTextureSet` slots
  pointing at `textures\isekai\weapons\systemblade.dds` and `..._n.dds`. Task 5's check
  verifies those paths exist.

**This is the user's work.** Not because the files are out of reach — they are not — but
because judging a collision hull against a blade, and judging whether a texture reads at
arm's length, means looking at the thing. There is no test to run.

- [ ] **Step 1: Compare against the vanilla references (already extracted)**

The reference meshes are unpacked to `E:\_skyrim_ref\` — `longsword.nif`,
`1stpersonlongsword.nif` and `irondagger.nif`, taken from `Skyrim - Meshes1.bsa` with
`BSArch64.exe` from the modlist's SSEEdit folder. **They are Bethesda's assets: they stay
outside the repository and are never committed.**

The vanilla file for the iron sword is `longsword.nif`. There is no `ironsword.nif` — the
only files by that name are broken-sword clutter props.

Measured from those files, so the target is known rather than guessed:

| | axis | length | Y range | blade / hilt | blade tris |
|---|---|---|---|---|---|
| iron sword | Y | 77.33 u | −13.17 … +64.16 | 83 / 17 | 425 |
| iron dagger | Y | 35.38 u | −11.68 … +23.70 | 67 / 33 | 416 |

Open the new blade beside `longsword.nif` and check that it runs along **+Y** with the
pommel at negative Y.

**The origin sits at the guard, not in the middle of the grip.** In both reference files the
negative Y extent equals the hilt length exactly — the sword's hilt is 17% of 77.33 units
and its minimum is −13.17; the dagger's is 33% of 35.38 and its minimum is −11.68. So the
blade occupies the whole positive Y range and the hilt the whole negative one.

If the axis is wrong, fix it in Blender and re-export — not in NifSkope, so the source
stays the truth.

- [ ] **Step 2: Give it a collision shape**

Copy the `bhkCollisionObject` branch from the vanilla sword into the new NIF in NifSkope,
then adjust its dimensions to the new blade. Without it the weapon falls through the floor
when dropped.

It is not a single box: `longsword.nif` carries `bhkCollisionObject` → `bhkRigidBody` →
**`bhkListShape`** with several `bhkConvexTransformShape` children, so refitting means
several primitives rather than one.

If a copied shape cannot be made to fit, generate a simple convex shape instead — more
work, well trodden, and the spec records this as the expected fallback.

- [ ] **Step 3: Convert the textures to DDS**

Export Meshy's maps as PNG, fold roughness into the normal map's alpha, then convert with
`texconv`. **Meshy's base colour arrives oversized** — the accepted generation ships an
8192² base colour, more than most whole-body textures in a load order, alongside a 4096²
ORM and a 4096² normal. A weapon wants 4096² at most, so resize in the same pass:

```
texconv -f BC7_UNORM -w 4096 -h 4096 -y -o textures\isekai\weapons systemblade.png
texconv -f BC7_UNORM -w 4096 -h 4096 -y -o textures\isekai\weapons systemblade_n.png
```

**BC7 for both, and the normal map especially.** BC5 stores two channels and has no alpha,
so it cannot carry the specular mask that gives a metal blade its shine — a blade exported
that way is uniformly matte, with no error to say so. Earlier drafts of this plan said BC5,
copied from the general advice for normal maps without checking it against Skyrim's shader.

Meshy's maps are the source, not the shipped form. Skyrim SE's standard shader is not PBR,
so they are converted:

| Meshy produces | Skyrim SE consumes |
|---|---|
| Base colour | diffuse, `systemblade.dds` |
| Normal | normal RGB, **with specular in the alpha channel** |
| Roughness | inverted into that alpha |
| Metallic | selects the environment-map shader and its cubemap |

Surface ornament belongs here rather than in geometry — the grip wrap, the engraving on
guard and pommel, the wear. Silhouette does not: no map changes an outline, which is why
the mesh carries the target triangle count instead of leaning on the texture. The glow in
the fuller is Skyrim's enchantment shader plus a glow map, neither geometry nor diffuse.

A **PBR set for Community Shaders' True PBR is planned as an optional FOMOD component**,
following the same base-plus-optional pattern as the ImGui overlay and the PrismaUI view.
It is deliberately not part of the first weapon: the asset chain is unproven, and two
texture sets would make a failure ambiguous. Keeping Meshy's original maps is what makes it
possible later.

- [ ] **Step 4: Point the NIF at the texture paths**

In NifSkope, set the `BSShaderTextureSet` slots to
`textures\isekai\weapons\systemblade.dds` (slot 0) and `..._n.dds` (slot 1). Paths are
relative to `Data\` and use backslashes.

Done when NifSkope renders the blade with its texture rather than flat white.

- [ ] **Step 5: Write the guide**

Create `docs/WEAPON_ASSET_GUIDE.md` recording what was actually done, with the real
settings. Follow the tone of `docs/CREATION_KIT_ESP.md`: numbered steps, and a warning
wherever a step misbehaves silently.

Record the failures that produce no error, because they are the whole value of the guide:

- no UV map — the export dies with a `NoneType` attribute error
- `intuit_defaults=True` — every passed setting is discarded, output is Legacy Edition
- `export_modifiers=False` — bevels vanish and a mirrored guard exports as one half
- unscaled export — a 1.2 cm blade, reported as successful
- a BC5 normal map — no alpha, so no specular, so a permanently matte blade
- missing collision — the weapon falls through the floor

- [ ] **Step 6: Commit**

```bash
git add meshes/ textures/ docs/WEAPON_ASSET_GUIDE.md
git commit -m "feat(assets): collision, textures, and the weapon asset guide"
```

---

### Task 3: The WEAP record, in the Creation Kit

**Files:**
- Modify: `plugin/IsekaiHero.esp`
- Modify: `docs/WEAPON_ASSET_GUIDE.md` (a second section)

**Interfaces:**
- Consumes: `meshes/isekai/weapons/systemblade.nif` — exported in Task 1, completed in Task 2.
- Produces: a WEAP record with editor ID `IsekaiSystemBlade` at a local FormID `<= 0xFFF`. Task 4's catalog row names that FormID.

**This is the user's work,** through the Creation Kit rather than a script. A WEAP carries a dozen subrecords and a wrong field does not raise an error — it produces a weapon that deals no damage or fits no animation. Automating this is worth doing once there is a known-good record to diff against, which is exactly what does not exist yet.

- [ ] **Step 1: Duplicate a vanilla sword**

In the Creation Kit with `IsekaiHero.esp` as the active file, find `IronSword` under Items → Weapon, duplicate it, and rename the editor ID to `IsekaiSystemBlade`. Duplicating rather than creating from scratch inherits the animation type, equip slot, sounds and keywords already known to be correct — `WeapTypeSword` and the ordinary one-handed animations. A short sword is still a sword to the game; only the mesh is smaller.

- [ ] **Step 2: Point it at the new mesh**

Set the model to `isekai\weapons\systemblade.nif` — relative to `Data\meshes\`, so the leading `meshes\` is not part of the path.

- [ ] **Step 3: Set the name, damage and value**

Give it a display name, and set damage and value to whatever the System's tier of reward should feel like. These are balance numbers, not correctness: they can be retuned any time without touching anything else.

- [ ] **Step 4: Save and note the FormID**

Save the plugin. Note the record's FormID; only the low three digits matter — the local ID. Task 4 needs it.

- [ ] **Step 5: Check the plugin is still light**

Run: `node tools/check.mjs`
Expected: PASS, including `ESP: every FormID is valid for a light plugin`. If the new record landed above `0xFFF`, the Creation Kit assigned a high ID and it must be renumbered in SSEEdit before going further.

- [ ] **Step 6: First in-game verification**

This is the moment the pipeline is either proven or not. Repackage and install through MO2 — a `build.bat` deploy does not reach the test setup. Then, with the console:

```
help "System Blade" 4
player.additem <formid> 1
```

Check all five:
1. The weapon appears in the inventory with its name.
2. Equipped, it sits in the hand at a believable size beside a vanilla sword — shorter, deliberately.
3. It draws and sheathes with the one-handed animation.
4. Dropped, it lands on the ground rather than falling through it.
5. Picked up again, it returns with its name and stats intact.

**If any of these fail, stop and fix before Task 4.** Everything after this point assumes a working asset.

- [ ] **Step 7: Add the record section to the guide**

Append a section to `docs/WEAPON_ASSET_GUIDE.md` covering Steps 1–5, including the FormID ceiling for a light plugin, which is the one thing here that is specific to this project.

- [ ] **Step 8: Commit**

```bash
git add plugin/IsekaiHero.esp docs/WEAPON_ASSET_GUIDE.md
git commit -m "feat(esp): the System Blade weapon record"
```

---

### Task 4: Sell it in the System Shop

**Files:**
- Modify: `src/Shop.cpp` (the `Shelf` enum, `kShelfNames`, and `kCatalog`)
- Regenerate: `lang/template.txt`

**Interfaces:**
- Consumes: the local FormID from Task 3.
- Produces: a catalog row whose translation keys are `shop.systemBlade` and `shop.systemBlade.qty`.

- [ ] **Step 1: Add the shelf**

In `src/Shop.cpp`, extend the `Shelf` enum — a weapon does not belong on any existing shelf:

```cpp
        enum class Shelf {
            kMaterials,     // the crafting packs
            kWealth,        // septims
            kRestoratives,  // the cheap, spammable potions
            kElixirs,       // the expensive ones
            kArmaments,     // weapons the System hands over
            kCount
        };
```

- [ ] **Step 2: Name it**

```cpp
        constexpr const char* kShelfNames[] = { "MATERIALS", "WEALTH", "RESTORATIVES",
                                                "ELIXIRS", "ARMAMENTS" };
```

The `static_assert` immediately below already requires one name per shelf, so forgetting this is a compile error rather than an empty tab.

- [ ] **Step 3: Add the catalog row**

At the end of `kCatalog`, using the FormID from Task 3 in place of `0x000D90`:

```cpp
            { "shop.systemBlade", "Flameforged Oathblade", "x1", 40,
              "shop_blade.png",
              Kind::kOurItem, Cat::kSmithing, 1, Shelf::kArmaments, 0x000D90 },
```

`Cat::kSmithing` is the delivery category every non-pack entry uses; it decides where the item lands in storage, not which shelf it is shown on.

The icon `shop_blade.png` does not exist yet. That is deliberate and safe: `docs/SHOP_ICON_PROMPTS.md` records that a card whose icon file is missing renders without an image and nothing breaks. It can be filled in later.

- [ ] **Step 4: Regenerate the string table**

Run: `node tools/extract-strings.mjs`
Expected: `lang/template.txt` gains `shop.systemBlade` and `shop.systemBlade.qty`.

- [ ] **Step 5: Build and check**

Run: `node tools/check.mjs`
Expected: PASS.

Run: `build.bat`
Expected: `BUILD_OK`.

- [ ] **Step 6: Commit**

```bash
git add src/Shop.cpp lang/template.txt
git commit -m "feat(shop): an ARMAMENTS shelf, and the System Blade on it"
```

---

### Task 5: Ship the assets, and guard the paths

**Files:**
- Modify: `package.ps1` (after the sounds block, around line 114)
- Modify: `tools/make-fomod.mjs` (the `requiredInstallFiles` block)
- Modify: `tools/check.mjs` (`parseEsp`, plus a new check)

**Interfaces:**
- Consumes: `meshes/` from Tasks 1-2 and `textures/` from Task 2; the WEAP record from Task 3.
- Produces: nothing other code uses. This is what makes the asset reach a player.

- [ ] **Step 1: Write the failing test**

`parseEsp` currently discards each record's body, so a check cannot see the mesh path. Add it. In `tools/check.mjs`, in `parseEsp`, change the `recs.push` line to keep the body:

```js
        recs.push({ sig, formID: fid, local: fid & 0x00ffffff, edid: edidOf(body), body });
```

Then add the check beside the other API checks at the end of the file:

```js
check("ESP: every mesh the plugin names exists in the repository", () => {
  const { recs } = parseEsp();

  // A MODL subrecord holds a zstring path relative to Data\meshes\. A record pointing
  // at a file nobody committed installs perfectly and then shows an invisible weapon
  // in game - silent exactly the way a missing icon is not.
  const modlOf = (body) => {
    let i = 0;
    while (i + 6 <= body.length) {
      const sig = body.subarray(i, i + 4).toString("ascii");
      const size = body.readUInt16LE(i + 4);
      if (sig === "MODL") {
        return body.subarray(i + 6, i + 6 + size).toString("ascii").replace(/\0.*$/, "");
      }
      i += 6 + size;
    }
    return "";
  };

  const named = recs
    .map((r) => ({ edid: r.edid, path: modlOf(r.body) }))
    .filter((r) => r.path);

  const missing = named.filter(
    (r) => !existsSync(join(ROOT, "meshes", r.path.replace(/\\/g, "/")))
  );
  need(
    missing.length === 0,
    `named in the ESP but not in meshes/: ${missing.map((m) => `${m.edid} -> ${m.path}`).join(", ")}`
  );

  return named.length ? `${named.length} mesh path(s), all present` : "no meshes named yet";
});
```

`existsSync` and `join` are already imported at the top of `tools/check.mjs` (lines 20 and 22), so no import change is needed.

- [ ] **Step 2: Run it**

Run: `node tools/check.mjs`
Expected: PASS with `1 mesh path(s), all present`.

To prove it can fail, rename `meshes/isekai/weapons/systemblade.nif` temporarily, re-run, confirm the failure names `IsekaiSystemBlade`, then rename it back. A check that cannot fail is worse than no check.

- [ ] **Step 3: Stage the assets when packaging**

In `package.ps1`, after the sounds block:

```powershell
# --- weapon assets ---
# Whole folders, unlike the sounds above: the ESP names these paths, and check.mjs
# verifies every named path exists, so the repo is already the authority on what is
# needed. Copying selectively here would put a second, quieter list beside it.
Copy-Item (Join-Path $root "meshes")   $stage -Recurse
Copy-Item (Join-Path $root "textures") $stage -Recurse
```

- [ ] **Step 4: Install them**

In `tools/make-fomod.mjs`, in `requiredInstallFiles`:

```
    <folder source="meshes"                            destination="meshes"/>
    <folder source="textures"                          destination="textures"/>
```

Required, not optional: the ESP names these paths unconditionally, so a player who skipped them would get an invisible weapon.

- [ ] **Step 5: Package and verify the gate**

Run: `.\package.ps1 -Version 0.8.0`

Expected, both lines:
- `Name scrub: replaced N occurrence(s) in the DLL.`
- `Privacy gate: N staged file(s) checked, all clean.`

If the gate trips on a NIF or DDS, the export embedded the source path. Fix the export — re-export without the path, or strip it in NifSkope — never the gate.

- [ ] **Step 6: Confirm the archive carries the assets**

```bash
"C:\Program Files\7-Zip\7z.exe" l dist\IsekaiHero-v0.8.0.7z | grep -iE "meshes|textures"
```

Expected: the NIF and both DDS files listed.

- [ ] **Step 7: Commit**

```bash
git add tools/check.mjs package.ps1 tools/make-fomod.mjs fomod/ModuleConfig.xml
git commit -m "build: ship meshes and textures, and check every path the ESP names"
```

---

## Final verification, in game

Install the packaged archive through MO2 and confirm the whole chain, not just the parts:

1. The System Shop shows an **ARMAMENTS** shelf.
2. The System Blade is on it and can be bought.
3. It arrives in the Dimensional Storage.
4. Equipped from there, it behaves as Task 3 Step 6 established.

Only when all four hold is the pipeline proven, and only then is a second weapon — or the armour route in #34 — worth planning.

## Out of scope, deliberately

No armour. No custom enchantment or magic effect. No world placement, levelled lists or crafting recipe. No second weapon. No shop icon — the card renders without one, and it can follow through the existing `docs/SHOP_ICON_PROMPTS.md` workflow.
