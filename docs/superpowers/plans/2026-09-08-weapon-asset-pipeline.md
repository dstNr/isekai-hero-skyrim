# Weapon Asset Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Get one one-handed sword from a concept image into the System Shop, and leave behind a written route so the second weapon costs a fraction of the first.

**Architecture:** Geometry, UVs and NIF export all happen in local Blender, scripted with `bpy` through the official Blender MCP and PyNifly. That is one continuous step with no handover: the mesh lands in the repository at the end of Task 1. What stays with the user is the work that needs a human at a GUI — collision in NifSkope, textures, and the WEAP record in the Creation Kit. Only after the asset is proven in game does the C++ side change: a shop entry, packaging, and a path check.

**Tech Stack:** Blender 5.2.1 LTS driven by the Blender Lab MCP server (`blender-mcp`, socket on `localhost:9876`), PyNifly 28.2, NifSkope, texconv, Creation Kit, C++23 / CommonLibSSE-NG, `tools/check.mjs`, `package.ps1`.

**Spec:** `docs/superpowers/specs/2026-09-08-weapon-asset-pipeline-design.md`

## Global Constraints

- **The sword is 0.75 m to 0.9 m overall.** Skyrim uses roughly **70 units per metre** (a human is about 128 units tall). Blender models at metre scale, and **nothing applies that factor for you** — PyNifly writes Blender units into the NIF one for one. Scale by 70 as the last step before export and verify by reading the file back.
- **PyNifly must be called with `intuit_defaults=False`.** Left at its default it discards every setting passed to the operator and guesses instead, silently producing a Legacy Edition NIF. `export_modifiers=True` is needed too, or bevels and mirrors are dropped.
- **Every mesh needs a UV map before export.** PyNifly raises on `uv_layers.active` being `None` and reports only "see console window for details".
- **Nothing shipped may carry the author's real name.** `package.ps1`'s gate walks the whole staged tree. Exported NIF and DDS files sometimes embed their source path — if the gate trips, fix the export, never the gate.
- **The mod is tested from the packaged archive in MO2**, not from the base-game folder. `build.bat` deploys the plugin for development only and does not carry assets.
- **The plugin is ESL-flagged**: every local FormID must be `<= 0xFFF`. Current use is 35 records, so there is room.
- **All repository artifacts in English** — code, comments, commit messages, docs.
- **Never bump the project version.** `CMakeLists.txt` VERSION and `package.ps1`'s default `-Version` stay where they are.
- **No armour in this plan.** See the spec's non-goals; armour is #34.
- **`build.bat` needs `SKYRIM_DATA` set** and refuses to deploy while Skyrim is running.

## Two kinds of task in this plan

Tasks 1 and 4–5 are Claude's: code and repository changes, with the project's real verification (`node tools/check.mjs`, `build.bat`).

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

### Task 1: Model the blade, unwrap it, and export a Skyrim SE NIF

**Files:**
- Create: `meshes/isekai/weapons/systemblade.nif`

**Interfaces:**
- Consumes: the user's concept images — a side-on profile, PNG, at least 1024 px on the
  long edge, plain background, even lighting; optionally a second edge-on or front view.
- Produces: `meshes/isekai/weapons/systemblade.nif`, Skyrim SE format (BS version 100),
  blade along +Y, **60 Skyrim units** overall, origin at the grip where the hand closes.
  Task 2 adds collision and texture paths to the same file; Task 3's WEAP record names it.

**This task cannot start until the concept images exist.** They are the input, not a detail.

Everything here runs in local Blender through the Blender MCP. There is no handover in the
middle: geometry, UVs, scale and NIF export are one continuous scripted step.

- [ ] **Step 1: Confirm the toolchain before modelling anything**

```python
import bpy, addon_utils
result = {
    "blender": bpy.app.version_string,                # expect 5.2.x
    "pynifly": addon_utils.check("io_scene_nifly"),   # expect (True, True)
    "export_op": hasattr(bpy.ops.export_scene, "pynifly"),
}
```

