"""Give an exported weapon NIF its collision, BSX flags, attachment point and inventory marker.

A weapon exported straight out of Blender has none of these, and every one of them fails
silently: no collision means it drops through the floor, no BSX means the collision is
ignored anyway, no Prn means it has nowhere to sit on the body.

Rather than rebuild Havok blocks by hand, a vanilla weapon is imported as a template so
PyNifly's exact block structure and Havok parameters are inherited. Every geometric number
is then replaced with one measured from our own blade, and all vanilla meshes are deleted,
so no Bethesda geometry reaches the output — a collision box is three numbers.

Run it headless:

    blender --background --python tools/add-weapon-collision.py -- \
        --nif meshes/isekai/weapons/systemblade.nif \
        --template <extracted>/longsword.nif --root-name SystemBlade

The template must be extracted from the game's own BSA (see docs/WEAPON_ASSET_GUIDE.md);
it is Bethesda's file and never belongs in this repository.
"""

import argparse
import importlib
import json
import logging
import os
import sys
import traceback

import bmesh
import bpy
import numpy as np
from mathutils import Matrix

# The vanilla sword puts its guard this far above the origin: the origin is where the hand
# closes, not where the blade begins.
GUARD_ABOVE_ORIGIN = 5.28


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument("--nif", required=True, help="weapon NIF to add collision to, edited in place")
    p.add_argument("--template", required=True, help="vanilla weapon NIF to inherit structure from")
    p.add_argument("--root-name", default=None, help="root node name (default: the NIF's basename)")
    return p.parse_args(argv)


