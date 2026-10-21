"""Import the four Greenlands nature families into the open editor (full editor; run with tools/Run-InEditor.py).

py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_nature.py [PineTall BroadleafYoung BoulderLargeMoss BedrockSlab]

Writes only under /Game/Frontier/Environment/Greenlands/Nature/{Textures,Materials,Meshes}. Reads each family's asset-manifest.json (materials = the specs that also drive
the Blender look). Masters: M_GN_Rock (opaque, one-sided), M_GN_Wood (opaque, two-sided: bark, needle sprays), M_GN_Leaf (masked, two-sided foliage). Every spec becomes an MI_ instance
with two texture layers (A, plus B blended in by vertex colour R: moss on rocks, orange upper trunk and lenticels on trees). All four families are Nanite (masked leaves included). Rocks: convex decomposition. Trees: trunk-only convex prisms imported from the FBX (UCX_), no canopy collision, no LOD chain.
No level is touched; placement stays with the PCG dressing.
"""
import unreal as u,json,math,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;LIB='/Game/Frontier/Environment/Greenlands/Nature'
tools=u.AssetToolsHelpers.get_asset_tools();mel=u.MaterialEditingLibrary;eal=u.EditorAssetLibrary;sme=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
assert sme,'Full editor required'
FAMILIES=[a for a in sys.argv[1:] if (ROOT/a/'asset-manifest.json').exists()] or ['BoulderLargeMoss','BedrockSlab','BroadleafYoung','PineTall','CobblePebbleScatter','MossPatch','LeafNeedleLitter','HerbBroadleaf','SaplingBroadleaf','GraniteDomeFar']
COL={'BoulderLargeMoss':'convex','BedrockSlab':'convex','CobblePebbleScatter':'convex','BroadleafYoung':'ucx','PineTall':'ucx','SaplingBroadleaf':'ucx'}   # other families: no collision
ROCKISH=('BoulderLargeMoss','BedrockSlab','CobblePebbleScatter','MossPatch','GraniteDomeFar')   # opaque one-sided master, Nanite fallback 10%
TEX={}


def texture(rel,kind):
    p=ROOT/rel;name='T_GN_'+p.stem;path=f'{LIB}/Textures/{name}'
    if path not in TEX:
        if not eal.does_asset_exist(path):
            task=u.AssetImportTask();task.filename=str(p);task.destination_path=LIB+'/Textures';task.destination_name=name;task.automated=True;task.save=True;task.replace_existing=True
            tools.import_asset_tasks([task])
        t=u.load_asset(path);assert t,path
        if kind=='normal':t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);t.set_editor_property('srgb',False);t.set_editor_property('flip_green_channel',True)  # GL -> DX
        elif kind=='mask':t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS);t.set_editor_property('srgb',False)
        eal.save_loaded_asset(t);TEX[path]=t
    return TEX[path]


def node(m,cls,x,y,**kw):
    e=mel.create_material_expression(m,cls,x,y)
    for k,v in kw.items():e.set_editor_property(k,v)
    return e


def link(a,out,b,inp):assert mel.connect_material_expressions(a,out,b,inp),(a,out,b,inp)


