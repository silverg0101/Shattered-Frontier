"""Finish the arrival/settlement tract; preserve the connected region layout."""
import unreal as u,json,sys,math,random
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import height,road_distance,AREAS,ROUTE
P='/Game/Frontier/Environment/GreenlandsPreview';D=P+'/Arrival';TAG='GreenlandsArrivalV2'
tools=u.AssetToolsHelpers.get_asset_tools();levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
def ex(m,cls,**kw):
    e=u.MaterialEditingLibrary.create_material_expression(m,cls)
    for k,v in kw.items():e.set_editor_property(k,v)
    return e
def link(a,b,pin,out=''):u.MaterialEditingLibrary.connect_material_expressions(a,out,b,pin)
def mat(name):
    path=D+'/Materials/'+name;m=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Materials',u.Material,u.MaterialFactoryNew());u.MaterialEditingLibrary.delete_all_material_expressions(m);return m
def tex(m,file,kind='',uv=None):
    path=D+'/Textures/'+file.stem
    if not u.EditorAssetLibrary.does_asset_exist(path):
        t=u.AssetImportTask();t.filename=str(file);t.destination_path=D+'/Textures';t.automated=True;t.save=True;tools.import_asset_tasks([t])
    a=u.load_asset(path)
    if kind=='normal':a.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);a.set_editor_property('srgb',False)
    if kind=='linear':a.set_editor_property('srgb',False);a.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
    u.EditorAssetLibrary.save_loaded_asset(a);e=ex(m,u.MaterialExpressionTextureSample,texture=a)
    if kind=='normal':e.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    elif kind=='linear':e.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_MASKS)
    if uv:link(uv,e,'UVs')
    return e
def pbr(name,asset,repeats=1,road=False):
    m=mat(name);uv=ex(m,u.MaterialExpressionTextureCoordinate,u_tiling=repeats,v_tiling=repeats)
    for chan,prop,kind in [('Diffuse',u.MaterialProperty.MP_BASE_COLOR,''),('nor_dx',u.MaterialProperty.MP_NORMAL,'normal'),('Rough',u.MaterialProperty.MP_ROUGHNESS,'linear')]:
        e=tex(m,ROOT/'ArrivalSurfaces'/(asset+'_'+chan+'.jpg'),kind,uv)
        u.MaterialEditingLibrary.connect_material_property(e,'RGB' if chan!='Rough' else 'R',prop)
    if road:
        m.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED);m.set_editor_property('opacity_mask_clip_value',.25)
        c=ex(m,u.MaterialExpressionVertexColor);u.MaterialEditingLibrary.connect_material_property(c,'R',u.MaterialProperty.MP_OPACITY_MASK)
    u.MaterialEditingLibrary.recompile_material(m);assert u.EditorAssetLibrary.save_loaded_asset(m);return m
