import unreal as u, json, math, random
from pathlib import Path
ROOT=Path(__file__).resolve().parent;P='/Game/Frontier/Environment/GreenlandsPreview'
tools=u.AssetToolsHelpers.get_asset_tools();tex={}
for f in (ROOT/'Surfaces').glob('*.jpg'):
    t=u.AssetImportTask();t.filename=str(f);t.destination_path=P+'/Textures';t.automated=True;t.save=True
    if not u.EditorAssetLibrary.does_asset_exist(P+'/Textures/'+f.stem):tools.import_asset_tasks([t])
    tex[f.stem]=u.load_asset(P+'/Textures/'+f.stem)
    a=tex[f.stem]
    if 'nor_dx' in f.stem:a.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);a.set_editor_property('srgb',False)
    elif 'Rough' in f.stem:a.set_editor_property('srgb',False)
    u.EditorAssetLibrary.save_loaded_asset(a)
def expr(m,cls,**kw):
    e=u.MaterialEditingLibrary.create_material_expression(m,cls)
    for k,v in kw.items():e.set_editor_property(k,v)
    return e
def link(a,b,pin,out=''):u.MaterialEditingLibrary.connect_material_expressions(a,out,b,pin)
def sample(m,key):
    e=expr(m,u.MaterialExpressionTextureSample,texture=tex[key])
    if 'nor_dx' in key:e.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    return e
for name,asset in [('M_Ground','grass_ground'),('M_Rock','rock_face_03')]:
    m=u.load_asset(P+'/Materials/'+name);u.MaterialEditingLibrary.delete_all_material_expressions(m)
    for chan,prop in [('Diffuse',u.MaterialProperty.MP_BASE_COLOR),('nor_dx',u.MaterialProperty.MP_NORMAL),('Rough',u.MaterialProperty.MP_ROUGHNESS)]:
        e=sample(m,asset+'_'+chan)
        if name=='M_Ground':
            soil=sample(m,'brown_mud_dry_'+chan);v=expr(m,u.MaterialExpressionVertexColor);mix=expr(m,u.MaterialExpressionLinearInterpolate)
            link(e,mix,'A');link(soil,mix,'B');link(v,mix,'Alpha','R');e=mix
        u.MaterialEditingLibrary.connect_material_property(e,'',prop)
    u.MaterialEditingLibrary.recompile_material(m);u.EditorAssetLibrary.save_loaded_asset(m)
# Replace high frequency procedural foliage colour with a broad variation.
for name,col in [('M_Leaves',(.13,.24,.035)),('M_Grass',(.17,.24,.035)),('M_Bark',(.11,.065,.025))]:
    m=u.load_asset(P+'/Materials/'+name);u.MaterialEditingLibrary.delete_all_material_expressions(m)
    e=expr(m,u.MaterialExpressionConstant3Vector,constant=u.LinearColor(*col,1));u.MaterialEditingLibrary.connect_material_property(e,'',u.MaterialProperty.MP_BASE_COLOR)
    rough=expr(m,u.MaterialExpressionConstant,r=.9);u.MaterialEditingLibrary.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    u.MaterialEditingLibrary.recompile_material(m);u.EditorAssetLibrary.save_loaded_asset(m)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);levels.load_level(P+'/Greenlands_EnvironmentPreview');actors=u.get_editor_subsystem(u.EditorActorSubsystem)
t=u.AssetImportTask();t.filename=str(ROOT/'SM_HorizonHills.fbx');t.destination_path=P+'/Meshes';t.automated=True;t.save=True
if not u.EditorAssetLibrary.does_asset_exist(P+'/Meshes/SM_HorizonHills'):tools.import_asset_tasks([t])
hill=u.load_asset(P+'/Meshes/SM_HorizonHills')
hill.set_material(0,u.load_asset(P+'/Materials/M_Ground'));hill.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);u.EditorAssetLibrary.save_loaded_asset(hill)
if not any(a.get_actor_label()=='Distant Greenlands hills' for a in actors.get_all_level_actors()):
    a=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector());a.set_actor_label('Distant Greenlands hills');a.static_mesh_component.set_static_mesh(hill);a.static_mesh_component.set_collision_profile_name('BlockAll')
for a in actors.get_all_level_actors():
    if isinstance(a,u.SkyLight):a.light_component.set_editor_property('intensity',2.0)
u.EditorLevelLibrary.set_level_viewport_camera_info(u.Vector(-2300,-1300,1000),u.Rotator(pitch=-14,yaw=28,roll=0))
levels.save_current_level()
u.EditorAssetLibrary.save_directory(P,only_if_is_dirty=True,recursive=True)
(ROOT/'surface-checkpoint.json').write_text(json.dumps({'map':P+'/Greenlands_EnvironmentPreview','surfaces':'Poly Haven CC0 1k diffuse / DirectX normal / roughness; see Surfaces/provenance.json','imported_textures':len(tex)},indent=2))
