"""Run via UE5.8 Python commandlet. Owns only Greenlands content and the Greenlands_ArtReview map."""
import unreal as u, math, random
from pathlib import Path
ROOT=Path(__file__).resolve().parent
DEST='/Game/Frontier/Environment/Greenlands'
MAP='/Game/Frontier/Maps/Test/Greenlands_ArtReview'
if u.EditorAssetLibrary.does_asset_exist(MAP):raise RuntimeError('Refusing to overwrite preview map')
tools=u.AssetToolsHelpers.get_asset_tools()
def expr(m,cls,**kw):
    e=u.MaterialEditingLibrary.create_material_expression(m,cls)
    for k,v in kw.items():e.set_editor_property(k,v)
    return e
def link(a,b,pin,out=''):u.MaterialEditingLibrary.connect_material_expressions(a,out,b,pin)
def color(m,c):return expr(m,u.MaterialExpressionConstant3Vector,constant=u.LinearColor(*c,1))
def material(name,a,b,scale=.025,ground=False):
    m=u.load_asset(DEST+'/Materials/'+name) if u.EditorAssetLibrary.does_asset_exist(DEST+'/Materials/'+name) else tools.create_asset(name,DEST+'/Materials',u.Material,u.MaterialFactoryNew())
    u.MaterialEditingLibrary.delete_all_material_expressions(m)
    pos=expr(m,u.MaterialExpressionWorldPosition)
    sc=expr(m,u.MaterialExpressionMultiply);link(pos,sc,'A');sc.set_editor_property('const_b',scale)
    noise=expr(m,u.MaterialExpressionNoise,scale=1.0,levels=3);link(sc,noise,'Position')
    lerp=expr(m,u.MaterialExpressionLinearInterpolate);link(color(m,a),lerp,'A');link(color(m,b),lerp,'B');link(noise,lerp,'Alpha')
    base=lerp
    if ground:
        soil=expr(m,u.MaterialExpressionLinearInterpolate);link(color(m,(.095,.055,.022)),soil,'A');link(color(m,(.24,.16,.07)),soil,'B');link(noise,soil,'Alpha')
        v=expr(m,u.MaterialExpressionVertexColor)
        mix=expr(m,u.MaterialExpressionLinearInterpolate);link(lerp,mix,'A');link(soil,mix,'B');link(v,mix,'Alpha','R');base=mix
    u.MaterialEditingLibrary.connect_material_property(base,'',u.MaterialProperty.MP_BASE_COLOR)
    rough=expr(m,u.MaterialExpressionConstant,r=.88);u.MaterialEditingLibrary.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    if name=='M_Grass':m.set_editor_property('two_sided',True)
    u.MaterialEditingLibrary.recompile_material(m);u.EditorAssetLibrary.save_loaded_asset(m);return m
materials={
 'Bark':material('M_Bark',(.065,.032,.013),(.21,.115,.04),.07),
 'Leaves':material('M_Leaves',(.035,.095,.012),(.20,.32,.045),.025),
 'Rock':material('M_Rock',(.10,.125,.105),(.34,.36,.27),.035),
 'Grass':material('M_Grass',(.08,.16,.018),(.29,.38,.065),.05),
 'Petal':material('M_Petal',(.55,.31,.015),(.95,.78,.24)),
 'Ground':material('M_Ground',(.075,.135,.022),(.25,.32,.055),.03,True)}
meshes={}
files=list(ROOT.glob('*.fbx'))+[ROOT.parent/'GreenlandsWaystone/SM_GreenlandsWaystone.fbx']
for file in files:
    task=u.AssetImportTask();task.filename=str(file);task.destination_path=DEST+'/Meshes';task.automated=True;task.save=True;task.replace_existing=False
    opt=u.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False
    opt.static_mesh_import_data.combine_meshes=True
    opt.static_mesh_import_data.set_editor_property('vertex_color_import_option',u.VertexColorImportOption.REPLACE)
    opt.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs',False)
    task.options=opt
    if not u.EditorAssetLibrary.does_asset_exist(DEST+'/Meshes/'+file.stem):tools.import_asset_tasks([task])
    imported=[u.load_asset(p) for p in task.imported_object_paths]
    mesh=next((a for a in imported if isinstance(a,u.StaticMesh)),None) or u.load_asset(DEST+'/Meshes/'+file.stem)
    if not mesh:raise RuntimeError('No static mesh: '+str(file))
    for i,slot in enumerate(mesh.static_materials):
        key=str(slot.material_slot_name)
        replacement=materials.get(key,materials['Rock'])
        if 'gold' in key.lower() or 'rune' in key.lower():replacement=materials['Petal']
        mesh.set_material(i,replacement)
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    u.EditorAssetLibrary.save_loaded_asset(mesh);meshes[file.stem]=mesh
# Local mode reuses the existing walking pawn without the E1 fixture-spawning mode.
factory=u.BlueprintFactory();factory.set_editor_property('parent_class',u.GameModeBase)
bp=u.load_asset(DEST+'/BP_GreenlandsWalkingMode') if u.EditorAssetLibrary.does_asset_exist(DEST+'/BP_GreenlandsWalkingMode') else tools.create_asset('BP_GreenlandsWalkingMode',DEST,u.Blueprint,factory)
u.BlueprintEditorLibrary.compile_blueprint(bp)
mode=u.load_class(None,DEST+'/BP_GreenlandsWalkingMode.BP_GreenlandsWalkingMode_C')
u.get_default_object(mode).set_editor_property('default_pawn_class',u.load_class(None,'/Script/ShatteredFrontier.SFE1Character'))
u.EditorAssetLibrary.save_loaded_asset(bp)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
if not levels.new_level(MAP):raise RuntimeError('new_level failed')
world=u.EditorLevelLibrary.get_editor_world();world.get_world_settings().set_editor_property('default_game_mode',mode)
def spawn(cls,loc,rot=None,label=None):
    a=actors.spawn_actor_from_class(cls,u.Vector(*loc),u.Rotator(roll=rot[0],pitch=rot[1],yaw=rot[2]) if rot else u.Rotator())
    if label:a.set_actor_label(label)
    return a
