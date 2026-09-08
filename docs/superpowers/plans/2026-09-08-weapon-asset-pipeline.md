# Weapon Asset Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Get one one-handed sword from a concept image into the System Shop, and leave behind a written route so the second weapon costs a fraction of the first.

**Architecture:** Geometry is modelled in a remote Blender worker driven by `bpy` and exported as GLB. The user converts GLB to NIF and DDS locally, because those steps need Skyrim's own files and format plugins that the worker cannot reach. The ESP record is made in the Creation Kit. Only then does anything change in the repository: a shop entry, packaging, and a path check.

**Tech Stack:** Blender 5.2 via `scene_builder_3d_run_python`, PyNifly (local), NifSkope (local), Creation Kit (local), C++23 / CommonLibSSE-NG, `tools/check.mjs`, `package.ps1`.

**Spec:** `docs/superpowers/specs/2026-09-08-weapon-asset-pipeline-design.md`

## Global Constraints

- **The sword is 0.75 m to 0.9 m overall.** Skyrim uses roughly **70 units per metre** (a human is about 128 units tall). Blender models at metre scale; the conversion applies the factor.
- **Nothing shipped may carry the author's real name.** `package.ps1`'s gate walks the whole staged tree. Exported NIF and DDS files sometimes embed their source path — if the gate trips, fix the export, never the gate.
- **The mod is tested from the packaged archive in MO2**, not from the base-game folder. `build.bat` deploys the plugin for development only and does not carry assets.
- **The plugin is ESL-flagged**: every local FormID must be `<= 0xFFF`. Current use is 35 records, so there is room.
- **All repository artifacts in English** — code, comments, commit messages, docs.
- **Never bump the project version.** `CMakeLists.txt` VERSION and `package.ps1`'s default `-Version` stay where they are.
- **No armour in this plan.** See the spec's non-goals; armour is #34.
- **`build.bat` needs `SKYRIM_DATA` set** and refuses to deploy while Skyrim is running.

## Two kinds of task in this plan

Tasks 1 and 4–5 are Claude's: code and repository changes, with the project's real verification (`node tools/check.mjs`, `build.bat`).

Tasks 2 and 3 are the user's: local work in Blender, NifSkope and the Creation Kit. Those steps are written as precise instructions with a stated done-condition rather than as a test-first cycle, because there is no test to run — the verification is looking at the thing. Dressing them up as red-green-refactor would be theatre.

---

## File Structure

| File | Responsibility |
|---|---|
| `meshes/isekai/weapons/systemblade.nif` *(create, Task 2)* | The weapon mesh, mirroring the `Data\` layout. |
| `textures/isekai/weapons/systemblade.dds` *(create, Task 2)* | Diffuse texture. |
| `textures/isekai/weapons/systemblade_n.dds` *(create, Task 2)* | Normal map. |
| `docs/WEAPON_ASSET_GUIDE.md` *(create, Task 2)* | The route from GLB to a working NIF and from the NIF to a WEAP record. Written once, followed for every later weapon. |
| `plugin/IsekaiHero.esp` *(modify, Task 3)* | Gains one WEAP record, made in the Creation Kit. |
| `src/Shop.cpp` *(modify, Task 4)* | A new `Shelf::kArmaments`, its display name, and the catalog row. |
| `lang/template.txt` *(regenerated, Task 4)* | Picks up the new translation keys. |
| `package.ps1` *(modify, Task 5)* | Stages `meshes\` and `textures\`. |
| `tools/make-fomod.mjs` *(modify, Task 5)* | Installs both folders as required files. |
| `tools/check.mjs` *(modify, Task 5)* | Every mesh path the ESP names must exist in the repo. |

---

### Task 1: Model the blade and export a GLB

**Files:**
- No repository files. The deliverable is a GLB handed to the user.

**Interfaces:**
- Consumes: the user's concept images — a side-on profile, PNG, at least 1024 px on the long edge, plain background, even lighting; optionally a second edge-on or front view.
- Produces: `systemblade.glb`, blade running along +Y, 0.85 m overall, origin at the grip where the hand closes.

**This task cannot start until the concept images exist.** They are the input, not a detail.

- [ ] **Step 1: Create the Blender project**

Call `scene_builder_3d_create_project`. Record the returned `projectId` — every later call needs it, plus the current `revision` and `expectedSceneSequence` from `scene_builder_3d_get_project`.

- [ ] **Step 2: Lay down a measured blockout**

The first commit establishes scale and orientation before any shaping, because both are what fail silently later. Run through `scene_builder_3d_run_python`:

```python
import bpy

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)

