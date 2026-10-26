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
on the shared skeleton) with the GLB's key count. Which way the creature faces is noted in `reason`. The result is logged as
`CREATURECHECK result {...}` and POSTed as {state, reason} to <forge url>/api/models/<Name>/unreal when a url is given.
Native video2rig/direct_gait manifests instead create their own skeleton and compare against the GLB skin's joint
names (including Interchange's dot-to-underscore conversion). They never use the shared quadruped skeleton.
An isolated-project pass proves file import, joint names and clip/key preservation, not game-project or visual acceptance.
"""
import json
import math
import os
import re
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
NOTES = []  # reported in `reason` without failing the check


def glb_keys(path) -> dict:
    """{animation name: key count (its longest sampler input)} from a GLB's JSON chunk. combine_model starts every clip
    at t=0, where Unreal starts sampling, so a match means Unreal adds no held first frame."""
    b = path.read_bytes()
    n = struct.unpack_from("<I", b, 12)[0]
    js = json.loads(b[20:20 + n])
    return {a["name"]: max(js["accessors"][s["input"]]["count"] for s in a["samplers"]) for a in js.get("animations", [])}


def native_bones(path):
    """Expected native joint names; never force a bird onto the shared quadruped rig."""
    raw=path.read_bytes()
    document=json.loads(raw[20:20+struct.unpack_from('<I',raw,12)[0]])
    skins=document.get('skins',[])
    if len(skins)!=1:raise ValueError('native engine check expects one skin')
    # Interchange replaces dots in joint names (Thigh.L -> Thigh_L).
    names=[document['nodes'][i].get('name','').replace('.', '_') for i in skins[0]['joints']]
    if not all(names) or len(set(names))!=len(names):raise ValueError('native joint names must be unique')
    return names


def assets_of(cls):
    return [x for x in (lib.load_asset(p) for p in lib.list_assets(DEST, recursive=True)) if isinstance(x, cls)]


def run() -> list:
    """-> the list of problems (empty: pass)."""
    if not re.fullmatch(r"[\w-]+", NAME):  # it lands in an asset path and the POST url
        return [f"bad model name {NAME!r}"]
    man = json.loads((FOLDER / f"{NAME}.json").read_text())
    glb = FOLDER / f"{NAME}_animated.glb"
    if not glb.is_file():
        return [f"no {glb.name} in the model folder"]
    baked = [c["name"] for c in man["clips"] if c.get("baked")]
    keys = glb_keys(glb)
    native=man.get('workflow') in ('video2rig','direct_gait')
    expected=native_bones(glb) if native else BONES
    if lib.does_directory_exist(DEST) and not lib.delete_directory(DEST):
        return [f"could not clear previous import destination {DEST}"]
    skel = lib.load_asset(SHARED) if not native and lib.does_asset_exist(SHARED) else None
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
    if native:
        skel=mesh.skeleton
    elif not skel:  # the first legacy creature import makes the shared skeleton
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
    if not native and names[:1] != ["root"]:
        problems.append(f"top bone is {names[:1]}, not root")
    if sorted(names) != sorted(expected):
        problems.append(f"bones differ from source skeleton: missing {sorted(set(expected) - set(names))}, extra {sorted(set(names) - set(expected))}")
    elif "head" in names and "pelvis" in names:
        at = lambda b: u.AnimPoseExtensions.get_ref_bone_pose(pose, b, u.AnimPoseSpaces.WORLD).translation
        d = at("head") - at("pelvis")
        yaw = math.degrees(math.atan2(d.y, d.x))
        u.log(f"CREATURECHECK facing yaw {yaw:.1f} (head minus pelvis {d.x:.1f}, {d.y:.1f})")
        NOTES.append(f"faces yaw {yaw:.0f} degrees (+Y, 90, is Unreal's skeletal-mesh forward, as the Mannequin)")
    # Interchange names multi-clip imports <glb stem><Clip>, but currently labels a
    # single imported animation with the generic suffix "Anim". Map it only when
    # the GLB, manifest, and import each contain exactly that one animation.
    imported_anims=assets_of(u.AnimSequence)
    only_clip = baked[0] if len(baked) == 1 and list(keys) == baked and len(imported_anims) == 1 else None
    for a in imported_anims:  # Interchange names them <glb stem><Clip>; the project's name is <Name>_<Clip>
        if a.get_name().startswith(glb.stem):
            suffix=a.get_name()[len(glb.stem):].lstrip('_')
            if suffix=='Anim' and only_clip:
                suffix=only_clip
            lib.rename_asset(a.get_path_name().split(".")[0], f"{DEST}/{NAME}_{suffix}")
    lib.save_directory(DEST)
    anims = {a.get_name(): a for a in assets_of(u.AnimSequence)}
    want = {f"{NAME}_{c}": c for c in baked}
    for a in sorted(set(anims) - set(want)):
        problems.append(f"{a}: an AnimSequence for no baked clip")
    for a, c in want.items():
        if a not in anims:
            problems.append(f"clip {c}: baked in the manifest, but no AnimSequence {a}" +
                            ("" if c in keys else f" ({glb.name} has no animation {c})"))
            continue
        if anims[a].get_skeleton() != skel:
            problems.append(f"clip {c}: on {anims[a].get_skeleton().get_path_name()}, not {SHARED}")
        n = u.AnimationLibrary.get_num_keys(anims[a])
        if c in keys and n != keys[c]:
            problems.append(f"clip {c}: {n} keys in Unreal, {keys[c]} in the GLB" +
                            (" (its first key is after t=0, so Unreal holds the first pose)" if n == keys[c] + 1 else ""))
        u.log(f"CREATURECHECK clip {c}: {n} keys (GLB {keys.get(c)})")
    return problems


try:
    problems = run()
except Exception as e:  # a crash is a fail the page can show, not a silent pending
    u.log_error(traceback.format_exc())
    problems = [f"check crashed: {type(e).__name__}: {str(e).splitlines()[0] if str(e) else ''}"]
result = {"state": "fail" if problems else "pass", "reason": "; ".join(problems + NOTES)}
u.log(f"CREATURECHECK result {json.dumps(result)}")
if FORGE and re.fullmatch(r"[\w-]+", NAME):
    req = urllib.request.Request(f"{FORGE}/api/models/{NAME}/unreal", json.dumps(result).encode(),
                                 {"Content-Type": "application/json"}, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            u.log(f"CREATURECHECK posted to {FORGE}: {r.status}")
    except Exception as e:  # the result line above still holds the verdict
        u.log_error(f"CREATURECHECK could not post to {FORGE}: {e}")
