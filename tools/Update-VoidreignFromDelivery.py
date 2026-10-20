"""Update the Voidreign's mesh and clips in place from a newer delivery packet (Editor Python, editor open).

    python tools/Run-InEditor.py tools/Update-VoidreignFromDelivery.py --source <Delivery folder>

Same skeleton, new skin weights and/or new clips. Nothing is deleted or renamed, so the blueprint, the hand-wired
AnimBP, blend spaces and physics asset keep their references (Import-Voidreign.py --delete would lose them):
  - SK_Voidreign is re-imported over itself (no takes), then its material, physics asset and reduction LODs restored;
  - each take is imported to a scratch folder and its local bone poses written as keys into the existing
    A_Voidreign_<clip> (root motion, sync markers and metadata stay).
"""
import argparse, hashlib, json, sys
from pathlib import Path
import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = '/Game/Frontier/Creatures/Voidreign/V1'; MESH = ROOT + '/Mesh/SK_Voidreign'; SCRATCH = ROOT + '/_Update'
p = argparse.ArgumentParser(); p.add_argument('--source', type=Path, required=True); p.add_argument('--lods', type=int, default=4)
a = p.parse_known_args(sys.argv[1:])[0]; src = a.source if a.source.is_absolute() else PROJECT / a.source
report = json.loads((src / 'delivery-report.json').read_text()); fbx = src / 'SK_Voidreign.fbx'
if report['status'] != 'delivery-export-verified' or hashlib.sha256(fbx.read_bytes()).hexdigest() != report['fbx_sha256']:
    raise RuntimeError('Delivery packet is not the verified export')
eal = unreal.EditorAssetLibrary; tools = unreal.AssetToolsHelpers.get_asset_tools()
mesh = unreal.load_asset(MESH); skel = mesh.get_editor_property('skeleton')
mats = [(m.get_editor_property('material_slot_name'), m.get_editor_property('material_interface')) for m in mesh.get_editor_property('materials')]
physics = mesh.get_editor_property('physics_asset')


def options(import_mesh):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', import_mesh); ui.set_editor_property('import_as_skeletal', True)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH if import_mesh else unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property('skeleton', skel); ui.set_editor_property('import_animations', not import_mesh)
    ui.set_editor_property('import_materials', False); ui.set_editor_property('import_textures', False); ui.set_editor_property('create_physics_asset', False)
    ui.skeletal_mesh_import_data.set_editor_property('import_morph_targets', False)
    ui.skeletal_mesh_import_data.set_editor_property('convert_scene_unit', True); ui.anim_sequence_import_data.set_editor_property('convert_scene_unit', True)
    ui.anim_sequence_import_data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    ui.anim_sequence_import_data.set_editor_property('import_bone_tracks', True)
    return ui


def run(dest, name, ui, replace):
    t = unreal.AssetImportTask()
    for k, v in dict(filename=str(fbx), destination_path=dest, automated=True, save=False, replace_existing=replace, options=ui).items():
        t.set_editor_property(k, v)
    if name: t.set_editor_property('destination_name', name)
    tools.import_asset_tasks([t])


# Mesh over itself, then what the importer resets.
run(ROOT + '/Mesh', 'SK_Voidreign', options(True), True)
mesh = unreal.load_asset(MESH)
mesh.set_editor_property('materials', [unreal.SkeletalMaterial(material_interface=m, material_slot_name=s) for s, m in mats])
if physics: mesh.set_editor_property('physics_asset', physics)
sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem); sub.regenerate_lod(mesh, a.lods, False, False)
eal.save_asset(MESH, only_if_is_dirty=False)

# Clips: keys copied into the existing sequences.
if eal.does_directory_exist(SCRATCH): eal.delete_directory(SCRATCH)
run(SCRATCH, None, options(False), False)
reg = unreal.AssetRegistryHelpers.get_asset_registry(); reg.scan_paths_synchronous([SCRATCH], True)
fresh = [d.get_asset() for d in reg.get_assets_by_path(SCRATCH, recursive=True)]
fresh = {x.get_name(): x for x in fresh if isinstance(x, unreal.AnimSequence)}
updated, missing = [], []
opts = unreal.AnimPoseEvaluationOptions(); LOCAL = unreal.AnimPoseSpaces.LOCAL
for cid in report['takes']:
    key = cid.replace('.', '_'); new = next((s for n, s in fresh.items() if n.endswith(key)), None)
    old = unreal.load_asset(f'{ROOT}/Animations/A_Voidreign_{key}')
    if not (new and old): missing.append(cid); continue
    n = unreal.AnimationLibrary.get_num_frames(new) - 1  # sampled keys; the model counts frame intervals
    bones = [str(b) for b in unreal.AnimationLibrary.get_animation_track_names(new)]
    poses = [unreal.AnimPoseExtensions.get_anim_pose_at_frame(new, f, opts) for f in range(n + 1)]
    ctl = old.controller; ctl.open_bracket('Voidreign clip update')
    ctl.set_number_of_frames(unreal.FrameNumber(n))
    have = {str(b) for b in unreal.AnimationLibrary.get_animation_track_names(old)}
    for b in bones:
        if b not in have: ctl.add_bone_track(b)
        tf = [unreal.AnimPoseExtensions.get_bone_pose(pz, b, LOCAL) for pz in poses]
        ctl.set_bone_track_keys(b, [t.translation for t in tf], [t.rotation for t in tf], [t.scale3d for t in tf])
    ctl.close_bracket()
    eal.save_loaded_asset(old, only_if_is_dirty=False); updated.append(cid)
eal.delete_directory(SCRATCH)
unreal.log('VOIDREIGN_UPDATE ' + json.dumps(dict(mesh=MESH, updated=len(updated), takes=len(report['takes']), missing=missing[:5],
                                                 lods=sub.get_lod_count(mesh), physics=bool(mesh.get_editor_property('physics_asset')))))
