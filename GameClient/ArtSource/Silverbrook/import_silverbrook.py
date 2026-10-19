"""Import Silverbrook building packages into the open editor (full editor, Run-InEditor.py).

python tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/import_silverbrook.py [Cottage1 ...]   (default: every package)

One shared library under /Game/Frontier/Environment/Greenlands/Silverbrook: the Poly Haven
textures are imported once; M_SB_Scan (weathering from vertex colour R, lichen from G on UV1 =
WorldUV, Nanite tessellation displacement from the scan's height map; needs r.Nanite.AllowTessellation=1) and M_SB_Solid are the masters; each palette material is an MI_ instance. Each package
becomes one Nanite static mesh, SM_<package's unreal_mesh>, with convex simple collision.
No level is touched; the town builder places the meshes.
"""
import unreal as u,json,math,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;LIB='/Game/Frontier/Environment/Greenlands/Silverbrook'
DISPLACEMENT_CM=3.0  # full 0..1 height range of a scan; real mortar joints and slate laps are 1-3 cm deep
tools=u.AssetToolsHelpers.get_asset_tools();mel=u.MaterialEditingLibrary;eal=u.EditorAssetLibrary;sme=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
assert sme,'Full editor required'
packages=[a for a in sys.argv[1:] if (ROOT/a/'asset-manifest.json').exists()] or sorted(p.parent.name for p in ROOT.glob('*/asset-manifest.json'))
manifests={p:json.loads((ROOT/p/'asset-manifest.json').read_text()) for p in packages}

def texture(asset,channel):
    path=f'{LIB}/Textures/T_SB_{asset}_{channel}'
    if not eal.does_asset_exist(path):
        task=u.AssetImportTask();task.filename=str(ROOT/'Textures'/f'{asset}_{channel}.jpg');task.destination_path=LIB+'/Textures';task.destination_name=f'T_SB_{asset}_{channel}';task.automated=True;task.save=True;tools.import_asset_tasks([task])
        t=u.load_asset(path);t.set_editor_property('srgb',channel=='Diffuse')
        if channel=='Displacement':t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_GRAYSCALE)
        elif channel=='nor_gl':t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);t.set_editor_property('flip_green_channel',True)  # OpenGL -> DirectX
        elif channel=='Rough':t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
        eal.save_loaded_asset(t)
    return u.load_asset(path)

def material(name):
    path=f'{LIB}/Materials/{name}'
    m=u.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name,LIB+'/Materials',u.Material,u.MaterialFactoryNew())
    mel.delete_all_material_expressions(m);return m
def node(m,cls,x,y,**props):
    e=mel.create_material_expression(m,cls,x,y)
    for k,v in props.items():e.set_editor_property(k,v)
    return e
def link(a,out,b,inp):assert mel.connect_material_expressions(a,out,b,inp),(a,out,b,inp)
def out(e,o,prop):assert mel.connect_material_property(e,o,prop)

