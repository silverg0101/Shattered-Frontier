"""Import a creature model folder's <Name>_animated.glb into Unreal and check it, then POST pass/fail to the Forge.

Unreal runs on the desktop, not on the Forge server, so a creature job finishes with `unreal: pending` and this tool
flips it. Headless (editor closed), from the repo root:
    & 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' GameClient/ShatteredFrontier.uproject
        -run=pythonscript "-script=tools/asset-manager/unreal_creature_check.py <model folder> [<forge url>]"
        -unattended -nosplash -nosound -nullrhi
    or set CREATURECHECK_ARGS="<model folder>|<forge url>" and pass just -script=tools/asset-manager/unreal_creature_check.py
With the editor open: python tools/Run-InEditor.py tools/asset-manager/unreal_creature_check.py <model folder> [<forge url>]

<model folder> is a local copy of Generated/Creatures/<Name>/ (manifest <Name>.json). The GLB is imported as a skeletal
mesh under /Game/Frontier/Tests/AssetForge/<Name> (replacing an earlier import) against the shared skeleton
/Game/Frontier/Creatures/Shared/SK_Quadruped_v1, which the first import creates when it is missing. Checks: the bone
list is quadruped_v1's with `root` on top, one AnimSequence per manifest clip with `baked: true` (named <Name>_<Clip>,
on the shared skeleton) with the GLB's frame count. A creature not facing Unreal +X is noted in `reason`. The result is logged as
`CREATURECHECK result {...}` and POSTed as {state, reason} to <forge url>/api/models/<Name>/unreal when a url is given.
"""
import json
import math
import os
import struct
import sys
import traceback
import urllib.request
from pathlib import Path

import unreal as u

ARGS = [a for a in (sys.argv[1:] or os.environ.get("CREATURECHECK_ARGS", "").split("|")) if a]
FOLDER = Path(ARGS[0]).resolve()
FORGE = ARGS[1].rstrip("/") if len(ARGS) > 1 else ""
NAME = FOLDER.name
DEST = f"/Game/Frontier/Tests/AssetForge/{NAME}"
SHARED = "/Game/Frontier/Creatures/Shared/SK_Quadruped_v1"
# quadruped_v1, as motion/zoo_curate.py BONES (that module needs numpy, which Unreal's Python lacks)
BONES = ["root", "pelvis", "spine_01", "spine_02", "spine_03", "neck", "head", *[f"tail_{i:02d}" for i in range(1, 7)],
         *[f"{leg}_{i:02d}" for leg in ("FL", "FR", "HL", "HR") for i in range(1, 5)]]
lib = u.EditorAssetLibrary
FPS = 30  # ponytail: the Forge bakes every clip at 30 fps; read the rate from the GLB if that changes
NOTES = []  # reported in `reason` without failing the check


def glb_frames(path) -> dict:
    """{animation name: frame count} from a GLB's JSON chunk: its last key time at FPS. Blender's keys start at frame
    1, Unreal's at 0, so the key counts differ by one while the frames match."""
    b = path.read_bytes()
    n = struct.unpack_from("<I", b, 12)[0]
    js = json.loads(b[20:20 + n])
    return {a["name"]: round(max(js["accessors"][s["input"]]["max"][0] for s in a["samplers"]) * FPS)
            for a in js.get("animations", [])}


def assets_of(cls):
    return [x for x in (lib.load_asset(p) for p in lib.list_assets(DEST, recursive=True)) if isinstance(x, cls)]


