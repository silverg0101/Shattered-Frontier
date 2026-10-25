"""blender -b -P clip_sheet.py -- CLIP.glb CLIP.json OUT.png
Contact sheet of a retargeted clip (zoo_retarget.py): 8 side views across the clip (top two rows, head to the right),
4 front views (third row), orthographic on a floor plane with a red floor line, then a plot of each paw's height
above the floor per frame (CLIP.json heights_m; planted frames marked).
python clip_sheet.py --review OUT.png CLIP.json [PROMPT]   Claude checks the sheet (ANTHROPIC_API_KEY) against PROMPT
(what the motion should show; default the clip name): prints REVIEW PASS or REVIEW FAIL: <reason>."""
import json, math, sys
from pathlib import Path

W, H = 520, 360
PAW_RGB = {"FL": (0.12, 0.47, 0.71), "FR": (1.0, 0.5, 0.05), "HL": (0.17, 0.63, 0.17), "HR": (0.84, 0.15, 0.16)}


def plot(meta, width, height=300):
    """The paw-height plot as an RGBA float array (rows bottom-up, as Blender images are)."""
    import numpy as np
    img = np.ones((height, width, 4), np.float32); img[..., :3] = 0.97
    hs = meta["heights_m"]; n = len(next(iter(hs.values())))
    top = max(0.01, max(max(v) for v in hs.values())) * 1.1
    bot = min(0.0, min(min(v) for v in hs.values())) - 0.002
    x0, x1, y0, y1 = 50, width - 20, 30, height - 20
    X = lambda i: x0 + (x1 - x0) * i / max(1, n - 1)
    Y = lambda h: y0 + (y1 - y0) * (h - bot) / (top - bot)
    def line(xa, ya, xb, yb, rgb, w=1):
        for t in np.linspace(0, 1, int(max(abs(xb - xa), abs(yb - ya)) * 2) + 2):
            x, y = int(round(xa + (xb - xa) * t)), int(round(ya + (yb - ya) * t))
            img[max(0, y - w):y + w + 1, max(0, x - w):x + w + 1, :3] = rgb
    for cm in range(0, int(top * 100) + 1, 2):  # grid every 2 cm
        line(x0, Y(cm / 100), x1, Y(cm / 100), (0.85, 0.85, 0.85), 0)
    for i in range(n):
        line(X(i), y0, X(i), y0 + 6, (0.4, 0.4, 0.4), 0)
    line(x0, Y(0), x1, Y(0), (1, 0, 0), 1)  # the floor
    for k, (p, v) in enumerate(hs.items()):
        rgb = PAW_RGB.get(p, (0, 0, 0))
        for i in range(n - 1):
            line(X(i), Y(v[i]), X(i + 1), Y(v[i + 1]), rgb, 1)
        planted = meta.get("contacts", {}).get(p, "")
        for i, c in enumerate(planted):  # planted frames: a block under the axis, one row per paw
            if c == "#":
                img[8 + 5 * k:12 + 5 * k, int(X(i)) - 3:int(X(i)) + 4, :3] = rgb
        img[height - 14:height - 6, x0 + 60 * k:x0 + 60 * k + 40, :3] = rgb  # legend swatches: FL FR HL HR, left to right
    return img


