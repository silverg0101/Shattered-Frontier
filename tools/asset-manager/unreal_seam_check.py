"""Import Asset Forge GLBs into Unreal and render them side by side, to check for shading seams.

Headless (no editor needed), from the repo root:
    & 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' GameClient/ShatteredFrontier.uproject
        -run=pythonscript "-script=tools/asset-manager/unreal_seam_check.py <out folder> <a.glb> [<b.glb> ...]"
        -unattended -nosplash -nosound
    or set SEAMCHECK_ARGS="<out folder>|<a.glb>|<b.glb>" and pass just -script=tools/asset-manager/unreal_seam_check.py
The pythonscript commandlet does not render, so for the images run it in an offscreen editor instead, which quits
when done: UnrealEditor.exe <uproject> "-ExecCmds=py <this file>" -RenderOffscreen -unattended -nosplash -nosound

Each GLB is imported as a static mesh under /Game/Frontier/Tests/AssetForge/<file stem> (replacing an earlier import),
placed left to right under a low raking key light, and captured twice into <out folder>: clay.png (one grey material,
which shows shading seams plainly) and textured.png (imported materials). stats.json records vertices/triangles.
"""
import json
import sys
from pathlib import Path

import unreal as u

import os
ARGS = sys.argv[1:] or os.environ.get("SEAMCHECK_ARGS", "").split("|")  # env form avoids -script= quoting trouble
OUT = Path(ARGS[0]).resolve()
GLBS = [Path(p).resolve() for p in ARGS[1:]]
DEST = "/Game/Frontier/Tests/AssetForge"
IN_EDITOR = "-run=" not in u.SystemLibrary.get_command_line().lower()  # commandlets cannot render
OUT.mkdir(parents=True, exist_ok=True)

meshes, stats = [], {}
for glb in GLBS:
    task = u.AssetImportTask()
    task.filename = str(glb)
    task.destination_path = f"{DEST}/{glb.stem}"
    task.automated = True
    task.replace_existing = True
    task.save = True
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    found = [a for a in u.EditorAssetLibrary.list_assets(task.destination_path, recursive=True)
             if isinstance(u.EditorAssetLibrary.load_asset(a), u.StaticMesh)]
    if not found:
        raise RuntimeError(f"no static mesh imported from {glb}")
    mesh = u.EditorAssetLibrary.load_asset(found[0])
    stats[glb.stem] = {"asset": found[0], "nanite_triangles": mesh.get_num_nanite_triangles()}
    ns = mesh.get_editor_property("nanite_settings")
    if ns.enabled:  # offscreen captures may show Nanite's coarse proxy; render the real triangles instead
        ns.enabled = False
        u.get_editor_subsystem(u.StaticMeshEditorSubsystem).set_nanite_settings(mesh, ns, apply_changes=True)
    meshes.append((glb.stem, mesh))
    stats[glb.stem].update(render_triangles=mesh.get_num_triangles(0), render_vertices=mesh.get_num_vertices(0))
    u.log(f"SEAMCHECK imported {glb.name}: {stats[glb.stem]}")

if IN_EDITOR:  # a fresh empty level, so the project's start map stays out of the shot
    u.get_editor_subsystem(u.LevelEditorSubsystem).new_level(f"{DEST}/SeamCheckMap")
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
spawned = []

def spawn(cls, loc=u.Vector(0, 0, 0), rot=u.Rotator(0, 0, 0)):
    a = actors.spawn_actor_from_class(cls, loc, rot)
    spawned.append(a)
    return a

# Imported glTF characters face +Y in Unreal: lay them out along X and look at them from +Y.
placed, x = [], 0.0
for name, mesh in meshes:
    a = spawn(u.StaticMeshActor, u.Vector(0, 0, 0))
    a.static_mesh_component.set_static_mesh(mesh)
    origin, extent = a.get_actor_bounds(False)
    a.set_actor_location(u.Vector(x + extent.x - origin.x, -origin.y, extent.z - origin.z), False, False)
    placed.append(a)
    x += extent.x * 2.6
bounds = [a.get_actor_bounds(False) for a in placed]
lo_x = min(o.x - e.x for o, e in bounds); hi_x = max(o.x + e.x for o, e in bounds)
top = max(o.z + e.z for o, e in bounds); depth = max(e.y for _, e in bounds)

key = spawn(u.DirectionalLight, rot=u.Rotator(roll=0, pitch=-20, yaw=-125))  # from the camera side, low and raking
back = spawn(u.DirectionalLight, rot=u.Rotator(roll=0, pitch=-35, yaw=-30))
back.light_component.set_intensity(4.0)
key.light_component.set_intensity(20.0)
fill = spawn(u.SkyLight)
fill.light_component.set_intensity(1.5)
fill.light_component.set_editor_property("source_type", u.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
fill.light_component.set_editor_property("cubemap", u.load_asset("/Engine/EngineResources/GrayLightTextureCube"))
fill.light_component.recapture_sky()

cam = spawn(u.SceneCapture2D, u.Vector((lo_x + hi_x) / 2, depth * 8, top / 2), u.Rotator(roll=0, pitch=0, yaw=-90))
cap = cam.capture_component2d
cap.set_editor_property("capture_source", u.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
cap.set_editor_property("projection_type", u.CameraProjectionMode.ORTHOGRAPHIC)
cap.set_editor_property("ortho_width", max(hi_x - lo_x, top * 2) * 1.08)
pp = cap.get_editor_property("post_process_settings")  # fixed exposure, so both captures are comparable
pp.set_editor_property("override_auto_exposure_method", True)
pp.set_editor_property("auto_exposure_method", u.AutoExposureMethod.AEM_MANUAL)
pp.set_editor_property("override_auto_exposure_bias", True)
pp.set_editor_property("auto_exposure_bias", 11.0)
cap.set_editor_property("post_process_settings", pp)
rt = u.RenderingLibrary.create_render_target2d(world, 2400, 1200, u.TextureRenderTargetFormat.RTF_RGBA8)
cap.set_editor_property("texture_target", rt)

clay = u.load_asset("/Engine/BasicShapes/BasicShapeMaterial")  # flat colour, no normal map: shading = mesh normals only
originals = [[c.get_material(i) for i in range(c.get_num_materials())] for c in (a.static_mesh_component for a in placed)]

def capture_all():
    for label, use_clay in (("clay", True), ("textured", False)):
        for a, mats in zip(placed, originals):
            for i, m in enumerate(mats):
                a.static_mesh_component.set_material(i, clay if use_clay else m)
        cap.capture_scene()
        u.RenderingLibrary.export_render_target(world, rt, str(OUT), f"{label}.png")
        u.log(f"SEAMCHECK wrote {OUT / (label + '.png')}")
    for a in spawned:
        actors.destroy_actor(a)
    (OUT / "stats.json").write_text(json.dumps(stats, indent=2))
    u.log("SEAMCHECK done")

if not IN_EDITOR:
    capture_all()
else:
    # The renderer needs some frames to stream Nanite and register the new lights; a same-frame capture shows black
    # proxies. Run via -ExecCmds="py ..." (-ExecutePythonScript quits a few frames after the script returns).
    state = {"t": 0.0}
    def on_tick(dt):
        state["t"] += dt
        if state["t"] < 30.0:  # also lets imported material shaders finish compiling
            return
        u.unregister_slate_post_tick_callback(handle)
        capture_all()
        u.SystemLibrary.quit_editor()
    handle = u.register_slate_post_tick_callback(on_tick)
