"""Import an auto-rigged NPC (Build-NpcAutoRig.py output) into a fresh Content folder (Editor Python, UE 5.8).

Headless:
    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended -nosplash
        -script="<project>/tools/Import-NpcSkeletal.py --rig <AutoRig folder> --name BeastfolkWarrior --version V1 [--nanite-variant]"
Imports SK_<name> (own skeleton, physics asset, FBX materials/textures, every take) and generates engine-reduced
LODs, the default NPC treatment. --nanite-variant also writes SK_<name>_Nanite, a copy with Nanite enabled, for
the skinned-Nanite trial; use it only to re-test, not for shipping. Treatment by category (evidence:
docs/evidence/npc-nanite-bench.md; 100 walking 286k-triangle NPCs, LOD 2.9 ms vs skinned Nanite 3.5-3.7 ms frame):
    skinned humanoid NPCs and townsfolk  engine LODs (this script's default), Nanite off
    skinned creatures and mounts         engine LODs, same reason (not separately measured)
    static props, buildings, rocks       not measured here; keep the project's existing static-mesh practice
Writes Artifacts/Art/NPC/<name>-<version>-import.json. Refuses to overwrite.
"""
import argparse, hashlib, json, shlex, sys
from pathlib import Path
import unreal

PROJECT = Path(__file__).resolve().parents[1]


def args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rig', type=Path, required=True); p.add_argument('--name', required=True)
    p.add_argument('--version', default='V1'); p.add_argument('--lods', type=int, default=4)
    p.add_argument('--nanite-variant', action='store_true')
    argv = sys.argv[1:]
    if not argv:  # -script="file args" passes arguments through the command line only
        toks = shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)
        i = next(i for i, t in enumerate(toks) if 'Import-NpcSkeletal.py' in t)
        argv = [t.strip('"') for t in toks[i + 1:] if not t.startswith('-') or t.startswith('--')]
    return p.parse_known_args(argv)[0]


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda: f.read(8 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def build_material(a, rig, root, mesh, imported):
    """M_<name>: BaseColor from the FBX image; the optional second image (Pixal metallic-roughness) gives G=Roughness, B=Metallic."""
    base = next(o for o in imported if isinstance(o, unreal.Texture2D))
    mr_png = next(iter(sorted((rig / 'textures').glob('*_Image_1.png'))), None)
    mr = None
    if mr_png:
        t = unreal.AssetImportTask()
        for k, v in [('filename', str(mr_png)), ('destination_path', root + '/Mesh'), ('destination_name', f'T_{a.name}_MR'),
                     ('automated', True), ('save', False), ('replace_existing', False)]:
            t.set_editor_property(k, v)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
        mr = unreal.load_asset(f'{root}/Mesh/T_{a.name}_MR')
        mr.set_editor_property('srgb', False); mr.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    mel = unreal.MaterialEditingLibrary
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(f'M_{a.name}', root + '/Mesh', unreal.Material, unreal.MaterialFactoryNew())
    tb = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 0); tb.set_editor_property('texture', base)
    mel.connect_material_property(tb, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    if mr:
        tm = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 300); tm.set_editor_property('texture', mr)
        tm.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        mel.connect_material_property(tm, 'G', unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(tm, 'B', unreal.MaterialProperty.MP_METALLIC)
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)  # else the default material renders in game
    mel.recompile_material(mat)
    # Struct elements of an Unreal array come back as copies: build new entries rather than editing them in place.
    mesh.set_editor_property('materials', [unreal.SkeletalMaterial(material_interface=mat, material_slot_name=m.get_editor_property('material_slot_name'))
                                           for m in mesh.get_editor_property('materials')])
    return mat


