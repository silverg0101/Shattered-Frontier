"""Import rider takes onto an existing Unreal skeleton and verify the mount's rider seat bone (Editor Python).

    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended
        -script="<project>/tools/Import-RiderClips.py --source <RiderClips folder> --mount /Game/.../SK_Voidreign --dest /Game/.../Rider"

Checks: every take imported, seated pelvis height above the socket matches the authored offset (axis/unit
conversion), seat bone present on the mount. Writes a manifest under Artifacts/Art/Voidreign.
"""
import json, shlex
from pathlib import Path
import unreal

PROJECT = Path(__file__).resolve().parents[1]
toks = shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)
i = next(i for i, t in enumerate(toks) if 'Import-RiderClips.py' in t)
argv = [t.strip('"') for t in toks[i + 1:]]
arg = lambda k: argv[argv.index(k) + 1]
src, mount_path, dest = Path(arg('--source')), arg('--mount'), arg('--dest')
rep = json.loads((src / 'rider-report.json').read_text())
if not rep['passed']: raise RuntimeError('Rider clips did not pass their checks')
if '--skip-socket-check' in argv: pass
if unreal.EditorAssetLibrary.does_directory_exist(dest): raise RuntimeError('Refusing to overwrite ' + dest)
skel = unreal.load_asset(rep['skeleton'].split('.')[0]) or unreal.load_asset(rep['skeleton'])
mount = unreal.load_asset(mount_path)
if skel is None or mount is None: raise RuntimeError('Missing skeleton or mount')
unreal.EditorAssetLibrary.make_directory(dest)

ui = unreal.FbxImportUI()
ui.set_editor_property('import_mesh', False); ui.set_editor_property('import_animations', True); ui.set_editor_property('import_materials', False)
ui.set_editor_property('import_textures', False); ui.set_editor_property('skeleton', skel)
ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_ANIMATION)
ui.anim_sequence_import_data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
ui.anim_sequence_import_data.set_editor_property('convert_scene_unit', '--convert-units' in argv)
t = unreal.AssetImportTask()
for k, v in [('filename', str(src / 'Rider-Mannequin.fbx')), ('destination_path', dest), ('automated', True), ('save', False), ('options', ui)]:
    t.set_editor_property(k, v)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
anims = [unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(dest, recursive=False)]
anims = [x for x in anims if isinstance(x, unreal.AnimSequence)]
clips = {c['id']: c for c in rep['clips']}; got = {}
for anim in anims:
    cid = next((c for c in clips if anim.get_name().endswith(c.replace('.', '_')) or anim.get_name().endswith(c)), None)
    if cid is None: continue
    for ev, fr in (clips[cid].get('events') or {}).items():
        unreal.AnimationLibrary.add_animation_notify_track(anim, 'Events')
        unreal.AnimationLibrary.add_animation_sync_marker(anim, ev, (fr - 1) / 30.0, 'Events')
    new = f"{dest}/A_Rider_{cid.replace('.', '_')}"; unreal.EditorAssetLibrary.rename_asset(anim.get_path_name(), new); got[cid] = new

idle = unreal.load_asset(got['rider.seat.idle']) if 'rider.seat.idle' in got else None
pelvis_cm = unreal.AnimationLibrary.get_bone_pose_for_frame(idle, 'pelvis', 0, False).translation if idle else None

sock_cfg = rep['socket']  # attachment is the mount bone named like the socket (Python cannot author socket names)
mount_bones = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(unreal.AnimPoseExtensions.get_reference_pose(mount.get_editor_property('skeleton')))]
unreal.EditorAssetLibrary.save_directory(dest, only_if_is_dirty=False, recursive=True)

expected_z = 100 * (0.968 - 0.85)  # seated pelvis offset authored in make-profile (rest pelvis 96.8 cm, lowered 85 cm)
checks = dict(all_takes=len(got) == len(clips), seat_bone=sock_cfg['name'] in mount_bones,
              seated_pelvis_height=pelvis_cm is not None and abs(pelvis_cm.z - expected_z) < 3.0)
manifest = dict(schema_version=1, source=str(src), fbx_sha256=rep['fbx_sha256'], skeleton=skel.get_path_name(), mount=mount_path,
                animations=got, socket=sock_cfg, seated_pelvis_cm=[pelvis_cm.x, pelvis_cm.y, pelvis_cm.z] if pelvis_cm else None,
                checks=checks, status='imported-verified' if all(checks.values()) else 'imported-needs-review')
out = PROJECT / 'Artifacts' / 'Art' / 'Voidreign' / 'unreal-rider-import.json'; out.write_text(json.dumps(manifest, indent=1))
unreal.log('RIDER_IMPORT ' + json.dumps(dict(status=manifest['status'], checks=checks, pelvis=manifest['seated_pelvis_cm'])))
