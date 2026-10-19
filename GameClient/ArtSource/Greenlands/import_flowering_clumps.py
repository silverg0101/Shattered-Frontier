"""Import CC0 blooms with explicit opacity and four actual mesh LODs. Full editor."""
import unreal as u,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;D='/Game/Frontier/Environment/Greenlands/Arrival'
tools=u.AssetToolsHelpers.get_asset_tools();sub=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
assert sub,'Run -ExecutePythonScript in full editor mode'
name='M_PH_FloweringClumps';path=D+'/Materials/'+name
m=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Materials',u.Material,u.MaterialFactoryNew())
u.MaterialEditingLibrary.delete_all_material_expressions(m)
for prop,value in [('two_sided',True),('used_with_instanced_static_meshes',True),('blend_mode',u.BlendMode.BLEND_MASKED),('opacity_mask_clip_value',.3),('shading_model',u.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)]:m.set_editor_property(prop,value)
base=None
for suffix,output,prop in [('diff_1k.jpg','RGB',u.MaterialProperty.MP_BASE_COLOR),('nor_gl_1k.jpg','RGB',u.MaterialProperty.MP_NORMAL),('arm_1k.jpg','G',u.MaterialProperty.MP_ROUGHNESS),('opacity.png','R',u.MaterialProperty.MP_OPACITY_MASK)]:
    file=ROOT/'ArrivalAssets/flower_heliophila/textures'/('flower_heliophila_'+suffix);path=D+'/Textures/'+file.stem
    if not u.EditorAssetLibrary.does_asset_exist(path):
        task=u.AssetImportTask();task.filename=str(file);task.destination_path=D+'/Textures';task.automated=True;task.save=True;tools.import_asset_tasks([task])
    texture=u.load_asset(path);assert texture
    normal=suffix.startswith('nor');linear=normal or suffix.startswith(('arm','opacity'))
    texture.set_editor_property('srgb',not linear)
    if normal:texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);texture.set_editor_property('flip_green_channel',True)
    elif linear:texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
    u.EditorAssetLibrary.save_loaded_asset(texture)
    e=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionTextureSample);e.set_editor_property('texture',texture)
    if linear:e.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else u.MaterialSamplerType.SAMPLERTYPE_MASKS)
    assert u.MaterialEditingLibrary.connect_material_property(e,output,prop)
    if suffix.startswith('diff'):base=e
tint=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionMultiply);tint.set_editor_property('const_b',.35);u.MaterialEditingLibrary.connect_material_expressions(base,'RGB',tint,'A');u.MaterialEditingLibrary.connect_material_property(tint,'',u.MaterialProperty.MP_SUBSURFACE_COLOR)
u.MaterialEditingLibrary.recompile_material(m);u.EditorAssetLibrary.save_loaded_asset(m)
rows=json.loads((ROOT/'PreparedArrival/flowering-clumps.json').read_text())
for row in rows:
    path=D+'/Meshes/'+row['mesh']
    if not u.EditorAssetLibrary.does_asset_exist(path):
        task=u.AssetImportTask();task.filename=str(ROOT/'PreparedArrival'/(row['mesh']+'.fbx'));task.destination_path=D+'/Meshes';task.automated=True;task.save=True
        options=u.FbxImportUI();options.import_mesh=True;options.import_materials=False;options.import_textures=False;options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=False;task.options=options;tools.import_asset_tasks([task])
    mesh=u.load_asset(path);assert mesh
    for slot in range(len(mesh.static_materials)):mesh.set_material(slot,m)
    settings=u.EditorScriptingMeshReductionOptions();settings.auto_compute_lod_screen_size=False
    settings.reduction_settings=[u.EditorScriptingMeshReductionSettings(percent_triangles=p,screen_size=s) for p,s in [(1,1),(.35,.18),(.1,.07),(.025,.025)]]
    sub.set_lods(mesh,settings);assert mesh.get_num_lods()==4;u.EditorAssetLibrary.save_loaded_asset(mesh)
    row['lods']=mesh.get_num_lods()
(ROOT/'flowering-import.json').write_text(json.dumps(rows,indent=2))
