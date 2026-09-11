# Weapon asset guide: from a concept image to a weapon in the System Shop

How the System Blade — sold in game as the **Flameforged Oathblade** — was made, written so
the second weapon costs a fraction of the first. Every number here was read back out of a
written file rather than trusted from a tool's success message: several of the steps below
report success while producing something unusable.

## Status

**Proven end to end.** On 2026-09-11 the weapon was bought from the System Shop and used in
game from the packaged archive, and it worked. The chain from a concept image to a
purchasable weapon has now run once, completely, and every stage below is the one that
actually ran.

What is still open is listed under **Open for the refinement pass** at the end: one planned
check that was never built, one texture convention not examined on its own, and the stages
that were done by hand or from throwaway scripts and are worth turning into tools before
the next weapon.

**No scabbard.** Most weapon mods ship without one and Skyrim copes — the weapon simply
appears whole on the body when sheathed. It was a deliberate decision, not an omission.

## The route, in one list

| | Stage | Who | Tool | Output |
|---|---|---|---|---|
| A | Generate | user | Meshy image-to-3D | a remeshed GLB with PBR maps |
| B | Archive | either | a folder outside the repository | the only copy that cannot be regenerated |
| C | Import and check | Claude | Blender over the Blender MCP | UVs and a normal map confirmed |
| D | Orient and scale | Claude | Blender | blade along +Y, at its intended length |
| E | Stage and export | Claude | PyNifly | `meshes/isekai/weapons/<name>.nif` |
| F | Textures | Claude | Pillow, `texconv` | two BC7 DDS files at 2048² |
| G | Collision | Claude | `tools/add-weapon-collision.py` | collision, BSX, `Prn`, `INV` |
| H | Records | user, then Claude | Creation Kit, then a repair | a WEAP and a STAT, in range |
| I | Shop | Claude | `src/Shop.cpp`, Higgsfield, `tools/icon-cutout.py` | a catalog row and its icon |
| J | Package | Claude | `package.ps1` | the assets inside the archive |
| K | Verify in game | user | MO2 | a bought, equipped, working weapon |

---

## The failures that produce no error

Read this list before starting. Every one of these was hit while building the first weapon,
and none of them raises anything you would notice:

| What | What you get | Where |
|---|---|---|
| Mesh has no UV map | Export dies with a `NoneType` attribute error, reported only as "see console window for details" | C |
| `intuit_defaults` left at `True` | Every setting you passed is discarded; you get a **Legacy Edition** NIF and the word "successful" | E |
| `export_modifiers` left at `False` | Bevels vanish, a mirrored part exports as one half | E |
| Nothing applies the Skyrim scale | A 1.2 cm weapon, reported as successful | D |
| Source images staged under a home directory | The author's real name is written into a shipped mesh | E |
| Normal map converted as BC5 | BC5 has no alpha, so no specular mask, so a permanently matte blade | F |
| Texturing run without a normal map | Slots empty, weapon renders untextured | B |
| The mesh is not under the game's own `Data\meshes\` | The Creation Kit's model picker does not list it; there is nothing to select | H |
| A WEAP duplicated from `IronSword`, only its model changed | Correct in third person, a vanilla iron sword in first | H |
| New records saved from the Creation Kit | FormIDs `0x2311`–`0x2313` in an ESL-flagged plugin, whose ceiling is `0xFFF` | H |
| The same save | Object bounds blanked to `0,0,0 / 0,0,0` | H |
| An image model asked for a transparent background | The transparency checkerboard painted into RGB pixels, no alpha at all | I |

---

## Tools

The archive and texture tools need no download if a modlist is installed — they ship with
one:

| Tool | Where it was found |
|---|---|
| `BSArch64.exe` | the modlist's `tools/SSEEdit …/` folder |
| `texconv.exe`, `texdiag.exe` | the modlist's `tools/Octagon/` folder |
| Blender **5.2.1 LTS** | a normal install |
| PyNifly **28.2** | `github.com/BadDogSkyrim/PyNifly`, asset `io_scene_nifly.zip` |
| Creation Kit | Steam, alongside the game |
| Python with Pillow, NumPy, SciPy | the texture channels and the icon cutout |
| Higgsfield, model `gpt_image_2` | the shop icon, over the Higgsfield MCP |