If PyNifly is missing, install the latest `io_scene_nifly.zip` from
`github.com/BadDogSkyrim/PyNifly` with
`bpy.ops.preferences.addon_install(filepath=..., overwrite=True)` then `addon_enable`.
It is a legacy addon, not an extension package — `extension install-file` will not take it.

- [ ] **Step 2: Lay down a measured blockout**

Scale and orientation are established before any shaping, because both are what fail
silently later. Blade along +Y, origin in the grip.

```python
import bpy

for o in list(bpy.data.objects):
    if o.type == "MESH":
        bpy.data.objects.remove(o, do_unlink=True)

# 0.85 m overall. Blade runs along +Y so the tip points away from the grip.
def box(name, dim, loc):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    ob = bpy.context.active_object
    ob.name = name
    ob.scale = dim                      # size=1.0 -> verts at +-0.5, so scale == dimension
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return ob

box("blade",  (0.052, 0.660, 0.009), (0,  0.410, 0))
box("guard",  (0.190, 0.028, 0.022), (0,  0.072, 0))
box("grip",   (0.030, 0.115, 0.026), (0,  0.000, 0))
box("pommel", (0.048, 0.048, 0.040), (0, -0.078, 0))
```

Expected: four objects, overall length within 0.80–0.90 m.

- [ ] **Step 3: Render the blockout and check it against the concept**

Render an orthographic profile view and compare the silhouette to the concept image:
proportions of blade to grip, guard width, taper.

Concept art is usually drawn with a hand-and-a-half grip. A one-handed sword needs it
shorter or the hand sits wrong — decide that here, not after shaping.

- [ ] **Step 4: Shape the blade to the reference**

Build cross-sections and loft them rather than deforming a cube: that gives direct control
over the taper and lets the fuller fade out before the point. Keep it hard-surface — flat
faces and clean bevels survive the NIF conversion without topology surprises.

```python
import bpy, bmesh, math

def loft(name, rings, tip=None):
    bm = bmesh.new()
    vr = [[bm.verts.new(p) for p in ring] for ring in rings]
    n = len(rings[0])
    for a, b in zip(vr, vr[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((a[i], a[j], b[j], b[i]))
    if tip is not None:
        tv = bm.verts.new(tip)
        for i in range(n):
            bm.faces.new((vr[-1][i], vr[-1][(i + 1) % n], tv))
    else:
        bm.faces.new(tuple(vr[-1]))
    bm.faces.new(tuple(reversed(vr[0])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    return ob

# Ten-point blade section: two cutting edges, two ridges, a fuller in each face.
# `fuller` fades to 0 so the channel ends before the point.
W, T = 0.036, 0.0055

def blade_ring(y, ws, ts, fuller):
    w, t = W * ws, T * ts
    fz = t * (0.26 + 0.74 * (1.0 - fuller))
    xz = [(-w, 0.0), (-0.66*w, t), (-0.34*w, fz), (0.34*w, fz), (0.66*w, t),
          (w, 0.0), (0.66*w, -t), (0.34*w, -fz), (-0.34*w, -fz), (-0.66*w, -t)]
    return [(x, y, z) for x, z in xz]
```

Bevel every hard edge once so the silhouette catches light instead of reading flat:

```python
for ob in [o for o in bpy.data.objects if o.type == "MESH"]:
    m = ob.modifiers.new(name="bevel", type="BEVEL")
    m.width, m.segments = 0.0008, 2
    m.limit_method, m.angle_limit = "ANGLE", math.radians(30)
```

Re-render after each coherent edit and compare. Watch for parts that sit inside other
parts — a gem buried in its collar renders as nothing and is easy to miss in profile.

Done when the rendered silhouette matches the concept closely enough that the user
approves it. **Ask them; do not decide this alone.**

- [ ] **Step 5: Apply Principled materials**

Base colour and roughness only. Procedural shaders do not survive export, and anything
more elaborate is repainted as a texture in Task 2.

