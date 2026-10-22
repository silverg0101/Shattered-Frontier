"""In-editor step 1 of NS_ChimneySmoke (WP6g review): T_ChimneyPuffs + M_ChimneySmokeSprite and ET_ChimneySmoke.

Run: py -3 tools/Run-InEditor.py GameClient/ArtSource/VFX/ChimneySmoke/build_effect_type.py
build_chimney_niagara.py runs this first, then authors the system over the editor MCP.
"""
import sys
from pathlib import Path
import unreal as u

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'StreetLamp'))
import importlib, build_materials as bm  # noqa: E402
importlib.reload(bm)

DEST = '/Game/Frontier/VFX/ChimneySmoke'
bm._texture('T_ChimneyPuffs')
bm.build_chimney_sprite()

path = DEST + '/ET_ChimneySmoke'
et = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else \
    u.AssetToolsHelpers.get_asset_tools().create_asset('ET_ChimneySmoke', DEST, u.NiagaraEffectType, u.NiagaraEffectTypeFactoryNew())
assert et, path
# Resume: plain DeactivateImmediate (the review's wording) never reactivates, so chimneys culled at level start stayed off
# when the camera came near (WP6h first capture showed no smoke at 14 m).
et.set_editor_property('cull_reaction', u.NiagaraCullReaction.DEACTIVATE_IMMEDIATE_RESUME)
et.set_editor_property('update_frequency', u.NiagaraScalabilityUpdateFrequency.CONTINUOUS)
arr = et.get_editor_property('system_scalability_settings')
settings = list(arr.get_editor_property('settings')) or [u.NiagaraSystemScalabilitySettings()]   # a new effect type has none; one entry with default platforms = all
for s in settings:   # one entry per quality level in the default array; the same cull on every platform
    s.set_editor_property('cull_by_distance', True)
    s.set_editor_property('max_distance', 12000.0)
    s.set_editor_property('cull_max_instance_count', False)
    s.set_editor_property('cull_per_system_max_instance_count', False)
arr.set_editor_property('settings', settings)
et.set_editor_property('system_scalability_settings', arr)
assert u.EditorAssetLibrary.save_loaded_asset(et)
print('ET_CHIMNEY', path, len(settings), [(s.get_editor_property('cull_by_distance'), s.get_editor_property('max_distance')) for s in settings])