Two scripts this weapon left in the repository:

| Script | Stage | What it does |
|---|---|---|
| `tools/add-weapon-collision.py` | G | collision, BSX flags, attachment point and inventory marker, from measured numbers |
| `tools/icon-cutout.py` | I | turns a painted-in background into real transparency, at 512² |

PyNifly is a **legacy addon**, not an extension package. Install it with
`bpy.ops.preferences.addon_install(filepath=..., overwrite=True)` followed by
`addon_enable(module="io_scene_nifly")` — `blender --command extension install-file` will
refuse it. Blender's own MCP add-on drives all of this from outside; it needs
*Preferences → System → Allow Online Access* switched on or it will not start its server.

---

## Reference numbers

Extracted from `Skyrim - Meshes1.bsa` and measured, so the targets are known rather than
guessed. **The vanilla iron sword's mesh is `longsword.nif`** — there is no `ironsword.nif`;
the files by that name are broken-sword clutter props.

| | axis | length | Y range | guard at | blade / hilt | blade tris |
|---|---|---|---|---|---|---|
| iron sword (`longsword.nif`) | Y | 74.87 u | −11.72 … +63.15 | **+5.28** | 77.3 / 22.7 | 425 |
| iron dagger (`irondagger.nif`) | Y | 28.65 u | −6.80 … +21.85 | **+3.88** | 62.7 / 37.3 | 416 |

> **Measure the weapon mesh alone.** An earlier draft measured whole files and reported
> 83/17 and 67/33 with the origin sitting at the guard. Both were wrong: the scabbard is
> wider than the guard, so "the widest slice" found the scabbard instead. The numbers above
> come from the blade mesh on its own and are corroborated by the guard collision box, which
> straddles Y 4.77–6.32 on the sword.

Two conventions follow:

- **The blade runs along +Y**, the pommel sits at negative Y.
- **The origin is where the hand closes**, inside the grip — *not* at the guard. The guard
  sits **5.28 units above the origin** on the sword and 3.88 on the dagger. A hand is a
  constant size, so this offset barely changes with weapon length; use ~5.3 for a sword.

Skyrim's weapons are larger relative to the body than real ones — the iron sword's 74.87 units is about 1.07 m
at 70 units per metre. A weapon scaled to real-world dimensions looks stubby beside them.

To extract more references: `BSArch64.exe unpack "<archive>" <folder> -q -mt`. There is no
single-file filter, so unpack to a scratch folder, copy out what you need and delete the
rest. The meshes archive is 361 MB and takes about three seconds.

---

## A. Generate in Meshy

Image-to-3D from the concept image. Three settings matter:

- **Target polygon count: about 8,000 triangles.** Check whether the field counts polygons
  or triangles — with quads that is a factor of two. The accepted generation came in at
  10,159, which is over and perfectly fine. Vanilla's 400–800 is a 2011 console budget, not
  a design goal; modern weapon mods run 5,000–20,000, and a weapon is the cheapest place in
  the game to spend triangles because one or two are on screen and they are held closest to
  the camera.
- **UV unwrap: on.** The export cannot proceed without it.
- **PBR textures: on.**

> **Run the remesh, and download the remeshed result.** A download whose name ends in
> `_texture` is the raw texturing stage — the first one weighed 74 MB and held **1,678,170**
> triangles. The remeshed object arrives named `output_unwrapped`, which is the quickest way
> to tell them apart after import.

> **Do not publish to the Meshy Community page.** That dedicates the output under CC0,
> irrevocably.

### Proportions decide the weapon class, not the other way round

Concept art draws heroic proportions and image-to-3D reproduces them faithfully. The System
Blade measures **68 % blade to 32 % grip**, which sits between a vanilla sword (77/23) and a
vanilla dagger (63/37). The class is therefore chosen by the length you scale to:

| scaled to | blade | grip | reads as |
|---|---|---|---|
| 0.38 m | 26 cm | 12 cm | dagger, cleanly proportioned |
| **0.70 m** — shipped as 60 units, see D | 48 cm | 22 cm | **short sword — a generous grip, but sound** |
| 0.97 m | 66 cm | 31 cm | hand-and-a-half; visibly wrong for one hand |

## B. Archive the download before anything else

Save the mesh and **every** map to a folder outside the repository.

This is an obligation, not housekeeping. Meshy's terms reserve the right to delete generated
output, including from inactive accounts, and state that backing up anything worth keeping is
the customer's responsibility. The conversion to Skyrim's shader is lossy and generative
output is not reproducible, so these files cannot be recovered by regenerating.

> **Check that a normal map actually came through.** The first remesh download shipped a
> base colour and a packed metallic/roughness map only, with the Principled node's Normal
> input unconnected. Re-running the texturing stage produced a full set — base colour, ORM
> and a 4096² normal — on **byte-identical geometry**. A missing normal map is a property of
> one texturing run, not of the remesh, so texture again rather than working around it.

## C. Import and check what actually arrived

```python
bpy.ops.import_scene.gltf(filepath=glb)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]

missing = [o.name for o in meshes if not o.data.uv_layers.active]
assert not missing, f"no UV map on: {missing}"

for o in meshes:
    for mat in [m for m in o.data.materials if m]:
        bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
        assert bsdf and bsdf.inputs["Normal"].is_linked, f"no normal map on {mat.name}"
```

Also read the triangle count, the bounding box and the material wiring. A generated mesh
often arrives as a single merged object, which is fine for a weapon.

## D. Orientation, then scale

Orientation first: scaling happens about the origin, and a rotation afterwards would move
the pivot.

> `transform_apply` is a **no-op in world space** by definition — it bakes the object
> transform into the mesh and resets the object. To actually move geometry, transform the
> mesh data: `ob.data.transform(matrix)`.

The generated mesh arrives along **Z**. `Matrix.Rotation(radians(90), 4, "X")` maps −Z onto
+Y; use −90° if the tip is at +Z instead. Scale last.

Where the origin lands along Y does not matter much at this stage: G moves the mesh so the
guard sits **5.28 units above the origin**, where a vanilla sword puts it, whatever position
it arrived in. The axis and the direction are what have to be right here.

> **PyNifly does not convert metres to Skyrim units**, and `blender_xf` does not either —
> that flag governs bone orientation, not asset scale. Worse, **a generated mesh is not at
> metre scale at all**, so 70 is the wrong constant to reach for.

Derive the factor from the intended length. The System Blade measured **1.9098** in its own
units and the target was 60 Skyrim units, so the factor was `60 / 1.9098` = **31.4175**.

**Set the length in Skyrim units against a vanilla weapon, not in metres.** The first
estimate took the 0.70 m from the class table and multiplied by 70 units per metre, which
gives 49. Vanilla weapons are drawn larger than real ones (see *Reference numbers*), so that
would have been a stubby blade. 60 units is about **80 % of the iron sword's 74.87** —
shorter, deliberately, and still of the same family.

Result after this stage:

```
Y range   -19.05 .. +40.95     (hilt 19.05, blade 40.95, total 60.00)
extent    10.18 x 60.00 x 2.50
```

G then moves the mesh so the guard sits at +5.28, which leaves the shipped range at
**Y −13.59 … +46.40**, extent unchanged.

## E. Stage the images, then export

**PyNifly writes each image's filesystem path into the NIF's texture slots.** Images still
embedded in the imported GLB have no path, so the slots come out empty and the weapon renders
untextured — with only a `WindowsPath('.') has an empty name` warning to say so.

