"""Run in the open Silverbrook editor after silverbrook_props.dress_all()."""
import json, sys
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'GameClient/ArtSource/Greenlands'))
import pcg_dressing
sys.path.insert(0, str(ROOT / 'GameClient/ArtSource/Silverbrook'))
import silverbrook_props

world, terrain = pcg_dressing._terrain()
actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
report = {'checked': 0, 'floating_over_5_cm': [], 'no_terrain_hit': [], 'worst_gap_cm': 0.0,
          'by_kind': {}, 'armour_area': []}

def check(name, kind, loc):
    hit = terrain.static_mesh_component.line_trace_component(
        u.Vector(loc.x, loc.y, loc.z + 3000), u.Vector(loc.x, loc.y, loc.z - 3000), True, False)
    report['checked'] += 1
    report['by_kind'][kind] = report['by_kind'].get(kind, 0) + 1
    if not hit:
        report['no_terrain_hit'].append(name)
        return
    gap = round(loc.z - hit[0].z, 2)
    report['worst_gap_cm'] = max(report['worst_gap_cm'], gap)
    if gap > 5:
        report['floating_over_5_cm'].append({'name': name, 'gap_cm': gap,
                                            'xyz_cm': [round(loc.x), round(loc.y), round(loc.z)]})

verge_meshes = set(silverbrook_props.cfg()['verge']['meshes'])
for actor in actors:
    label = actor.get_actor_label()
    if 'SilverbrookDressing' in [str(t) for t in actor.tags]:
        check(label, 'street_prop', actor.get_actor_location())
    if 'armor' in label.lower() or 'armour' in label.lower() or 'barrel' in label.lower():
        p = actor.get_actor_location()
        row = {'name': label, 'xyz_cm': [round(p.x), round(p.y), round(p.z)]}
        if isinstance(actor, u.StaticMeshActor):
            hit = terrain.static_mesh_component.line_trace_component(u.Vector(p.x,p.y,p.z+3000),
                                                                       u.Vector(p.x,p.y,p.z-3000),True,False)
            if hit:
                center,extent = actor.get_actor_bounds(False)
                row['pivot_gap_cm'] = round(p.z-hit[0].z,2)
                row['bounds_bottom_gap_cm'] = round(center.z-extent.z-hit[0].z,2)
        report['armour_area'].append(row)

for actor in actors:
    if not isinstance(actor, u.InstancedFoliageActor):
        continue
    for comp in actor.get_components_by_class(u.InstancedStaticMeshComponent):
        mesh = comp.static_mesh
        if not mesh or mesh.get_name() not in verge_meshes:
            continue
        for i in range(comp.get_instance_count()):
            check(f'{comp.get_name()}_{i}', 'verge', comp.get_instance_transform(i, True).translation)

out = Path(__file__).with_name('ground-gap.json')
out.write_text(json.dumps(report, indent=2), encoding='utf-8')
print('WP6_GROUND', json.dumps({k: v for k, v in report.items() if k not in ('armour_area', 'floating_over_5_cm', 'no_terrain_hit')}),
      'floating=', len(report['floating_over_5_cm']), 'no_hit=', len(report['no_terrain_hit']))
assert not report['floating_over_5_cm'] and not report['no_terrain_hit']
