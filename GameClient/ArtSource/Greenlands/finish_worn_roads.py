"""Finish continuous travelled earth, shallow ruts, gravel and planted shoulders."""
import unreal as u,json,sys,random,math
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import height
P='/Game/Frontier/Environment/Greenlands';D=P+'/Arrival'
tools=u.AssetToolsHelpers.get_asset_tools();levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
m=u.load_asset(D+'/Materials/M_WornTrack');u.MaterialEditingLibrary.delete_all_material_expressions(m)
def ex(cls,**props):
    n=u.MaterialEditingLibrary.create_material_expression(m,cls)
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def link(a,b,pin,out=''):assert u.MaterialEditingLibrary.connect_material_expressions(a,out,b,pin)
def scalar(v):return ex(u.MaterialExpressionConstant,r=v)
def op(cls,a,b):
    n=ex(cls);link(a,n,'A');link(b,n,'B');return n
def mul(a,b):return op(u.MaterialExpressionMultiply,a,b)
def lerp(a,b,t):
    n=ex(u.MaterialExpressionLinearInterpolate);link(a,n,'A');link(b,n,'B');link(t,n,'Alpha');return n
def sample(channel,uv):
    name='gravel_road_'+channel;path=D+'/Textures/'+name
    if not u.EditorAssetLibrary.does_asset_exist(path):
        task=u.AssetImportTask();task.filename=str(ROOT/'RoadSurfaces'/(name+'.jpg'));task.destination_path=D+'/Textures';task.automated=True;task.save=True;tools.import_asset_tasks([task])
    t=u.load_asset(path)
    if channel=='nor_dx':t.set_editor_property('srgb',False);t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP)
    elif channel!='Diffuse':t.set_editor_property('srgb',False);t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
    u.EditorAssetLibrary.save_loaded_asset(t)
    node=ex(u.MaterialExpressionTextureSample,texture=t)
    node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=='nor_dx' else u.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=='Diffuse' else u.MaterialSamplerType.SAMPLERTYPE_MASKS)
    link(uv,node,'UVs');return node
