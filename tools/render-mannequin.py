"""Render a Skyrim reference body as a plain grey mannequin, the Picture 1 of the dress workflow.

The armour is generated worn on this exact body in its bind pose, so the mannequin must be the
reference body itself: the body NIF, plus vanilla head, hands and feet. Everything is skinned to
the same skeleton, so the parts meet at neck, wrists and ankles without being moved.

Run it headless:

    blender --background --python tools/render-mannequin.py -- \
        --body "<CBBE 3BA Ref.nif>" --head <femalehead.nif> --hands <femalehands_1.nif> \
        --feet <femalefeet_1.nif> --out <3ba.png>

The meshes are other mod authors' work and Bethesda's; they and the render stay outside the
repository.
"""

import argparse
import json
import math
import os
import sys

import bpy
import numpy as np

SIZE = 1024
FILL = 0.9       # figure height as a share of the frame
FIGURE = 0.35    # matte mid grey; Meshy fails on a background close to the object's colour
BACKGROUND = 0.8


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument("--body", required=True)
    p.add_argument("--shape", default=None, help="keep only this shape from the body NIF")
    p.add_argument("--head", required=True)
    p.add_argument("--hands", required=True)
    p.add_argument("--feet", required=True)
    p.add_argument("--out", required=True)
    return p.parse_args(argv)


def import_meshes(path, shape=None):
    before = set(bpy.data.objects.keys())
    bpy.ops.import_scene.pynifly(filepath=os.path.abspath(path))
    fresh = [bpy.data.objects[n] for n in bpy.data.objects.keys() if n not in before]
    meshes = [o for o in fresh if o.type == "MESH" and not o.name.startswith("bhk")]
    if shape:
        meshes = [o for o in meshes if o.get("pynNodeName", o.name) == shape]
    assert meshes, "no mesh %sin %s" % ("named %r " % shape if shape else "", path)
    return meshes


def world_verts(ob):
    me = ob.data
    n = len(me.vertices)
    co = np.empty(n * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    co = co.reshape(n, 3)
    m = np.array(ob.matrix_world.to_4x4())
    return (np.hstack([co, np.ones((n, 1))]) @ m.T)[:, :3]


def main():
    args = parse_args()
    out = os.path.abspath(args.out)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)

    parts = {
        "body": import_meshes(args.body, args.shape),
        "head": import_meshes(args.head),
        "hands": import_meshes(args.hands),
        "feet": import_meshes(args.feet),
    }
    keep = [o for group in parts.values() for o in group]

    # The rest mesh is the bind pose: detach from the imported armatures, drop their modifiers.
    for ob in keep:
        mw = ob.matrix_world.copy()
        ob.parent = None
        ob.matrix_world = mw
        for mod in list(ob.modifiers):
            ob.modifiers.remove(mod)
    for o in list(bpy.data.objects):
        if o not in keep:
            bpy.data.objects.remove(o, do_unlink=True)

    allv = np.vstack([world_verts(o) for o in keep])
    lo, hi = allv.min(axis=0), allv.max(axis=0)
    centre = (lo + hi) / 2.0
    height, width = hi[2] - lo[2], hi[0] - lo[0]
    assert width < height, "figure wider than tall — the arms are not in the bind pose"

    # Toes reach further from the ankle than the heel does: that side is the front.
    feet = np.vstack([world_verts(o) for o in parts["feet"]])
    ankle = feet[feet[:, 2] > feet[:, 2].max() - 0.2 * np.ptp(feet[:, 2])]
    ankle_y = ankle[:, 1].mean()
    forward = 1.0 if feet[:, 1].max() - ankle_y > ankle_y - feet[:, 1].min() else -1.0

    scene = bpy.context.scene
    cam_data = bpy.data.cameras.new("Mannequin")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = height / FILL
    distance = np.ptp(allv[:, 1]) + 200.0
    cam_data.clip_end = distance * 4.0
    cam = bpy.data.objects.new("Mannequin", cam_data)
    scene.collection.objects.link(cam)
    cam.location = (centre[0], centre[1] + forward * distance, centre[2])
    # A camera looks down -Z; +90° about X turns that to +Y, a further 180° about Z to -Y.
    cam.rotation_euler = (math.pi / 2.0, 0.0, math.pi if forward > 0 else 0.0)
    scene.camera = cam

    if scene.world is None:
        scene.world = bpy.data.worlds.new("Mannequin")
    scene.world.color = (BACKGROUND,) * 3
    shading = scene.display.shading
    shading.light = "STUDIO"
    shading.color_type = "SINGLE"
    shading.single_color = (FIGURE,) * 3
    scene.view_settings.view_transform = "Standard"

    r = scene.render
    r.engine = "BLENDER_WORKBENCH"
    r.resolution_x = r.resolution_y = SIZE
    r.resolution_percentage = 100
    r.film_transparent = False
    r.image_settings.file_format = "PNG"
    r.image_settings.color_mode = "RGB"
    r.filepath = out
    bpy.ops.render.render(write_still=True)

    # --- verify the written file, never the render call --------------------------
    img = bpy.data.images.load(out)
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]
    figure = np.abs(px - px[0, 0]).max(axis=2) > 0.02
    report = {
        "out": os.path.basename(out),
        "size": [w, h],
        "forward": "+Y" if forward > 0 else "-Y",
        "height": round(float(height), 2),
        "width": round(float(width), 2),
        "meshes": {k: [o.name for o in v] for k, v in parts.items()},
        "figure_share": round(float(figure.mean()), 3),
        "border_clear": bool(not (figure[0].any() or figure[-1].any()
                                  or figure[:, 0].any() or figure[:, -1].any())),
    }
    assert [w, h] == [SIZE, SIZE], "wrong size %s" % [w, h]
    assert figure.any(), "nothing rendered — background only"
    assert report["border_clear"], "figure cut at the frame's border"
    print("MANNEQUIN_REPORT " + json.dumps(report))


main()