def run() -> list:
    """-> the list of problems (empty: pass)."""
    man = json.loads((FOLDER / f"{NAME}.json").read_text())
    glb = FOLDER / f"{NAME}_animated.glb"
    if not glb.is_file():
        return [f"no {glb.name} in the model folder"]
    baked = [c["name"] for c in man["clips"] if c.get("baked")]
    frames = glb_frames(glb)
    if lib.does_directory_exist(DEST):
        lib.delete_directory(DEST)
    skel = lib.load_asset(SHARED) if lib.does_asset_exist(SHARED) else None
    pipe = u.InterchangeGenericAssetsPipeline()
    if skel:
        pipe.common_skeletal_meshes_and_animations_properties.skeleton = skel
    params = u.ImportAssetParameters()  # AssetImportTask.options doesn't reach Interchange; an override pipeline does
    params.is_automated = True
    params.override_pipelines = [u.SoftObjectPath(pipe.get_path_name())]
    u.InterchangeManager.get_interchange_manager_scripted().import_asset(
        DEST, u.InterchangeManager.create_source_data(str(glb)), params)
    lib.save_directory(DEST)
    meshes = assets_of(u.SkeletalMesh)
    if not meshes:
        return [f"no skeletal mesh imported from {glb.name}"]
    mesh = meshes[0]
    if not skel:  # the first creature import makes the shared skeleton
        made = assets_of(u.Skeleton)
        if not made or not lib.rename_asset(made[0].get_path_name().split(".")[0], SHARED):
            return ["could not create the shared skeleton " + SHARED]
        lib.save_asset(SHARED)
        lib.save_directory(DEST)  # the mesh and clips now point at SHARED; unsaved, they'd keep the moved path
        skel = lib.load_asset(SHARED)
        u.log(f"CREATURECHECK created {SHARED}")
    problems = []
    if mesh.skeleton != skel:
        problems.append(f"the mesh uses {mesh.skeleton.get_path_name()}, not {SHARED}")
    pose = mesh.skeleton.get_reference_pose()
    names = [str(n) for n in u.AnimPoseExtensions.get_bone_names(pose)]
    if names[:1] != ["root"]:
        problems.append(f"top bone is {names[:1]}, not root")
    if sorted(names) != sorted(BONES):
        problems.append(f"bones differ from quadruped_v1: missing {sorted(set(BONES) - set(names))}, extra {sorted(set(names) - set(BONES))}")
    elif "head" in names and "pelvis" in names:
        at = lambda b: u.AnimPoseExtensions.get_ref_bone_pose(pose, b, u.AnimPoseSpaces.WORLD).translation
        d = at("head") - at("pelvis")
        yaw = math.degrees(math.atan2(d.y, d.x))
        u.log(f"CREATURECHECK facing yaw {yaw:.1f} (head minus pelvis {d.x:.1f}, {d.y:.1f})")
        if abs(yaw) > 30:  # a note, not a fail: every glTF import turns the same way, so the fix is a project decision
            NOTES.append(f"faces yaw {yaw:.0f} degrees, not +X: needs {-yaw:.0f} degrees about Z")
    for a in assets_of(u.AnimSequence):  # Interchange names them <glb stem><Clip>; the project's name is <Name>_<Clip>
        if a.get_name().startswith(glb.stem):
            lib.rename_asset(a.get_path_name().split(".")[0], f"{DEST}/{NAME}_{a.get_name()[len(glb.stem):].lstrip('_')}")
    lib.save_directory(DEST)
    anims = {a.get_name(): a for a in assets_of(u.AnimSequence)}
    want = {f"{NAME}_{c}": c for c in baked}
    for a in sorted(set(anims) - set(want)):
        problems.append(f"{a}: an AnimSequence for no baked clip")
    for a, c in want.items():
        if a not in anims:
            problems.append(f"clip {c}: baked in the manifest, but no AnimSequence {a}" +
                            ("" if c in frames else f" ({glb.name} has no animation {c})"))
            continue
        if anims[a].get_skeleton() != skel:
            problems.append(f"clip {c}: on {anims[a].get_skeleton().get_path_name()}, not {SHARED}")
        n = u.AnimationLibrary.get_num_frames(anims[a])
        if c in frames and n != frames[c]:
            problems.append(f"clip {c}: {n} frames in Unreal, {frames[c]} in the GLB")
        u.log(f"CREATURECHECK clip {c}: {n} frames (GLB {frames.get(c)})")
    return problems


try:
    problems = run()
except Exception as e:  # a crash is a fail the page can show, not a silent pending
    u.log_error(traceback.format_exc())
    problems = [f"check crashed: {type(e).__name__}: {str(e).splitlines()[0] if str(e) else ''}"]
result = {"state": "fail" if problems else "pass", "reason": "; ".join(problems + NOTES)}
u.log(f"CREATURECHECK result {json.dumps(result)}")
if FORGE:
    req = urllib.request.Request(f"{FORGE}/api/models/{NAME}/unreal", json.dumps(result).encode(),
                                 {"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=30) as r:
        u.log(f"CREATURECHECK posted to {FORGE}: {r.status}")
