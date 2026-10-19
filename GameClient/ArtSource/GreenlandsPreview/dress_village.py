"""Assemble the arrival and settlement with acquired village scenery."""
import unreal as u
import json,math,random,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import height,road_distance,AREAS
P='/Game/Frontier/Environment/GreenlandsPreview';TAG='GreenlandsVillage'
manifest=json.loads((ROOT/'village-selection.json').read_text())
records={r['name']:r for r in manifest['meshes']}
meshes={name:u.load_asset(r['package']) for name,r in records.items()}
for i in range(4):
    name='SM_PH_grass_medium_01_%02d'%i
    records[name]=dict(minimum=[0,0,0]);meshes[name]=u.load_asset(P+'/Arrival/Meshes/'+name)
assert all(meshes.values())
# The sample's custom foliage shader expects its landscape/RVT setup. Reuse the
# supplied albedo/opacity, normal, roughness and translucency maps in an owned
# instancing-compatible material for this continuous mesh-terrain preview.
for asset,roughness,names in [('GroundCoverA','RMF',['SM_GroundCover_02','SM_GroundCover_03']),('WheatGrassA','R',['SM_WheatGrass00_V2'])]:
    folder=P+'/Village/Megascans/3D_Plants/'+asset
    path=P+'/Arrival/Materials/M_Village_'+asset
    m=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset('M_Village_'+asset,P+'/Arrival/Materials',u.Material,u.MaterialFactoryNew())
    u.MaterialEditingLibrary.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED);m.set_editor_property('opacity_mask_clip_value',.25)
    m.set_editor_property('two_sided',True);m.set_editor_property('used_with_instanced_static_meshes',True)
    m.set_editor_property('shading_model',u.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    for suffix,prop,output in [('A',u.MaterialProperty.MP_BASE_COLOR,'RGB'),('N',u.MaterialProperty.MP_NORMAL,'RGB'),(roughness,u.MaterialProperty.MP_ROUGHNESS,'R'),('AO',u.MaterialProperty.MP_AMBIENT_OCCLUSION,'R'),('T',u.MaterialProperty.MP_SUBSURFACE_COLOR,'RGB')]:
        texture=u.load_asset(folder+'/T_'+asset+'_'+suffix);assert texture
        node=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionTextureSample);node.set_editor_property('texture',texture)
        if suffix=='N':node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        elif suffix not in ['A','T']:node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        assert u.MaterialEditingLibrary.connect_material_property(node,output,prop)
        if suffix=='A':assert u.MaterialEditingLibrary.connect_material_property(node,'A',u.MaterialProperty.MP_OPACITY_MASK)
    u.MaterialEditingLibrary.recompile_material(m);assert u.EditorAssetLibrary.save_loaded_asset(m)
    for name in names:
        for i in range(len(meshes[name].static_materials)):meshes[name].set_material(i,m)
        assert u.EditorAssetLibrary.save_loaded_asset(meshes[name])