wood=pbr('M_WeatheredTimber','brown_planks_03',2)
plaster=pbr('M_LimePlaster','rough_plaster_03',3)
roof=pbr('M_GreyRoofTiles','grey_roof_tiles_02',3)
meadow=pbr('M_LivingMeadow','leafy_grass',1)
road=pbr('M_WornTrack','grass_path_3',1,True)
meshes={}
for name in ['SM_GreenlandsWornRoads','SM_RoughFieldFence','SM_SettlementSign','SM_ClosedShutters']:
    path=D+'/Meshes/'+name
    if not u.EditorAssetLibrary.does_asset_exist(path):
        t=u.AssetImportTask();t.filename=str(ROOT/'PreparedArrival'/(name+'.fbx'));t.destination_path=D+'/Meshes';t.automated=True;t.save=True
        opts=u.FbxImportUI();opts.import_materials=False;opts.import_textures=False;opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.set_editor_property('vertex_color_import_option',u.VertexColorImportOption.REPLACE);t.options=opts;tools.import_asset_tasks([t])
    mesh=u.load_asset(path);assert mesh;mesh.set_material(0,road if name=='SM_GreenlandsWornRoads' else wood)
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);u.EditorAssetLibrary.save_loaded_asset(mesh);meshes[name]=mesh
for info in json.loads((ROOT/'PreparedArrival/meshes.json').read_text()):meshes[info['mesh']]=u.load_asset(D+'/Meshes/'+info['mesh'])
levels.load_level(P+'/Greenlands_EnvironmentPreview')
existing=actors.get_all_level_actors();treefile=ROOT/'arrival-tree-positions.json'
if treefile.exists():tree_positions=json.loads(treefile.read_text())
else:
    tree_positions=[]
    for a in existing:
        if isinstance(a,u.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name().startswith('SM_Oak_'):
            l=a.get_actor_location();s=a.get_actor_scale3d();r=a.get_actor_rotation();tree_positions.append([l.x/100,l.y/100,s.x*1.65,r.yaw])
    treefile.write_text(json.dumps(tree_positions))
for a in existing:
    if TAG in [str(t) for t in a.tags]:actors.destroy_actor(a);continue
    if isinstance(a,u.StaticMeshActor):
        mesh=a.static_mesh_component.static_mesh
        if not mesh:continue
        name=mesh.get_name();loc=a.get_actor_location()
        if name.startswith('SM_Oak_') or name=='SM_RegionGrass':actors.destroy_actor(a);continue
        if name.startswith('SM_Boulder_') and -220<loc.x/100<-50 and -120<loc.y/100<10:actors.destroy_actor(a);continue
        if name=='SM_Wildflowers' and -220<loc.x/100<-50:actors.destroy_actor(a);continue
        if name=='SM_GreenlandsRegion':a.static_mesh_component.set_material(0,meadow)
        if name=='SM_FootholdCottage':
            for i,s in enumerate(mesh.static_materials):
                key=str(s.material_slot_name)
                if key=='Bark':a.static_mesh_component.set_material(i,wood)
                elif key=='Plaster':a.static_mesh_component.set_material(i,plaster)
                elif key=='Roof':a.static_mesh_component.set_material(i,roof)
        if name in ['SM_RouteMarker','SM_LogStack','SM_MarketStall']:
            for i,s in enumerate(mesh.static_materials):
                if str(s.material_slot_name)=='Bark':a.static_mesh_component.set_material(i,wood)
def place(name,x,y,z=None,yaw=0,scale=1,collision=True,label=None):
    a=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x*100,y*100,(height(x,y) if z is None else z)*100),u.Rotator(yaw=yaw));a.set_actor_label(label or name);a.tags=[TAG];a.static_mesh_component.set_static_mesh(meshes[name]);a.set_actor_scale3d(u.Vector(scale,scale,scale));a.static_mesh_component.set_collision_profile_name('BlockAll' if collision else 'NoCollision');return a
holder_class=u.load_class(None,D+'/BP_SceneryInstances.BP_SceneryInstances_C');assert holder_class
counts={}
def instances(name,poses,collision=False,cull=0,shadow=True):
    if not poses:return
    a=actors.spawn_actor_from_class(holder_class,u.Vector());a.set_actor_label('Arrival scenery - '+name);a.tags=[TAG]
    comp=a.get_component_by_class(u.HierarchicalInstancedStaticMeshComponent);assert comp
    comp.set_static_mesh(meshes[name]);comp.set_collision_profile_name('BlockAll' if collision else 'NoCollision');comp.set_editor_property('cast_shadow',shadow)
    if cull:comp.set_cull_distances(int(cull*.72),cull)
    transforms=[u.Transform(location=u.Vector(x*100,y*100,height(x,y)*100),rotation=u.Rotator(yaw=yaw).quaternion(),scale=u.Vector(s,s,s)) for x,y,s,yaw in poses]
    comp.add_instances(transforms,False,True,False);counts[name]=len(poses)
place('SM_GreenlandsWornRoads',0,0,0,collision=False,label='Continuous Greenlands road and settlement footpaths')
random.seed(591)
# Preserve existing forests and add irregular clusters along the approach.
for i in range(52):
    t=random.random();side=random.choice([-1,1]);offset=random.uniform(8,23)*side
    x=-185+95*t-.428*offset;y=-85+45*t+.904*offset
    if math.dist((x,y),(-90,-40))<32 or math.dist((x,y),(-185,-85))<10:continue
    tree_positions.append([x,y,random.uniform(1.4,2.1),random.uniform(0,360)])
instances('SM_PH_Broadleaf',tree_positions,True,35000)
# Dense detailed verge, with open road and cropped settlement circulation.
grass={i:[] for i in range(4)}
for i in range(95000):
    x=random.uniform(-215,-55);y=random.uniform(-112,8);dist=road_distance(x,y)
    if dist<2.8 or math.dist((x,y),(-90,-40))<17 or math.dist((x,y),(-185,-85))<6:continue
    if dist>26 and math.dist((x,y),(-90,-40))>40:continue
    if any(abs(x-cx)<6 and abs(y-cy)<5 for cx,cy in [(-107,-48),(-77,-55),(-109,-23),(-76,-20)]):continue
    s=random.uniform(1.25,2.2);grass[i%4].append((x,y,s,random.uniform(0,360)))
