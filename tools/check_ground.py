"""Terrain gap of ground-level dressing actors: Run-InEditor.py tools/check_ground.py [--terrain <label>].

Terrain is the StaticMeshActor labelled <label> (dev maps: Terrain), else the SM_Silverbrook_Terrain mesh actor.
Prints DRESSING_GROUND {...}; fails when any base is over 5 cm above the terrain or has no terrain under it.
"""
import sys
import unreal as u

label = sys.argv[sys.argv.index('--terrain') + 1] if '--terrain' in sys.argv else None
actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
meshes = [a for a in actors if isinstance(a, u.StaticMeshActor) and a.static_mesh_component.static_mesh]
terrain = next(a for a in meshes if (a.get_actor_label() == label if label else a.static_mesh_component.static_mesh.get_name() == 'SM_Silverbrook_Terrain'))
SKIP = ('ChimneySmoke_', 'Flower_', 'StoopTub_', 'StoopFlower_', 'ShopSign_', 'SillPlanter_')  # smoke, flowers in tubs, stoop tubs, wall signs, sill planters
rows = []
for a in actors:
    tags = [str(t) for t in a.tags]  # dressing props and 'static' package parts (street barrels, benches)
    if not ('SilverbrookDressing' in tags or 'static' in tags) or a.get_actor_label().startswith(SKIP):
        continue
    p = a.get_actor_location()
    hit = terrain.static_mesh_component.line_trace_component(u.Vector(p.x, p.y, p.z + 3000), u.Vector(p.x, p.y, p.z - 3000), True, False)
    rows.append(dict(actor=a.get_actor_label(), gap_cm=round(p.z - hit[0].z, 2) if hit else None))
fail = [r for r in rows if r['gap_cm'] is None or r['gap_cm'] > 5]
print('DRESSING_GROUND', dict(checked=len(rows), floating=len(fail), floating_over_5_cm=fail, max_positive_gap_cm=max((r['gap_cm'] or 0 for r in rows), default=0)))
assert not fail
