"""Import the Voidreign delivery packet into a fresh Unreal Content folder (Editor Python, UE 5.8).

Headless example (no renderer, CPU only):
    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended -nosplash
        -script="<project>/tools/Import-Voidreign.py --source <Delivery folder> --orm <ORM png> --version V1"

Imports SK_Voidreign (mesh, skeleton, physics asset, every FBX take), BaseColor/ORM textures and a material
with an AO-strength parameter; enables root motion per clips.json; adds clip events as sync markers;
generates engine-reduced LODs; validates counts/bounds; writes a manifest. Refuses to overwrite.
"""
import argparse, hashlib, json, shlex, sys
from pathlib import Path
import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT_MOTION = {'root-motion', 'root-extracted', 'root-authored'}


def args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True); p.add_argument('--orm', type=Path, required=True)
    p.add_argument('--version', default='V1'); p.add_argument('--lods', type=int, default=4)
    p.add_argument('--delete', action='store_true', help='delete this version folder and exit; re-import in a fresh process')
    argv = sys.argv[1:]
    if not argv:  # -script="file args" passes arguments through the command line only
        toks = shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)
        i = next(i for i, t in enumerate(toks) if 'Import-Voidreign.py' in t)
        argv = [t.strip('"') for t in toks[i + 1:] if not t.startswith('-') or t.startswith('--')]
    return p.parse_known_args(argv)[0]


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda: f.read(8 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def task(filename, dest, options=None, name=None):
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', str(filename)); t.set_editor_property('destination_path', dest)
    t.set_editor_property('automated', True); t.set_editor_property('save', False); t.set_editor_property('replace_existing', False)
    if name: t.set_editor_property('destination_name', name)
    if options: t.set_editor_property('options', options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    paths = list(t.get_editor_property('imported_object_paths'))
    if not paths: raise RuntimeError('Import produced nothing: ' + str(filename))
    return paths


def main():
    a = args(); src = a.source.resolve()
    report = json.loads((src / 'delivery-report.json').read_text())
    fbx = src / 'SK_Voidreign.fbx'; base = src / 'SK_Voidreign.fbm' / 'BaseColor-4K.png'
    if report['status'] != 'delivery-export-verified' or sha(fbx) != report['fbx_sha256']:
        raise RuntimeError('Delivery packet is not the verified export')
    lib = json.loads((PROJECT / json.loads((src / 'profile.json').read_text())['clips_report']).read_text())
    clips = {c['id']: c for c in lib['clips'] if c['id'] in report['takes']}
    root = f'/Game/Frontier/Creatures/Voidreign/{a.version}'
    if a.delete:  # same-process delete + import collides with lingering objects
        unreal.EditorAssetLibrary.delete_directory(root); unreal.log('VOIDREIGN_DELETED ' + root); return
    if unreal.EditorAssetLibrary.does_directory_exist(root):
        raise RuntimeError('Refusing to overwrite ' + root + ' (run once with --delete)')
    for d in ['', '/Mesh', '/Animations', '/Textures', '/Materials']:
        unreal.EditorAssetLibrary.make_directory(root + d)

    # Textures
    bc = unreal.load_asset(task(base, root + '/Textures', name='T_Voidreign_BaseColor')[0])
    orm = unreal.load_asset(task(a.orm.resolve(), root + '/Textures', name='T_Voidreign_ORM')[0])
    orm.set_editor_property('srgb', False); orm.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)

    # Skeletal mesh + takes
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True); ui.set_editor_property('import_as_skeletal', True)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property('import_animations', True); ui.set_editor_property('import_materials', False); ui.set_editor_property('import_textures', False)
    ui.set_editor_property('create_physics_asset', True)
    ui.skeletal_mesh_import_data.set_editor_property('import_morph_targets', False)
    # convert_scene_unit defaults False, which leaves a x100 scale on the root bone; convert to cm instead.
    ui.skeletal_mesh_import_data.set_editor_property('convert_scene_unit', True); ui.anim_sequence_import_data.set_editor_property('convert_scene_unit', True)
    ui.anim_sequence_import_data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    ui.anim_sequence_import_data.set_editor_property('import_bone_tracks', True)
    paths = task(fbx, root + '/Mesh', ui)
    mesh = next(unreal.load_asset(p) for p in paths if isinstance(unreal.load_asset(p), unreal.SkeletalMesh))
    skel = mesh.get_editor_property('skeleton')
    anims = [unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(root + '/Mesh', recursive=False) if isinstance(unreal.load_asset(p), unreal.AnimSequence)]

    # Material: BaseColor; ORM R=AO (lerp by AOStrength), G=Roughness, B=Metallic.
    mel = unreal.MaterialEditingLibrary
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_Voidreign', root + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    tb = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 0); tb.set_editor_property('texture', bc)
    to = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 300); to.set_editor_property('texture', orm)
    to.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    one = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 500); one.set_editor_property('r', 1.0)
    strength = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 600)
    strength.set_editor_property('parameter_name', 'AOStrength'); strength.set_editor_property('default_value', 0.6)
    lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -200, 450)
    mel.connect_material_expressions(one, '', lerp, 'A'); mel.connect_material_expressions(to, 'R', lerp, 'B'); mel.connect_material_expressions(strength, '', lerp, 'Alpha')
    mel.connect_material_property(tb, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(lerp, '', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(to, 'G', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(to, 'B', unreal.MaterialProperty.MP_METALLIC)
    mat.set_editor_property('two_sided', True)  # hair sheets are single-sided cards
    mel.recompile_material(mat)
    mats = mesh.get_editor_property('materials')
    for m in mats: m.set_editor_property('material_interface', mat)
    mesh.set_editor_property('materials', mats)

    # Animations: move, root motion, events as sync markers.
    moved, missing = [], []
    for anim in anims:
        name = anim.get_name(); cid = next((c for c in clips if name.endswith(c) or name.endswith(c.replace('.', '_'))), None)
        if cid is None: missing.append(name); continue
        c = clips[cid]; anim.set_editor_property('enable_root_motion', c['root_motion'] in ROOT_MOTION)
        anim.set_editor_property('force_root_lock', False)
        fps = c['fps']
        if c.get('events'): unreal.AnimationLibrary.add_animation_notify_track(anim, 'Events')
        for ev, frames in (c.get('events') or {}).items():
            for fr in (frames if isinstance(frames, list) else [frames]):
                unreal.AnimationLibrary.add_animation_sync_marker(anim, ev, (fr - 1) / fps, 'Events')
        unreal.EditorAssetLibrary.set_metadata_tag(anim, 'VoidreignClip', cid)
        unreal.EditorAssetLibrary.set_metadata_tag(anim, 'Loop', str(c['loop']))
        new = f"{root}/Animations/A_Voidreign_{cid.replace('.', '_')}"
        unreal.EditorAssetLibrary.rename_asset(anim.get_path_name(), new); moved.append(cid)

    # Engine skeletal reduction LODs.
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    lod_ok = sub.regenerate_lod(mesh, a.lods, False, False) if hasattr(sub, 'regenerate_lod') else unreal.SkeletalMeshEditorSubsystem.regenerate_lod(mesh, a.lods, False, False)
    lod_count = sub.get_lod_count(mesh) if hasattr(sub, 'get_lod_count') else None

    bounds = mesh.get_bounds(); ext = bounds.box_extent
    checks = dict(bones=skel is not None and len(report['takes']) > 0,
                  animations=len(moved) == len(report['takes']) and not missing,
                  height_cm=1.0 * 2 * ext.z, lods=lod_count == a.lods)
    checks['height_ok'] = 200 < checks['height_cm'] < 320
    ref = unreal.AnimPoseExtensions.get_reference_pose(skel)
    checks['root_scale_1'] = abs(unreal.AnimPoseExtensions.get_ref_bone_pose(ref, 'root', unreal.AnimPoseSpaces.LOCAL).scale3d.x - 1) < 1e-4  # units converted, not carried as root scale
    unreal.EditorAssetLibrary.save_directory(root, only_if_is_dirty=False, recursive=True)
    manifest = dict(schema_version=1, content_root=root, fbx=str(fbx), fbx_sha256=report['fbx_sha256'], orm=str(a.orm), orm_sha256=sha(a.orm),
                    mesh=mesh.get_path_name(), skeleton=skel.get_path_name() if skel else None, material=mat.get_path_name(),
                    animations=sorted(moved), unmatched_takes=missing, lod_regenerated=bool(lod_ok), lod_count=lod_count,
                    bounds_extent_cm=[ext.x, ext.y, ext.z], checks=checks,
                    status='imported-verified' if all(v for k, v in checks.items() if k != 'height_cm') else 'imported-needs-review')
    out = PROJECT / 'Artifacts' / 'Art' / 'Voidreign' / f'unreal-import-{a.version}.json'
    out.parent.mkdir(parents=True, exist_ok=True); out.write_text(json.dumps(manifest, indent=1))
    markers = sum(len(unreal.AnimationLibrary.get_animation_sync_markers(unreal.load_asset(f"{root}/Animations/A_Voidreign_{c.replace('.', '_')}"))) for c in moved)
    expected = sum(len(v if isinstance(v, list) else [v]) for c in moved for v in (clips[c].get('events') or {}).values())
    manifest['event_markers'] = [markers, expected]; manifest['checks']['event_markers'] = markers == expected
    if markers != expected: manifest['status'] = 'imported-needs-review'
    out.write_text(json.dumps(manifest, indent=1))
    unreal.log('VOIDREIGN_IMPORT ' + json.dumps(dict(status=manifest['status'], checks=checks, animations=len(moved), unmatched=missing[:5])))


main()
