"""Blender: render a rigged model's rest pose as one image, front view left, left-side view right.

    blender -b -P render_views.py -- MODEL OUT_DIR [--turn DEG]

Writes OUT_DIR/views.png (the video's first frame) and OUT_DIR/joints.json: every bone's head and tail
point, where each lands in both panels, whether it lands on the model at all, and the camera numbers that
map panel coordinates back to world.
The model must face -Y (glTF +Z, the usual export); --turn rotates it about Z first if it doesn't.
"""
import json, math, sys
from pathlib import Path

import bpy
import numpy as np
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector

W, H = 960, 1080  # one panel; the two together make a 16:9 frame
PANEL_ASPECT = W / H
GREY = 0.5


def load(model, turn=0.0):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ext = Path(model).suffix.lower()
    if ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=model)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=model)
    else:
        raise SystemExit(f"unsupported model type {ext}")
    arm = next((o for o in bpy.context.scene.objects if o.type == "ARMATURE"), None)
    if not arm:
        raise SystemExit("the model has no armature: rig it first")
    if turn:
        root = [o for o in bpy.context.scene.objects if o.parent is None]
        for o in root:
            o.rotation_euler.z += math.radians(turn)
    bpy.context.view_layer.update()
    return arm


def bounds(arm):
    """Bounds of the meshes the skeleton moves. Anything else (a rig's bone-shape widget, a stray helper) is
    hidden from the render and ignored, or a 2 m widget sphere shrinks the character to a corner of the frame."""
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    skinned = [o for o in meshes if o.parent == arm or any(m.type == "ARMATURE" and m.object == arm for m in o.modifiers)]
    for o in meshes:
        if skinned and o not in skinned:
            o.hide_render = True
    pts = [o.matrix_world @ Vector(c) for o in (skinned or meshes) for c in o.bound_box]
    lo = Vector([min(p[i] for p in pts) for i in range(3)])
    hi = Vector([max(p[i] for p in pts) for i in range(3)])
    return lo, hi


def weighted_bones(arm):
    """Names of the bones some vertex carries weight for: the ones that deform the model. glTF has no deform
    flag, so IK helpers arrive looking like any other bone; their missing weights give them away."""
    weighted = set()
    for o in bpy.context.scene.objects:
        if o.type == "MESH" and any(m.type == "ARMATURE" and m.object == arm for m in o.modifiers):
            names = {g.index: g.name for g in o.vertex_groups}
            weighted |= {names[g.group] for v in o.data.vertices for g in v.groups if g.weight > 0.01}
    return weighted


def points(arm):
    """Bone heads and tails in world space, deduplicated (a connected child's head is its parent's tail).
    Taken from the current pose, not the rest pose: an imported rig is often posed (an A-pose over a T-pose
    skeleton) and the render shows the pose."""
    pts, bones = [], []
    def pid(co):
        for i, p in enumerate(pts):
            if (p - co).length < 1e-5:
                return i
        pts.append(co.copy())
        return len(pts) - 1
    weighted = weighted_bones(arm)
    for b in arm.pose.bones:
        bones.append({"name": b.name, "parent": b.parent.name if b.parent else None,
                      "deform": b.name in weighted if weighted else b.bone.use_deform,
                      "head": pid(arm.matrix_world @ b.head), "tail": pid(arm.matrix_world @ b.tail)})
    return pts, bones


