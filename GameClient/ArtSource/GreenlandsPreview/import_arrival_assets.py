"""Import CC0 scenery, PBR materials and real distance LODs into owned content."""
import unreal as u,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;P='/Game/Frontier/Environment/GreenlandsPreview';D=P+'/Arrival'
tools=u.AssetToolsHelpers.get_asset_tools();textures={}
def texture(file,normal=False,linear=False):
    file=Path(file);name=file.stem;path=D+'/Textures/'+name
    if path not in textures:
        if not u.EditorAssetLibrary.does_asset_exist(path):
            t=u.AssetImportTask();t.filename=str(file);t.destination_path=D+'/Textures';t.automated=True;t.save=True;tools.import_asset_tasks([t])
        a=u.load_asset(path);assert a
        if normal:a.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);a.set_editor_property('srgb',False);a.set_editor_property('flip_green_channel','nor_gl' in name)
        elif linear:a.set_editor_property('srgb',False);a.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
        u.EditorAssetLibrary.save_loaded_asset(a);textures[path]=a
    return textures[path]
def expr(m,cls,**kw):
    e=u.MaterialEditingLibrary.create_material_expression(m,cls)
    for k,v in kw.items():e.set_editor_property(k,v)
    return e
def link(a,b,pin,out=''):assert u.MaterialEditingLibrary.connect_material_expressions(a,out,b,pin)
def material(name):
    path=D+'/Materials/'+name
    m=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Materials',u.Material,u.MaterialFactoryNew())
    u.MaterialEditingLibrary.delete_all_material_expressions(m);return m
def sample(m,file,normal=False,linear=False,uv=None):
    e=expr(m,u.MaterialExpressionTextureSample,texture=texture(file,normal,linear))
    if normal:e.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    elif linear:e.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_MASKS)
    if uv:link(uv,e,'UVs')
    return e
mats={}
for info in json.loads((ROOT/'PreparedArrival/materials.json').read_text()):
    m=material('M_PH_'+info['name']);m.set_editor_property('two_sided',info['two_sided'])
    uv=expr(m,u.MaterialExpressionTextureCoordinate,coordinate_index=info.get('uv',0))
    tr=info.get('transform',{});sc=tr.get('scale',[1,1]);offset=tr.get('offset',[0,0])
    uv.set_editor_property('u_tiling',sc[0]);uv.set_editor_property('v_tiling',sc[1])
    adjusted=[offset[0],1-sc[1]-offset[1]]
    if adjusted!=[0,0]:
        add=expr(m,u.MaterialExpressionAdd);link(uv,add,'A');c=expr(m,u.MaterialExpressionConstant2Vector,r=adjusted[0],g=adjusted[1]);link(c,add,'B');uv=add
    base=sample(m,ROOT/info['base'],uv=uv);u.MaterialEditingLibrary.connect_material_property(base,'RGB',u.MaterialProperty.MP_BASE_COLOR)
    normal=sample(m,ROOT/info['normal'],normal=True,uv=uv);u.MaterialEditingLibrary.connect_material_property(normal,'RGB',u.MaterialProperty.MP_NORMAL)
    arm=sample(m,ROOT/info['arm'],linear=True,uv=uv)
    u.MaterialEditingLibrary.connect_material_property(arm,'R',u.MaterialProperty.MP_AMBIENT_OCCLUSION);u.MaterialEditingLibrary.connect_material_property(arm,'G',u.MaterialProperty.MP_ROUGHNESS);u.MaterialEditingLibrary.connect_material_property(arm,'B',u.MaterialProperty.MP_METALLIC)
    if info['alpha']!='OPAQUE':
        m.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED);m.set_editor_property('opacity_mask_clip_value',.35)
        alpha=sample(m,ROOT/info['opacity'],linear=True,uv=uv)
        fade=expr(m,u.MaterialExpressionPerInstanceFadeAmount);mult=expr(m,u.MaterialExpressionMultiply);link(alpha,mult,'A','R');link(fade,mult,'B')
        u.MaterialEditingLibrary.connect_material_property(mult,'',u.MaterialProperty.MP_OPACITY_MASK)
        m.set_editor_property('shading_model',u.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        tint=expr(m,u.MaterialExpressionMultiply,const_b=.35);link(base,tint,'A','RGB');u.MaterialEditingLibrary.connect_material_property(tint,'',u.MaterialProperty.MP_SUBSURFACE_COLOR)
    u.MaterialEditingLibrary.recompile_material(m);assert u.EditorAssetLibrary.save_loaded_asset(m);mats[info['name']]=m
rows=[]
for info in json.loads((ROOT/'PreparedArrival/meshes.json').read_text()):
    name=info['mesh'];path=D+'/Meshes/'+name
    if not u.EditorAssetLibrary.does_asset_exist(path):
        task=u.AssetImportTask();task.filename=str(ROOT/'PreparedArrival'/(name+'.fbx'));task.destination_path=D+'/Meshes';task.automated=True;task.save=True
        opt=u.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=True;task.options=opt;tools.import_asset_tasks([task])
    mesh=u.load_asset(path);assert isinstance(mesh,u.StaticMesh),name
    for i,s in enumerate(mesh.static_materials):
        key=str(s.material_slot_name);assert key in mats,(name,key);mesh.set_material(i,mats[key])
    if info['asset'] not in ['tree_small_02','grass_medium_01','fern_02']:mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    settings=u.EditorScriptingMeshReductionOptions();settings.auto_compute_lod_screen_size=False
    if info['asset']=='tree_small_02':pairs=[(1,1),(.5,.35),(.2,.15),(.07,.06)]
    elif info['asset'] in ['grass_medium_01','fern_02']:pairs=[(1,1),(.5,.12),(.18,.025)]
    else:pairs=[(1,1),(.5,.2),(.18,.06)]
    settings.reduction_settings=[u.EditorScriptingMeshReductionSettings(percent_triangles=p,screen_size=s) for p,s in pairs]
    u.EditorStaticMeshLibrary.set_lods(mesh,settings);assert u.EditorAssetLibrary.save_loaded_asset(mesh)
    rows.append(dict(mesh=name,lods=mesh.get_num_lods(),source_triangles=info['triangles']))
# Reusable HISM holder: art-only instancing, no construction or network behavior.
bp_path=D+'/BP_SceneryInstances'
if not u.EditorAssetLibrary.does_asset_exist(bp_path):
    exec(compile((ROOT/'create_arrival_instances.py').read_text(),str(ROOT/'create_arrival_instances.py'),'exec'))
(ROOT/'arrival-import.json').write_text(json.dumps(dict(meshes=rows,materials=len(mats),textures=len(textures)),indent=2))
