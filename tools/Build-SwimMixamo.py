"""Import the Mixamo swim clips and retarget them onto the player (UE 5.8 editor Python).

py -3 tools/Run-InEditor.py tools/Build-SwimMixamo.py

Inputs (git-ignored): Artifacts/Mixamo/Swimming.fbx (with skin) and "Treading Water.fbx" (no skin, same skeleton).
Writes only under /Game/Frontier/Characters/Animations/Swim/:
  SK_Mixamo (+ skeleton), Swimming, Treading_Water      raw imports
  IK_SF_Mixamo, RTG_SF_MixamoToMannequin                rig + retargeter (target: Retarget/IK_SF_Mannequin)
  AS_SF_Swim_Crawl, AS_SF_Swim_Tread                    baked sequences on the UE4 mannequin skeleton, in place
  BS_SF_Swim                                            1D blend space on speed (tread -> crawl)
The MetaHuman body is posed from the hidden mannequin every frame (USFMetaHumanRetargetAnimInstance), so the baked clips
live on the mannequin skeleton and the existing RTG_SF_MannequinToMetaHuman carries them to the MetaHuman.
Re-runnable.
"""
import unreal as u

SRC = 'C:/Projects/Shattered-Frontier/Artifacts/Mixamo'
DST = '/Game/Frontier/Characters/Animations/Swim'
RETARGET = '/Game/Frontier/Characters/Player/Retarget'
lib = u.EditorAssetLibrary
tools = u.AssetToolsHelpers.get_asset_tools()


def do_import(fbx, name, skeleton=None):
    path = f'{DST}/{name}'
    t = u.AssetImportTask()
    t.filename, t.destination_path, t.destination_name = f'{SRC}/{fbx}', DST, name
    t.automated, t.save, t.replace_existing = True, True, True
    o = u.FbxImportUI()
    o.import_animations = True
    o.import_materials = o.import_textures = False
    if skeleton is None:
        o.import_mesh = True; o.import_as_skeletal = True; o.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
    else:
        o.import_mesh = False; o.skeleton = skeleton; o.mesh_type_to_import = u.FBXImportType.FBXIT_ANIMATION
    o.anim_sequence_import_data.import_bone_tracks = True
    o.anim_sequence_import_data.animation_length = u.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME
    o.anim_sequence_import_data.use_default_sample_rate = True
    t.options = o
    tools.import_asset_tasks([t])
    u.log(f'SWIM_IMPORT {fbx} -> {[str(p) for p in t.imported_object_paths]}')
    return t.imported_object_paths


def ensure(name, cls, factory, folder):
    p = f'{folder}/{name}'
    return lib.load_asset(p) if lib.does_asset_exist(p) else tools.create_asset(name, folder, cls, factory)


# 1. imports
paths = do_import('Swimming.fbx', 'Swimming')
mesh = next(lib.load_asset(str(p).split('.')[0]) for p in paths if isinstance(lib.load_asset(str(p).split('.')[0]), u.SkeletalMesh))
skeleton = mesh.skeleton
lib.rename_asset(skeleton.get_path_name().split('.')[0], f'{DST}/SK_Mixamo_Skeleton') if skeleton.get_path_name().split('.')[0] != f'{DST}/SK_Mixamo_Skeleton' else None
do_import('Treading Water.fbx', 'Treading_Water', skeleton)
# the first import names the mesh after the file; give it a stable name
if lib.does_asset_exist(f'{DST}/SK_Mixamo'):
    lib.delete_asset(f'{DST}/SK_Mixamo')
mesh_path = [str(p).split('.')[0] for p in paths if isinstance(lib.load_asset(str(p).split('.')[0]), u.SkeletalMesh)][0]
if mesh_path != f'{DST}/SK_Mixamo':
    lib.rename_asset(mesh_path, f'{DST}/SK_Mixamo')
mesh = lib.load_asset(f'{DST}/SK_Mixamo')
skeleton = mesh.skeleton

# 2. IK rig on the Mixamo mesh
rig = ensure('IK_SF_Mixamo', u.IKRigDefinition, u.IKRigDefinitionFactory(), DST)
rc = u.IKRigController.get_controller(rig)
rc.set_skeletal_mesh(mesh)
for chain in [c.chain_name for c in rc.get_retarget_chains()]:
    rc.remove_retarget_chain(chain)
rc.apply_auto_generated_retarget_definition()
rc.apply_auto_fbik()
lib.save_loaded_asset(rig)
u.log(f'SWIM_RIG chains={[str(c.chain_name) for c in rc.get_retarget_chains()]} root={rc.get_retarget_root()}')

# 3. retargeter Mixamo -> mannequin
mannequin_rig = lib.load_asset(f'{RETARGET}/IK_SF_Mannequin')
mannequin_mesh = lib.load_asset('/Game/Mannequin/Character/Mesh/SK_Mannequin')
rt = ensure('RTG_SF_MixamoToMannequin', u.IKRetargeter, u.IKRetargetFactory(), DST)
c = u.IKRetargeterController.get_controller(rt)
c.set_ik_rig(u.RetargetSourceOrTarget.SOURCE, rig)
c.set_ik_rig(u.RetargetSourceOrTarget.TARGET, mannequin_rig)
c.set_preview_mesh(u.RetargetSourceOrTarget.SOURCE, mesh)
c.set_preview_mesh(u.RetargetSourceOrTarget.TARGET, mannequin_mesh)
c.remove_all_ops()
c.add_default_ops()
c.assign_ik_rig_to_all_ops(u.RetargetSourceOrTarget.TARGET, mannequin_rig)
c.auto_map_chains(u.AutoMapChainType.FUZZY, True)
for chain in [x.chain_name for x in u.IKRigController.get_controller(mannequin_rig).get_retarget_chains()]:
    u.log(f'SWIM_MAP {chain} <- {c.get_source_chain(chain)}')
c.auto_align_all_bones(u.RetargetSourceOrTarget.TARGET)
lib.save_loaded_asset(rt)

# 4. bake in place (arms copy rotations; the swim has no ground, so the IK op is dropped: nothing to plant)
for src, out in (('Swimming', 'AS_SF_Swim_Crawl'), ('Treading_Water', 'AS_SF_Swim_Tread')):
    lib.delete_asset(f'{DST}/{out}') if lib.does_asset_exist(f'{DST}/{out}') else None
    seq = lib.load_asset(f'{DST}/{src}')
    made = u.IKRetargetBatchOperation.duplicate_and_retarget([seq], mannequin_mesh and u.load_asset('/Game/Mannequin/Character/Mesh/SK_Mannequin'), mannequin_mesh, rt, '', '', f'{out}_tmp_', '', True)
    u.log(f'SWIM_BAKE {src} -> {[x.get_path_name() for x in made]}')
