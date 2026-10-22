"""Import the HudVitals-v6 parts as UI textures and build the masked bar-fill material and its two instances.

Run in the open editor:   py -3 tools/Run-InEditor.py tools/ui/import_hud_vitals.py
Writes /Game/Frontier/UI/Kit/HudVitals/: T_HudVitals_<part> (no mips, UI group; the mask is linear grayscale), M_UI_BarFillMasked
(User Interface domain: opacity = mask x (U <= cut), cut = lerp(InnerStart, InnerEnd, Percent), so the cut stays aligned with the mask),
MI_UI_BarFill_Health and MI_UI_BarFill_Stamina. Rerun to rebuild; it replaces assets in place. Then (once) build the WBP:
tools/ui/create_widgets.py --force --only WBP_HudVitals.
"""
import os

import unreal

SRC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'GameClient', 'ArtSource', 'UI', 'HudVitals-v6'))
DEST = '/Game/Frontier/UI/Kit/HudVitals'
PARTS = ['badge_frame', 'badge_stag', 'bar_frame', 'bar_fill_health', 'bar_fill_stamina', 'bar_fill_mask', 'icon_heart', 'icon_stamina_bolt']
# Interior of bar_frame.png (mask columns 10..585 of 600): the fill's 0% and 100% cut positions in U.
INNER_START, INNER_END = 10.0 / 600.0, 586.0 / 600.0

lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

tasks = []
for part in PARTS:
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', os.path.join(SRC, part + '.png'))
    t.set_editor_property('destination_path', DEST)
    t.set_editor_property('destination_name', 'T_HudVitals_' + part)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('automated', True)
    t.set_editor_property('save', False)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

tex = {}
for part in PARTS:
    path = '%s/T_HudVitals_%s' % (DEST, part)
    tx = lib.load_asset(path)
    if tx is None:
        raise RuntimeError('import failed: ' + path)
    mask = part == 'bar_fill_mask'
    tx.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    tx.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tx.set_editor_property('srgb', not mask)
    tx.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE if mask else unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tx.set_editor_property('never_stream', True)
    lib.save_loaded_asset(tx)
    tex[part] = tx

MAT = DEST + '/M_UI_BarFillMasked'
if lib.does_asset_exist(MAT):  # rebuild in place: the instances and the WBP keep their references
    mat = lib.load_asset(MAT)
    mel.delete_all_material_expressions(mat)
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_UI_BarFillMasked', DEST, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)


def node(cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def link(a, out, b, inp):
    mel.connect_material_expressions(a, out, b, inp)


def scalar(name, value, x, y):
    n = node(unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', value)
    return n


def const(value, x, y):
    n = node(unreal.MaterialExpressionConstant, x, y)
    n.set_editor_property('r', value)
    return n


fill = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, -200)
fill.set_editor_property('parameter_name', 'FillTexture')
fill.set_editor_property('texture', tex['bar_fill_health'])
maskn = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, 100)
maskn.set_editor_property('parameter_name', 'MaskTexture')
maskn.set_editor_property('texture', tex['bar_fill_mask'])
maskn.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
tint = node(unreal.MaterialExpressionVectorParameter, -900, -400)
tint.set_editor_property('parameter_name', 'Tint')
tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))

uv = node(unreal.MaterialExpressionTextureCoordinate, -1100, 400)
u = node(unreal.MaterialExpressionComponentMask, -900, 400)
u.set_editor_property('r', True); u.set_editor_property('g', False); u.set_editor_property('b', False); u.set_editor_property('a', False)
link(uv, '', u, '')
start = scalar('InnerStart', INNER_START, -1100, 560)
end = scalar('InnerEnd', INNER_END, -1100, 640)
pct = scalar('Percent', 1.0, -1100, 720)
cut = node(unreal.MaterialExpressionLinearInterpolate, -700, 600)
link(start, '', cut, 'A'); link(end, '', cut, 'B'); link(pct, '', cut, 'Alpha')
one, zero = const(1.0, -700, 500), const(0.0, -700, 760)
inside = node(unreal.MaterialExpressionIf, -450, 500)  # U <= cut ? 1 : 0
link(u, '', inside, 'A'); link(cut, '', inside, 'B')
link(zero, '', inside, 'A > B'); link(one, '', inside, 'A == B'); link(one, '', inside, 'A < B')
opacity = node(unreal.MaterialExpressionMultiply, -250, 300)
link(maskn, 'R', opacity, 'A'); link(inside, '', opacity, 'B')
color = node(unreal.MaterialExpressionMultiply, -250, -300)
link(fill, 'RGB', color, 'A'); link(tint, 'RGB', color, 'B')
mel.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(mat)
lib.save_loaded_asset(mat)

for name, part in (('MI_UI_BarFill_Health', 'bar_fill_health'), ('MI_UI_BarFill_Stamina', 'bar_fill_stamina')):
    path = '%s/%s' % (DEST, name)
    mi = lib.load_asset(path) if lib.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', mat)
    mel.set_material_instance_texture_parameter_value(mi, 'FillTexture', tex[part])
    mel.set_material_instance_texture_parameter_value(mi, 'MaskTexture', tex['bar_fill_mask'])
    mel.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(1, 1, 1, 1))
    mel.set_material_instance_scalar_parameter_value(mi, 'Percent', 1.0)
    lib.save_loaded_asset(mi)
print('import_hud_vitals: %d textures, M_UI_BarFillMasked, 2 instances under %s' % (len(PARTS), DEST))