Save the maps to a folder whose path **contains** `textures\isekai\weapons\`, and PyNifly
truncates at the `textures\` segment on its own:

```python
stage = r"<outside the repo>\stage\textures\isekai\weapons"
img.filepath_raw = os.path.join(stage, "systemblade.png")
img.file_format = "PNG"
img.save()
```

> **Stage outside any home directory.** Whatever path the images sit at is what gets written.
> Staging under `C:\Users\<name>\…` puts the author's real name into a shipped mesh, which is
> exactly what `package.ps1`'s privacy gate exists to catch. Exporting from a work folder
> without the `textures\` segment produced an absolute `e:\…\systemblade_basecolor.dds` in
> the slots.

Give the shape a meaningful name first — the glTF import calls it `Mesh_0`, which says
nothing in NifSkope.

```python
bpy.ops.export_scene.pynifly(
    filepath=out,
    target_game="SKYRIMSE",
    export_modifiers=True,      # or bevels are dropped and mirrors export halved
    check_existing=False,
    intuit_defaults=False)      # REQUIRED
```

> **`intuit_defaults` defaults to `True`, and that silently discards every setting you
> passed.** The operator then guesses from the objects and falls back to the enum default
> `SKYRIM`, producing a Legacy Edition NIF while reporting success. In `export_nif.py`:
> `if self.intuit_defaults: self.target_game = self._discover_game(...)`.

PyNifly swallows export exceptions and prints them to a console you cannot see over MCP.
Attach a handler to get the real traceback:

```python
class Grab(logging.Handler):
    def emit(self, r):
        if r.exc_info:
            caught.append("".join(traceback.format_exception(*r.exc_info)))
logging.getLogger("pynifly").addHandler(Grab())
```

### Verify the written file, not the success message

```python
pynifly = importlib.import_module("io_scene_nifly.pyn.pynifly")
nif = pynifly.NifFile(out)
```

Expected for the System Blade: `game == "SKYRIMSE"`, 10,159 triangles, extent
`10.18 x 60.00 x 2.50`, and texture slots reading `textures\isekai\weapons\systemblade.dds`
and `..._n.dds`. In the raw header, BS version **100** is Skyrim SE and **83** is Legacy
Edition.

Then scan the bytes for the author's name and for drive letters. The privacy gate would
catch a leak before upload, but finding it here is cheaper.

## F. Textures

Meshy's PBR is the **source**, not the shipped form — Skyrim SE's standard shader is not PBR:

| Meshy produces | Skyrim SE consumes |
|---|---|
| Base colour | diffuse, `systemblade.dds` |
| Normal | normal RGB, **with the specular mask in the alpha** |
| Roughness (ORM green) | **inverted** into that alpha |
| Metallic | selects the environment-map shader and its cubemap |

Do the channel work with Pillow rather than in Blender, whose colour management makes a
silent gamma error easy in a non-colour map:

```python
n = np.asarray(Image.open("systemblade_n.png").convert("RGB"), np.uint8)
o = np.asarray(Image.open("systemblade_orm.png").convert("RGB"), np.uint8)
gloss = (255 - o[:, :, 1]).astype(np.uint8)          # glTF puts roughness in green
Image.fromarray(np.dstack([n, gloss]), "RGBA").save("out/systemblade_n.png")
```

Sanity numbers from the first conversion: roughness ranged 55–255 (mean 160), so the gloss
alpha came out 0–200 (mean 95), and the normal's green channel averaged **126.9** against
the 128 of a flat surface. A green mean far from 128 means something is wrong.

```
texconv -f BC7_UNORM -w 2048 -h 2048 -y -o textures\isekai\weapons systemblade.png
texconv -f BC7_UNORM -w 2048 -h 2048 -y -o textures\isekai\weapons systemblade_n.png
```

> **Never BC5 for the normal map.** BC5 stores two channels and has no alpha, so it cannot
> carry the specular mask that gives a metal blade its shine. Use BC7.

**2048, not 4096** — and the reason is the repository as much as the eye. BC7 with mipmaps
costs 22.4 MB per map at 4096² against 5.3 MB at 2048². Tracked content is 33 MB, so one
weapon at 4096² would more than double it and ten would add roughly 450 MB against 112 MB.
Meshy ships an 8192² base colour, more than most whole-body textures in a load order; keep
it in the archive and downscale for the repository.

Verify the written files: the header should read `2048x2048, 12 mips, dxgi=98 (BC7_UNORM)`,
and the normal map's alpha mode should be **`NonPM`**. `Opaque` means the specular mask was
dropped.

> **One convention is not settled.** glTF — and therefore Meshy — produces OpenGL (+Y)
> normals, and they were shipped unchanged. The weapon passed its in-game test that way with
> no lighting fault reported, but the lighting was not examined for this on its own, and an
> inverted green channel is subtle: carved detail reads as raised, and mostly under a raking
> light. If it ever shows, the fix is one line before conversion:
> `n[:, :, 1] = 255 - n[:, :, 1]`.

## G. Collision and the extra-data nodes

**NifSkope is not needed for this.** PyNifly reads and writes collision, so the whole job is
a script: `tools/add-weapon-collision.py`, run headless.

```
blender --background --python tools/add-weapon-collision.py -- \
    --nif meshes/isekai/weapons/systemblade.nif \
    --template <extracted>/longsword.nif --root-name SystemBlade
