"""Check the terrain gap for all ground-level WP6e dressing actors."""
import json
from pathlib import Path
import unreal as u

actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
terrain=next(a for a in actors if isinstance(a,u.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name()=='SM_Silverbrook_Terrain')
rows=[]
for a in actors:
    if 'SilverbrookDressing' not in [str(t) for t in a.tags]:continue
    label=a.get_actor_label()
    if label.startswith(('ChimneySmoke_','Flower_','StoopTub_','StoopFlower_','ShopSign_','SillPlanter_')):continue  # smoke cards, flowers in tubs, tubs on stoop steps, wall-mounted signs and sill planters
    p=a.get_actor_location()
    hit=terrain.static_mesh_component.line_trace_component(u.Vector(p.x,p.y,p.z+3000),u.Vector(p.x,p.y,p.z-3000),True,False)
    rows.append(dict(actor=label,gap_cm=round(p.z-hit[0].z,2) if hit else None))
fail=[r for r in rows if r['gap_cm'] is None or r['gap_cm']>5]
report=dict(checked=len(rows),floating_over_5_cm=fail,max_positive_gap_cm=max((r['gap_cm'] or 0 for r in rows),default=0))
Path(__file__).with_name('ground-gap.json').write_text(json.dumps(report,indent=1),encoding='utf-8')
print('DRESSING_GROUND',report)
assert not fail