lining_path=P+'/Arrival/Meshes/SM_VillageRoofLining'
if not u.EditorAssetLibrary.does_asset_exist(lining_path):
    task=u.AssetImportTask();task.filename=str(ROOT/'PreparedArrival/SM_VillageRoofLining.fbx');task.destination_path=P+'/Arrival/Meshes';task.automated=True;task.save=True
    options=u.FbxImportUI();options.import_materials=False;options.import_textures=False;task.options=options
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
meshes['SM_VillageRoofLining']=u.load_asset(lining_path)
lining_material=u.load_asset(P+'/Arrival/Materials/M_RoofTimberLining')
if not lining_material:lining_material=u.AssetToolsHelpers.get_asset_tools().duplicate_asset('M_RoofTimberLining',P+'/Arrival/Materials',u.load_asset(P+'/Arrival/Materials/M_WeatheredTimber'))
lining_material.set_editor_property('two_sided',True);u.MaterialEditingLibrary.recompile_material(lining_material);u.EditorAssetLibrary.save_loaded_asset(lining_material)
meshes['SM_VillageRoofLining'].set_material(0,lining_material)
assert u.EditorAssetLibrary.save_loaded_asset(meshes['SM_VillageRoofLining'])
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);levels.load_level(P+'/Greenlands_EnvironmentPreview')
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for a in actors.get_all_level_actors():
    if TAG in map(str,a.tags):actors.destroy_actor(a);continue
    if isinstance(a,u.StaticMeshActor):
        m=a.static_mesh_component.static_mesh
        if m and m.get_name() in ['SM_FootholdCottage','SM_ClosedShutters','SM_RoughFieldFence']:actors.destroy_actor(a);continue
        if m and m.get_name()=='SM_GreenlandsWaystone':
            loc=a.get_actor_location()
            for old,new in [((-185,-85),(-184,-88)),((-90,-40),(-92,-38))]:
                if math.dist((loc.x/100,loc.y/100),old)<.1:a.set_actor_location(u.Vector(new[0]*100,new[1]*100,height(*new)*100),False,False)
    if a.get_actor_label().startswith('Arrival scenery - '):
        c=a.get_component_by_class(u.HierarchicalInstancedStaticMeshComponent)
        if c and c.static_mesh and (c.static_mesh.get_name()=='SM_PH_Broadleaf' or 'grass_medium' in c.static_mesh.get_name()):actors.destroy_actor(a)

def place(name,x,y,z=None,yaw=0,scale=(1,1,1),collision=True,label=None):
    a=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x*100,y*100,(height(x,y) if z is None else z)*100),u.Rotator(yaw=yaw))
    a.tags=[TAG];a.set_actor_label(label or 'Village '+name)
    a.static_mesh_component.set_static_mesh(meshes[name]);a.set_actor_scale3d(u.Vector(*scale))
    a.static_mesh_component.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    return a
cube=u.load_asset('/Engine/BasicShapes/Cube')
timber=u.load_asset(P+'/Arrival/Materials/M_WeatheredTimber')
stone=u.load_asset(P+'/Materials/M_Rock')
def box(label,x,y,z,size,material,yaw):
    a=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x*100,y*100,z*100),u.Rotator(yaw=yaw));a.tags=[TAG];a.set_actor_label(label)
    a.static_mesh_component.set_static_mesh(cube);a.static_mesh_component.set_material(0,material);a.set_actor_scale3d(u.Vector(*size));return a
houses=[(-107,-59,90),(-77,-55,180),(-109,-23,90),(-68,-24,0)]
# Front is local -Y; doors face the existing connecting footpaths.
for index,(cx,cy,yaw) in enumerate(houses):
    an=math.radians(yaw);cs=math.cos(an);sn=math.sin(an);floor=height(cx,cy)+.16
    def xy(x,y):return cx+x*cs-y*sn,cy+x*sn+y*cs
    def part(name,x,y,z,angle=0,scale=(1,1,1)):
        x,y=xy(x,y);return place(name,x,y,floor+z,yaw+angle,scale,label='Settlement house %d - %s'%(index+1,name))
    box('House %d stone footing / walkable floor'%(index+1),cx,cy,floor-.1,(8,6,.2),stone,yaw)
    for row in [0,1]:
        z=row*2
        # 2m clear front entrance; upper row bridges it at a 2m lintel.
        if row==0:
            part('SM_MedievalModularWall3x2M',-4,-3,z)
            part('SM_MedievalModularWall3x2M',1,-3,z)
        else:
            for i,x in enumerate([-4,-2,0,2]):part(['SM_MedievalModularWall2x2MD','SM_MedievalModularWall2x2MH'][(i+index)%2],x,-3,z)
        for i,x in enumerate([-4,-2,0,2]):part('SM_MedievalModularWall2x2MG',-x,3,z,180)
        for y in [-3,0]:
            part('SM_MedievalModularWall3x2M',4,y,z,90)
            part('SM_MedievalModularWall3x2M',-4,-y,z,270)
    # Supplied roof is 8.26 x 6.68m; keep native scale and centre its bounds.
    roof=records['SM_House08_Roof'];lo=roof['minimum'];hi=roof['maximum']
    part('SM_House08_Roof',-(lo[0]+hi[0])/200,-(lo[1]+hi[1])/200,4-lo[2]/100)
    part('SM_VillageRoofLining',0,0,4-lo[2]/100)
    for y in [-3,3]:
        fx,fy=xy(0,y);box('House timber eaves beam',fx,fy,floor+4.02,(8.1,.22,.25),timber,yaw)
    for x in [-4,4]:
        fx,fy=xy(x,0);box('House timber eaves beam',fx,fy,floor+4.02,(.22,6.1,.25),timber,yaw)
    ex,ey=xy(0,-3.1);box('House %d entry lintel'%(index+1),ex,ey,floor+2.02,(2.15,.22,.16),timber,yaw)
    for local in [(-2,-4.2),(2,-4.2)]:
        x,y=xy(*local);place('SM_OldWoodenBench',x,y,scale=(1,1,1),yaw=yaw+90)
    # Short garden fences leave front doors and the main loop open.
    for y in [-1.5,1.5]:
        x,y=xy(-5.1,y);place('SM_Fence',x,y,yaw=yaw+90)

