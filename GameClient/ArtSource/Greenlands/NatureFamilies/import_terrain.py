"""Import the two Greenlands terrain materials (full editor; run with tools/Run-InEditor.py).

py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/import_terrain.py

Writes /Game/Frontier/Environment/Greenlands/Nature/{Textures,Materials}: master M_GN_Terrain (world-aligned XY UVs = world cm / tile cm, so it works on any mesh or landscape;
world-aligned triplanar colour and roughness (normal from the top projection), layer B weight = saturate((mask R - Low) * InvRange)) and the instances MI_GN_ForestFloor / MI_GN_Bedrock from TerrainMaterials/asset-manifest.json. No level is touched.
"""
import unreal as u,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;LIB='/Game/Frontier/Environment/Greenlands/Nature'
tools=u.AssetToolsHelpers.get_asset_tools();mel=u.MaterialEditingLibrary;eal=u.EditorAssetLibrary
man=json.loads((ROOT/'TerrainMaterials/asset-manifest.json').read_text())


def texture(rel,kind):
    p=ROOT/rel;name='T_GN_'+p.stem;path=f'{LIB}/Textures/{name}'
    if not eal.does_asset_exist(path):
        t=u.AssetImportTask();t.filename=str(p);t.destination_path=LIB+'/Textures';t.destination_name=name;t.automated=True;t.save=True;t.replace_existing=True;tools.import_asset_tasks([t])
    a=u.load_asset(path);assert a,path
    if kind=='normal':a.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);a.set_editor_property('srgb',False);a.set_editor_property('flip_green_channel',True)
    elif kind=='mask':a.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS);a.set_editor_property('srgb',False)
    eal.save_loaded_asset(a);return a


def node(m,cls,x,y,**kw):
    e=mel.create_material_expression(m,cls,x,y)
    for k,v in kw.items():e.set_editor_property(k,v)
    return e


def link(a,out,b,inp):assert mel.connect_material_expressions(a,out,b,inp),(a,out,b,inp)