- [ ] **Step 6: Unwrap every mesh — the export fails without it**

PyNifly reads `uv_layers.active.data` unconditionally. A procedurally built mesh has no UV
map, and the export dies with `AttributeError: 'NoneType' object has no attribute 'data'`,
surfaced only as "see console window for details".

```python
from math import radians
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
for o in meshes:
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode="OBJECT")

assert all(o.data.uv_layers.active for o in meshes), "a mesh is still unwrapped"
```

Smart UV Project is adequate for a hard-surface weapon whose textures will be baked. It is
an awkward layout to hand-paint on; if the textures are to be painted, place seams by hand.

- [ ] **Step 7: Apply the Skyrim scale, then export**

**PyNifly does not convert metres to Skyrim units.** It writes Blender units into the NIF
one for one. `blender_xf` does not help — that flag governs bone orientation and armature
scale, not asset scale. Exporting a 0.857 m sword unscaled yields a NIF measuring 0.86
units, about 1.2 cm beside a 128-unit human, and reports "Export successful".

```python
import bpy, logging, traceback

SKYRIM_UNITS_PER_METRE = 70.0     # a 128-unit human is about 1.83 m
out = r"<repo>\meshes\isekai\weapons\systemblade.nif"

meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]

# Scale about the world origin, which sits in the grip, then bake it in.
bpy.ops.transform.resize(value=(SKYRIM_UNITS_PER_METRE,) * 3,
                         center_override=(0.0, 0.0, 0.0))
bpy.ops.object.transform_apply(location=True, rotation=False, scale=True)

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
    export_modifiers=True,      # or bevels are dropped and a mirrored guard exports halved
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
result = {"game": str(nif.game), "extent_units": ext,
          "shapes": sorted(sh.name for sh in nif.shapes)}
```

Expected: `game == "SKYRIMSE"`, longest extent **60 units** (0.857 m x 70), one shape per
part. The raw header carries the same answer — BS version **100** is Skyrim SE, **83** is
Legacy Edition.

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

- [ ] **Step 1: Verify the orientation against a vanilla sword**

Still unproven, and it produces no error when wrong. Extract
`meshes\weapons\iron\ironsword.nif` from the game BSA and import it beside the new blade.
The new blade must run along the same axis and point the same way, and should be within
roughly ±15% of its length.

If the axis is wrong, fix it in Blender and re-export — not in NifSkope, so the source
stays the truth.

- [ ] **Step 2: Give it a collision shape**

Copy the `bhkCollisionObject` branch from the vanilla sword into the new NIF in NifSkope,
then adjust its dimensions to the new blade. Without it the weapon falls through the floor
when dropped.

If a copied shape cannot be made to fit, generate a simple convex shape instead — more
work, well trodden, and the spec records this as the expected fallback.

- [ ] **Step 3: Convert the textures to DDS**

Bake or paint the maps, export as PNG, then convert with `texconv`:

```
texconv -f BC7_UNORM -y -o textures\isekai\weapons systemblade.png
texconv -f BC5_UNORM -y -o textures\isekai\weapons systemblade_n.png
```

BC7 for colour, BC5 for the normal map — that is what Skyrim SE expects.

Most of the detail in a concept image lives here rather than in geometry: the grip wrap,
the engraving on guard and pommel, the surface wear. Vanilla one-handed swords run about
1000–2000 triangles, so the geometry is meant to be plain. The glow in the fuller is
Skyrim's enchantment shader plus a glow map, not geometry and not the diffuse map.

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
- unscaled export — a 1.2 cm sword, reported as successful
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

In the Creation Kit with `IsekaiHero.esp` as the active file, find `IronSword` under Items → Weapon, duplicate it, and rename the editor ID to `IsekaiSystemBlade`. Duplicating rather than creating from scratch inherits the animation type, equip slot, sounds and keywords already known to be correct.

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
2. Equipped, it sits in the hand at a believable size beside a vanilla sword.
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
            { "shop.systemBlade", "System Blade", "x1", 40,
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