well=records['SM_WellBase'];place('SM_WellBase',-88,-31,height(-88,-31)-well['minimum'][2]/100,yaw=15,label='Settlement well beside market')
for x,y,angle in [(-97,-32,20),(-82,-34,-15),(-102,-34,90)]:place('SM_OldWoodenTable',x,y,yaw=angle)
for i in list(range(5))+list(range(7,14)):
    x=-176+i*3*.904;y=-90+i*3*.428;place('SM_Fence',x,y,yaw=25.3,label='Field fence along arrival road')

holder=u.load_class(None,P+'/Arrival/BP_SceneryInstances.BP_SceneryInstances_C')
ground_holder=u.load_class(None,P+'/Arrival/BP_GroundCoverInstances.BP_GroundCoverInstances_C')
counts={}
def instances(name,poses,collision=False,cull=15000):
    if not poses:return
    transforms=[]
    for x,y,s,yaw in poses:
        z=height(x,y)-records[name]['minimum'][2]/100*s
        transforms.append(u.Transform(location=u.Vector(x*100,y*100,z*100),rotation=u.Rotator(yaw=yaw),scale=u.Vector(s,s,s)))
    if not collision:
        ft_path=P+'/Arrival/Foliage/FT_'+name
        ft=u.load_asset(ft_path) if u.EditorAssetLibrary.does_asset_exist(ft_path) else u.AssetToolsHelpers.get_asset_tools().create_asset('FT_'+name,P+'/Arrival/Foliage',u.FoliageType_InstancedStaticMesh,u.FoliageType_InstancedStaticMeshFactory())
        ft.set_editor_property('mesh',meshes[name]);ft.set_editor_property('enable_density_scaling',False)
        u.EditorAssetLibrary.save_loaded_asset(ft)
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
        u.InstancedFoliageActor.remove_all_instances(world,ft)
        u.InstancedFoliageActor.add_instances(world,ft,transforms)
    else:
        ox=sum(p[0] for p in poses)/len(poses);oy=sum(p[1] for p in poses)/len(poses)
        a=actors.spawn_actor_from_class(holder,u.Vector(ox*100,oy*100,height(ox,oy)*100));a.tags=[TAG];a.set_actor_label('Village instances - '+name)
        c=a.get_component_by_class(u.InstancedStaticMeshComponent);c.set_static_mesh(meshes[name])
        c.set_collision_profile_name('BlockAll');c.set_cull_distances(int(cull*.8),cull)
        c.add_instances(transforms,False,True,False)
    counts[name]=len(poses)
