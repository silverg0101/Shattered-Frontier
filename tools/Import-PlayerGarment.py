"""Import the starter garment (tools/Build-PlayerGarment.py output) as a skeletal mesh on the player's MetaHuman skeleton, with cloth materials.

UnrealEditor.exe <uproject> -RenderOffscreen -unattended -nosplash -ExecCmds="py <abs path of this file>, QUIT_EDITOR"
Reads Artifacts/Characters/PlayerBase/garment_tunic_leggings.fbx (env SF_GARMENT_FBX overrides; env SF_OUTFIT_SUFFIX, e.g. _F for the
female fit, is appended to the mesh names). Writes under /Game/Frontier/Characters/Player/Garments:
SKM_Tunic_Leggings (slots M_Tunic, M_Leggings), M_PlayerCloth (two-sided tiling fabric: base colour x tint, normal x strength, roughness, tiling),
MI_PlayerLinen and MI_PlayerLeggings, and the cloth textures from GameClient/ArtSource/Characters/Player/garments/:
T_linen_oatmeal (grey-beige linen) and T_wool_brown_knit, each with a _N normal made from its albedo's luminance (high-passed central difference).
Garment UVs are 1 unit per 40 cm; the linen image holds ~200 threads, so Tiling 3 gives ~15 threads/cm.
T_linen_stitch_trim is imported but unused: the garment has no separate trim slot or UV island yet.
Re-runnable. The skeleton is the built body's, so it needs a build from tools/Build-PlayerMetaHumans.py.
"""
import os
from pathlib import Path

import unreal

ROOT = '/Game/Frontier/Characters/Player/Garments'
BODY = '/Game/Frontier/Characters/Player/Built/MHC_Orlando/Body/SKM_MHC_Orlando_BodyMesh'
ART = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())) / 'ArtSource/Characters/Player/garments'
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def tex(name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(ART / f'{name}.png'))
    task.set_editor_property('destination_path', ROOT)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', False)
    tools.import_asset_tasks([task])
    t = lib.load_asset(f'{ROOT}/{name}')
    if t is None:
        raise RuntimeError(f'Missing texture {name}')
    if name.endswith('_N'):
        t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property('srgb', False)
    t.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)  # full mip chain: no shimmer at distance
    lib.save_loaded_asset(t)
    return t