for i,poses in grass.items():instances('SM_PH_grass_medium_01_%02d'%i,poses,False,6500,False)
ferns={i:[] for i in range(4)}
for i in range(400):
    tx,ty,_,_=random.choice(tree_positions);x=tx+random.uniform(-3,3);y=ty+random.uniform(-3,3)
    if -220<x<-50 and -120<y<15 and road_distance(x,y)>3:ferns[i%4].append((x,y,random.uniform(.85,1.5),random.uniform(0,360)))
for i,poses in ferns.items():instances('SM_PH_fern_02_%02d'%i,poses,False,11000,False)
rocks={i:[] for i in range(6)}
for i in range(100):
    t=random.random();side=random.choice([-1,1]);off=random.uniform(3.8,18)*side;x=-185+95*t-.428*off;y=-85+45*t+.904*off
    if math.dist((x,y),(-90,-40))<27:continue
    rocks[i%6].append((x,y,random.uniform(.25,.75),random.uniform(0,360)))
for i,poses in rocks.items():instances('SM_PH_rock_moss_set_01_%02d'%i,poses,True,20000)
for x,y in [(-170,-71),(-156,-51),(-126,-65),(-117,-28)]:place('SM_PH_tree_stump_02_00',x,y,scale=random.uniform(.85,1.2),yaw=random.uniform(0,360))
barrel=max((m for m in json.loads((ROOT/'PreparedArrival/meshes.json').read_text()) if m['asset']=='wine_barrel_01'),key=lambda m:m['dimensions_metres'][2])['mesh']
for x,y in [(-98,-29),(-97,-28),(-82,-31),(-103,-45),(-80,-52)]:place(barrel,x,y,yaw=random.uniform(0,360))
for x,y in [(-99,-30),(-84,-29),(-103,-46)]:place('SM_PH_wooden_crate_01_00',x,y,yaw=random.uniform(-15,15))
# Field edges follow the route; 6m gaps preserve access rather than walling it off.
for i in list(range(0,5))+list(range(7,13)):
    x=-176+i*4*.904;y=-90+i*4*.428;place('SM_RoughFieldFence',x,y,yaw=25.3,label='Open field fence beside approach')
for x,y,yaw in [(-107,-50,90),(-107,-46,90),(-77,-57,180),(-109,-25,90),(-76,-18,0)]:
    # Shutters mount on exterior faces, retaining the actual open doors.
    a=place('SM_ClosedShutters',x,y,z=height(x,y)+1.9,yaw=yaw,collision=False)
for x,y,label in [(-180,-89,'Settlement'),(-139,-65,'Settlement'),(-70,-8,'Grove / Build plots')]:
    place('SM_SettlementSign',x,y,yaw=-70)
    text=actors.spawn_actor_from_class(u.TextRenderActor,u.Vector(x*100-5,y*100-12,(height(x,y)+1.9)*100),u.Rotator(yaw=200));text.tags=[TAG];text.set_actor_label('Wooden wayfinding - '+label);text.text_render.set_text(label);text.text_render.set_world_size(13);text.text_render.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER);text.text_render.set_text_render_color(u.Color(239,225,188,255))
for a in actors.get_all_level_actors():
    if isinstance(a,u.DirectionalLight):a.set_actor_rotation(u.Rotator(pitch=-32,yaw=-42),False);a.light_component.set_editor_property('intensity',5.5)
    if isinstance(a,u.SkyLight):a.light_component.set_editor_property('intensity',1.7)
u.EditorLevelLibrary.set_level_viewport_camera_info(u.Vector(-15800,-9400,700),u.Rotator(pitch=-5,yaw=36))
assert levels.save_current_level()
(ROOT/'arrival-checkpoint.json').write_text(json.dumps(dict(map=P+'/Greenlands_EnvironmentPreview',seed=591,instances=counts,route='Existing metre-scale coordinates preserved; continuous road joins resource/build routes and full Greenlands loop',materials='Poly Haven CC0 PBR; original source hashes in ArrivalAssets/provenance.json and arrival-surface-provenance.json',limits='Environment art only; no new construction, claim, harvesting, combat, networking or default/cook changes'),indent=2))