# 0.85 m overall: 0.70 m blade, 0.03 m guard, 0.12 m grip including pommel.
# Blade runs along +Y so the tip points away from the grip; the conversion step
# maps this to the axis Skyrim expects.
def box(name, dim, loc):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    ob = bpy.context.active_object
    ob.name = name
    ob.scale = (dim[0] / 2.0, dim[1] / 2.0, dim[2] / 2.0)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return ob

box("blade", (0.045, 0.70, 0.006), (0, 0.35 + 0.075, 0))
box("guard", (0.16, 0.03, 0.018), (0, 0.075, 0))
box("grip",  (0.030, 0.10, 0.030), (0, 0.010, 0))
box("pommel",(0.045, 0.04, 0.045), (0, -0.055, 0))

result = {
    "overall_m": max(o.dimensions.y + abs(o.location.y) for o in bpy.data.objects),
    "objects": sorted(o.name for o in bpy.data.objects),
}
```

Expected `result`: four objects, and an overall length within 0.80–0.90.

- [ ] **Step 3: Render the blockout and check it against the concept**

Add a camera and a key light, render a front-orthographic view, and publish it as a PNG artifact. Compare the silhouette to the concept image: proportions of blade to grip, guard width, taper. This is the feedback loop — it is why the geometry is modelled here rather than generated blind.

- [ ] **Step 4: Shape the blade to the reference**

Iterate against the renders. The shape itself comes from the concept image, but the operations do not — these are the ones that do the work on a blade:

```python
import bpy

blade = bpy.data.objects["blade"]

# Taper: scale the tip end of the blade in X and Z, leaving the base alone.
import bmesh
me = blade.data
bm = bmesh.new(); bm.from_mesh(me)
ymax = max(v.co.y for v in bm.verts)
for v in bm.verts:
    if abs(v.co.y - ymax) < 1e-4:
        v.co.x *= 0.35
        v.co.z *= 0.6
bm.to_mesh(me); bm.free()

# Bevel every hard edge once, so the silhouette catches light instead of reading flat.
for ob in bpy.data.objects:
    if ob.type != "MESH":
        continue
    m = ob.modifiers.new(name="bevel", type="BEVEL")
    m.width = 0.0015
    m.segments = 2
    m.limit_method = "ANGLE"
    m.angle_limit = 0.52  # 30 degrees
