"""Silverbrook town dressing (GFX-01 WP6): cobbled streets, verges, props, windows lit by the sun's height.

python tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/silverbrook_props.py   (open Greenlands_Silverbrook; re-run replaces)

Tuning is silverbrook-props.json (typed, bounded). Four steps, each safe to repeat:
  roads    graybox Roads/Primary|Secondary strips get M_SB_CobbleRoad: scan cobbles, dithered shoulders (the Greenlands
           finish_worn_roads.py approach) and packed earth at the edge.
  windows  emissive M_SB_Solid instances move to M_SB_WindowGlow, whose emission follows the atmosphere sun's elevation, so
           lit glass is dark at noon and glows at dusk without any clock code. (import_silverbrook.py rebuilds M_SB_Solid
           parenting; run this again afterwards.)
  verge    herbs, pebbles and moss along both street edges as foliage instances (cosmetic, no collision).
  props    json rules along the streets inside the wall; tagged SilverbrookDressing (place_props.py's SilverbrookProp is separate).
build_silverbrook.py should call dress_all() once after its street and building spawns.
"""
import unreal as u,json,math,hashlib,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;LAYOUT=ROOT.parent/'Greenlands'/'silverbrook-town-layout.json'
sys.path.insert(0,str(ROOT))
LIB='/Game/Frontier/Environment/Greenlands/Silverbrook';TAG='SilverbrookDressing'
mel=u.MaterialEditingLibrary;eal=u.EditorAssetLibrary;tools=u.AssetToolsHelpers.get_asset_tools()

def cfg():
    c=json.loads((ROOT/'silverbrook-props.json').read_text(encoding='utf-8'))
    def walk(d,path=''):
        if isinstance(d,dict):
            if 'value' in d and 'min' in d:
                vs=d['value'] if isinstance(d['value'],list) else [d['value']]
                assert all(d['min']<=v<=d['max'] for v in vs),f'{path} {d["value"]} outside [{d["min"]},{d["max"]}] {d["unit"]}'
            else:
                for k,v in d.items():walk(v,path+'/'+k)
        elif isinstance(d,list):
            for i,v in enumerate(d):walk(v,path+f'[{i}]')
    walk(c);return c
def val(d):return d['value']

def _h(s):return int(hashlib.md5(s.encode()).hexdigest()[:8],16)/0xFFFFFFFF
def _world():return u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
def _actors():return u.get_editor_subsystem(u.EditorActorSubsystem)
def _asset(path):
    return u.load_asset(path) if path and eal.does_asset_exist(path) else None

# ---- materials -------------------------------------------------------------------------------------------------
def _node(m,cls,**p):
    n=mel.create_material_expression(m,cls)
    for k,v in p.items():n.set_editor_property(k,v)
    return n
def _link(a,b,pin,out=''):assert mel.connect_material_expressions(a,out,b,pin),(a,b,pin)
def _tex(asset,channel):
    path=f'{LIB}/Textures/T_SB_{asset}_{channel}'
    if not eal.does_asset_exist(path):
        t=u.AssetImportTask();t.filename=str(ROOT/'Textures'/f'{asset}_{channel}.jpg');t.destination_path=LIB+'/Textures';t.destination_name=f'T_SB_{asset}_{channel}';t.automated=True;t.save=True
        tools.import_asset_tasks([t])
        x=u.load_asset(path);x.set_editor_property('srgb',channel=='Diffuse')
        if channel=='nor_gl':x.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);x.set_editor_property('flip_green_channel',True)
        elif channel=='Rough':x.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
        eal.save_loaded_asset(x)
    return u.load_asset(path)
def _master(name):
    path=f'{LIB}/Materials/{name}'
    m=u.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name,LIB+'/Materials',u.Material,u.MaterialFactoryNew())
    mel.delete_all_material_expressions(m);return m
def _instance(name,parent):
    path=f'{LIB}/Materials/{name}'
    mi=u.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name,LIB+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi,parent);return mi

