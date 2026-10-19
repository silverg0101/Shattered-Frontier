import unreal as u,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import height
u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/Frontier/Environment/GreenlandsPreview/Greenlands_EnvironmentPreview')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();out=[]
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if isinstance(a,u.InstancedFoliageActor):
        for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
            out.append(dict(label='Native foliage '+c.static_mesh.get_name(),count=c.get_instance_count()))
        continue
    if not a.get_actor_label().startswith('Village instances - '):continue
    c=a.get_component_by_class(u.InstancedStaticMeshComponent)
    row=dict(label=a.get_actor_label(),count=c.get_instance_count())
    if row['count']:
        t=c.get_instance_transform(0,True);p=t.translation
        result=u.SystemLibrary.line_trace_single(world,u.Vector(p.x,p.y,p.z+2000),u.Vector(p.x,p.y,p.z-2000),u.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[],u.DrawDebugTrace.NONE,True)
        row.update(position=[p.x,p.y,p.z],authored_height_cm=height(p.x/100,p.y/100)*100,hit=str(result))
        if isinstance(result,u.HitResult):row['terrain_hit']=str(u.GameplayStatics.break_hit_result(result))
    out.append(row)
(ROOT/'saved-ground-inspection.json').write_text(json.dumps(out,indent=2))
