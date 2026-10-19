"""Import workshop B and replace only the second placeholder cottage. Full editor."""
import unreal as u,json,math,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT.parent));from region_layout import height
P='/Game/Frontier/Environment/GreenlandsPreview';D=P+'/BuildingFamilyV1';TAG='GreenlandsWorkshop';cx,cy,yaw=-77,-55,180
tools=u.AssetToolsHelpers.get_asset_tools();sub=u.get_editor_subsystem(u.StaticMeshEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem);levels=u.get_editor_subsystem(u.LevelEditorSubsystem);assert sub
manifest=json.loads((ROOT/'workshop-manifest.json').read_text());meshes={}
for row in manifest['meshes']:
    name=row['mesh'];task=u.AssetImportTask();task.filename=str(ROOT/(name+'.fbx'));task.destination_path=D+'/Meshes';task.automated=True;task.save=True;task.replace_existing=True
    opts=u.FbxImportUI();opts.import_materials=False;opts.import_textures=False;opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.auto_generate_collision=False;opts.static_mesh_import_data.vertex_color_import_option=u.VertexColorImportOption.REPLACE;task.options=opts
    if name.endswith('_Approach'):
        opts.static_mesh_import_data.vertex_color_import_option=u.VertexColorImportOption.OVERRIDE;opts.static_mesh_import_data.vertex_override_color=u.Color(255,76,0,255)
    tools.import_asset_tasks([task])
    mesh=u.load_asset(D+'/Meshes/'+name);assert mesh
    for i,slot in enumerate(mesh.static_materials):
        m=u.load_asset(P+'/Arrival/Materials/M_WornTrack' if name.endswith('_Approach') else P+'/CottageConceptV1/Materials/M_'+str(slot.material_slot_name));assert m,str(slot.material_slot_name);mesh.set_material(i,m)
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    lod=u.EditorScriptingMeshReductionOptions();lod.auto_compute_lod_screen_size=False;lod.reduction_settings=[u.EditorScriptingMeshReductionSettings(percent_triangles=p,screen_size=s) for p,s in [(1,1),(.6,.25),(.3,.1),(.12,.035)]]
    sub.set_lods(mesh,lod);assert mesh.get_num_lods()==4;u.EditorAssetLibrary.save_loaded_asset(mesh);meshes[name]=mesh
levels.load_level(P+'/Greenlands_EnvironmentPreview');removed=[];cleared=[];bench=None
def local(p):return -(p.x/100-cx),-(p.y/100-cy)
for actor in actors.get_all_level_actors():
    label=actor.get_actor_label();tags=set(map(str,actor.tags));p=actor.get_actor_location();lx,ly=local(p);inside=abs(lx)<4.7 and abs(ly)<3.75
    old=label.startswith('Settlement house 2 -') or label.startswith('House 2 ')
    detail=inside and (label=='House timber eaves beam' or ('GreenlandsSettlementFinish' in tags and label.startswith(('Cottage ','Open cottage','Window box'))))
    skin='GreenlandsCottageExterior' in tags and math.hypot(lx,ly)<1
    if TAG in tags or old or detail or skin:
        if isinstance(actor,u.StaticMeshActor) and actor.static_mesh_component.static_mesh and actor.static_mesh_component.static_mesh.get_name()=='SM_OldWoodenBench':bench=actor.static_mesh_component.static_mesh
        removed.append(label);actors.destroy_actor(actor);continue
    # Extend the existing mown footprint under the new canopy, without rerolling
    # the meadow or removing the surrounding trees.
    for comp in actor.get_components_by_class(u.InstancedStaticMeshComponent):
        mesh=comp.static_mesh
        if not mesh or not any(word in mesh.get_name().lower() for word in ['grass','fern','flower','dandelion','everlasting','groundcover','rock_moss']):continue
        for i in reversed(range(comp.get_instance_count())):
            t=comp.get_instance_transform(i,True);x,y=local(t.translation)
            if (3.8<x<6.7 and abs(y)<3.65) or (-.3<x<2.1 and -4.9<y<-2.8):
                cleared.append(dict(mesh=mesh.get_path_name(),location_cm=[t.translation.x,t.translation.y,t.translation.z]));comp.remove_instance(i)
    if isinstance(actor,u.StaticMeshActor) and -4.8<lx<6.7 and -5.6<ly<3.65 and 'GreenlandsGardenPlanting' in tags:removed.append(label);actors.destroy_actor(actor)
for name,mesh in meshes.items():
    actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(cx*100,cy*100,(height(cx,cy)+.16)*100),u.Rotator(yaw=yaw));actor.tags=[TAG];actor.set_actor_label('Carpenter workshop - '+name);actor.static_mesh_component.set_static_mesh(mesh);actor.static_mesh_component.set_collision_profile_name('BlockAll')
for lx,ly,z,power in [(2.45,-3.47,2.17,45),(-2.8,.2,2.65,450)]:
    actor=actors.spawn_actor_from_class(u.PointLight,u.Vector((cx-lx)*100,(cy-ly)*100,(height(cx,cy)+.16+z)*100));actor.tags=[TAG];actor.set_actor_label('Workshop warm practical light');light=actor.get_component_by_class(u.PointLightComponent);light.set_mobility(u.ComponentMobility.MOVABLE);light.set_editor_property('intensity_units',u.LightUnits.LUMENS);light.set_intensity(power);light.set_light_color(u.LinearColor(1,.74,.48));light.set_attenuation_radius(400 if power>100 else 150)
# Native bench origins are not at their visual centres. The old pair encroaches
# on the new offset door; retain one properly centred seat beneath the window.
for actor in actors.get_all_level_actors():
    if not isinstance(actor,u.StaticMeshActor):continue
    mesh=actor.static_mesh_component.static_mesh
    if not mesh or mesh.get_name()!='SM_OldWoodenBench':continue
    lx,ly=local(actor.get_actor_location())
    if abs(lx)<5 and -6<ly<-3.6:bench=mesh;removed.append(actor.get_actor_label());actors.destroy_actor(actor)
if bench:
    b=bench.get_bounds();angle=math.radians(yaw+90);ox=b.origin.x*math.cos(angle)-b.origin.y*math.sin(angle);oy=b.origin.x*math.sin(angle)+b.origin.y*math.cos(angle)
    actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector((cx+2.7)*100-ox,(cy+4.6)*100-oy,height(cx+2.7,cy+4.6)*100-(b.origin.z-b.box_extent.z)),u.Rotator(yaw=yaw+90));actor.tags=[TAG];actor.set_actor_label('Workshop centred window bench');actor.static_mesh_component.set_static_mesh(bench);actor.static_mesh_component.set_collision_profile_name('BlockAll')
assert levels.save_current_level()
(ROOT/'workshop-ue-report.json').write_text(json.dumps(dict(site_metres=[cx,cy],yaw=yaw,main_shell_metres=[8,6],meshes=list(meshes),lods=4,removed=removed,canopy_instances_cleared=cleared,material_dependency=P+'/CottageConceptV1/Materials',pending='Live collision, native pawn movement and visual review'),indent=2));print('WORKSHOP_IMPORTED',len(removed),len(cleared),flush=True)

