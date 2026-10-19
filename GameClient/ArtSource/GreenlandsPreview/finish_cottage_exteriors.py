"""Apply continuous scanned PBR exterior walls at physical scale, owned preview."""
import unreal as u,json,sys
from pathlib import Path
from dataclasses import dataclass
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from planting_layout import load_settings
from region_layout import height
P='/Game/Frontier/Environment/GreenlandsPreview';D=P+'/Arrival';TAG='GreenlandsCottageExterior'
@dataclass(frozen=True)
class ExteriorSettings:
    wall_tile_metres:float
    plaster_tint_blend:float
    plaster_tints_linear_rgb:list
    timber_tint_linear_rgb:list
s=ExteriorSettings(**json.loads((ROOT/'exterior-material-settings.json').read_text()));assert .75<=s.wall_tile_metres<=4 and 0<=s.plaster_tint_blend<=.5 and len(s.plaster_tints_linear_rgb)==4
for colour in s.plaster_tints_linear_rgb+[s.timber_tint_linear_rgb]:assert len(colour)==3 and all(.05<=x<=.8 for x in colour)
tools=u.AssetToolsHelpers.get_asset_tools();levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
def material(name,asset,timber=False):
    path=D+'/Materials/'+name;m=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Materials',u.Material,u.MaterialFactoryNew());u.MaterialEditingLibrary.delete_all_material_expressions(m)
    uv=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionTextureCoordinate);uv.set_editor_property('u_tiling',2/s.wall_tile_metres);uv.set_editor_property('v_tiling',2/s.wall_tile_metres)
    for suffix,prop in [('Diffuse',u.MaterialProperty.MP_BASE_COLOR),('nor_dx',u.MaterialProperty.MP_NORMAL),('Rough',u.MaterialProperty.MP_ROUGHNESS)]:
        texture=u.load_asset(D+'/Textures/'+asset+'_'+suffix);assert texture
        node=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionTextureSample);node.set_editor_property('texture',texture);u.MaterialEditingLibrary.connect_material_expressions(uv,'',node,'UVs')
        if suffix=='nor_dx':node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        elif suffix=='Rough':node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_MASKS)
        output='R' if suffix=='Rough' else 'RGB'
        if suffix=='Diffuse':
            tint=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','SurfaceTint');tint.set_editor_property('default_value',u.LinearColor(*(s.timber_tint_linear_rgb if timber else s.plaster_tints_linear_rgb[0]),1))
            blend=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionMultiply if timber else u.MaterialExpressionLinearInterpolate)
            u.MaterialEditingLibrary.connect_material_expressions(node,'RGB',blend,'A');u.MaterialEditingLibrary.connect_material_expressions(tint,'RGB',blend,'B')
            if not timber:blend.set_editor_property('const_alpha',s.plaster_tint_blend)
            node=blend;output=''
        assert u.MaterialEditingLibrary.connect_material_property(node,output,prop)
    u.MaterialEditingLibrary.recompile_material(m);u.EditorAssetLibrary.save_loaded_asset(m);return m
plaster=material('M_CottageExteriorPlaster','rough_plaster_03');wood=material('M_CottageExteriorTimber','brown_planks_03',True)
stone=u.load_asset(P+'/Materials/M_Rock');meshes={}
for name,mat in [('SM_CottagePlasterExterior',plaster),('SM_CottageExteriorFraming',wood),('SM_CottageFieldstonePlinth',stone)]:
    task=u.AssetImportTask();task.filename=str(ROOT/'PreparedArrival'/(name+'.fbx'));task.destination_path=D+'/Meshes';task.automated=True;task.save=True;task.replace_existing=True
    options=u.FbxImportUI();options.import_materials=False;options.import_textures=False;options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=False;task.options=options;tools.import_asset_tasks([task])
    mesh=u.load_asset(D+'/Meshes/'+name);assert mesh
    for i in range(len(mesh.static_materials)):mesh.set_material(i,mat)
    u.EditorAssetLibrary.save_loaded_asset(mesh);meshes[name]=mesh
for name in ['SM_CottageShutterJoinery','SM_CottageOpenShutters','SM_CottageWindowBox']:
    mesh=u.load_asset(D+'/Meshes/'+name);assert mesh
    for i,slot in enumerate(mesh.static_materials):
        if str(slot.material_slot_name)=='WeatheredWood':mesh.set_material(i,wood)
    u.EditorAssetLibrary.save_loaded_asset(mesh)
levels.load_level(P+'/Greenlands_EnvironmentPreview')
for actor in actors.get_all_level_actors():
    if TAG in map(str,actor.tags):actors.destroy_actor(actor)
for index,(x,y,yaw) in enumerate(load_settings().cottages_metres_yaw):
    name='MI_CottagePlaster_'+str(index);path=D+'/Materials/'+name;mi=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    u.MaterialEditingLibrary.set_material_instance_parent(mi,plaster);u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi,'SurfaceTint',u.LinearColor(*s.plaster_tints_linear_rgb[index],1));u.EditorAssetLibrary.save_loaded_asset(mi)
    for name,mesh in meshes.items():
        actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x*100,y*100,(height(x,y)+.16)*100),u.Rotator(yaw=yaw));actor.tags=[TAG];actor.set_actor_label('Cottage %d finished exterior - %s'%(index+1,name));actor.static_mesh_component.set_static_mesh(mesh);actor.static_mesh_component.set_collision_profile_name('NoCollision')
        if name=='SM_CottagePlasterExterior':actor.static_mesh_component.set_material(0,mi)
# Slight extra eave projection covers the finished wall skin at the corners.
for actor in actors.get_all_level_actors():
    if not isinstance(actor,u.StaticMeshActor):continue
    mesh=actor.static_mesh_component.static_mesh
    if mesh and mesh.get_name() in ['SM_House08_Roof','SM_VillageRoofLining']:actor.set_actor_scale3d(u.Vector(1.055,1.055,1))
assert levels.save_current_level()
(ROOT/'cottage-exterior-report.json').write_text(json.dumps(dict(cottages=4,wall_material=plaster.get_path_name(),wood_material=wood.get_path_name(),wall_texture_metres=s.wall_tile_metres,channels=['Scanned diffuse','DirectX tangent normal','Scanned roughness'],scope='Owned exterior skins only; existing wall collision, doors, other maps and gameplay remain intact'),indent=2))
