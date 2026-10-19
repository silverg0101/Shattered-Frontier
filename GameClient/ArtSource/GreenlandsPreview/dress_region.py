"""Promote the owned preview map to the connected, metre-scale slice layout."""
import unreal as u, sys, random, math, json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import *
P='/Game/Frontier/Environment/GreenlandsPreview';MAP=P+'/Greenlands_EnvironmentPreview'
tools=u.AssetToolsHelpers.get_asset_tools();actors=u.get_editor_subsystem(u.EditorActorSubsystem);levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert u.EditorAssetLibrary.does_asset_exist(MAP)
def flat(name,c):
    path=P+'/Materials/'+name
    if u.EditorAssetLibrary.does_asset_exist(path):return u.load_asset(path)
    m=tools.create_asset(name,P+'/Materials',u.Material,u.MaterialFactoryNew())
    e=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant3Vector);e.set_editor_property('constant',u.LinearColor(*c,1));u.MaterialEditingLibrary.connect_material_property(e,'',u.MaterialProperty.MP_BASE_COLOR)
    r=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant);r.set_editor_property('r',.9);u.MaterialEditingLibrary.connect_material_property(r,'',u.MaterialProperty.MP_ROUGHNESS);u.MaterialEditingLibrary.recompile_material(m);u.EditorAssetLibrary.save_loaded_asset(m);return m
materials={k:u.load_asset(P+'/Materials/'+v) for k,v in {'Ground':'M_Ground','Grass':'M_Grass','Stone':'M_Rock','Bark':'M_Bark'}.items()}
materials.update(Plaster=flat('M_WarmPlaster',(.48,.37,.23)),Roof=flat('M_SlateRoof',(.065,.083,.08)),Canvas=flat('M_OchreCanvas',(.43,.28,.075)))
meshes={}
for name in ['SM_GreenlandsRegion','SM_RegionGrass','SM_FootholdCottage','SM_MarketStall','SM_LogStack','SM_RouteMarker']:
    path=P+'/Meshes/'+name
    if name in ['SM_GreenlandsRegion','SM_RegionGrass'] or not u.EditorAssetLibrary.does_asset_exist(path):
        t=u.AssetImportTask();t.filename=str(ROOT/(name+'.fbx'));t.destination_path=P+'/Meshes';t.automated=True;t.save=True;t.replace_existing=True
        opt=u.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.set_editor_property('vertex_color_import_option',u.VertexColorImportOption.REPLACE);t.options=opt;tools.import_asset_tasks([t])
    mesh=u.load_asset(path);assert isinstance(mesh,u.StaticMesh)
    for i,s in enumerate(mesh.static_materials):
        key=str(s.material_slot_name)
        mesh.set_material(i,materials.get(key,materials['Grass'] if name=='SM_RegionGrass' else materials['Ground']))
    if name!='SM_RegionGrass':mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    u.EditorAssetLibrary.save_loaded_asset(mesh);meshes[name]=mesh
for name in ['SM_Oak_00','SM_Oak_01','SM_Oak_02','SM_Boulder_00','SM_Boulder_01','SM_Boulder_02','SM_GreenlandsWaystone','SM_Wildflowers']:meshes[name]=u.load_asset(P+'/Meshes/'+name)
levels.load_level(MAP)
# This map is exclusively owned by the environment task. Other maps are never loaded.
for a in actors.get_all_level_actors():
    if isinstance(a,(u.StaticMeshActor,u.TextRenderActor)):actors.destroy_actor(a)
def place(name,x,y,z=None,scale=1,yaw=0,collision=True,label=None):
    z=height(x,y) if z is None else z
    a=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x*100,y*100,z*100),u.Rotator(yaw=yaw))
    a.set_actor_label(label or name);a.static_mesh_component.set_static_mesh(meshes[name]);a.set_actor_scale3d(u.Vector(scale,scale,scale));a.static_mesh_component.set_collision_profile_name('BlockAll' if collision else 'NoCollision');return a
place('SM_GreenlandsRegion',0,0,0,label='Greenlands - continuous 600m terrain')
place('SM_RegionGrass',0,0,0,collision=False)
# Arrival orientation and settlement beacon: same existing landmark art, visual only.
place('SM_GreenlandsWaystone',-185,-85,scale=1.2,label='Arrival landmark')
place('SM_GreenlandsWaystone',-90,-40,scale=1.7,label='Settlement beacon - visual location')
for x,y,yaw in [(-107,-48,90),(-77,-55,180),(-109,-23,90),(-76,-20,0)]:
    place('SM_FootholdCottage',x,y,yaw=yaw+180,label='Settlement cottage - open doorway, 8m x 6m')
