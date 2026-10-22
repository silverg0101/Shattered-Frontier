"""WP6g review fix 4 gate: NS_ChimneySmoke exists with its eight User parameters, one NiagaraActor per smoking house, no card left.

Smoking house = the dress_house_details rule: a Townhouse mesh whose _h(label) < .4 (the review text says _h(label+'smoke');
the code has always used _h(label), and keeping it keeps the same 85 houses smoking).
User parameters are checked by a set/get round trip on a spawned-and-destroyed component (exposed_parameters is not
readable from Python). Prints CHIMNEY_SMOKE_CHECK ... and asserts.
"""
import sys
from pathlib import Path
import unreal as u

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / 'GameClient/ArtSource/Silverbrook'))
import silverbrook_props as sp

ns = u.load_asset('/Game/Frontier/VFX/ChimneySmoke/NS_ChimneySmoke')
et = u.load_asset('/Game/Frontier/VFX/ChimneySmoke/ET_ChimneySmoke')
assert ns and et, (ns, et)
A = u.get_editor_subsystem(u.EditorActorSubsystem)
probe = A.spawn_actor_from_class(u.NiagaraActor, u.Vector(0, 0, -100000))
c = probe.get_editor_property('niagara_component'); c.set_asset(ns)
params = {}
def back(r):   # getters return (value, valid) in 5.8
    return r if not isinstance(r, tuple) else (r[0] if r[-1] is True else None)
for n, v in (('SpawnRate', 7.), ('Lifetime', 3.5), ('RiseSpeed', 150.), ('StartSize', 55.), ('EndSize', 210.), ('Opacity', .5)):
    c.set_variable_float('User.' + n, v); g = back(c.get_variable_float('User.' + n)); params[n] = g is not None and abs(g - v) < 1e-4
c.set_variable_vec3('User.Wind', u.Vector(70, 0, 0)); g = back(c.get_variable_vec3('User.Wind')); params['Wind'] = g is not None and abs(g.x - 70) < 1e-4
c.set_variable_linear_color('User.Tint', u.LinearColor(.4, .4, .43, 1)); g = back(c.get_variable_color('User.Tint')); params['Tint'] = g is not None and abs(g.b - .43) < 1e-4
A.destroy_actor(probe)
actors = A.get_all_level_actors()
houses = [a for a in actors if isinstance(a, u.StaticMeshActor) and str(a.get_folder_path()).startswith('Districts/')
          and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name().startswith('SM_SilverbrookTownhouse')
          and '_Door' not in a.static_mesh_component.static_mesh.get_name() and sp._h(a.get_actor_label()) < .4]
smokes = {a.get_actor_label(): a for a in actors if isinstance(a, u.NiagaraActor) and a.get_actor_label().startswith('ChimneySmoke_')}
missing = [h.get_actor_label() for h in houses if 'ChimneySmoke_' + h.get_actor_label() not in smokes]
wrong_asset = [l for l, a in smokes.items() if a.get_editor_property('niagara_component').get_asset() != ns]
cards = [a.get_actor_label() for a in actors if isinstance(a, u.StaticMeshActor) and a.get_actor_label().startswith('ChimneySmoke_')]
print('CHIMNEY_SMOKE_CHECK smoking_houses', len(houses), 'niagara_actors', len(smokes), 'missing', missing[:5], 'wrong_asset', len(wrong_asset),
      'card_actors', len(cards), 'effect_type', ns.get_editor_property('effect_type').get_name() if ns.get_editor_property('effect_type') else None,
      'float_roundtrip', params)
assert all(params.values()), params
assert len(houses) == len(smokes) and not missing and not wrong_asset and not cards
assert ns.get_editor_property('effect_type') == et