def main():
    a = args(); rig = a.rig.resolve()
    report = json.loads((rig / 'rig-report.json').read_text())
    fbx = next(rig.glob('*.fbx'))
    if sha(fbx) != report['fbx_sha256']:
        raise RuntimeError('FBX does not match its rig report')
    root = f'/Game/Frontier/Characters/NPC/{a.name}/{a.version}'
    if unreal.EditorAssetLibrary.does_directory_exist(root):
        raise RuntimeError('Refusing to overwrite ' + root)
    for d in ['', '/Mesh', '/Animations']:
        unreal.EditorAssetLibrary.make_directory(root + d)

    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True); ui.set_editor_property('import_as_skeletal', True)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property('import_animations', True); ui.set_editor_property('import_materials', True)
    ui.set_editor_property('import_textures', True); ui.set_editor_property('create_physics_asset', True)
    ui.skeletal_mesh_import_data.set_editor_property('import_morph_targets', False)
    ui.skeletal_mesh_import_data.set_editor_property('convert_scene_unit', True)
    ui.anim_sequence_import_data.set_editor_property('convert_scene_unit', True)
    ui.anim_sequence_import_data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    t = unreal.AssetImportTask()
    for k, v in [('filename', str(fbx)), ('destination_path', root + '/Mesh'), ('destination_name', f'SK_{a.name}'),
                 ('automated', True), ('save', False), ('replace_existing', False), ('options', ui)]:
        t.set_editor_property(k, v)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    imported = [unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(root + '/Mesh', recursive=False)]
    mesh = next(o for o in imported if isinstance(o, unreal.SkeletalMesh))
    skel = mesh.get_editor_property('skeleton')
    anims = []
    for anim in [o for o in imported if isinstance(o, unreal.AnimSequence)]:
        clip = anim.get_name().split('_')[-1]
        new = f'{root}/Animations/A_{a.name}_{clip}'
        unreal.EditorAssetLibrary.rename_asset(anim.get_path_name(), new); anims.append(new)

    material = build_material(a, rig, root, mesh, imported)

    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    sub.regenerate_lod(mesh, a.lods, False, False)
    lods = sub.get_lod_count(mesh)
    variants = {'lod': mesh.get_path_name()}
    if a.nanite_variant:
        path = f'{root}/Mesh/SK_{a.name}_Nanite'
        nan = unreal.EditorAssetLibrary.duplicate_asset(mesh.get_path_name(), path)
        ns = nan.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', True)
        nan.set_editor_property('nanite_settings', ns)  # set_editor_property runs PostEditChange: Nanite data builds here
        variants['nanite'] = nan.get_path_name()

    ext = mesh.get_bounds().box_extent
    ref = unreal.AnimPoseExtensions.get_reference_pose(skel)
    checks = dict(animations=len(anims) == len(report['clips']), lods=lods == a.lods,
                  height_ok=abs(2 * ext.z - 100 * report['height_m']) < 0.15 * 100 * report['height_m'],
                  root_scale_1=abs(unreal.AnimPoseExtensions.get_ref_bone_pose(ref, 'root', unreal.AnimPoseSpaces.LOCAL).scale3d.x - 1) < 1e-4,
                  nanite_enabled=(not a.nanite_variant) or unreal.load_asset(variants['nanite']).get_editor_property('nanite_settings').get_editor_property('enabled'))
    unreal.EditorAssetLibrary.save_directory(root, only_if_is_dirty=False, recursive=True)
    manifest = dict(schema_version=1, content_root=root, fbx=str(fbx), fbx_sha256=report['fbx_sha256'], skeleton=skel.get_path_name(),
                    variants=variants, animations=sorted(anims), lod_count=lods, triangles_lod0=report['triangles'],
                    bounds_extent_cm=[ext.x, ext.y, ext.z], checks=checks,
                    status='imported-verified' if all(checks.values()) else 'imported-needs-review')
    out = PROJECT / 'Artifacts' / 'Art' / 'NPC' / f'{a.name}-{a.version}-import.json'
    out.parent.mkdir(parents=True, exist_ok=True); out.write_text(json.dumps(manifest, indent=1))
    unreal.log('NPC_IMPORT ' + json.dumps(dict(status=manifest['status'], checks=checks, variants=variants, anims=anims)))


main()
