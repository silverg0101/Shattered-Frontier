"""Cottage joinery/interiors and layered woodland edge. Full editor, owned map."""
import unreal as u,json,math,random,sys
from dataclasses import dataclass
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import height,road_distance
from planting_layout import load_settings,allowed,local_point
P='/Game/Frontier/Environment/GreenlandsPreview';D=P+'/Arrival';TAG='GreenlandsSettlementFinish'
@dataclass(frozen=True)
class FinishSettings:
    seed:int
    woodland_clusters:int
    ferns_per_cluster:int
    woodland_bounds_metres:list
    tree_height_metres:list
    fern_height_metres:list
s=FinishSettings(**json.loads((ROOT/'settlement-finish-settings.json').read_text()));planting=load_settings()
assert 0<=s.seed<2**32 and 1<=s.woodland_clusters<=60 and 1<=s.ferns_per_cluster<=60
assert 2<=s.tree_height_metres[0]<=s.tree_height_metres[1]<=6 and .1<=s.fern_height_metres[0]<=s.fern_height_metres[1]<=.6
assert len(s.woodland_bounds_metres)==4
tools=u.AssetToolsHelpers.get_asset_tools();sub=u.get_editor_subsystem(u.StaticMeshEditorSubsystem);assert sub,'Use full editor -ExecutePythonScript'
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
wood=u.load_asset(D+'/Materials/M_CottageExteriorTimber') or u.load_asset(D+'/Materials/M_WeatheredTimber');soil=u.load_asset(D+'/Materials/M_GardenSoil');assert wood and soil
path=D+'/Materials/M_ForgedWindowIron';iron=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset('M_ForgedWindowIron',D+'/Materials',u.Material,u.MaterialFactoryNew())
u.MaterialEditingLibrary.delete_all_material_expressions(iron)
for prop,value in [(u.MaterialProperty.MP_BASE_COLOR,.025),(u.MaterialProperty.MP_ROUGHNESS,.65),(u.MaterialProperty.MP_METALLIC,.85)]:
    node=u.MaterialEditingLibrary.create_material_expression(iron,u.MaterialExpressionConstant);node.set_editor_property('r',value);u.MaterialEditingLibrary.connect_material_property(node,'',prop)
u.MaterialEditingLibrary.recompile_material(iron);u.EditorAssetLibrary.save_loaded_asset(iron)
path=D+'/Materials/M_CottageWindowGlass';glass=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset('M_CottageWindowGlass',D+'/Materials',u.Material,u.MaterialFactoryNew())
u.MaterialEditingLibrary.delete_all_material_expressions(glass)
node=u.MaterialEditingLibrary.create_material_expression(glass,u.MaterialExpressionConstant3Vector);node.set_editor_property('constant',u.LinearColor(.025,.045,.065,1));u.MaterialEditingLibrary.connect_material_property(node,'',u.MaterialProperty.MP_BASE_COLOR)
for prop,value in [(u.MaterialProperty.MP_ROUGHNESS,.18),(u.MaterialProperty.MP_SPECULAR,.6)]:
    node=u.MaterialEditingLibrary.create_material_expression(glass,u.MaterialExpressionConstant);node.set_editor_property('r',value);u.MaterialEditingLibrary.connect_material_property(node,'',prop)
u.MaterialEditingLibrary.recompile_material(glass);u.EditorAssetLibrary.save_loaded_asset(glass)
joinery={}
for name in ['SM_CottageShutterJoinery','SM_CottageOpenShutters','SM_CottageWindowBox']:
    path=D+'/Meshes/'+name
    task=u.AssetImportTask();task.filename=str(ROOT/'PreparedArrival'/(name+'.fbx'));task.destination_path=D+'/Meshes';task.automated=True;task.save=True;task.replace_existing=True
    options=u.FbxImportUI();options.import_materials=False;options.import_textures=False;options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=False;task.options=options;tools.import_asset_tasks([task])
    mesh=u.load_asset(path);assert mesh
    for i,slot in enumerate(mesh.static_materials):mesh.set_material(i,{'WeatheredWood':wood,'ForgedIron':iron,'GardenSoil':soil,'WindowGlass':glass}[str(slot.material_slot_name)])
    u.EditorAssetLibrary.save_loaded_asset(mesh);joinery[name]=mesh
records={row['name']:row for row in json.loads((ROOT/'village-selection.json').read_text())['meshes']}
native={name:u.load_asset(row['package']) for name,row in records.items()}
for i in range(4):
    fern=u.load_asset(D+'/Meshes/SM_PH_fern_02_%02d'%i);assert fern
    if fern.get_num_lods()<3:
        settings=u.EditorScriptingMeshReductionOptions();settings.auto_compute_lod_screen_size=False;settings.reduction_settings=[u.EditorScriptingMeshReductionSettings(percent_triangles=p,screen_size=z) for p,z in [(1,1),(.35,.12),(.1,.035)]];sub.set_lods(fern,settings);assert fern.get_num_lods()==3;u.EditorAssetLibrary.save_loaded_asset(fern)