def master():
    path=f'{LIB}/Materials/M_GN_Terrain'
    m=u.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset('M_GN_Terrain',LIB+'/Materials',u.Material,u.MaterialFactoryNew())
    mel.delete_all_material_expressions(m);S=u.MaterialSamplerType;WRAP=u.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS
    sc=lambda n,d,x,y:node(m,u.MaterialExpressionScalarParameter,x,y,parameter_name=n,default_value=d)
    wp=node(m,u.MaterialExpressionWorldPosition,-2400,0)
    def comp(src,r,g,b,x,y):
        c=node(m,u.MaterialExpressionComponentMask,x,y,r=r,g=g,b=b,a=False);link(src,'',c,'');return c
    xy=comp(wp,True,True,False,-2250,0);xz=node(m,u.MaterialExpressionAppendVector,-2250,100);yz=node(m,u.MaterialExpressionAppendVector,-2250,200)
    x_=comp(wp,True,False,False,-2400,300);y_=comp(wp,False,True,False,-2400,400);z_=comp(wp,False,False,True,-2400,500)
    link(x_,'',xz,'A');link(z_,'',xz,'B');link(y_,'',yz,'A');link(z_,'',yz,'B')
    # triplanar weights from the vertex normal: |n|^4 normalised; wz = top (XY), wy = faces along Y (XZ), wx = faces along X (YZ)
    vn=node(m,u.MaterialExpressionVertexNormalWS,-2400,700);ab=node(m,u.MaterialExpressionAbs,-2250,700);link(vn,'',ab,'')
    pw=node(m,u.MaterialExpressionPower,-2100,700,const_exponent=4.0);link(ab,'',pw,'Base')
    px=comp(pw,True,False,False,-1950,650);py_=comp(pw,False,True,False,-1950,750);pz=comp(pw,False,False,True,-1950,850)
    s1=node(m,u.MaterialExpressionAdd,-1800,700);link(px,'',s1,'A');link(py_,'',s1,'B');s2=node(m,u.MaterialExpressionAdd,-1650,700);link(s1,'',s2,'A');link(pz,'',s2,'B')
    W={}
    for k,e in (('x',px),('y',py_),('z',pz)):
        d=node(m,u.MaterialExpressionDivide,-1500,700+{'x':0,'y':100,'z':200}[k]);link(e,'',d,'A');link(s2,'',d,'B');W[k]=d
    dummy=man['materials'][0]

    def uvset(p,y,tile):
        t=sc('TileCm'+p,tile,-1800,y+150);out=[]
        for i,src in enumerate((xy,yz,xz)):
            d=node(m,u.MaterialExpressionDivide,-1650,y+i*60);link(src,'',d,'A');link(t,'',d,'B');out.append(d)
        return out   # top, x-facing (YZ), y-facing (XZ)

    def tri(name,tex,kind,uvs,y,st=S.SAMPLERTYPE_COLOR):
        smp=[node(m,u.MaterialExpressionTextureSampleParameter2D,-1100,y+i*220,parameter_name=name,texture=tex,sampler_type=st,sampler_source=WRAP) for i in range(3)]
        for sp,uv in zip(smp,uvs):link(uv,'',sp,'UVs')
        acc=None
        for sp,k in zip(smp,('z','x','y')):
            mu=node(m,u.MaterialExpressionMultiply,-900,y+(0 if k=='z' else 100 if k=='x' else 200));link(sp,'RGB',mu,'A');link(W[k],'',mu,'B')
            if acc is None:acc=mu
            else:
                ad=node(m,u.MaterialExpressionAdd,-780,y);link(acc,'',ad,'A');link(mu,'',ad,'B');acc=ad
        return acc

    def layer(p,y,tile):
        uvs=uvset(p,y,tile);base=tri('Base'+p,texture(dummy['base'],'color'),'color',uvs,y)
        rough=tri('Rough'+p,texture(dummy['rough'],'mask'),'mask',uvs,y+700,S.SAMPLERTYPE_MASKS);rr=comp(rough,True,False,False,-650,y+700)
        nor=node(m,u.MaterialExpressionTextureSampleParameter2D,-1100,y+1400,parameter_name='Normal'+p,texture=texture(dummy['normal'],'normal'),sampler_type=S.SAMPLERTYPE_NORMAL,sampler_source=WRAP);link(uvs[0],'',nor,'UVs')
        tint=node(m,u.MaterialExpressionVectorParameter,-900,y-100,parameter_name='Tint'+p,default_value=u.LinearColor(1,1,1,1))
        tm=node(m,u.MaterialExpressionMultiply,-500,y);link(base,'',tm,'A');link(tint,'',tm,'B')
        one=node(m,u.MaterialExpressionConstant,-700,y-200,r=1.0);ds=node(m,u.MaterialExpressionDesaturation,-400,y-100);link(tm,'',ds,'');link(one,'',ds,'Fraction')
        sat=sc('Sat'+p,1,-400,y+50);col=node(m,u.MaterialExpressionLinearInterpolate,-250,y);link(ds,'',col,'A');link(tm,'',col,'B');link(sat,'',col,'Alpha')
        return col,nor,rr
    ca,na,ra=layer('A',0,200);cb,nb,rb=layer('B',2400,150)
    muv=uvset('Mask',5000,900)[0];mk=node(m,u.MaterialExpressionTextureSampleParameter2D,-1100,5000,parameter_name='Mask',texture=texture(dummy['blend']['mask'],'mask'),sampler_type=S.SAMPLERTYPE_MASKS,sampler_source=WRAP);link(muv,'',mk,'UVs')
    lo=sc('Low',.5,-400,5000);inv=sc('InvRange',5,-400,5100);amt=sc('BlendAmount',1,-400,5200)
    s=node(m,u.MaterialExpressionSubtract,-200,5000);link(mk,'R',s,'A');link(lo,'',s,'B');d=node(m,u.MaterialExpressionMultiply,-50,5000);link(s,'',d,'A');link(inv,'',d,'B')
    sa=node(m,u.MaterialExpressionSaturate,100,5000);link(d,'',sa,'');w=node(m,u.MaterialExpressionMultiply,250,5000);link(sa,'',w,'A');link(amt,'',w,'B')
    def mix(a,b,y,ao='',bo=''):
        l=node(m,u.MaterialExpressionLinearInterpolate,500,y);link(a,ao,l,'A');link(b,bo,l,'B');link(w,'',l,'Alpha');return l
    col=mix(ca,cb,0);nrm=mix(na,nb,250,'RGB','RGB');rgh=mix(ra,rb,500)
    mel.connect_material_property(col,'',u.MaterialProperty.MP_BASE_COLOR);mel.connect_material_property(nrm,'',u.MaterialProperty.MP_NORMAL);mel.connect_material_property(rgh,'',u.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m);eal.save_loaded_asset(m);return m


M=master();LC=lambda v:u.LinearColor(v[0],v[1],v[2],1);report=[]
for spec in man['materials']:
    path=f"{LIB}/Materials/{spec['name']}"
    mi=u.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(spec['name'],LIB+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi,M);b=spec['blend']
    for p,s in (('A',spec),('B',b)):
        for k,f,kd in (('Base','base','color'),('Normal','normal','normal'),('Rough','rough','mask')):mel.set_material_instance_texture_parameter_value(mi,k+p,texture(s[f],kd))
        mel.set_material_instance_scalar_parameter_value(mi,'TileCm'+p,s['tile_m']*100);mel.set_material_instance_scalar_parameter_value(mi,'Sat'+p,s['sat']);mel.set_material_instance_vector_parameter_value(mi,'Tint'+p,LC(s['tint']))
    mel.set_material_instance_texture_parameter_value(mi,'Mask',texture(b['mask'],'mask'));mel.set_material_instance_scalar_parameter_value(mi,'TileCmMask',b['mask_tile_m']*100)
    mel.set_material_instance_scalar_parameter_value(mi,'Low',b['low']);mel.set_material_instance_scalar_parameter_value(mi,'InvRange',1/(b['high']-b['low']))
    eal.save_loaded_asset(mi);report.append(path)
print('TERRAIN_IMPORT',json.dumps(report))