def place(name,x,y,z,scale=1,yaw=0,collision=True):
    a=spawn(u.StaticMeshActor,(x*100,y*100,z*100),(0,0,yaw),name)
    a.static_mesh_component.set_static_mesh(meshes[name]);a.set_actor_scale3d(u.Vector(scale,scale,scale))
    a.static_mesh_component.set_collision_profile_name('BlockAll' if collision else 'NoCollision');return a
def h(x,y):
    f=max(0,min(1,(math.hypot(x,y)-10)/15));return f*(1.1+1.2*math.sin(x*.105)+.65*math.sin(y*.18)+.45*math.cos((x+y)*.14))
place('SM_MeadowTerrain',0,0,0)
place('SM_MeadowGrass',0,0,0,collision=False)
random.seed(56)
for i in range(72):
    x=random.uniform(-45,45);y=random.uniform(-45,45)
    if math.hypot(x,y)<15 or abs(y-(2*math.sin(x*.12)-3))<4:continue
    place('SM_Oak_%02d'%(i%3),x,y,h(x,y)-.05,random.uniform(.75,1.3),random.uniform(0,360))
for i in range(46):
    x=random.uniform(-42,42);y=random.uniform(-42,42)
    if math.hypot(x,y)<11:continue
    place('SM_Boulder_%02d'%(i%3),x,y,h(x,y)-.12,random.uniform(.4,1.9),random.uniform(0,360))
for i in range(24):
    x=random.uniform(-27,27);y=random.uniform(-27,27)
    if math.hypot(x,y)>10:place('SM_Wildflowers',x,y,h(x,y),random.uniform(.6,1.1),collision=False)
place('SM_GreenlandsWaystone',-11,-8,h(-11,-8),1,120)
sun=spawn(u.DirectionalLight,(0,0,1500),(0,-32,-38),'Warm afternoon sun')
sun.light_component.set_editor_property('intensity',3.2)
sun.light_component.set_mobility(u.ComponentMobility.MOVABLE)
sun.light_component.set_editor_property('light_color',u.Color(255,235,201,255))
sky=spawn(u.SkyLight,(0,0,500));sky.light_component.set_editor_property('real_time_capture',True);sky.light_component.set_editor_property('intensity',.85)
sky.light_component.set_mobility(u.ComponentMobility.MOVABLE)
world.get_world_settings().set_editor_property('force_no_precomputed_lighting',True)
spawn(u.SkyAtmosphere,(0,0,0));spawn(u.VolumetricCloud,(0,0,0))
fog=spawn(u.ExponentialHeightFog,(0,0,-300));fog.component.set_editor_property('fog_density',.006);fog.component.set_editor_property('fog_height_falloff',.2)
pp=spawn(u.PostProcessVolume,(0,0,0));pp.set_editor_property('unbound',True)
settings=pp.get_editor_property('settings');settings.set_editor_property('override_auto_exposure_bias',True);settings.set_editor_property('auto_exposure_bias',.3);pp.set_editor_property('settings',settings)
spawn(u.PlayerStart,(-1800,-450,600),(0,0,15),'Arrival - follow path to clearing')
u.EditorLevelLibrary.set_level_viewport_camera_info(u.Vector(-2400,-1900,1100),u.Rotator(-14,40,0))
if not levels.save_current_level():raise RuntimeError('Save failed')
u.EditorAssetLibrary.save_directory(DEST,only_if_is_dirty=True,recursive=True)
u.log('GREENLANDS_PREVIEW_READY '+MAP)



