"""WP4c: natural shrub leaf tint and a fresh compile of the Frontier rock/wood/leaf masters.

Run in the open editor after import_nature.py SaplingBroadleaf. Only Frontier assets are touched.
"""
import unreal as u

mel = u.MaterialEditingLibrary
eal = u.EditorAssetLibrary
root = '/Game/Frontier/Environment/Greenlands/Nature/Materials/'
shrub = u.load_asset(root + 'MI_ShrubBroadleaf_Leaf')
assert shrub
mel.set_material_instance_scalar_parameter_value(shrub, 'UseBlend', 0.0)
for p in ('TintA', 'TintB'):
    mel.set_material_instance_vector_parameter_value(shrub, p, u.LinearColor(1.08, 1.18, 0.78, 1))
for p in ('SatA', 'SatB'):
    mel.set_material_instance_scalar_parameter_value(shrub, p, 0.8)
assert eal.save_loaded_asset(shrub)
for name in ('M_GN_Rock', 'M_GN_Wood', 'M_GN_Leaf'):
    master = u.load_asset(root + name)
    assert master, name
    mel.recompile_material(master)
    assert eal.save_loaded_asset(master), name
u.log('WP4C_MATERIALS shrub Tint=(1.08,1.18,0.78) Sat=0.8; Frontier masters recompiled')