```

It imports a vanilla weapon purely as a **template**, so PyNifly's exact block structure and
Havok parameters are inherited rather than guessed, then replaces every geometric number
with one measured from our own blade and deletes all vanilla meshes. A collision box is
three numbers, so nothing of Bethesda's survives into the output — verified by checking that
no `Iron…` string remains in the written file.

What a vanilla weapon carries, and what the script therefore reproduces:

| Block | Value | Without it |
|---|---|---|
| `bhkCollisionObject` → `bhkRigidBody` → `bhkListShape` | layer `WEAPON`, `SPHERE_STABILIZED`, quality `MOVING` | falls through the floor |
| three `bhkBoxShape` children | material `MATERIAL_BLADE_1HAND` | — |
| `BSXFlags:BSX` | `HAVOC \| DYNAMIC \| ARTICULATED` | collision is ignored even when present |
| `NiStringExtraData:Prn` | `WeaponSword` | nowhere to sit on the body |
| `BSInvMarker:INV` | rotation, zoom 1.2 | wrong angle in the inventory |

The three collision boxes are grip, guard and blade, and they are deliberately **slimmer
than the visual mesh** — vanilla's blade box is 0.28 deep against a 3.13-deep blade. The
script preserves those ratios while remapping the Y span onto our own pommel, guard and tip.

Result for the System Blade:

```
grip    y -12.26 .. 3.29    3.56 x 15.55 x 0.82
guard   y   4.72 .. 6.01    8.82 x  1.29 x 1.48
blade   y   5.86 .. 46.35   4.27 x 40.50 x 0.22
```

> **Set `pynNodeFlags` on the shape.** Our own export drops them, and re-importing then
> exporting warns `Error setting pynNodeFlags`. Copy the template's value.

## H. The records: Creation Kit, then a repair

Step by step as **Part K** of `CREATION_KIT_ESP.md`: copy the assets into the game's own
`Data\`, create a STAT, duplicate `IronSword`, set both model fields, the name and the stats,
save. What the System Blade ended up with:

| Field | Value |
|---|---|
| Records | `IsekaiSystemBlade1st` (STAT), `IsekaiSystemBlade` (WEAP) |
| Model, both records | `isekai\weapons\systemblade.nif` |
| 1st Person Model Object (`WNAM`) | the STAT |
| Name | Flameforged Oathblade |
| Damage / Critical | 20 / 10 |
| Value / Weight | 2500 / 6.0 |
| Speed / Reach | 1.0 / 1.0, inherited |
| Keywords | `WeapTypeSword`, `WeapMaterialIron`, `VendorItemWeapon`, inherited |

### What came back from the Creation Kit, and the repair

**Expect to repair the plugin after every Creation Kit session that adds records.** The save
looked fine inside the CK and was wrong in ways it does not report:

| Found in the saved file | Repaired to | Caught by |
|---|---|---|
| STAT at `0x2311`, WEAP at `0x2313` — above the ESL ceiling of `0xFFF` | `0xD8D` and `0xD8F`, continuing after the highest existing record, with `WNAM` rewritten to follow | `tools/check.mjs` |
| `OBND` blanked to `0,0,0 / 0,0,0` on both | `-6,-14,-2 / 6,47,2`, the mesh's measured extents rounded outward | reading the file back |
| A deleted, empty WEAP at `0x2312` — the discarded first duplicate | renumbered to `0xD8E` and left in place | reading the file back |

Both repairs were in-place byte patches: a FormID is four bytes in a record header, a `WNAM`
reference four more, an `OBND` twelve — nothing changes size, so no group header needs
recalculating. SSEEdit's *Compact FormIDs for ESL* can do the FormID half instead. The
patch scripts themselves were throwaway; see the refinement list.

> **The shop finds the weapon by its local FormID**, `0x000D8F`, so the repair has to come
> before the ID goes into `src/Shop.cpp`, not after.

After a repair the game folder's copy of the plugin is the stale one. Copy the repository's
back over it, and never re-save from a Creation Kit session that still has the old one
loaded.

### The first-person model is a separate record, not a separate mesh

A weapon's first-person appearance does not come from the WEAP's own model. It comes from
`WNAM`, which points at a **STAT record** that carries its own mesh. Read out of
`Skyrim.esm`:

```
WEAP  MODL: Weapons\Iron\LongSword.nif
      WNAM: 0x00036BB0  ->  STAT, EDID "1stPersonIronSword"