def master(name,two_sided,leaf):
    path=f'{LIB}/Materials/{name}'
    if eal.does_asset_exist(path):return u.load_asset(path)   # masters are built once; delete the asset to rebuild after changing this function
    m=tools.create_asset(name,LIB+'/Materials',u.Material,u.MaterialFactoryNew())
    mel.delete_all_material_expressions(m);m.set_editor_property('two_sided',two_sided);m.set_editor_property('used_with_instanced_static_meshes',True)
    S=u.MaterialSamplerType;tc=node(m,u.MaterialExpressionTextureCoordinate,-1800,0)
    sc=lambda n,d,x,y:node(m,u.MaterialExpressionScalarParameter,x,y,parameter_name=n,default_value=d)

    def layer(p,y):
        us,vs,uo,vo=sc('UScale'+p,1,-1800,y+150),sc('VScale'+p,1,-1800,y+250),sc('UOff'+p,0,-1800,y+350),sc('VOff'+p,0,-1800,y+450)
        sca=node(m,u.MaterialExpressionAppendVector,-1600,y+200);link(us,'',sca,'A');link(vs,'',sca,'B')
        off=node(m,u.MaterialExpressionAppendVector,-1600,y+400);link(uo,'',off,'A');link(vo,'',off,'B')
        mul=node(m,u.MaterialExpressionMultiply,-1400,y);link(tc,'',mul,'A');link(sca,'',mul,'B');uv=node(m,u.MaterialExpressionAdd,-1250,y);link(mul,'',uv,'A');link(off,'',uv,'B')
        base=node(m,u.MaterialExpressionTextureSampleParameter2D,-1000,y,parameter_name='Base'+p,texture=texture('Sources/pine_tree_01/textures/pine_tree_01_bark_diff_2k.jpg','color'))
        nor=node(m,u.MaterialExpressionTextureSampleParameter2D,-1000,y+250,parameter_name='Normal'+p,texture=texture('Sources/pine_tree_01/textures/pine_tree_01_bark_nor_gl_2k.jpg','normal'),sampler_type=S.SAMPLERTYPE_NORMAL)
        arm=node(m,u.MaterialExpressionTextureSampleParameter2D,-1000,y+500,parameter_name='Arm'+p,texture=texture('Sources/pine_tree_01/textures/pine_tree_01_bark_arm_2k.jpg','mask'),sampler_type=S.SAMPLERTYPE_MASKS)
        for t in (base,nor,arm):link(uv,'',t,'UVs')
        tint=node(m,u.MaterialExpressionVectorParameter,-800,y-100,parameter_name='Tint'+p,default_value=u.LinearColor(1,1,1,1))
        tm=node(m,u.MaterialExpressionMultiply,-650,y);link(base,'RGB',tm,'A');link(tint,'',tm,'B')
        one=node(m,u.MaterialExpressionConstant,-800,y-200,r=1.0);ds=node(m,u.MaterialExpressionDesaturation,-500,y-100);link(tm,'',ds,'');link(one,'',ds,'Fraction')
        sat=sc('Sat'+p,1,-500,y+50);col=node(m,u.MaterialExpressionLinearInterpolate,-350,y);link(ds,'',col,'A');link(tm,'',col,'B');link(sat,'',col,'Alpha')
        packed=sc('ArmPacked'+p,1,-700,y+600);rough=node(m,u.MaterialExpressionLinearInterpolate,-350,y+500);link(arm,'R',rough,'A');link(arm,'G',rough,'B');link(packed,'',rough,'Alpha')
        return col,nor,rough,uv
    ca,na,ra,uva=layer('A',0);cb,nb,rb,_=layer('B',900)
    vc=node(m,u.MaterialExpressionVertexColor,-300,1700);amt=sc('BlendAmount',1,-300,1850);lo=sc('BlendLow',.5,-300,1950);inv=sc('BlendInvRange',2.0,-300,2050);use=sc('UseBlend',0,-300,2150)
    a=node(m,u.MaterialExpressionMultiply,-100,1700);link(vc,'R',a,'A');link(amt,'',a,'B');s=node(m,u.MaterialExpressionSubtract,50,1700);link(a,'',s,'A');link(lo,'',s,'B')
    d=node(m,u.MaterialExpressionMultiply,200,1700);link(s,'',d,'A');link(inv,'',d,'B');sa=node(m,u.MaterialExpressionSaturate,350,1700);link(d,'',sa,'');w=node(m,u.MaterialExpressionMultiply,500,1700);link(sa,'',w,'A');link(use,'',w,'B')
    def mix(a_,b_,y,ao='',bo=''):
        l=node(m,u.MaterialExpressionLinearInterpolate,700,y);link(a_,ao,l,'A');link(b_,bo,l,'B');link(w,'',l,'Alpha');return l
    col=mix(ca,cb,0);nrm=mix(na,nb,250,'RGB','RGB');rgh=mix(ra,rb,500)
    mel.connect_material_property(col,'',u.MaterialProperty.MP_BASE_COLOR);mel.connect_material_property(nrm,'',u.MaterialProperty.MP_NORMAL);mel.connect_material_property(rgh,'',u.MaterialProperty.MP_ROUGHNESS)
    if leaf:
        al=node(m,u.MaterialExpressionTextureSampleParameter2D,-1000,-300,parameter_name='Alpha',texture=texture('Sources/tree_small_02/textures/tree_small_02_leaves_alpha_2k.png','mask'),sampler_type=S.SAMPLERTYPE_MASKS);link(uva,'',al,'UVs')
        m.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED);m.set_editor_property('opacity_mask_clip_value',.35);m.set_editor_property('shading_model',u.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        mel.connect_material_property(al,'R',u.MaterialProperty.MP_OPACITY_MASK)
        t=node(m,u.MaterialExpressionMultiply,900,-200,const_b=.35);link(col,'',t,'A');mel.connect_material_property(t,'',u.MaterialProperty.MP_SUBSURFACE_COLOR)
    mel.recompile_material(m);assert eal.save_loaded_asset(m);return m


MASTERS={'rock':master('M_GN_Rock',False,False),'wood':master('M_GN_Wood',True,False),'leaf':master('M_GN_Leaf',True,True)}


def instance(spec,kind):
    path=f"{LIB}/Materials/{spec['name']}"
    mi=u.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(spec['name'],LIB+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi,MASTERS[kind]);LC=lambda v:u.LinearColor(v[0],v[1],v[2],1)
    def layer(p,s):
        rough='arm' not in s;nrm=s['normal'];tx={'Base':(s['base'],'color'),'Normal':(nrm,'normal'),'Arm':(s['arm'] if not rough else s['rough'],'mask')}
        for k,(f,kd) in tx.items():mel.set_material_instance_texture_parameter_value(mi,k+p,texture(f,kd))
        sc=s.get('uv_scale',[1,1]);of=s.get('uv_offset',[0,0])
        for k,val in (('UScale',sc[0]),('VScale',sc[1]),('UOff',of[0]),('VOff',1-sc[1]-of[1]),('Sat',s.get('sat',1)),('ArmPacked',0 if rough else 1)):
            mel.set_material_instance_scalar_parameter_value(mi,k+p,val)
        mel.set_material_instance_vector_parameter_value(mi,'Tint'+p,LC(s.get('tint',[1,1,1])))
    layer('A',spec);b=spec.get('blend')
    layer('B',b if b else spec)
    if b:
        c=b.get('contrast',.25);amt=b.get('amount',1.0)
        for k,val in (('UseBlend',1),('BlendAmount',amt),('BlendLow',.5-c),('BlendInvRange',1/(2*c))):mel.set_material_instance_scalar_parameter_value(mi,k,val)
    else:mel.set_material_instance_scalar_parameter_value(mi,'UseBlend',0)
    # Blender maps the blend as smoothstep(.5-c, .5+c, R*amount); UE uses the linear ramp over the same range
    eal.save_loaded_asset(mi);return mi


def import_mesh(fbx,name,rock,instances,col):
    task=u.AssetImportTask();task.filename=str(fbx);task.destination_path=LIB+'/Meshes';task.destination_name=name;task.automated=True;task.save=True;task.replace_existing=True
    o=u.FbxImportUI();o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False
    d=o.static_mesh_import_data;d.combine_meshes=True;d.auto_generate_collision=False;d.generate_lightmap_u_vs=True;d.vertex_color_import_option=u.VertexColorImportOption.REPLACE
    task.options=o;tools.import_asset_tasks([task]);mesh=u.load_asset(f'{LIB}/Meshes/{name}');assert mesh,name
    for i,slot in enumerate(mesh.static_materials):
        mi=instances.get(str(slot.material_slot_name));assert mi,f'{name}: unmapped slot {slot.material_slot_name}';mesh.set_material(i,mi)
    ns=mesh.get_editor_property('nanite_settings');ns.set_editor_property('enabled',True)
    try:ns.set_editor_property('fallback_percent_triangles',.03 if not rock else .1)  # trees are 300k+: keep the non-Nanite fallback tiny
    except Exception:pass
    if not rock:ns.set_editor_property('shape_preservation',u.NaniteShapePreservation.PRESERVE_AREA)  # foliage: keeps leaf/needle area as Nanite simplifies, else crowns go bare at range
    mesh.set_editor_property('nanite_settings',ns);bs=mesh.get_editor_property('body_setup');bs.set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_DEFAULT)
    if col=='convex':
        sme.remove_collisions(mesh);assert sme.set_convex_decomposition_collisions(mesh,12,16,100000)>=0,name
    elif col=='none':
        sme.remove_collisions(mesh)
    else:   # trunk-only convex prisms come from the FBX (UCX_<mesh>_NN); the canopy has no collision
        assert len(bs.get_editor_property('agg_geom').get_editor_property('convex_elems'))>=1,f'{name}: trunk UCX collision missing from the FBX'
    eal.save_loaded_asset(mesh);b=mesh.get_bounding_box()
    return dict(mesh=f'{LIB}/Meshes/{name}',size_m=[round((b.max.x-b.min.x)/100,2),round((b.max.y-b.min.y)/100,2),round((b.max.z-b.min.z)/100,2)],
                min_z_cm=round(b.min.z,1),nanite=True,convex_hulls=len(mesh.get_editor_property('body_setup').get_editor_property('agg_geom').get_editor_property('convex_elems')),triangles=mesh.get_num_triangles(0))


report={}
# Interchange's FBX path drops UCX_ collision, so the legacy FBX importer runs for this script and the flag is restored after.
W=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();CV='Interchange.FeatureFlags.Import.FBX'
was=u.SystemLibrary.get_console_variable_bool_value(CV);u.SystemLibrary.execute_console_command(W,f'{CV} 0')
try:
    for fam in FAMILIES:
        man=json.loads((ROOT/fam/'asset-manifest.json').read_text());rock=fam in ROCKISH;inst={}
        for s in man['materials']:inst[s['name']]=instance(s,'leaf' if s.get('alpha') else ('rock' if rock or 'Flower' in s['name'] else 'wood'))
        rep=[import_mesh(ROOT/fam/r['fbx'],r['mesh'],rock,inst,COL.get(fam,'none')) for r in man['variants']];report[fam]=rep
        (ROOT/fam/'ue-import-report.json').write_text(json.dumps(rep,indent=2)+'\n')
finally:u.SystemLibrary.execute_console_command(W,f'{CV} {int(was)}')
print('NATURE_IMPORT',json.dumps(report))