def make_master(base_default, normal_default):
    path = f'{ROOT}/M_PlayerCloth'
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    m = tools.create_asset('M_PlayerCloth', ROOT, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided', True)  # slit, neck and cuff insides show cloth, not a grey backface

    def node(cls, x, y, **pins):
        n = mel.create_material_expression(m, cls, x, y)
        for pin, src in pins.items():
            mel.connect_material_expressions(src, '', n, pin)
        return n

    def param(name, v, x, y):
        p = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', v)
        return p
    M = unreal.MaterialExpressionMultiply
    uv = node(M, -700, 50, A=node(unreal.MaterialExpressionTextureCoordinate, -900, 0), B=param('Tiling', 3.0, -900, 150))

    def sampler(name, y, sampler_type, default):
        s = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -450, y)
        s.set_editor_property('parameter_name', name)
        s.set_editor_property('sampler_type', sampler_type)
        s.set_editor_property('texture', default)
        mel.connect_material_expressions(uv, '', s, 'UVs')
        return s
    base = sampler('BaseTex', -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, base_default)
    nrm = sampler('NormalTex', 150, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, normal_default)
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -450, -420)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    colour = mel.create_material_expression(m, M, 0, -250)
    mel.connect_material_expressions(base, 'RGB', colour, 'A')
    mel.connect_material_expressions(tint, '', colour, 'B')
    mel.connect_material_property(colour, '', unreal.MaterialProperty.MP_BASE_COLOR)
    flat = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -250, 300)
    flat.set_editor_property('constant', unreal.LinearColor(0, 0, 1, 1))
    bend = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, 0, 150)
    mel.connect_material_expressions(flat, '', bend, 'A')
    mel.connect_material_expressions(nrm, 'RGB', bend, 'B')
    mel.connect_material_expressions(param('NormalStrength', 0.6, -250, 400), '', bend, 'Alpha')
    mel.connect_material_property(bend, '', unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(param('Roughness', 0.85, 0, 450), '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.set_material_usage(m, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    mel.recompile_material(m)
    lib.save_loaded_asset(m)
    return m


def make_instance(name, master, base, normal, tint, tiling, normal_strength, roughness):
    path = f'{ROOT}/{name}'
    mi = lib.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(name, ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', master)
    mel.set_material_instance_texture_parameter_value(mi, 'BaseTex', base)
    mel.set_material_instance_texture_parameter_value(mi, 'NormalTex', normal)
    mel.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(*tint))
    mel.set_material_instance_scalar_parameter_value(mi, 'Tiling', tiling)
    mel.set_material_instance_scalar_parameter_value(mi, 'NormalStrength', normal_strength)
    mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', roughness)
    lib.save_loaded_asset(mi)
    return mi


def import_skeletal(fbx, name, skeleton):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', True)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('skeleton', skeleton)
    data = ui.get_editor_property('skeletal_mesh_import_data')
    data.set_editor_property('import_morph_targets', False)
    data.set_editor_property('update_skeleton_reference_pose', False)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(fbx))
    task.set_editor_property('destination_path', ROOT)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', False)
    task.set_editor_property('options', ui)
    tools.import_asset_tasks([task])
    mesh = lib.load_asset(f'{ROOT}/{name}')
    if mesh is None:
        raise RuntimeError(f'{fbx.name} import produced no mesh')
    lib.save_loaded_asset(mesh)
    return mesh


def run():
    fbx = Path(os.environ.get('SF_GARMENT_FBX', Path(unreal.Paths.project_dir()).parent / 'Artifacts/Characters/PlayerBase/garment_tunic_leggings.fbx'))
    body = lib.load_asset(BODY)
    if body is None:
        raise RuntimeError('No built body: run tools/Build-PlayerMetaHumans.py')
    skeleton = body.get_editor_property('skeleton')
    linen_tex, linen_n = tex('T_linen_oatmeal'), tex('T_linen_oatmeal_N')
    wool_tex, wool_n = tex('T_wool_brown_knit'), tex('T_wool_brown_knit_N')
    tex('T_linen_stitch_trim')
    master = make_master(linen_tex, linen_n)
    linen = make_instance('MI_PlayerLinen', master, linen_tex, linen_n, (1, 1, 1, 1), 3.0, 0.6, 0.85)
    legs = make_instance('MI_PlayerLeggings', master, wool_tex, wool_n, (1, 1, 1, 1), 4.0, 0.35, 0.9)
    name = 'SKM_Tunic_Leggings' + os.environ.get('SF_OUTFIT_SUFFIX', '')
    mesh = import_skeletal(fbx, name, skeleton)
    # The body patch the garment covers (tools/Build-GarmentBodyClip.py), removed from the body by Mutable (tools/Create-PlayerOutfit.py).
    import_skeletal(fbx.with_name(fbx.stem + '_bodyclip.fbx'), name + '_BodyClip', skeleton)
    mats = []
    for slot in mesh.get_editor_property('materials'):
        name = str(slot.get_editor_property('material_slot_name'))
        slot.set_editor_property('material_interface', linen if 'Tunic' in name else legs)
        mats.append(slot)
    mesh.set_editor_property('materials', mats)
    lib.save_loaded_asset(mesh)
    unreal.log(f'PLAYER_GARMENT_IMPORTED slots={[str(s.get_editor_property("material_slot_name")) for s in mats]} '
               f'skeleton={mesh.get_editor_property("skeleton").get_name()} extent={mesh.get_bounds().box_extent} body_extent={body.get_bounds().box_extent}')


try:
    run()
except Exception as error:
    unreal.log_error(f'PLAYER_GARMENT_FAILED {error}')