random.seed(20261018)
tree_poses={n:[] for n in ['spruce_full_01','spruce_full_02']}
positions=json.loads((ROOT/'arrival-tree-positions.json').read_text())
for x,y,_,yaw in positions:
    if road_distance(x,y)<8 or math.dist((x,y),(-190,-89))<18 or any(math.dist((x,y),a['center'])<a['radius']+3 for a in AREAS.values()):continue
    name=random.choice(list(tree_poses));tree_poses[name].append((x,y,random.uniform(.75,1.2),yaw))
for i in range(75):
    t=random.random();off=random.choice([-1,1])*random.uniform(8,28);x=-185+95*t-.428*off;y=-85+45*t+.904*off
    if road_distance(x,y)<8 or math.dist((x,y),(-90,-40))<35 or math.dist((x,y),(-190,-89))<18:continue
    name=random.choice(list(tree_poses));tree_poses[name].append((x,y,random.uniform(.72,1.1),random.uniform(0,360)))
for name,poses in tree_poses.items():instances(name,poses,True,40000)
cover={n:[] for n in ['SM_GroundCover_02','SM_GroundCover_03','SM_WheatGrass00_V2',*['SM_PH_grass_medium_01_%02d'%i for i in range(4)]]}
for ix in range(348):
    for iy in range(260):
        x=-210+ix*.45+random.uniform(-.18,.18);y=-110+iy*.45+random.uniform(-.18,.18)
        d=road_distance(x,y)
        if d<3 or d>18 and math.dist((x,y),(-90,-40))>34:continue
        if math.dist((x,y),(-90,-40))<14 or math.dist((x,y),(-185,-85))<7:continue
        if any(abs(x-cx)<7 and abs(y-cy)<7 for cx,cy,_ in houses):continue
        if random.random()<.12:continue
        choice=random.random()
        if choice<.82:name='SM_PH_grass_medium_01_%02d'%random.randrange(4);s=random.uniform(2.4,3.6)
        elif choice<.98:name=random.choice(['SM_GroundCover_02','SM_GroundCover_03']);s=random.uniform(.65,1.05)
        else:name='SM_WheatGrass00_V2';s=random.uniform(.32,.45)
        cover[name].append((x,y,s,random.uniform(0,360)))
for name,poses in cover.items():instances(name,poses,False,0)
for a in actors.get_all_level_actors():
    if isinstance(a,u.DirectionalLight):a.light_component.set_editor_property('intensity',7);a.set_actor_rotation(u.Rotator(pitch=-30,yaw=-50),False)
    elif isinstance(a,u.SkyLight):a.light_component.set_editor_property('intensity',.85)
u.EditorLevelLibrary.set_level_viewport_camera_info(u.Vector(-9900,-6400,(height(-99,-64)+3)*100),u.Rotator(pitch=-4,yaw=55))
views={}
for label,x,y,z,pitch,yaw in [('Arrival',-190,-89,1.65,0,27),('Settlement',-124,-56,1.7,-3,24),('Courtyard',-93,-40,1.7,2,135)]:
    camera=actors.spawn_actor_from_class(u.CameraActor,u.Vector(x*100,y*100,(height(x,y)+z)*100),u.Rotator(pitch=pitch,yaw=yaw))
    camera.tags=[TAG];camera.set_actor_label('Greenlands '+label+' view');camera.camera_component.set_editor_property('field_of_view',75)
    views[label]=camera.get_name()
assert levels.save_current_level()
(ROOT/'village-views.json').write_text(json.dumps(views,indent=2))
(ROOT/'village-checkpoint.json').write_text(json.dumps(dict(map=P+'/Greenlands_EnvironmentPreview',houses=[dict(center=[x,y],yaw=a,footprint_metres=[8,6],entrance_metres=[2,2]) for x,y,a in houses],instances=counts,source=manifest['listing'],limits='Static environment only; construction and networking unchanged; outer tract remains future dressing'),indent=2))
print('VILLAGE_DRESSED',counts)