def scan_master():
    m=material('M_SB_Scan');S=u.MaterialSamplerType
    uv0=node(m,u.MaterialExpressionTextureCoordinate,-1400,0);uv1=node(m,u.MaterialExpressionTextureCoordinate,-1400,600,coordinate_index=1)
    dif=node(m,u.MaterialExpressionTextureSampleParameter2D,-1100,-200,parameter_name='Diffuse',texture=texture('stone_wall_04','Diffuse'))
    rough=node(m,u.MaterialExpressionTextureSampleParameter2D,-1100,100,parameter_name='Roughness',texture=texture('stone_wall_04','Rough'),sampler_type=S.SAMPLERTYPE_MASKS)
    nor=node(m,u.MaterialExpressionTextureSampleParameter2D,-1100,350,parameter_name='Normal',texture=texture('stone_wall_04','nor_gl'),sampler_type=S.SAMPLERTYPE_NORMAL)
    lich=node(m,u.MaterialExpressionTextureSampleParameter2D,-1100,650,parameter_name='Lichen',texture=texture('lichen_rock','Diffuse'))
    for t in (dif,rough,nor):link(uv0,'',t,'UVs')
    link(uv1,'',lich,'UVs')
    vc=node(m,u.MaterialExpressionVertexColor,-1100,-450)
    tint=node(m,u.MaterialExpressionVectorParameter,-900,-350,parameter_name='DarkTint',default_value=u.LinearColor(.42,.36,.28,1))
    dark=node(m,u.MaterialExpressionMultiply,-700,-300);link(dif,'RGB',dark,'A');link(tint,'',dark,'B')
    worn=node(m,u.MaterialExpressionLinearInterpolate,-500,-250);link(dif,'RGB',worn,'A');link(dark,'',worn,'B');link(vc,'R',worn,'Alpha')
    # Lichen mask: scanned lichen luminance 0.03..0.08 -> 0..1, times per-element G, times LichenAmount.
    lum=node(m,u.MaterialExpressionDotProduct,-800,650);w=node(m,u.MaterialExpressionConstant3Vector,-1000,900,constant=u.LinearColor(.3,.59,.11,1));link(lich,'RGB',lum,'A');link(w,'',lum,'B')
    sub=node(m,u.MaterialExpressionSubtract,-650,650,const_b=.03);link(lum,'',sub,'A')
    div=node(m,u.MaterialExpressionDivide,-520,650,const_b=.05);link(sub,'',div,'A')
    sat=node(m,u.MaterialExpressionSaturate,-400,650);link(div,'',sat,'')
    amt=node(m,u.MaterialExpressionScalarParameter,-520,850,parameter_name='LichenAmount',default_value=0)
    g=node(m,u.MaterialExpressionMultiply,-280,700);link(sat,'',g,'A');link(vc,'G',g,'B')
    mask=node(m,u.MaterialExpressionMultiply,-160,700);link(g,'',mask,'A');link(amt,'',mask,'B')
    warm=node(m,u.MaterialExpressionMultiply,-400,450);ochre=node(m,u.MaterialExpressionConstant3Vector,-600,500,constant=u.LinearColor(1,.6,.2,1));link(lich,'RGB',warm,'A');link(ochre,'',warm,'B')
    base=node(m,u.MaterialExpressionLinearInterpolate,-100,-100);link(worn,'',base,'A');link(warm,'',base,'B');link(mask,'',base,'Alpha')
    ns=node(m,u.MaterialExpressionScalarParameter,-900,450,parameter_name='NormalStrength',default_value=1)
    flat=node(m,u.MaterialExpressionConstant3Vector,-900,520,constant=u.LinearColor(0,0,1,1))
    nl=node(m,u.MaterialExpressionLinearInterpolate,-600,380);link(flat,'',nl,'A');link(nor,'RGB',nl,'B');link(ns,'',nl,'Alpha')
    # Nanite tessellation: scanned height around 0.5 (no offset), scaled per instance; magnitude in cm.
    hgt=node(m,u.MaterialExpressionTextureSampleParameter2D,-1100,1000,parameter_name='Height',texture=texture('stone_wall_04','Displacement'),sampler_type=S.SAMPLERTYPE_LINEAR_GRAYSCALE);link(uv0,'',hgt,'UVs')
    ds=node(m,u.MaterialExpressionScalarParameter,-900,1150,parameter_name='DisplacementScale',default_value=1)
    half=node(m,u.MaterialExpressionConstant,-900,1250,r=.5)
    disp=node(m,u.MaterialExpressionLinearInterpolate,-600,1050);link(half,'',disp,'A');link(hgt,'R',disp,'B');link(ds,'',disp,'Alpha')
    # Displacement is not exposed as a MaterialProperty in Python, so drive everything through attributes.
    attrs=node(m,u.MaterialExpressionMakeMaterialAttributes,100,0)
    link(base,'',attrs,'BaseColor');link(rough,'R',attrs,'Roughness');link(nl,'',attrs,'Normal');link(disp,'',attrs,'Displacement')
    m.set_editor_property('use_material_attributes',True);out(attrs,'',u.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    m.set_editor_property('enable_tessellation',True)
    scale=m.get_editor_property('displacement_scaling');scale.set_editor_property('magnitude',DISPLACEMENT_CM);scale.set_editor_property('center',.5);m.set_editor_property('displacement_scaling',scale)
    mel.recompile_material(m);eal.save_loaded_asset(m);return m

def solid_master():
    m=material('M_SB_Solid')
    c=node(m,u.MaterialExpressionVectorParameter,-500,-100,parameter_name='Colour',default_value=u.LinearColor(.5,.5,.5,1))
    r=node(m,u.MaterialExpressionScalarParameter,-500,50,parameter_name='Roughness',default_value=.6)
    me=node(m,u.MaterialExpressionScalarParameter,-500,150,parameter_name='Metallic',default_value=0)
    e=node(m,u.MaterialExpressionVectorParameter,-700,250,parameter_name='Emission',default_value=u.LinearColor(0,0,0,1))
    s=node(m,u.MaterialExpressionScalarParameter,-700,400,parameter_name='EmissionStrength',default_value=0)
    em=node(m,u.MaterialExpressionMultiply,-400,300);link(e,'',em,'A');link(s,'',em,'B')
    out(c,'',u.MaterialProperty.MP_BASE_COLOR);out(r,'',u.MaterialProperty.MP_ROUGHNESS);out(me,'',u.MaterialProperty.MP_METALLIC);out(em,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m);eal.save_loaded_asset(m);return m

masters=dict(scan=scan_master(),solid=solid_master());instances={}
def instance(row):
    name='MI_'+row['name'];path=f'{LIB}/Materials/{name}'
    mi=u.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name,LIB+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    LC=lambda v:u.LinearColor(*v,1)
    if 'scan' in row:
        mel.set_material_instance_parent(mi,masters['scan'])
        for p,ch in (('Diffuse','Diffuse'),('Roughness','Rough'),('Normal','nor_gl'),('Height','Displacement')):mel.set_material_instance_texture_parameter_value(mi,p,texture(row['scan'],ch))
        mel.set_material_instance_vector_parameter_value(mi,'DarkTint',LC(row['dark']));mel.set_material_instance_scalar_parameter_value(mi,'NormalStrength',row['normal'])
        mel.set_material_instance_scalar_parameter_value(mi,'LichenAmount',1 if row['lichen'] else 0)
        if row['lichen']:mel.set_material_instance_texture_parameter_value(mi,'Lichen',texture(row['lichen'],'Diffuse'))
    else:
        mel.set_material_instance_parent(mi,masters['solid'])
        mel.set_material_instance_vector_parameter_value(mi,'Colour',LC(row['colour']))
        for p,k in (('Roughness','roughness'),('Metallic','metallic')):mel.set_material_instance_scalar_parameter_value(mi,p,row[k])
        if row['emission']:mel.set_material_instance_vector_parameter_value(mi,'Emission',LC(row['emission']));mel.set_material_instance_scalar_parameter_value(mi,'EmissionStrength',row['strength'])
    eal.save_loaded_asset(mi);return mi

report={}
for pkg,man in manifests.items():
    for row in man['materials']:
        if row['name'] not in instances:instances[row['name']]=instance(row)
    name=man['unreal_mesh'];task=u.AssetImportTask();task.filename=str(ROOT/pkg/(man['asset']+'.fbx'));task.destination_path=LIB+'/Meshes';task.destination_name=name
    task.automated=True;task.save=True;task.replace_existing=True
    opts=u.FbxImportUI();opts.import_materials=False;opts.import_textures=False;opts.import_as_skeletal=False
    d=opts.static_mesh_import_data;d.combine_meshes=True;d.auto_generate_collision=False;d.generate_lightmap_u_vs=False;d.vertex_color_import_option=u.VertexColorImportOption.REPLACE
    task.options=opts;tools.import_asset_tasks([task]);mesh=u.load_asset(f'{LIB}/Meshes/{name}');assert mesh,name
    for i,slot in enumerate(mesh.static_materials):
        mi=instances.get(str(slot.material_slot_name));assert mi,f'{name}: unmapped slot {slot.material_slot_name}';mesh.set_material(i,mi)
    nanite=mesh.get_editor_property('nanite_settings');nanite.set_editor_property('enabled',True);mesh.set_editor_property('nanite_settings',nanite)
    sme.remove_collisions(mesh);assert sme.set_convex_decomposition_collisions(mesh,12,16,100000),name
    b=mesh.get_bounding_box();size=[round((b.max.x-b.min.x)/100,2),round((b.max.y-b.min.y)/100,2),round((b.max.z-b.min.z)/100,2)]
    # Baked lighting (the project has no dynamic GI): lightmap UVs go to channel 2 because UV1 carries the lichen WorldUV.
    bs=sme.get_lod_build_settings(mesh,0);bs.generate_lightmap_u_vs=True;bs.src_lightmap_index=0;bs.dst_lightmap_index=2
    res=min(1024,2**math.ceil(math.log2(max(size)*16)));bs.min_lightmap_resolution=res;sme.set_lod_build_settings(mesh,0,bs)
    mesh.set_editor_property('light_map_coordinate_index',2);mesh.set_editor_property('light_map_resolution',res)
    eal.save_loaded_asset(mesh)
    report[pkg]=dict(mesh=f'{LIB}/Meshes/{name}',size_m=size,slot_m=man['slot_m'],fits=all(s<=t+.01 for s,t in zip(size,man['slot_m'])),slots=len(mesh.static_materials),
        nanite=mesh.get_editor_property('nanite_settings').get_editor_property('enabled'),convex_hulls=mesh.get_editor_property('body_setup').get_editor_property('agg_geom').get_editor_property('convex_elems').__len__(),
        nanite_fallback_triangles=mesh.get_num_triangles(0),lightmap=dict(uv=mesh.get_editor_property('light_map_coordinate_index'),resolution=res))
    (ROOT/pkg/'ue-import-report.json').write_text(json.dumps(report[pkg],indent=2)+'\n')
print('SILVERBROOK_IMPORT',json.dumps(report))