for x,y,yaw in [(-95,-29,0),(-84,-30,0)]:place('SM_MarketStall',x,y,yaw=yaw)
for x,y in [(-105,-57),(-75,-63),(86,76),(96,83)]:place('SM_LogStack',x,y)
# Three 20x20m level build plots. Corner stones mark space, without claim gameplay.
for key in ['Build_West','Build_Central','Build_East']:
    x,y=AREAS[key]['center']
    for dx,dy in [(-10,-10),(10,-10),(10,10),(-10,10)]:place('SM_Boulder_00',x+dx,y+dy,scale=.14,label=key+' corner - 20m plot')
random.seed(180)
tree_count=0
for i in range(780):
    x=random.uniform(-270,270);y=random.uniform(-260,260)
    if road_distance(x,y)<6:continue
    blocked=False
    for name,a in AREAS.items():
        d=math.dist((x,y),a['center'])
        if name!='Resource_Grove' and d<a['radius']+5:blocked=True;break
    if blocked:continue
    # Keep open grassland south/west; denser woods on the resource route.
    grove=math.dist((x,y),(100,100))<85
    if not grove and random.random()<.46:continue
    place('SM_Oak_%02d'%(i%3),x,y,scale=random.uniform(.85,1.55),yaw=random.uniform(0,360),label='Greenlands broadleaf grove')
    tree_count+=1
for i in range(120):
    x=random.uniform(-250,250);y=random.uniform(-230,230)
    if road_distance(x,y)<5 or any(math.dist((x,y),a['center'])<a['radius'] for a in AREAS.values()):continue
    place('SM_Boulder_%02d'%(i%3),x,y,z=height(x,y)-.15,scale=random.uniform(.5,2),yaw=random.uniform(0,360))
# Outcrop is scenery, never a harvestable resource node.
for i in range(24):
    x=155+random.uniform(-14,14);y=55+random.uniform(-14,14)
    if road_distance(x,y)<3:continue
    place('SM_Boulder_%02d'%(i%3),x,y,z=height(x,y)-.2,scale=random.uniform(1.5,4),yaw=random.uniform(0,360),label='Resource-route stone outcrop - visual only')
# Rocks frame danger's clearing and the return overlook, preserving open entrances.
for cx,cy,rad in [(130,-135,25),(-65,-150,16)]:
    for i in range(14):
        a=i*math.tau/14;x=cx+math.cos(a)*rad;y=cy+math.sin(a)*rad
        if road_distance(x,y)>5:place('SM_Boulder_01',x,y,scale=random.uniform(.8,2),yaw=i*47)
place('SM_GreenlandsWaystone',-65,-150,scale=.8,label='Return overlook landmark')
for i,(x,y) in enumerate(ROUTE[:-1]):place('SM_RouteMarker',x-3.3,y+3.3,yaw=i*33,label='Route junction marker')
for i in range(90):
    x=random.uniform(-200,200);y=random.uniform(-190,160)
    if soil(x,y)<.1:place('SM_Wildflowers',x,y,scale=random.uniform(.8,1.7),yaw=random.uniform(0,360),collision=False)
for a in actors.get_all_level_actors():
    if isinstance(a,u.PlayerStart):
        a.set_actor_location(u.Vector(-19000,-8900,height(-190,-89)*100+350),False,False);a.set_actor_rotation(u.Rotator(yaw=27),False)
    if isinstance(a,u.DirectionalLight):a.light_component.set_editor_property('intensity',5.0);a.set_actor_rotation(u.Rotator(pitch=-38,yaw=-35),False)
    if isinstance(a,u.SkyLight):a.light_component.set_editor_property('intensity',2.0)
u.EditorLevelLibrary.set_level_viewport_camera_info(u.Vector(-16300,-11000,1800),u.Rotator(pitch=-12,yaw=35))
assert levels.save_current_level()
(ROOT/'region-checkpoint.json').write_text(json.dumps(dict(map=MAP,terrain_metres=[600,600],route_metres=round(ROUTE_METRES,1),trees=tree_count,plots=3,plot_metres=[20,20],cottages=4,design_roles=list(AREAS),status='Authored environment and static scenery; no claims, nodes, combat, economy, terrain edits or networking implemented here.'),indent=2))