```

Keep it hard-surface — flat faces and clean bevels read as a System-forged weapon and survive the NIF conversion without topology surprises. Re-render after each coherent edit and compare.

Done when the rendered silhouette matches the concept closely enough that the user approves it. Ask them; do not decide this alone.

- [ ] **Step 5: Apply a Principled material**

The GLB carries a Principled BSDF with base colour and roughness; procedural shaders do not survive the export. Anything more elaborate is repainted as a texture in Task 2 anyway.

- [ ] **Step 6: Verify scale one last time, then export**

```python
import bpy
result = {
    o.name: {"dim_m": [round(v, 4) for v in o.dimensions],
             "loc_m": [round(v, 4) for v in o.location]}
    for o in bpy.data.objects if o.type == "MESH"
}
```

Confirm the blade's Y dimension is still what Step 2 set. Modelling operations can rescale without anyone noticing; this is the last cheap moment to catch it.

Then call `scene_builder_3d_get_glb` and hand the file to the user.

- [ ] **Step 7: Nothing to commit**

No repository file changed. The GLB is an intermediate handed over, not an artifact of this repo — the NIF that comes out of it in Task 2 is what gets committed.

---

### Task 2: Convert to NIF and DDS, and write the guide

**Files:**
- Create: `meshes/isekai/weapons/systemblade.nif`
- Create: `textures/isekai/weapons/systemblade.dds`, `textures/isekai/weapons/systemblade_n.dds`
- Create: `docs/WEAPON_ASSET_GUIDE.md`

**Interfaces:**
- Consumes: `systemblade.glb` from Task 1.
- Produces: the mesh at `meshes/isekai/weapons/systemblade.nif`, whose texture paths point at `textures\isekai\weapons\systemblade.dds` and `..._n.dds`. Task 3's WEAP record names the same mesh path; Task 5's check verifies it exists.

**This is the user's work.** The path is standard but has three places it silently goes wrong, and the guide exists so the second weapon does not rediscover them.

- [ ] **Step 1: Import the GLB into local Blender and export a NIF**

Blender with the PyNifly addon installed. Import the GLB, then export as NIF with the Skyrim SE preset. PyNifly applies the unit conversion; the check is the next step, not trust.

- [ ] **Step 2: Verify the scale against a vanilla sword**

Open `systemblade.nif` in NifSkope, and open a vanilla one-handed sword beside it (`meshes\weapons\iron\ironsword.nif`, extracted from the game's BSA). Compare the bounding boxes.

Done when the new blade is within roughly ±15% of the vanilla sword's length. If it is out by a factor near 70, the unit conversion did not apply — re-export with the scale option set rather than scaling the mesh by hand, so the fix survives the next export.

- [ ] **Step 3: Verify the orientation**

In the same two-window comparison, the blade must run along the same axis as the vanilla sword and point the same way. If not, rotate in Blender and re-export — not in NifSkope, so the source stays the truth.

- [ ] **Step 4: Give it a collision shape**

Copy the `bhkCollisionObject` branch from the vanilla sword into the new NIF in NifSkope, then adjust its dimensions to the new blade. Without it the weapon falls through the floor when dropped.

If a copied shape cannot be made to fit, generate a simple convex shape instead — more work, well documented, and the spec records this as the expected fallback.

- [ ] **Step 5: Convert the textures to DDS**

Export the base colour and normal maps from Blender as PNG, then convert with `texconv`:

```
texconv -f BC7_UNORM -y -o textures\isekai\weapons systemblade.png
texconv -f BC5_UNORM -y -o textures\isekai\weapons systemblade_n.png
```

BC7 for colour, BC5 for the normal map — that is what Skyrim SE expects.

- [ ] **Step 6: Point the NIF at the texture paths**

In NifSkope, set the `BSShaderTextureSet` slots to `textures\isekai\weapons\systemblade.dds` (slot 0) and `textures\isekai\weapons\systemblade_n.dds` (slot 1). Paths are relative to `Data\` and use backslashes.

Done when NifSkope renders the blade with its texture rather than flat white.

- [ ] **Step 7: Write the guide**

Create `docs/WEAPON_ASSET_GUIDE.md` recording exactly what was done in Steps 1–6, with the actual settings used — the PyNifly export preset, the scale option, the texconv formats, the NifSkope slots. Follow the tone of `docs/CREATION_KIT_ESP.md`: numbered steps, and a warning box wherever a step silently misbehaves.

Record the three silent failures explicitly: wrong scale, wrong axis, missing collision.

- [ ] **Step 8: Commit**

```bash
git add meshes/ textures/ docs/WEAPON_ASSET_GUIDE.md
git commit -m "feat(assets): the System blade mesh and textures, and how they were made"
```

---

### Task 3: The WEAP record, in the Creation Kit

**Files:**
- Modify: `plugin/IsekaiHero.esp`
- Modify: `docs/WEAPON_ASSET_GUIDE.md` (a second section)

**Interfaces:**
- Consumes: `meshes/isekai/weapons/systemblade.nif` from Task 2.
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
- Consumes: `meshes/` and `textures/` from Task 2; the WEAP record from Task 3.
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