def world_verts(ob):
    me = ob.data
    n = len(me.vertices)
    co = np.empty(n * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    co = co.reshape(n, 3)
    m = np.array(ob.matrix_world.to_4x4())
    return (np.hstack([co, np.ones((n, 1))]) @ m.T)[:, :3]


def guard_y(v):
    """Where the guard sits: the slice with the widest X extent."""
    y = v[:, 1]
    ymin, ymax = y.min(), y.max()
    bins = 240
    idx = np.clip(((y - ymin) / (ymax - ymin) * bins).astype(int), 0, bins - 1)
    width = np.zeros(bins)
    for b in range(bins):
        m = idx == b
        if m.any():
            width[b] = v[m, 0].max() - v[m, 0].min()
    return float(ymin + (int(np.argmax(width)) + 0.5) / bins * (ymax - ymin))


def main():
    args = parse_args()
    nif_path = os.path.abspath(args.nif)
    root_name = args.root_name or os.path.splitext(os.path.basename(nif_path))[0]

    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)

    # --- template -----------------------------------------------------------
    bpy.ops.import_scene.pynifly(filepath=os.path.abspath(args.template))
    roots = [o for o in bpy.context.scene.objects if o.get("pynRoot")]
    assert roots, "template has no root node"
    van_root = roots[0]
    van_blade = max(
        (o for o in bpy.context.scene.objects
         if o.type == "MESH" and not o.name.startswith("bhk")
         and "Scb" not in o.name and "Blood" not in o.name),
        key=lambda o: len(o.data.vertices))
    van_flags = van_blade.get("pynNodeFlags", "")
    vb = world_verts(van_blade)
    van_guard = guard_y(vb)
    van_ext = vb.max(axis=0) - vb.min(axis=0)
    van_pommel, van_tip = float(vb[:, 1].min()), float(vb[:, 1].max())

    boxes = sorted((o for o in bpy.context.scene.objects if o.name.startswith("bhkBoxShape")),
                   key=lambda o: o.name)
    assert boxes, "template has no collision boxes"
    spans = []
    for b in boxes:
        bv = world_verts(b)
        spans.append({"y0": float(bv[:, 1].min()), "y1": float(bv[:, 1].max()),
                      "dx": float(bv[:, 0].max() - bv[:, 0].min()),
                      "dz": float(bv[:, 2].max() - bv[:, 2].min())})

    for o in list(bpy.context.scene.objects):
        if o.type == "MESH" and not o.name.startswith("bhk"):
            bpy.data.objects.remove(o, do_unlink=True)

    # --- our weapon ---------------------------------------------------------
    before = set(bpy.data.objects.keys())
    bpy.ops.import_scene.pynifly(filepath=nif_path)
    fresh = [bpy.data.objects[n] for n in bpy.data.objects.keys() if n not in before]
    meshes = [o for o in fresh if o.type == "MESH"]
    assert len(meshes) == 1, "expected exactly one mesh, got %d" % len(meshes)
    blade = meshes[0]
    blade.parent = van_root
    blade.matrix_parent_inverse = Matrix.Identity(4)
    for o in fresh:
        if o.type == "EMPTY":
            bpy.data.objects.remove(o, do_unlink=True)

    ov = world_verts(blade)
    blade.data.transform(Matrix.Translation((0.0, GUARD_ABOVE_ORIGIN - guard_y(ov), 0.0)))
    ov = world_verts(blade)
    pommel, tip = float(ov[:, 1].min()), float(ov[:, 1].max())
    ext = ov.max(axis=0) - ov.min(axis=0)
    g = guard_y(ov)
    sx, sz = ext[0] / van_ext[0], ext[2] / van_ext[2]

    def remap(vy):
        """Template Y -> ours, anchored on pommel, guard and tip."""
        if vy <= van_guard:
            t = (vy - van_pommel) / (van_guard - van_pommel)
            return pommel + t * (g - pommel)
        t = (vy - van_guard) / (van_tip - van_guard)
        return g + t * (tip - g)

    built = []
    for ob, s in zip(boxes, spans):
        y0, y1 = remap(s["y0"]), remap(s["y1"])
        dx, dy, dz = s["dx"] * sx, y1 - y0, s["dz"] * sz
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        bmesh.ops.scale(bm, vec=(dx, dy, dz), verts=bm.verts)
        bm.to_mesh(ob.data)
        bm.free()
        ob.location = (0.0, (y0 + y1) / 2.0, 0.0)
        ob.rotation_euler = (0.0, 0.0, 0.0)
        ob.scale = (1.0, 1.0, 1.0)
        if hasattr(ob, "pyn_collshape"):
            ob.pyn_collshape.bhkRadius *= (sx + sz) / 2.0
        built.append({"box": ob.name, "y": [round(y0, 2), round(y1, 2)],
                      "dims": [round(dx, 2), round(dy, 2), round(dz, 2)]})

    # Nothing may keep the template's editor id.
    van_root.name = root_name + ":ROOT"
    van_root["pynNodeName"] = root_name
    blade.name = root_name
    blade["pynNodeName"] = root_name
    blade["pynNodeFlags"] = van_flags

    # --- export -------------------------------------------------------------
    caught = []

    class Grab(logging.Handler):
        def emit(self, r):
            if r.exc_info:
                caught.append("".join(traceback.format_exception(*r.exc_info)))
            elif r.levelno >= logging.WARNING:
                caught.append("%s: %s" % (r.levelname, r.getMessage()))

    lg = logging.getLogger("pynifly")
    handler = Grab()
    lg.addHandler(handler)
    bpy.ops.object.select_all(action="SELECT")
    bpy.context.view_layer.objects.active = van_root
    bpy.ops.export_scene.pynifly(filepath=nif_path, target_game="SKYRIMSE",
                                 export_modifiers=True, check_existing=False,
                                 intuit_defaults=False)
    lg.removeHandler(handler)

    # --- verify the written file, never the success message -----------------
    pynifly = importlib.import_module("io_scene_nifly.pyn.pynifly")
    nif = pynifly.NifFile(nif_path)
    root = nif.rootNode
    coll = getattr(root, "collision_object", None)
    body = getattr(coll, "body", None) if coll else None
    shape = getattr(body, "shape", None) if body else None
    allv = [v for s in nif.shapes for v in s.verts]

    report = {
        "warnings": caught,
        "boxes": built,
        "game": str(nif.game),
        "root": root.name,
        "tris": sum(len(s.tris) for s in nif.shapes),
        "textures": {k: v for k, v in nif.shapes[0].textures.items() if v},
        "collision": type(shape).__name__ if shape else None,
        "collision_children": len(getattr(shape, "children", []) or []) if shape else 0,
        "y_range": [round(min(v[1] for v in allv), 2), round(max(v[1] for v in allv), 2)],
        "extent": [round(max(v[i] for v in allv) - min(v[i] for v in allv), 2) for i in range(3)],
    }
    assert report["collision_children"] == len(boxes), "collision did not survive export"
    assert report["game"] == "SKYRIMSE", "wrong game: %s" % report["game"]

    print("COLLISION_REPORT " + json.dumps(report, default=str))


main()
