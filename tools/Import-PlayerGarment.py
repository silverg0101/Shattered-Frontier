"""Import the starter garment (tools/Build-PlayerGarment.py output) as a skeletal mesh on the player's MetaHuman skeleton, with cloth materials.

UnrealEditor.exe <uproject> -RenderOffscreen -unattended -nosplash -ExecCmds="py <abs path of this file>, QUIT_EDITOR"
Reads Artifacts/Characters/PlayerBase/garment_tunic_leggings.fbx (env SF_GARMENT_FBX overrides). Writes under /Game/Frontier/Characters/Player/Garments:
SKM_Tunic_Leggings (slots M_Tunic, M_Leggings), M_PlayerCloth (tiling fabric: base colour, normal, roughness, tint, tiling),
MI_PlayerLinen and MI_PlayerLeggings. Fabric: the project's Silverbrook denim weave (T_SB_denim_fabric_04_*, referenced not copied) desaturated and tinted, plus a procedural woven grid (the Silverbrook linen textures are 32 px stubs here).
Re-runnable. The skeleton is the built body's, so it needs a build from tools/Build-PlayerMetaHumans.py.
"""
import os
from pathlib import Path

import unreal

ROOT = '/Game/Frontier/Characters/Player/Garments'
BODY = '/Game/Frontier/Characters/Player/Built/MHC_Orlando/Body/SKM_MHC_Orlando_BodyMesh'
TEX = '/Game/Frontier/Environment/Greenlands/Silverbrook/Textures'
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def tex(name):
    t = lib.load_asset(f'{TEX}/{name}')
    if t is None:
        raise RuntimeError(f'Missing texture {name}')
    return t


def make_master():
    path = f'{ROOT}/M_PlayerCloth'
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    m = tools.create_asset('M_PlayerCloth', ROOT, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided', True)
    coord = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    tiling = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 150)
    tiling.set_editor_property('parameter_name', 'Tiling')
    tiling.set_editor_property('default_value', 2.5)
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -700, 50)
    mel.connect_material_expressions(coord, '', mul, 'A')
    mel.connect_material_expressions(tiling, '', mul, 'B')

    def sampler(name, y, sampler_type, default):
        s = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -450, y)
        s.set_editor_property('parameter_name', name)
        s.set_editor_property('sampler_type', sampler_type)
        s.set_editor_property('texture', tex(default))
        mel.connect_material_expressions(mul, '', s, 'UVs')
        return s
    base = sampler('BaseTex', -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 'T_SB_denim_fabric_04_Diffuse')
    nrm = sampler('NormalTex', 150, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 'T_SB_denim_fabric_04_nor_gl')
    rough = sampler('RoughTex', 450, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, 'T_SB_denim_fabric_04_Rough')
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -450, -420)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    # Colour = Tint x lerp(1, desaturated texture x 2.2, TexAmount) x a procedural woven grid (the linen texture here is a 32 px stub).
    def const(v, x, y):
        c = mel.create_material_expression(m, unreal.MaterialExpressionConstant, x, y)
        c.set_editor_property('r', v)
        return c

    def param(name, v, x, y):
        p = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', v)
        return p

    def node(cls, x, y, **pins):
        n = mel.create_material_expression(m, cls, x, y)
        for pin, src in pins.items():
            mel.connect_material_expressions(src, '', n, pin)
        return n
    M = unreal.MaterialExpressionMultiply
    desat = mel.create_material_expression(m, unreal.MaterialExpressionDesaturation, -250, -200)
    mel.connect_material_expressions(base, 'RGB', desat, '')
    bright = node(M, -100, -200, A=desat, B=const(2.2, -250, -100))
    lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, 50, -250)
    mel.connect_material_expressions(const(1.0, -250, -300), '', lerp, 'A')
    mel.connect_material_expressions(bright, '', lerp, 'B')
    mel.connect_material_expressions(param('TexAmount', 1.0, -250, -350), '', lerp, 'Alpha')
    grid_uv = node(M, -700, 700, A=coord, B=param('WeaveFreq', 60.0, -900, 750))
    masks = []
    for ch in ('r', 'g'):
        mk = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -550, 700)
        for c in ('r', 'g', 'b', 'a'):
            mk.set_editor_property(c, c == ch)
        mel.connect_material_expressions(grid_uv, '', mk, '')
        s = mel.create_material_expression(m, unreal.MaterialExpressionSine, -400, 700)
        s.set_editor_property('period', 1.0)
        mel.connect_material_expressions(mk, '', s, '')
        masks.append(s)
    cross = node(M, -250, 700, A=masks[0], B=masks[1])
    soft = node(M, -100, 700, A=cross, B=const(0.08, -250, 800))
    weave = node(unreal.MaterialExpressionAdd, 50, 700, A=soft, B=const(0.92, -100, 800))
    cloth = node(M, 200, -250, A=lerp, B=weave)
    tm = node(M, 350, -250, A=cloth, B=tint)
    mel.connect_material_property(tm, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(nrm, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, 'R', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.set_material_usage(m, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    mel.recompile_material(m)
    lib.save_loaded_asset(m)
    return m


def make_instance(name, master, base, normal, rough, tint, tiling, tex_amount):
    path = f'{ROOT}/{name}'
    mi = lib.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(name, ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', master)
    mel.set_material_instance_texture_parameter_value(mi, 'BaseTex', tex(base))
    mel.set_material_instance_texture_parameter_value(mi, 'NormalTex', tex(normal))
    mel.set_material_instance_texture_parameter_value(mi, 'RoughTex', tex(rough))
    mel.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(*tint))
    mel.set_material_instance_scalar_parameter_value(mi, 'Tiling', tiling)
    mel.set_material_instance_scalar_parameter_value(mi, 'TexAmount', tex_amount)
    lib.save_loaded_asset(mi)
    return mi


def run():
    fbx = Path(os.environ.get('SF_GARMENT_FBX', Path(unreal.Paths.project_dir()).parent / 'Artifacts/Characters/PlayerBase/garment_tunic_leggings.fbx'))
    body = lib.load_asset(BODY)
    if body is None:
        raise RuntimeError('No built body: run tools/Build-PlayerMetaHumans.py')
    skeleton = body.get_editor_property('skeleton')
    master = make_master()
    linen = make_instance('MI_PlayerLinen', master, 'T_SB_denim_fabric_04_Diffuse', 'T_SB_denim_fabric_04_nor_gl', 'T_SB_denim_fabric_04_Rough', (0.55, 0.43, 0.26, 1), 2.0, 0.35)
    legs = make_instance('MI_PlayerLeggings', master, 'T_SB_denim_fabric_04_Diffuse', 'T_SB_denim_fabric_04_nor_gl', 'T_SB_denim_fabric_04_Rough', (0.34, 0.18, 0.08, 1), 3.0, 1.0)

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
    task.set_editor_property('destination_name', 'SKM_Tunic_Leggings')
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', False)
    task.set_editor_property('options', ui)
    tools.import_asset_tasks([task])
    mesh = lib.load_asset(f'{ROOT}/SKM_Tunic_Leggings')
    if mesh is None:
        raise RuntimeError('Garment import produced no mesh')
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