levels.load_level(P+'/Greenlands_EnvironmentPreview');world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
for actor in actors.get_all_level_actors():
    if TAG in map(str,actor.tags):actors.destroy_actor(actor)
cleared=[]
def overlaps_cottage(x,y,radius):
    return any(abs(local_point(0,0,-yaw,x-cx,y-cy)[0])<4+radius and abs(local_point(0,0,-yaw,x-cx,y-cy)[1])<3+radius for cx,cy,yaw in planting.cottages_metres_yaw)
for actor in actors.get_all_level_actors():
    for component in actor.get_components_by_class(u.InstancedStaticMeshComponent):
        mesh=component.static_mesh
        if not mesh or not any(word in mesh.get_name().lower() for word in ['rock_moss','fern_02','grassclump','grass_medium','wheatgrass']):continue
        for i in reversed(range(component.get_instance_count())):
            t=component.get_instance_transform(i,True);p=t.translation;radius=math.hypot(mesh.get_bounds().box_extent.x,mesh.get_bounds().box_extent.y)/100*max(t.scale3d.x,t.scale3d.y)
            if overlaps_cottage(p.x/100,p.y/100,radius):component.remove_instance(i);cleared.append(dict(mesh=mesh.get_name(),metres=[p.x/100,p.y/100]))
    if isinstance(actor,u.StaticMeshActor):
        mesh=actor.static_mesh_component.static_mesh
        if not mesh or not any(word in mesh.get_name().lower() for word in ['rock_moss','tree_stump','wine_barrel','wooden_crate']):continue
        p=actor.get_actor_location();scale=actor.get_actor_scale3d();radius=math.hypot(mesh.get_bounds().box_extent.x,mesh.get_bounds().box_extent.y)/100*max(scale.x,scale.y)
        if overlaps_cottage(p.x/100,p.y/100,radius):cleared.append(dict(mesh=mesh.get_name(),metres=[p.x/100,p.y/100]));actors.destroy_actor(actor)
if cleared:(ROOT/'settlement-cleared-obstacles.json').write_text(json.dumps(cleared,indent=2))
counts={}
def place(mesh,x,y,z,yaw=0,scale=(1,1,1),label='Settlement detail',collision=False,center=False,material=None):
    if center:
        b=mesh.get_bounds();ox=b.origin.x/100*scale[0];oy=b.origin.y/100*scale[1];a=math.radians(yaw)
        x-=ox*math.cos(a)-oy*math.sin(a);y-=ox*math.sin(a)+oy*math.cos(a);z-=(b.origin.z-b.box_extent.z)/100*scale[2]
    actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x*100,y*100,z*100),u.Rotator(yaw=yaw));actor.tags=[TAG];actor.set_actor_label(label);actor.set_actor_scale3d(u.Vector(*scale));actor.static_mesh_component.set_static_mesh(mesh);actor.static_mesh_component.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    if material:actor.static_mesh_component.set_material(0,material)
    counts[label]=counts.get(label,0)+1;return actor
cube=u.load_asset('/Engine/BasicShapes/Cube');flower=u.load_asset(D+'/Meshes/SM_PH_FloweringClump_2')
barrel=u.load_asset(D+'/Meshes/SM_PH_wine_barrel_01_02');crate=u.load_asset(D+'/Meshes/SM_PH_wooden_crate_01_00')
for index,(cx,cy,yaw) in enumerate(planting.cottages_metres_yaw):
    floor=height(cx,cy)+.16
    def part(mesh,lx,ly,z,angle=0,**kw):
        x,y=local_point(cx,cy,yaw,lx,ly);return place(mesh,x,y,floor+z,yaw+angle,**kw)
    for window,x in enumerate([-3,-1,1,3]):
        closed=(window+index)%4==0;part(joinery['SM_CottageShutterJoinery' if closed else 'SM_CottageOpenShutters'],x,-3.2,3,scale=(.82,.82,.82) if closed else (1,1,1),label='Cottage upper shutter joinery')
    for side in [-1,1]:
        part(joinery['SM_CottageOpenShutters'],side*2.5,-3.2,1.13,label='Cottage ground shutter joinery')
        part(joinery['SM_CottageWindowBox'],side*2.5,-3.4,.33,label='Cottage flower box')
        for offset in [-.44,-.22,0,.22,.44]:part(flower,side*2.5+offset,-3.4,.43,scale=(.85,.85,.85),label='Window box blooms')
        part(joinery['SM_CottageOpenShutters'],side*4.2,-1.5,3,side*90,label='Cottage side shutter joinery')
        part(native['SM_OldWoodenDoor'],side,-2.98,0,90,label='Open cottage entry door',collision=True)
    part(cube,0,0,2.0,scale=(7.7,5.7,.12),material=wood,label='Cottage timber loft floor',collision=True)
    part(native['SM_OldWoodenTable'],-1.4,1.25,0,center=True,label='Cottage interior table',collision=True)
    part(native['SM_OldWoodenBench'],-1.4,2.25,0,90,center=True,label='Cottage interior bench',collision=True)
    for lx,ly,mesh,angle in [(-4.95,1.5,barrel,17),(-4.95,2.55,crate,0),(-4.95,2.55,crate,8)]:
        x,y=local_point(cx,cy,yaw,lx,ly)
        if allowed(x,y,planting):place(mesh,x,y,height(x,y)+(.33 if mesh==crate and angle==8 else 0),yaw+angle,label='Cottage side supplies',collision=True,center=True)