def sheet(glb, meta_path, out):
    import bpy, numpy as np
    from mathutils import Vector
    sys.path.insert(0, str(Path(__file__).parent))
    import render_views as rv
    meta = json.loads(Path(meta_path).read_text())
    arm = rv.load(glb, fps=meta.get("fps", 30))
    sc = bpy.context.scene
    for o in list(sc.objects):
        if o.type == "MESH" and o.name.startswith("Icosphere"): bpy.data.objects.remove(o)
    lo, hi = rv.bounds(arm)
    ground = lo.z  # the rest-pose floor zoo_retarget measured heights from
    c, size = (lo + hi) / 2, hi - lo
    act = arm.animation_data.action; f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])

    sc.render.engine = "CYCLES"; sc.cycles.samples = 16; sc.cycles.use_denoising = False
    prefs = bpy.context.preferences.addons["cycles"].preferences; prefs.compute_device_type = "CUDA"; prefs.refresh_devices()
    for d in prefs.devices: d.use = d.type == "CUDA"
    sc.cycles.device = "GPU"
    sc.render.resolution_x, sc.render.resolution_y = W, H; sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w"); world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.6, 0.62, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
    sc.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN")); sun.data.energy = 3
    sun.rotation_euler = (math.radians(35), 0, math.radians(-60)); sc.collection.objects.link(sun)
    L = max(size.y * 1.5, size.z * 1.45 * W / H)
    bpy.ops.mesh.primitive_plane_add(size=L * 6, location=(c.x, c.y, ground))
    pl = bpy.context.object; m = bpy.data.materials.new("g"); m.use_nodes = True
    m.node_tree.nodes["Principled BSDF"].inputs[0].default_value = (0.35, 0.4, 0.35, 1); pl.data.materials.append(m)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(c.x + L, c.y, ground)); ln = bpy.context.object  # floor line, edge-on
    ln.scale = (L * 0.002, L * 6, L * 0.002); mr = bpy.data.materials.new("r"); mr.use_nodes = True
    mr.node_tree.nodes["Principled BSDF"].inputs[0].default_value = (1, 0, 0, 1); ln.data.materials.append(mr)

    def cam(name, loc, rot):
        o = bpy.data.objects.new(name, bpy.data.cameras.new(name)); o.data.type = "ORTHO"; o.data.ortho_scale = L
        o.location, o.rotation_euler = loc, rot; o.data.clip_end = L * 20; sc.collection.objects.link(o); return o
    zc = (ground + hi.z) / 2 + 0.05 * size.z
    side = cam("side", (c.x - L * 5, c.y, zc), (math.radians(90), 0, math.radians(-90)))  # looks +X: the right side, head to the right
    front = cam("front", (c.x, c.y - L * 5, zc), (math.radians(90), 0, 0))  # looks +Y, at the face

    tmp = str(Path(out).with_suffix(".shot.png"))
    def shot(camo, fr):
        sc.camera = camo; sc.frame_set(fr); sc.render.filepath = tmp
        try:
            bpy.ops.render.render(write_still=True)
        except RuntimeError as e:  # the GPU is full (UniMate, ComfyUI): these 12 small frames render fine on the CPU
            if sc.cycles.device != "GPU": raise
            print("SHEET GPU render failed, rendering on the CPU:", str(e).strip().splitlines()[-1][:120])
            sc.cycles.device = "CPU"; bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(tmp); w, h = img.size
        px = np.empty(w * h * 4, np.float32); img.pixels.foreach_get(px); bpy.data.images.remove(img)
        return px.reshape(h, w, 4)
    n = f1 - f0
    side_frames = [f0 + round(i * n / 8) for i in range(8)]
    tiles = [shot(side, fr) for fr in side_frames]
    tiles += [shot(front, fr) for fr in side_frames[::2]]
    Path(tmp).unlink(missing_ok=True)
    rows = [np.concatenate(tiles[i:i + 4], axis=1) for i in (8, 4, 0)]  # pixel rows go bottom-up
    img = np.concatenate([plot(meta, 4 * W)] + rows, axis=0)
    im = bpy.data.images.new("sheet", img.shape[1], img.shape[0]); im.pixels.foreach_set(img.ravel())
    im.filepath_raw, im.file_format = out, "PNG"; im.save()
    print("SHEET", out, "side frames", side_frames)


def review(sheet_path, meta_path, prompt=None):
    import base64
    sys.path.insert(0, str(Path(__file__).parent))
    from zoo_curate import claude
    meta = json.loads(Path(meta_path).read_text())
    keys = ("min_paw_cm", "max_slide_cm", "seam_deg", "flips", "loop_gap_cm", "max_pitch_deg", "max_roll_deg", "max_neck_deg", "released_pct")
    m = {k: meta[k] for k in keys + (("travel_cm_s",) if meta.get("loop") else ()) if k in meta}
    tread = (f"The loop plays in place (travel_cm_s is the speed the game moves it at), so in a gait the planted paws "
             "sweep back under the body like on a treadmill while the body stays put: that is correct, not sliding. "
             if meta.get("loop") and m.get("travel_cm_s") else "")
    img = {"type": "image", "source": {"type": "base64", "media_type": "image/png",
                                       "data": base64.b64encode(Path(sheet_path).read_bytes()).decode()}}
    want = (f"The motion was asked for as: \"{prompt}\". It must show that, specifically: a different action, "
            "or the right action done in a clearly different way (for example rearing up and sniffing the air when "
            "asked to sniff the ground), is a FAIL even when it moves cleanly. ") if prompt else ""
    text = claude([img, {"type": "text", "text": (
        f"A library motion clip, \"{meta['clip']}\" ({'a loop' if meta['loop'] else 'a one-shot'}, {meta['frames']} frames "
        f"at {meta['fps']} fps), was retargeted onto a generated quadruped creature for a game. The sheet shows the "
        "creature from its right side at 8 evenly spaced frames (top two rows, read left to right, head to the right), "
        "from the front at 4 of those frames (third row), and a plot of each paw's height above the floor per frame "
        "(bottom; blue FL, orange FR, green HL, red HR; the red line is the floor; coloured blocks under the axis mark the "
        f"frames each paw is planted). Measured: {json.dumps(m)}.\n\n{tread}{want}Judge whether the motion is usable as it is: paws "
        "on or above the floor (red line) and planted while in contact, legs bending the natural way with no "
        "twisted, inverted or dislocated joints, the body and head in a believable pose for this motion, the tail "
        "attached and not through the floor, and the motion recognisably the named action. Ignore lighting, "
        "texture and model quality. Answer with one line only: PASS, or FAIL: <the reason>.")}], effort="medium")
    verdict = text.strip().splitlines()[-1].strip() if text.strip() else "FAIL: empty review"
    if not verdict.upper().startswith(("PASS", "FAIL")):
        verdict = f"FAIL: {verdict}"
    print("REVIEW", verdict)


if __name__ == "__main__":
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    if a[0] == "--review":
        review(a[1], a[2], a[3] if len(a) > 3 else None)
    else:
        try:
            sheet(*a[:3])
        except Exception:  # Blender exits 0 after a script error; the job must see the failure
            import traceback; traceback.print_exc(); sys.exit(1)
