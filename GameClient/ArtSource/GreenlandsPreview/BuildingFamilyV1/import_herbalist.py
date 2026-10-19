"""Import family A, replacing only the third placeholder cottage. Full editor."""
import unreal as u,json,math,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT.parent));from region_layout import height
P='/Game/Frontier/Environment/GreenlandsPreview';D=P+'/BuildingFamilyV1';TAG='GreenlandsHerbalist';cx,cy,yaw=-109,-23,90
tools=u.AssetToolsHelpers.get_asset_tools();sub=u.get_editor_subsystem(u.StaticMeshEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem);levels=u.get_editor_subsystem(u.LevelEditorSubsystem);assert sub
manifest=json.loads((ROOT/'herbalist-manifest.json').read_text());meshes={}
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
levels.load_level(P+'/Greenlands_EnvironmentPreview');removed=[];cleared=[]
def local(p):return p.y/100-cy,-(p.x/100-cx)
for actor in actors.get_all_level_actors():
    label=actor.get_actor_label();tags=set(map(str,actor.tags));lx,ly=local(actor.get_actor_location());inside=abs(lx)<4.7 and abs(ly)<3.75
    old=label.startswith('Settlement house 3 -') or label.startswith('House 3 ')
    detail=inside and (label=='House timber eaves beam' or ('GreenlandsSettlementFinish' in tags and label.startswith(('Cottage ','Open cottage','Window box'))))
    skin='GreenlandsCottageExterior' in tags and math.hypot(lx,ly)<1
    if TAG in tags or old or detail or skin:removed.append(label);actors.destroy_actor(actor);continue
    if isinstance(actor,u.StaticMeshActor):
        mesh=actor.static_mesh_component.static_mesh
        obsolete_bench=mesh and mesh.get_name()=='SM_OldWoodenBench' and abs(lx)<5 and -6<ly<-3
        old_garden='GreenlandsGardenPlanting' in tags and -5.3<lx<3.5 and -5.7<ly<4.6
        if obsolete_bench or old_garden:removed.append(label);actors.destroy_actor(actor);continue
    for comp in actor.get_components_by_class(u.InstancedStaticMeshComponent):
        mesh=comp.static_mesh
        if not mesh or not any(word in mesh.get_name().lower() for word in ['grass','fern','flower','dandelion','everlasting','groundcover','rock_moss']):continue
        for i in reversed(range(comp.get_instance_count())):
            t=comp.get_instance_transform(i,True);x,y=local(t.translation)
            footprint=(-3.3<x<3.3 and abs(y)<4.5) or (-5.2<x<-2.8 and -3.7<y<1.3) or (abs(x)<1.4 and -5.7<y<-3.7)
            if footprint:cleared.append(dict(mesh=mesh.get_path_name(),location_cm=[t.translation.x,t.translation.y,t.translation.z]));comp.remove_instance(i)
for name,mesh in meshes.items():
    actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(cx*100,cy*100,(height(cx,cy)+.16)*100),u.Rotator(yaw=yaw));actor.tags=[TAG];actor.set_actor_label('Herbalist cottage - '+name);actor.static_mesh_component.set_static_mesh(mesh);actor.static_mesh_component.set_collision_profile_name('BlockAll')
def plant(mesh,x,y,z,scale,label,angle):
    b=mesh.get_bounds();actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector((cx-y)*100,(cy+x)*100,(height(cx,cy)+.16+z)*100-(b.origin.z-b.box_extent.z)*scale),u.Rotator(yaw=angle));actor.tags=[TAG,'GreenlandsDecorativePlant'];actor.set_actor_label(label);actor.static_mesh_component.set_static_mesh(mesh);actor.set_actor_scale3d(u.Vector(scale,scale,scale));actor.static_mesh_component.set_collision_profile_name('NoCollision')
flower=u.load_asset(P+'/Arrival/Meshes/SM_PH_FloweringClump_2');leaf=u.load_asset(P+'/Garden/Megascans/3D_Plants/LeafyDandelions/SM_LeafyDandelions_00');assert flower and leaf
for wx,z in [(-1.95,.80),(1.95,.80),(0,4.05)]:
    for row in range(2):
        for i,dx in enumerate([-.36,0,.36]):plant(flower,wx+dx,-4.24-row*.15,z,1.1,'Herbalist tended window blooms',i*137+row*75)
    for i,dx in enumerate([-.21,.21]):plant(leaf,wx+dx,-4.40,z,.6,'Herbalist window greenery',i*155+35)
for j in range(3):plant(leaf,-3.45+(j%2)*.2,-2.2+j*.3,1.195+j*.035,.45,'Herbalist potted preparation herbs',j*125)
for lx,ly,z,power in [(1.35,-4.45,2.1,45),(-1.1,1.3,2.25,350)]:
    actor=actors.spawn_actor_from_class(u.PointLight,u.Vector((cx-ly)*100,(cy+lx)*100,(height(cx,cy)+.16+z)*100));actor.tags=[TAG];actor.set_actor_label('Herbalist warm practical light');light=actor.get_component_by_class(u.PointLightComponent);light.set_mobility(u.ComponentMobility.MOVABLE);light.set_editor_property('intensity_units',u.LightUnits.LUMENS);light.set_intensity(power);light.set_light_color(u.LinearColor(1,.74,.48));light.set_attenuation_radius(400 if power>100 else 150)
assert levels.save_current_level()
(ROOT/'herbalist-ue-report.json').write_text(json.dumps(dict(site_metres=[cx,cy],yaw=yaw,main_shell_metres=[6,8],meshes=list(meshes),lods=4,removed=removed,footprint_instances_cleared=cleared,decorative_plant_actors=27,plant_source='Poly Haven CC0 Heliophila and previously acquired Medieval Village Garden leaves; all dependencies inside owned subtree',pending='Live pawn, collision and visual review'),indent=2));print('HERBALIST_IMPORTED',len(removed),len(cleared),flush=True)