def setup(lo, hi):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 32
    scene.cycles.use_denoising = False
    prefs = bpy.context.preferences.addons["cycles"].preferences
    prefs.compute_device_type = "CUDA"
    prefs.refresh_devices()
    for d in prefs.devices:
        d.use = d.type == "CUDA"  # not the CPU: on these hosts it only slows the GPU down
    gpus = [d.name for d in prefs.devices if d.use]
    if not gpus:  # a CPU render takes minutes per frame here; fail instead of quietly crawling
        raise SystemExit("no CUDA GPU for Cycles (check CUDA_VISIBLE_DEVICES)")
    scene.cycles.device = "GPU"
    print(f"render device: {', '.join(gpus)}")
    scene.render.resolution_x, scene.render.resolution_y = W, H
    scene.render.film_transparent = False
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("grey")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (GREY, GREY, GREY, 1)
    scene.world = world
    sun = bpy.data.objects.new("key", bpy.data.lights.new("key", "SUN"))
    sun.data.energy = 3
    sun.rotation_euler = (math.radians(50), 0, math.radians(-35))
    scene.collection.objects.link(sun)

    c = (lo + hi) / 2
    size = hi - lo
    # Room for the walk to swing limbs and wings out past the rest pose.
    ortho = 1.35 * max(size.z, size.x / PANEL_ASPECT, size.y / PANEL_ASPECT)
    dist = 4 * max(size)
    cams = {}
    for name, loc, rot in (("front", (c.x, c.y - dist, c.z), (math.pi / 2, 0, 0)),
                           ("side", (c.x + dist, c.y, c.z), (math.pi / 2, 0, math.pi / 2))):
        cam = bpy.data.objects.new(name, bpy.data.cameras.new(name))
        cam.data.type = "ORTHO"
        cam.data.sensor_fit = "VERTICAL"
        cam.data.ortho_scale = ortho
        cam.data.clip_end = dist * 3
        cam.location, cam.rotation_euler = loc, rot
        scene.collection.objects.link(cam)
        cams[name] = cam
    bpy.context.view_layer.update()
    calib = {"center": list(c), "height": ortho, "width": ortho * PANEL_ASPECT, "model_height": size.z}
    return cams, calib


def project(cams, pts):
    """Full-frame normalized coordinates, origin top left: front panel is u 0..0.5, side panel u 0.5..1."""
    scene = bpy.context.scene
    out = {}
    for i, name in enumerate(("front", "side")):
        uv = []
        for p in pts:
            u, v, _ = world_to_camera_view(scene, cams[name], p)
            uv.append([0.5 * i + 0.5 * u, 1 - v])
        out[name] = uv
    return out


def render(cams, path):
    scene = bpy.context.scene
    panels = []
    for name in ("front", "side"):
        scene.camera = cams[name]
        scene.render.filepath = str(path.with_name(f"_{name}.png"))
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(scene.render.filepath)
        w, h = img.size  # the panel size times resolution_percentage
        px = np.empty(w * h * 4, np.float32)
        img.pixels.foreach_get(px)  # pixels[:] goes value by value: seconds per frame
        panels.append(px.reshape(h, w, 4))
        bpy.data.images.remove(img)
        Path(scene.render.filepath).unlink()
    both = np.concatenate(panels, axis=1)
    img = bpy.data.images.new("views", both.shape[1], both.shape[0])
    img.pixels.foreach_set(both.ravel())
    img.filepath_raw, img.file_format = str(path), "PNG"
    img.save()
    return both[::-1]  # top row first, like the uv coordinates


def seen(image, uv):
    """Per point: does it land on the model (not the plain background) in either panel? A point on empty
    background, such as a root bone on the floor between the feet, can't be tracked."""
    bg = image[2, 2, :3]
    out = []
    for i in range(len(uv["front"])):
        on = False
        for panel, (u, v) in enumerate((uv["front"][i], uv["side"][i])):
            x, y = int(u * 2 * W), int(v * H)
            if panel * W <= x < (panel + 1) * W and 0 <= y < H:  # off its own panel (an IK target far away): unseen
                on |= bool(np.abs(image[y, x, :3] - bg).max() > 0.02)
        out.append(on)
    return out


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    model, out = argv[0], Path(argv[1])
    turn = float(argv[argv.index("--turn") + 1]) if "--turn" in argv else 0.0
    out.mkdir(parents=True, exist_ok=True)
    arm = load(model, turn)
    lo, hi = bounds(arm)
    pts, bones = points(arm)
    cams, calib = setup(lo, hi)
    image = render(cams, out / "views.png")
    uv = project(cams, pts)
    (out / "joints.json").write_text(json.dumps({
        "calib": calib, "turn": turn, "points": [list(p) for p in pts], "bones": bones,
        "uv": uv, "seen": seen(image, uv)}))
    print(f"views: {len(pts)} points, {len(bones)} bones")


if __name__ == "__main__":
    main()