```

**Use the same mesh for both.** Vanilla ships a separate, denser first-person mesh — 1,019
triangles against 425 — but ours is already 10,159, so there is nothing to gain from a
second one. Most weapon mods do the same.

That still means **two records**: a STAT whose model is `isekai\weapons\systemblade.nif`,
and the WEAP's `WNAM` pointing at it.

> **A WEAP duplicated from `IronSword` inherits `WNAM` → `1stPersonIronSword`.** Change only
> `MODL` and the weapon is correct in third person and a vanilla iron sword in first, with
> nothing to warn you. Leaving `WNAM` empty is untested here; creating the STAT is one
> record and avoids finding out.

Incidentally the vanilla path confirms the convention: `Weapons\Iron\LongSword.nif` is
relative to `Data\meshes\`, so ours is `isekai\weapons\systemblade.nif` with no leading
`meshes\`.

---

## I. The shop: a catalog row and an icon

**The row.** One entry at the end of `kCatalog` in `src/Shop.cpp`:

```cpp
{ "shop.systemBlade", "Flameforged Oathblade", "x1", 40, "shop_blade.png",
  Kind::kOurItem, Cat::kSmithing, 1, Shelf::kArmaments, 0x000D8F },
```

The ARMAMENTS shelf this weapon introduced stays; the next weapon only adds a row.
`Cat::kSmithing` is the delivery category every non-pack entry uses — it decides where the
item lands in storage, not which shelf shows it.

**The card text comes from the record.** A potion's card lists its effects, and
`DescribeEffects()` used to return nothing for anything that was not a potion — a 40-point
weapon would have shown a name and a price and nothing between them. It now reads **damage
and critical damage** off a weapon (translation keys `shop.weaponDamage`,
`shop.weaponCritical`). Nothing to do per weapon.

**Two more files follow the row, and `tools/check.mjs` fails until they do:**

- `playground/mock.js` — the browser playground mirrors the catalog and the shelf list.
- `lang/template.txt` — regenerate it with `node tools/extract-strings.mjs`.

**The icon comes first.** `check.mjs` refuses a visible card whose icon file is missing, and
a card is visible as soon as its FormID is in the row. Generated with Higgsfield
(`gpt_image_2`, 1:1, two variants) from the shared style block in
`docs/SHOP_ICON_PROMPTS.md`, with a subject line that describes **the actual model** — the
card is a promise about what the player receives. The exact prompt is recorded there.

> **The generator painted the transparency checkerboard into the pixels.** Asked for a
> transparent background, it returned a 1024² RGB image with the grey-and-white grid drawn
> in. `tools/icon-cutout.py` removes only background connected to the frame border, so the
> white highlights inside the blade survive; it erodes a pixel against a white fringe,
> feathers the edge, drops stray specks and writes the 512² RGBA the shop expects:
>
> ```
> python tools/icon-cutout.py <raw>.png icons/shop_blade.png
> ```
>
> Then look at the result on a dark background, which is what the shop tile is. A fringe
> that is invisible on white is obvious there.

## J. Packaging

Nothing to do per weapon any more. `package.ps1` copies the whole `meshes\` and `textures\`
folders into the stage, and the FOMOD installs both as required files beside the ESP. Both
were missing before this weapon: the archive would have shipped a record pointing at a mesh
that was not in it.

The privacy gate walks every staged file, the NIF and the DDS files included — 96 files for
this build, all clean. Confirm the archive itself rather than the script's output:

```
7z l dist\IsekaiHero-v<version>.7z
```

Expected: the NIF, both DDS files, and the icon under `SKSE\Plugins\IsekaiHero\icons\`.

## K. Verify in game

From the packaged archive in MO2. `build.bat` carries no assets and does not reach the test
setup.

```
help "Oathblade" 4
player.additem <formid> 1
```

The list a weapon has to pass — also kept as a release check in `docs/MANUAL_TESTS.md`:

1. The shop shows it on its shelf, with its icon, damage and critical damage.
2. It can be bought, and arrives in the Dimensional Storage.
3. Equipped, it sits in the hand at a believable size beside a vanilla sword.
4. It draws and sheathes with the one-handed animation, and hangs at the hip when sheathed.
5. In first person it is **this** weapon, not an iron sword.
6. Dropped, it lands on the ground rather than falling through it.
7. Picked up again, it returns with its name and stats intact.

**Result for the System Blade, 2026-09-11:** bought from the shop and used in game, and
reported as working.

---

## Open for the refinement pass

What the first weapon left undone, or done by hand, roughly in the order it would pay off:

| What | Where it stands | Why it matters |
|---|---|---|
| A path check for the ESP | Planned as Task 5 Steps 1–2 of the plan; never built | A `MODL` naming a file nobody committed installs cleanly and shows an invisible weapon. The texture paths inside the NIF deserve the same check. |
| Stages C–E as one script | Run interactively over the Blender MCP | The longest manual stage, and the one with the most silent failures. `add-weapon-collision.py` already shows the shape: headless, arguments in, the written file verified at the end. |
| Stage F as a script | Pillow and `texconv`, by hand | Mechanical, and a wrong channel produces no error. |
| The plugin repair as a script | Two throwaway scripts in a session scratchpad | Needed after every Creation Kit session that adds records. |
| The WEAP record itself | Creation Kit, by hand | The spec deferred automating it until a known-good record existed to diff against. One does now. |
| The normal-map convention | Shipped as OpenGL, not examined on its own | See F. |

---

## Licensing

Meshy output is credited and treated as **CC BY 4.0**, the licence Meshy's terms attach to
free-plan output. That is the stricter of the two branches and correct either way: under the
free plan attribution is required, and on a paid plan it costs nothing while removing the
ambiguity the terms leave by never using the word "own" for paid customers.

`meshes/` and `textures/` are therefore carved out of the MIT grant in `LICENSE`, exactly as
the sound effects are, with the same practical effect: a fork may keep the files and may not
drop the credit. `package.ps1` stages `LICENSE` and the FOMOD installs it, so the notice
travels with the files as CC BY requires.

The shop icon is not Meshy output. It is generated artwork and falls under the MIT grant
together with the other shop icons, which `LICENSE` counts.

Vanilla meshes extracted for reference are Bethesda's. They stay outside the repository and
are never committed.