def build_road_material(c):
    m=_master('M_SB_CobbleRoad');S=u.MaterialSamplerType
    def param(cls,name,default):return _node(m,cls,parameter_name=name,default_value=default)
    tile=param(u.MaterialExpressionScalarParameter,'TileCm',240.)
    world=_node(m,u.MaterialExpressionWorldPosition)
    xy=_node(m,u.MaterialExpressionComponentMask,r=True,g=True,b=False,a=False);_link(world,xy,'')
    uv=_node(m,u.MaterialExpressionDivide);_link(xy,uv,'A');_link(tile,uv,'B')
    def sample(asset,channel,st):
        n=_node(m,u.MaterialExpressionTextureSample,texture=_tex(asset,channel),sampler_type=st);_link(uv,n,'UVs');return n
    diff=sample('cobblestone_pavement','Diffuse',S.SAMPLERTYPE_COLOR);rough=sample('cobblestone_pavement','Rough',S.SAMPLERTYPE_MASKS);nor=sample('cobblestone_pavement','nor_gl',S.SAMPLERTYPE_NORMAL)
    tint=param(u.MaterialExpressionVectorParameter,'CobbleTint',u.LinearColor(1,1,1,1))
    cob=_node(m,u.MaterialExpressionMultiply);_link(diff,cob,'A');_link(tint,cob,'B')
    # Across-width coordinate: the strip is a unit cube scaled (run, width, thickness), so local Y/50 runs -1..1.
    local=_node(m,u.MaterialExpressionTransformPosition,transform_source_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,transform_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL);_link(world,local,'')
    ly=_node(m,u.MaterialExpressionComponentMask,r=False,g=True,b=False,a=False);_link(local,ly,'')
    ab=_node(m,u.MaterialExpressionAbs);_link(ly,ab,'')
    across=_node(m,u.MaterialExpressionMultiply,const_b=1/50.);_link(ab,across,'A')  # 0 centre .. 1 edge
    inv=_node(m,u.MaterialExpressionOneMinus);_link(across,inv,'')
    shoulder=param(u.MaterialExpressionScalarParameter,'ShoulderFraction',.22)
    ramp=_node(m,u.MaterialExpressionDivide);_link(inv,ramp,'A');_link(shoulder,ramp,'B')
    noise=_node(m,u.MaterialExpressionNoise,scale=1,levels=2,quality=1,output_min=0,output_max=1,turbulence=False)
    nw=_node(m,u.MaterialExpressionMultiply,const_b=1/90.);_link(world,nw,'A');_link(nw,noise,'')
    nstr=param(u.MaterialExpressionScalarParameter,'EdgeNoise',.35)
    nmul=_node(m,u.MaterialExpressionMultiply);_link(noise,nmul,'A');_link(nstr,nmul,'B')
    cover=_node(m,u.MaterialExpressionSubtract);_link(ramp,cover,'A');_link(nmul,cover,'B')
    clamp=_node(m,u.MaterialExpressionClamp,min_default=0,max_default=1);_link(cover,clamp,'')
    # Earth creeps over the cobbles on the outer part of the shoulder.
    earth=param(u.MaterialExpressionVectorParameter,'ShoulderColour',u.LinearColor(.16,.13,.07,1))
    mix=_node(m,u.MaterialExpressionLinearInterpolate);_link(earth,mix,'A');_link(cob,mix,'B');_link(clamp,mix,'Alpha')
    dither=_node(m,u.MaterialExpressionMaterialFunctionCall)
    dither.set_material_function(u.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA'));_link(clamp,dither,'Alpha Threshold')
    m.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED);m.set_editor_property('opacity_mask_clip_value',.14)
    assert mel.connect_material_property(mix,'',u.MaterialProperty.MP_BASE_COLOR)
    assert mel.connect_material_property(nor,'RGB',u.MaterialProperty.MP_NORMAL)
    assert mel.connect_material_property(rough,'R',u.MaterialProperty.MP_ROUGHNESS)
    assert mel.connect_material_property(dither,'',u.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(m);assert eal.save_loaded_asset(m)
    r=c['road'];mi=_instance('MI_SB_CobbleRoad',m)
    mel.set_material_instance_scalar_parameter_value(mi,'TileCm',val(r['tile_m'])*100)
    mel.set_material_instance_scalar_parameter_value(mi,'ShoulderFraction',val(r['shoulder_fraction']))
    mel.set_material_instance_scalar_parameter_value(mi,'EdgeNoise',val(r['edge_noise']))
    mel.set_material_instance_vector_parameter_value(mi,'CobbleTint',u.LinearColor(*val(r['cobble_tint']),1))
    mel.set_material_instance_vector_parameter_value(mi,'ShoulderColour',u.LinearColor(*val(r['shoulder_colour']),1))
    assert eal.save_loaded_asset(mi);return mi

def build_glow_master(c):
    """M_SB_Solid plus a daylight gate: emission x lerp(1, night, Gate); night = how far the sun is below `begins`."""
    assert u.load_asset(f'{LIB}/Materials/M_SB_Solid'),'run import_silverbrook.py first'
    m=_master('M_SB_WindowGlow')
    def param(cls,name,default):return _node(m,cls,parameter_name=name,default_value=default)
    col=param(u.MaterialExpressionVectorParameter,'Colour',u.LinearColor(.5,.5,.5,1));rg=param(u.MaterialExpressionScalarParameter,'Roughness',.6);me=param(u.MaterialExpressionScalarParameter,'Metallic',0.)
    e=param(u.MaterialExpressionVectorParameter,'Emission',u.LinearColor(0,0,0,1));s=param(u.MaterialExpressionScalarParameter,'EmissionStrength',0.)
    em=_node(m,u.MaterialExpressionMultiply);_link(e,em,'A');_link(s,em,'B')
    sun=_node(m,u.MaterialExpressionSkyAtmosphereLightDirection,light_index=0)
    z=_node(m,u.MaterialExpressionComponentMask,r=False,g=False,b=True,a=False);_link(sun,z,'')   # sin(sun elevation)
    w=c['windows']
    hi=param(u.MaterialExpressionScalarParameter,'SinBegins',math.sin(math.radians(val(w['glow_begins_sun_elevation_deg']))))
    lo=param(u.MaterialExpressionScalarParameter,'SinFull',math.sin(math.radians(val(w['glow_full_sun_elevation_deg']))))
    num=_node(m,u.MaterialExpressionSubtract);_link(hi,num,'A');_link(z,num,'B')
    den=_node(m,u.MaterialExpressionSubtract);_link(hi,den,'A');_link(lo,den,'B')
    q=_node(m,u.MaterialExpressionDivide);_link(num,q,'A');_link(den,q,'B')
    night=_node(m,u.MaterialExpressionSaturate);_link(q,night,'')
    gate=param(u.MaterialExpressionScalarParameter,'Gate',0.)
    one=_node(m,u.MaterialExpressionConstant,r=1.)
    k=_node(m,u.MaterialExpressionLinearInterpolate);_link(one,k,'A');_link(night,k,'B');_link(gate,k,'Alpha')
    out=_node(m,u.MaterialExpressionMultiply);_link(em,out,'A');_link(k,out,'B')
    for node,prop in ((col,'MP_BASE_COLOR'),(rg,'MP_ROUGHNESS'),(me,'MP_METALLIC'),(out,'MP_EMISSIVE_COLOR')):
        assert mel.connect_material_property(node,'',getattr(u.MaterialProperty,prop))
    mel.recompile_material(m);assert eal.save_loaded_asset(m);return m

def gate_windows(c):
    master=build_glow_master(c);pats=c['windows']['name_patterns'];gated=[];free=[]
    solid=u.load_asset(f'{LIB}/Materials/M_SB_Solid')
    for path in eal.list_assets(LIB+'/Materials',recursive=False,include_folder=False):
        mi=u.load_asset(path)
        if not isinstance(mi,u.MaterialInstanceConstant):continue
        if mi.get_editor_property('parent') not in (solid,master):continue
        if mel.get_material_instance_scalar_parameter_value(mi,'EmissionStrength')<=0:continue
        is_win=any(re.search(p,mi.get_name(),re.I) for p in pats)
        em=mel.get_material_instance_vector_parameter_value(mi,'Emission');st=mel.get_material_instance_scalar_parameter_value(mi,'EmissionStrength')
        mel.set_material_instance_parent(mi,master)
        mel.set_material_instance_vector_parameter_value(mi,'Emission',em);mel.set_material_instance_scalar_parameter_value(mi,'EmissionStrength',st)
        mel.set_material_instance_scalar_parameter_value(mi,'Gate',1. if is_win else 0.)
        eal.save_loaded_asset(mi);(gated if is_win else free).append(mi.get_name())
    print('WINDOWS_GATED',gated,'ALWAYS_LIT',free);return gated

# ---- placement ---------------------------------------------------------------------------------------------------
def _roads(L,c):
    out=[]
    for tier in c['road']['tiers']:
        w=L['roads'][tier.lower()+'_width']
        for name,pts in L['roads'][tier.lower()].items():out.append((name,tier,pts,w))
    return out
def _stations(pts,step):
    """(x, y, tangent ux, uy) every `step` m along a polyline."""
    s=step/2
    for a,b in zip(pts,pts[1:]):
        seg=math.dist(a,b)
        if seg==0:continue
        ux,uy=(b[0]-a[0])/seg,(b[1]-a[1])/seg
        while s<seg:
            yield a[0]+ux*s,a[1]+uy*s,ux,uy
            s+=step
        s-=seg
def _inside(L,c,x,y):
    w=L['wall'];m=val(c['wall_margin_m'])
    return (x/(w['semi_axis_x']-m))**2+(y/(w['semi_axis_y']-m))**2<1

def dress_roads(c):
    mi=build_road_material(c);A=_actors();n=0
    for a in A.get_all_level_actors():
        if isinstance(a,u.StaticMeshActor) and any(str(a.get_folder_path()).startswith(f'Roads/{t}') for t in c['road']['tiers']):
            a.static_mesh_component.set_material(0,mi);n+=1
    print('ROAD_STRIPS_COBBLED',n);return n

def _obstacles(A):
    import place_props as pp
    rects=[]
    for a in A.get_all_level_actors():
        if isinstance(a,u.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name().startswith('SM_Silverbrook') and TAG not in [str(t) for t in a.tags]:
            rects.append(pp._rect(a))
    return pp,rects

def _clear(A):
    for a in A.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:A.destroy_actor(a)

def dress_verge(L,c):
    world=_world();v=c['verge'];A=_actors();pp,rects=_obstacles(A)
    meshes=[_asset(f'/Game/Frontier/Environment/Greenlands/Nature/Meshes/{n}') for n in v['meshes']];meshes=[m for m in meshes if m]
    types=[]
    for mesh in meshes:
        name=f'FT_SBVerge_{mesh.get_name()}';path=f'{LIB}/Foliage/{name}'
        ft=u.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name,LIB+'/Foliage',u.FoliageType_InstancedStaticMesh,u.FoliageType_InstancedStaticMeshFactory())
        ft.set_editor_property('mesh',mesh)
        ft.set_editor_property('cull_distance',u.Int32Interval(0,int(val(v['cull_end_m'])*100)))
        eal.save_loaded_asset(ft);u.InstancedFoliageActor.remove_all_instances(world,ft);types.append((ft,[]))
    lo,hi=val(v['scale']);step=val(v['step_m']);off=val(v['offset_m']);band=val(v['band_m'])
    for name,tier,pts,width in _roads(L,c):
        for k,(x,y,ux,uy) in enumerate(_stations(pts,step)):
            if not types or not _inside(L,c,x,y):continue
            for side in (-1,1):
                key=f'{name}{k}{side}';d=width/2+off+_h(key+'d')*band
                px,py=x-side*uy*d+(_h(key+'j')-.5)*step*ux,y+side*ux*d+(_h(key+'j')-.5)*step*uy
                if not pp._free((px*100-30,py*100-30,px*100+30,py*100+30),rects):continue
                z=pp._ground(world,px*100,py*100,0)
                if z is None:continue
                ft,tx=types[int(_h(key+'t')*len(types))%len(types)]
                s=lo+(hi-lo)*_h(key+'s')
                tx.append(u.Transform(location=u.Vector(px*100,py*100,z),rotation=u.Rotator(yaw=_h(key+'y')*360),scale=u.Vector(s,s,s)))
    total=0
    for ft,tx in types:
        if tx:u.InstancedFoliageActor.add_instances(world,ft,tx);total+=len(tx)
    print('VERGE_INSTANCES',total);return total

def dress_props(L,c):
    world=_world();A=_actors();pp,rects=_obstacles(A);placed={}
    for rule in c['props']:
        mesh=_asset(rule.get('fab')) or (None if rule.get('optional_only') else _asset(rule['asset']))
        if not mesh:print('PROP_SKIPPED',rule['id']);continue
        n=0;sp=rule['spacing_m'];off=rule['offset_m'];clr=rule['clear_m']*100;slo,shi=rule.get('scale',[1,1])
        for name,tier,pts,width in _roads(L,c):
            if tier not in rule['tiers']:continue
            for k,(x,y,ux,uy) in enumerate(_stations(pts,sp)):
                key=f"{rule['id']}{name}{k}"
                if n>=rule['max_count']:break
                if not _inside(L,c,x,y) or _h(key+'p')<.35:continue
                side=1 if _h(key+'q')<.5 else -1;d=width/2+off
                px,py=(x-side*uy*d)*100,(y+side*ux*d)*100
                if not pp._free((px-clr-50,py-clr-50,px+clr+50,py+clr+50),rects):continue
                z=pp._ground(world,px,py,0)
                if z is None:continue
                to_road=math.degrees(math.atan2(side*ux,-side*uy))   # direction from the prop toward the street centre
                yaw={'road':to_road-90,'along':math.degrees(math.atan2(uy,ux))-90,'random':_h(key+'y')*360}[rule['face']]  # mesh front is +Y (FBX flip)
                s=slo+(shi-slo)*_h(key+'s')
                a=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(px,py,z),u.Rotator(roll=0,pitch=0,yaw=yaw))
                a.static_mesh_component.set_static_mesh(mesh);a.set_actor_scale3d(u.Vector(s,s,s));a.tags=[TAG];a.set_folder_path('Silverbrook/Dressing')
                a.set_actor_label(f'{rule["id"]}_{n:02d}')
                rects.append((px-clr,py-clr,px+clr,py+clr));n+=1
        placed[rule['id']]=n
    print('PROPS_PLACED',placed);return placed

def dress_all():
    c=cfg();L=json.loads(LAYOUT.read_text(encoding='utf-8'));_clear(_actors())
    dress_roads(c);gate_windows(c);dress_verge(L,c);dress_props(L,c)

if __name__=='__main__':
    dress_all()