# Anchor understory to existing conifer positions, rather than scattering
# shrubs across the open arrival road and construction meadow.
centres=[]
for actor in actors.get_all_level_actors():
    for component in actor.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent):
        if not component.static_mesh or not component.static_mesh.get_name().startswith('spruce_full'):continue
        for i in range(component.get_instance_count()):
            p=component.get_instance_transform(i,True).translation;x,y=p.x/100,p.y/100
            xmin,ymin,xmax,ymax=s.woodland_bounds_metres
            if xmin<x<xmax and ymin<y<ymax and 10<road_distance(x,y)<34 and allowed(x,y,planting):centres.append((x,y))
centres=sorted(centres);rng=random.Random(s.seed);rng.shuffle(centres);chosen=[]
for centre in centres:
    if all(math.dist(centre,p)>5 for p in chosen):chosen.append(centre)
    if len(chosen)>=s.woodland_clusters:break
assert chosen,'Existing village conifer anchors must be present'
fern_rows=json.loads((ROOT/'PreparedArrival/meshes.json').read_text());fern_rows=[r for r in fern_rows if r['asset']=='fern_02'];transforms={i:[] for i in range(4)}
tree=u.load_asset(D+'/Meshes/SM_PH_Broadleaf');rocks=[u.load_asset(D+'/Meshes/SM_PH_rock_moss_set_01_%02d'%i) for i in range(6)];stump=u.load_asset(D+'/Meshes/SM_PH_tree_stump_02_00')
for cluster,(cx,cy) in enumerate(chosen):
    rng=random.Random(s.seed+cluster*104729)
    for i in range(s.ferns_per_cluster):
        angle=rng.uniform(0,math.tau);radius=rng.uniform(.8,4.2);x=cx+math.cos(angle)*radius;y=cy+math.sin(angle)*radius
        if not allowed(x,y,planting) or road_distance(x,y)<5:continue
        variant=rng.randrange(4);row=fern_rows[variant];scale=rng.uniform(*s.fern_height_metres)/row['dimensions_metres'][2]
        transforms[variant].append(u.Transform(location=u.Vector(x*100,y*100,height(x,y)*100),rotation=u.Rotator(yaw=rng.uniform(0,360)),scale=u.Vector(scale,scale,scale)))
    angle=rng.uniform(0,math.tau);x=cx+math.cos(angle)*3.5;y=cy+math.sin(angle)*3.5
    if allowed(x,y,planting) and road_distance(x,y)>9 and all(math.dist((x,y),p)>2.8 for p in centres):
        scale=rng.uniform(*s.tree_height_metres)/(tree.get_bounds().box_extent.z*2/100)
        place(tree,x,y,height(x,y),rng.uniform(0,360),(scale,scale,scale),label='Young broadleaf woodland edge',collision=True,center=True)
    for j in range(2):
        angle=rng.uniform(0,math.tau);x=cx+math.cos(angle)*rng.uniform(1,3);y=cy+math.sin(angle)*rng.uniform(1,3)
        if allowed(x,y,planting) and road_distance(x,y)>5:
            scale=rng.uniform(.2,.45);place(rng.choice(rocks),x,y,height(x,y)-.05,rng.uniform(0,360),(scale,scale,scale),label='Woodland mossy root stones',collision=True,center=True)
    if cluster%5==0:place(stump,cx+1,cy,height(cx+1,cy),rng.uniform(0,360),(.7,.7,.7),label='Woodland old stump',collision=True,center=True)
for i,row in enumerate(fern_rows):
    name='FT_WoodlandFern_'+str(i);path=D+'/Foliage/'+name;ft=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Foliage',u.FoliageType_InstancedStaticMesh,u.FoliageType_InstancedStaticMeshFactory())
    ft.set_editor_property('mesh',u.load_asset(D+'/Meshes/'+row['mesh']));ft.set_editor_property('cast_shadow',False);ft.set_editor_property('enable_density_scaling',False);u.EditorAssetLibrary.save_loaded_asset(ft)
    u.InstancedFoliageActor.remove_all_instances(world,ft);u.InstancedFoliageActor.add_instances(world,ft,transforms[i])
for actor in actors.get_all_level_actors():
    if isinstance(actor,u.InstancedFoliageActor):
        for component in actor.get_components_by_class(u.InstancedStaticMeshComponent):component.set_collision_profile_name('NoCollision')
assert levels.save_current_level()
(ROOT/'settlement-finish-report.json').write_text(json.dumps(dict(cottages=4,details=counts,woodland_clusters=chosen,fern_count=sum(map(len,transforms.values())),scope='Static environment dressing only; entrances remain open; no shared gameplay or other maps'),indent=2))