world=ex(u.MaterialExpressionWorldPosition);uv=ex(u.MaterialExpressionTextureCoordinate)
uv2=op(u.MaterialExpressionAdd,mul(uv,scalar(.613)),ex(u.MaterialExpressionConstant2Vector,r=4.17,g=7.31))
noise=ex(u.MaterialExpressionNoise,scale=1,levels=2,quality=1,output_min=0,output_max=1,turbulence=False);link(mul(world,scalar(1/1900)),noise,'')
color=lerp(sample('Diffuse',uv),sample('Diffuse',uv2),noise)
vertex=ex(u.MaterialExpressionVertexColor)
rut=ex(u.MaterialExpressionComponentMask,r=False,g=True,b=False,a=False);link(vertex,rut,'')
green=ex(u.MaterialExpressionComponentMask,r=False,g=False,b=True,a=False);link(vertex,green,'')
# Compacted wheel lanes are darker, smoother and slightly cooler than loose shoulders.
tone=lerp(ex(u.MaterialExpressionConstant3Vector,constant=u.LinearColor(.84,.82,.77,1)),ex(u.MaterialExpressionConstant3Vector,constant=u.LinearColor(.55,.59,.58,1)),mul(rut,scalar(.66)))
color=mul(color,tone)
color=lerp(color,mul(color,ex(u.MaterialExpressionConstant3Vector,constant=u.LinearColor(.68,.92,.45,1))),green)
assert u.MaterialEditingLibrary.connect_material_property(color,'',u.MaterialProperty.MP_BASE_COLOR)
assert u.MaterialEditingLibrary.connect_material_property(sample('nor_dx',uv),'RGB',u.MaterialProperty.MP_NORMAL)
rough=lerp(scalar(.95),scalar(.73),rut)
assert u.MaterialEditingLibrary.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
edge=ex(u.MaterialExpressionComponentMask,r=True,g=False,b=False,a=False);link(vertex,edge,'')
fine=ex(u.MaterialExpressionNoise,scale=1,levels=2,quality=1,output_min=0,output_max=1,turbulence=False);link(mul(world,scalar(1/28)),fine,'')
mask=op(u.MaterialExpressionSubtract,edge,mul(fine,scalar(.28)))
m.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED);m.set_editor_property('opacity_mask_clip_value',.14)
# Temporal dithering blends the physically raised shoulders into the meadow.
# Keep the centre opaque; only the authored edge ramp is feathered.
coverage=mul(mask,scalar(1.4))
mask=ex(u.MaterialExpressionClamp,min_default=0,max_default=1);link(coverage,mask,'')
dither=ex(u.MaterialExpressionMaterialFunctionCall)
dither.set_material_function(u.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA'))
link(mask,dither,'Alpha Threshold')
assert u.MaterialEditingLibrary.connect_material_property(dither,'',u.MaterialProperty.MP_OPACITY_MASK)
u.MaterialEditingLibrary.recompile_material(m);assert u.EditorAssetLibrary.save_loaded_asset(m)
print('ROAD_MATERIAL_FINISHED',flush=True)
if '-RoadMaterialOnly' not in u.SystemLibrary.get_command_line():
    for name in ['SM_GreenlandsWornRoads','SM_RoadShoulderStones']:
        task=u.AssetImportTask();task.filename=str(ROOT/'PreparedArrival'/(name+'.fbx'));task.destination_path=D+'/Meshes';task.automated=True;task.save=True;task.replace_existing=True
        options=u.FbxImportUI();options.import_materials=False;options.import_textures=False;options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.set_editor_property('vertex_color_import_option',u.VertexColorImportOption.REPLACE);task.options=options
        tools.import_asset_tasks([task]);mesh=u.load_asset(D+'/Meshes/'+name)
        mesh.set_material(0,m if name=='SM_GreenlandsWornRoads' else u.load_asset(P+'/Materials/M_Rock'))
        if name=='SM_GreenlandsWornRoads':mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        u.EditorAssetLibrary.save_loaded_asset(mesh)
    levels.load_level('/Game/Frontier/Maps/Test/Greenlands_ArtReview')
    for actor in actors.get_all_level_actors():
        if 'GreenlandsRoadFinish' in [str(t) for t in actor.tags]:actors.destroy_actor(actor)
        elif isinstance(actor,u.CameraActor) and actor.get_actor_label()=='Greenlands Settlement view':
            actor.set_actor_location(u.Vector(-12400,-5600,(height(-124,-56)+1.7)*100),False,False);actor.set_actor_rotation(u.Rotator(pitch=-3,yaw=24),False)
        elif isinstance(actor,u.StaticMeshActor):
            mesh=actor.static_mesh_component.static_mesh;loc=actor.get_actor_location()
            if mesh and mesh.get_name()=='SM_GreenlandsWornRoads':actor.static_mesh_component.set_collision_profile_name('BlockAll')
            if mesh and mesh.get_name()=='SM_OldWoodenTable' and abs(loc.x+9900)<10 and abs(loc.y+4300)<10:actor.set_actor_location(u.Vector(-10200,-3400,height(-102,-34)*100),False,False)
    stones=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector());stones.tags=['GreenlandsRoadFinish'];stones.set_actor_label('Loose gravel on travelled road shoulders');stones.static_mesh_component.set_static_mesh(u.load_asset(D+'/Meshes/SM_RoadShoulderStones'));stones.static_mesh_component.set_collision_profile_name('NoCollision')
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    poses=json.loads((ROOT/'PreparedArrival/road-verges.json').read_text());rng=random.Random(921)
    for variant in range(4):
        name='FT_RoadVerge_%d'%variant;path=D+'/Foliage/'+name
        ft=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Foliage',u.FoliageType_InstancedStaticMesh,u.FoliageType_InstancedStaticMeshFactory())
        ft.set_editor_property('mesh',u.load_asset(D+'/Meshes/SM_PH_grass_medium_01_%02d'%variant));ft.set_editor_property('enable_density_scaling',False)
        u.EditorAssetLibrary.save_loaded_asset(ft);u.InstancedFoliageActor.remove_all_instances(world,ft)
        transforms=[]
        for x,y,z,s,yaw in poses[variant::4]:
            # Keep the reserved construction plots and cottage interiors clear.
            if any(abs(x-cx)<11 and abs(y-5)<11 for cx in [-25,5,35]):continue
            if any(abs(x-cx)<4.5 and abs(y-cy)<3.5 for cx,cy in [(-107,-59),(-77,-55),(-109,-23),(-68,-24)]):continue
            transforms.append(u.Transform(location=u.Vector(x*100,y*100,z*100),rotation=u.Rotator(yaw=yaw),scale=u.Vector(s,s,s)))
        u.InstancedFoliageActor.add_instances(world,ft,transforms)
    for actor in actors.get_all_level_actors():
        if isinstance(actor,u.InstancedFoliageActor):
            for component in actor.get_components_by_class(u.InstancedStaticMeshComponent):component.set_collision_profile_name('NoCollision')
    assert levels.save_current_level()
    (ROOT/'worn-road-finish.json').write_text(json.dumps(dict(detail='2m CC0 earth/gravel scan, two-scale variation, darker compacted wheel lanes, 6.5cm rut relief, crowned geometry, irregular shoulders, 3D pebbles and native verge foliage',source='https://polyhaven.com/a/gravel_road',coverage='Main continuous loop, both expansion corridors and cottage footpaths',physical_collision='Complex road mesh collision follows crown and ruts above continuous terrain',verge_clumps=len(poses)),indent=2))
    


