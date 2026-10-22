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
    A=_actors()  # WP3 builds roads into the terrain; strip actors only exist in older maps
    strips=[a for a in A.get_all_level_actors() if isinstance(a,u.StaticMeshActor) and any(str(a.get_folder_path()).startswith(f'Roads/{t}') for t in c['road']['tiers'])]
    n=0
    if strips:
        mi=build_road_material(c)
        for a in strips:a.static_mesh_component.set_material(0,mi);n+=1
    print('ROAD_STRIPS_COBBLED',n);return n

def _obstacles(A):
    import place_props as pp
    rects=[]
    for a in A.get_all_level_actors():
        if isinstance(a,u.StaticMeshActor) and '_Stoop' in a.get_actor_label():rects.append(pp._rect(a))
        elif isinstance(a,u.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name().startswith('SM_Silverbrook') and TAG not in [str(t) for t in a.tags]:
            r=pp._rect(a)
            if max(r[2]-r[0],r[3]-r[1])<=4000:  # terrain and backdrop are not street obstacles
                rects.append(r)
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
                preferred=1 if _h(key+'q')<.5 else -1;d=width/2+off
                for side in (preferred,-preferred):
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
                    break
        placed[rule['id']]=n
    print('PROPS_PLACED',placed);return placed

def dress_street_groups(L,c):
    """Small, mixed Frontier prop groups just outside the walkable road."""
    A=_actors();world=_world();pp,rects=_obstacles(A)
    import pcg_dressing
    _,terrain=pcg_dressing._terrain()
    g=c['street_groups'];step=val(g['spacing_m']);offset=val(g['offset_m']);clear=val(g['clear_m'])*100
    names=['RainBarrel','SackPile','FirewoodStack','Bench','MarketGoods','Wheelbarrow','HayBales','HitchingPost']
    meshes=[_asset(LIB+'/Meshes/SM_SilverbrookProp'+n) for n in names]
    assert all(meshes),names
    total=0;road_counts={}
    for road,tier,pts,width in _roads(L,c):
        road_counts[road]=0
        for k,(x,y,ux,uy) in enumerate(_stations(pts,step)):
            if total>=val(g['max_groups']) or not _inside(L,c,x,y):continue
            key=f'group:{road}:{k}';side=1 if k%2 else -1
            mesh=meshes[int(_h(key)*len(meshes))%len(meshes)]
            for side,shift in ((side,0),(side,0.8),(-side,0),(-side,0.8)):
                d=width/2+offset+shift
                px=(x-side*uy*d)*100;py=(y+side*ux*d)*100
                box=(px-clear,py-clear,px+clear,py+clear)
                if not pp._free(box,rects):continue
                hit=terrain.static_mesh_component.line_trace_component(u.Vector(px,py,100000),u.Vector(px,py,-100000),True,False)
                if not hit:continue
                z=hit[0].z
                yaw=math.degrees(math.atan2(uy,ux))-90
                a=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(px,py,z),u.Rotator(yaw=yaw))
                a.static_mesh_component.set_static_mesh(mesh)
                a.set_actor_label(f'StreetGroup_{road.replace(" ", "")}_{k:03d}')
                a.set_folder_path('Silverbrook/Dressing');a.tags=[TAG]
                rects.append(box);total+=1;road_counts[road]+=1
                break
    print('STREET_GROUPS',total,road_counts)
    return road_counts

def dress_house_details():
    """Side gardens, small flowering tubs and the existing animated smoke-card VFX."""
    A=_actors();world=_world();import pcg_dressing
    _,terrain=pcg_dressing._terrain()
    fence=_asset(LIB+'/Meshes/SM_SilverbrookPropFenceSection')
    barrel=_asset(LIB+'/Meshes/SM_SilverbrookPropRainBarrel')
    crop=_asset('/Game/Frontier/Environment/Greenlands/Nature/Meshes/SM_HerbBroadleaf_00')
    flower=_asset('/Game/Frontier/Environment/Greenlands/Arrival/Meshes/SM_PH_FloweringClump_0')
    smoke=_asset('/Game/Frontier/VFX/StreetLamp/M_LampSmoke');plane=_asset('/Engine/BasicShapes/Plane')
    assert all((fence,barrel,crop,flower,smoke,plane))
    stacks={1:(2.2,1.5,8.85),2:(-2.05,0,8.7),3:(2.45,-.55,8.75),4:(-2.1,.3,8.65),5:(2.25,.55,8.7),6:(-2.15,1.2,8.55)}
    counts=dict(gardens=0,crops=0,pots=0,smoking=0)
    def spawn(label,mesh,x,y,z,yaw,scale):
        a=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x,y,z),u.Rotator(pitch=90 if mesh==plane else 0,yaw=yaw))
        a.static_mesh_component.set_static_mesh(mesh)
        a.set_actor_scale3d(u.Vector(*scale))
        a.set_actor_label(label);a.set_folder_path('Silverbrook/Dressing');a.tags=[TAG]
        if mesh==plane:
            a.static_mesh_component.set_material(0,smoke)
            a.static_mesh_component.set_collision_profile_name('NoCollision')
            a.static_mesh_component.set_editor_property('cast_shadow',False)
        return a
    for a in A.get_all_level_actors():
        if not isinstance(a,u.StaticMeshActor) or not str(a.get_folder_path()).startswith('Districts/'):continue
        mesh=a.static_mesh_component.static_mesh
        if not mesh or not mesh.get_name().startswith('SM_Silverbrook') or '_Door' in mesh.get_name():continue
        label=a.get_actor_label();h=_h(label);b=mesh.get_bounding_box();p=a.get_actor_location()
        yaw=a.get_actor_rotation().yaw;co,si=math.cos(math.radians(yaw)),math.sin(math.radians(yaw))
        def pt(lx,ly):return p.x+co*lx-si*ly,p.y+si*lx+co*ly
        def terrain_z(x,y):
            hit=terrain.static_mesh_component.line_trace_component(u.Vector(x,y,100000),u.Vector(x,y,-100000),True,False)
            return hit[0].z if hit else None
        if h<.38:
            side=-1 if _h(label+'side')<.5 else 1
            sx=side*((b.max.x-b.min.x)*.42+140)
            x,y=pt(sx,0);z=terrain_z(x,y)
            if z is not None:
                spawn('GardenFence_'+label,fence,x,y,z,yaw,(.82,.82,.72))
                for i in range(4):
                    cx,cy=pt(sx+side*(55+(i%2)*85),(i//2-.5)*130)
                    cz=terrain_z(cx,cy)
                    if cz is not None:
                        spawn(f'GardenCrop_{label}_{i}',crop,cx,cy,cz,yaw,(1.15,1.15,1.15));counts['crops']+=1
                counts['gardens']+=1
        if h<.5:
            side=-1 if _h(label+'pot')<.5 else 1
            x,y=pt(side*170,b.min.y*.43);z=terrain_z(x,y)
            if z is not None:
                spawn('FlowerTub_'+label,barrel,x,y,z,yaw,(.25,.25,.25))
                spawn('Flower_'+label,flower,x,y,z+25,yaw,(.35,.35,.35));counts['pots']+=1
        if mesh.get_name().startswith('SM_SilverbrookTownhouse') and h<.4:
            number=int(mesh.get_name().removeprefix('SM_SilverbrookTownhouse'))
            sx,sy,sz=stacks[number];x,y=pt(sx*100,sy*100)
            for k in range(2):spawn(f'ChimneySmoke_{label}_{k}',plane,x,y,p.z+sz*100+80,yaw+35+90*k,(1.6,.8,1))
            counts['smoking']+=1
    print('HOUSE_DETAILS',counts)
    return counts

def vary_house_finishes():
    """Three shared, subtle roof/plaster tints assigned deterministically per house."""
    master=_asset(LIB+'/Materials/M_SB_Scan');assert master
    expr=mel.get_material_expressions(master)
    if not any(isinstance(e,u.MaterialExpressionVectorParameter) and str(e.get_editor_property('parameter_name'))=='BaseTint' for e in expr):
        attrs=next(e for e in expr if isinstance(e,u.MaterialExpressionMakeMaterialAttributes))
        source=mel.get_inputs_for_material_expression(master,attrs)[0]
        assert source, 'scan master has no BaseColor input'
        tint=_node(master,u.MaterialExpressionVectorParameter,parameter_name='BaseTint',default_value=u.LinearColor(1,1,1,1))
        mul=_node(master,u.MaterialExpressionMultiply);_link(source,mul,'A');_link(tint,mul,'B')
        _link(mul,attrs,'BaseColor')
        mel.recompile_material(master);assert eal.save_loaded_asset(master)
    palettes={'MI_SB_LimePlaster':[(.96,.92,.86),(1.02,.99,.94),(.91,.95,.98)],
              'MI_SB_Slate':[(.91,.96,1.04),(1.04,.99,.91),(.98,1.0,1.01)]}
    variants={}
    for source,cols in palettes.items():
        parent=_asset(LIB+'/Materials/'+source);assert parent
        for i,col in enumerate(cols):
            name=f'{source}_HouseTone{i+1}'
            mi=_instance(name,parent)
            mel.set_material_instance_vector_parameter_value(mi,'BaseTint',u.LinearColor(*col,1))
            assert eal.save_loaded_asset(mi)
            variants[(source,i)]=mi
    n=0
    for a in _actors().get_all_level_actors():
        if not isinstance(a,u.StaticMeshActor) or not str(a.get_folder_path()).startswith('Districts/'):continue
        mesh=a.static_mesh_component.static_mesh
        if not mesh or not mesh.get_name().startswith('SM_Silverbrook') or '_Door' in mesh.get_name():continue
        i=int(_h(a.get_actor_label()+'tone')*3)%3
        for slot in range(a.static_mesh_component.get_num_materials()):
            mat=a.static_mesh_component.get_material(slot)
            if not mat:continue
            name=mat.get_name().split('_HouseTone')[0]
            if (name,i) in variants:
                a.static_mesh_component.set_material(slot,variants[(name,i)])
        n+=1
    print('HOUSE_TINTS',n)

_BASE={}
def _edge(v,low,mass=.08,band=6.):
    """Outermost wall plane along one axis: the extreme coordinate with at least `mass` of the base vertices within `band` cm,
    so a doorstep, post or drainpipe sticking out of the wall does not become the foundation edge."""
    v=v if low else v[::-1];n=max(1,int(len(v)*mass));sign=1 if low else -1
    for i in range(len(v)-n+1):
        if abs(v[i+n-1]-v[i])<=band:return v[i]
    return v[0]
def _base_box(mesh,band):
    """(xmin,xmax,ymin,ymax) in mesh space (cm) of the vertices within `band` cm of the mesh's lowest point."""
    key=(mesh.get_name(),band)
    if key not in _BASE:
        dm=u.DynamicMesh()
        dm,out=u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(mesh,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD())
        assert out==u.GeometryScriptOutcomePins.SUCCESS,mesh.get_name()
        pts=u.GeometryScript_List.convert_vector_list_to_array(u.GeometryScript_MeshQueries.get_all_vertex_positions(dm,False)[1])
        low=min(p.z for p in pts);near=[p for p in pts if p.z<=low+band]
        _BASE[key]=(_edge(sorted(p.x for p in near),True),_edge(sorted(p.x for p in near),False),
                    _edge(sorted(p.y for p in near),True),_edge(sorted(p.y for p in near),False))
    return _BASE[key]

def dress_footings(c):
    """A matte stone course under every house, including level lots; repeatable after a rebuild."""
    f=c['footing'];path=f['material'];m=_asset(path)
    if not m:
        m=tools.create_asset('M_SB_FoundationStone',LIB+'/Materials',u.Material,u.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    # World-aligned stone: the plinth is a stretched cube, so UVs come from world position (no smeared courses).
    def mask(src,**ch):
        n=_node(m,u.MaterialExpressionComponentMask,**{'r':False,'g':False,'b':False,'a':False,**ch});_link(src,n,'');return n
    def op(cls,a,b):
        n=_node(m,cls);_link(a,n,'A');_link(b,n,'B');return n
    wp=_node(m,u.MaterialExpressionWorldPosition);an=_node(m,u.MaterialExpressionAbs);_link(_node(m,u.MaterialExpressionVertexNormalWS),an,'')
    across=op(u.MaterialExpressionAdd,op(u.MaterialExpressionMultiply,mask(wp,r=True),mask(an,g=True)),
              op(u.MaterialExpressionMultiply,mask(wp,g=True),mask(an,r=True)))
    side=op(u.MaterialExpressionAppendVector,across,mask(wp,b=True))
    top_uv=_node(m,u.MaterialExpressionComponentMask,r=True,g=True,b=False,a=False);_link(wp,top_uv,'')
    off=_node(m,u.MaterialExpressionAdd,const_b=-.5);_link(mask(an,b=True),off,'A')
    gain=_node(m,u.MaterialExpressionMultiply,const_b=100.);_link(off,gain,'A')
    up=_node(m,u.MaterialExpressionSaturate);_link(gain,up,'')
    pick=_node(m,u.MaterialExpressionLinearInterpolate);_link(side,pick,'A');_link(top_uv,pick,'B');_link(up,pick,'Alpha')
    div=_node(m,u.MaterialExpressionDivide,const_b=180.);_link(pick,div,'A')
    tex=_node(m,u.MaterialExpressionTextureSample,texture=_tex('rock_wall_08','Diffuse'),sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR)
    _link(div,tex,'UVs')
    nor=_node(m,u.MaterialExpressionTextureSample,texture=_tex('rock_wall_08','nor_gl'),sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    _link(div,nor,'UVs');assert mel.connect_material_property(nor,'',u.MaterialProperty.MP_NORMAL)
    tint=_node(m,u.MaterialExpressionVectorParameter,parameter_name='StoneTint',default_value=u.LinearColor(.62,.64,.67,1))
    col=_node(m,u.MaterialExpressionMultiply);_link(tex,col,'A');_link(tint,col,'B')
    rough=_node(m,u.MaterialExpressionScalarParameter,parameter_name='Roughness',default_value=.86)
    assert mel.connect_material_property(col,'',u.MaterialProperty.MP_BASE_COLOR)
    assert mel.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m);assert eal.save_loaded_asset(m)
    sys.path.insert(0,str(ROOT.parent/'Greenlands'))
    import pcg_dressing
    _,terrain=pcg_dressing._terrain()
    A=_actors();actors=A.get_all_level_actors();by_label={a.get_actor_label():a for a in actors}
    cube=_asset('/Engine/BasicShapes/Cube');n=0;created=0
    def ground(x,y):
        hit=terrain.static_mesh_component.line_trace_component(u.Vector(x,y,100000),u.Vector(x,y,-100000),True,False)
        assert hit,(x,y)
        return hit[0].z
    for building in actors:
        if not isinstance(building,u.StaticMeshActor) or not str(building.get_folder_path()).startswith('Districts/'):
            continue
        mesh=building.static_mesh_component.static_mesh
        if not mesh or not mesh.get_name().startswith('SM_Silverbrook') or '_Door' in mesh.get_name():continue
        b=mesh.get_bounding_box();s=building.get_actor_scale3d();loc=building.get_actor_location()
        # Footprint = the mesh's own wall base: vertices within base_band_cm of its lowest point, plus a few cm of proud edge.
        # (Whole-mesh bounds include eaves, bays and signs, which left one-sided ledges.)
        x0,x1,y0,y1=_base_box(mesh,val(f['base_band_cm']))
        pad=val(f['proud_cm'])
        w=(x1-x0+2*pad)*s.x;d=(y1-y0+2*pad)*s.y;cx=(x0+x1)/2*s.x;cy=(y0+y1)/2*s.y
        yaw=math.radians(building.get_actor_rotation().yaw);co,si=math.cos(yaw),math.sin(yaw)
        px=loc.x+co*cx-si*cy;py=loc.y+si*cx+co*cy
        heights=[ground(px+co*x-si*y,py+si*x+co*y) for x in (-w/2,w/2) for y in (-d/2,d/2)]
        heights.append(ground(px,py))
        marker='SilverbrookFoundationRaised'
        wall_base=loc.z+b.min.z*s.z
        wanted=max(heights)+val(f['visible_cm'])
        raised=marker in [str(t) for t in building.tags]
        if not raised or wall_base-max(heights)<50:  # first pass lifts the house onto its plinth; later passes only top up
            shift=wanted-wall_base if raised else max(val(f['visible_cm']),wanted-wall_base)
            building.set_actor_location(u.Vector(loc.x,loc.y,loc.z+shift),False,False)
            if not raised:building.tags=list(building.tags)+[marker]
            wall_base+=shift
        assert wall_base-max(heights)>=50-0.1,(building.get_actor_label(),wall_base-max(heights))
        bottom=min(heights)-val(f['burial_cm'])
        plinth=by_label.get(building.get_actor_label()+'_Plinth')
        if not plinth:
            plinth=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(px,py,(bottom+wall_base)/2),building.get_actor_rotation())
            plinth.static_mesh_component.set_static_mesh(cube)
            plinth.set_actor_label(building.get_actor_label()+'_Plinth')
            plinth.set_folder_path(building.get_folder_path())
            created+=1
        plinth.set_actor_location(u.Vector(px,py,(bottom+wall_base)/2),False,False)
        plinth.set_actor_rotation(building.get_actor_rotation(),False)
        plinth.set_actor_scale3d(u.Vector(w/100,d/100,(wall_base-bottom)/100))
        plinth.static_mesh_component.set_material(0,m)
        # Stoop: equal low risers from the doorstep down to the street, so the raised doorway stays walkable.
        for old in [x for x in by_label.values() if x.get_actor_label().startswith(building.get_actor_label()+'_Stoop')]:A.destroy_actor(old)
        doors=[x for x in building.get_attached_actors() if 'Door' in x.get_actor_label()]
        if doors:
            dc,_=doors[0].get_actor_bounds(False)
            dx,dy=dc.x-px,dc.y-py;lx,ly=co*dx+si*dy,-si*dx+co*dy
            ax,ay=((1 if lx>0 else -1),0) if abs(lx)/(w/2)>abs(ly)/(d/2) else (0,(1 if ly>0 else -1))
            face=w/2 if ax else d/2;lat=ly if ax else lx
            gx,gy=px+co*(ax*face)-si*(ay*face)+0,py+si*(ax*face)+co*(ay*face)
            rise=wall_base-ground(gx+(co*ax-si*ay)*60,gy+(si*ax+co*ay)*60)
            steps=max(0,math.ceil(rise/22)) if rise>2 else 0
            for k in range(steps):
                off=face+20+40*k;cxl,cyl=(ax*off+(0 if ax else lat),ay*off+(lat if ax else 0))
                sx,sy=px+co*cxl-si*cyl,py+si*cxl+co*cyl
                top=wall_base-1-k*rise/steps;low=ground(sx,sy)-50
                st=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(sx,sy,(top+low)/2),building.get_actor_rotation())
                st.static_mesh_component.set_static_mesh(cube);st.static_mesh_component.set_material(0,m)
                st.set_actor_scale3d(u.Vector((.4 if ax else 1.5),(1.5 if ax else .4),(top-low)/100))
                st.set_actor_label(f'{building.get_actor_label()}_Stoop{k}');st.set_folder_path(building.get_folder_path())
        n+=1
    # Existing non-house courses use the same matte stone, without shifting public buildings.
    for a in actors:
        if isinstance(a,u.StaticMeshActor) and a.get_actor_label().endswith('_Plinth') and not str(a.get_folder_path()).startswith('Districts/'):
            a.static_mesh_component.set_material(0,m)
    print('HOUSE_FOUNDATIONS',n,'CREATED',created)

def hide_debug_actors():
    """Authoring visualisers never render in game (GFX-02 J1b); build_silverbrook.py sets the same on a fresh build."""
    n=0
    for a in _actors().get_all_level_actors():
        marker=isinstance(a,u.StaticMeshActor) and str(a.get_folder_path())=="Places" and a.static_mesh_component.get_material(0) and a.static_mesh_component.get_material(0).get_name()=="MI_Graybox_Marker"
        if marker or a.get_class().get_name() in ("TextRenderActor","ZoneShape","ZoneGraphData") or str(a.get_folder_path()) in ("Labels","Townsfolk/Lanes"):
            a.set_actor_hidden_in_game(True);n+=1
    print("DEBUG_HIDDEN_IN_GAME",n)

def dress_all():
    c=cfg();L=json.loads(LAYOUT.read_text(encoding='utf-8'));_clear(_actors())
    dress_footings(c);hide_debug_actors();vary_house_finishes();dress_roads(c);gate_windows(c);dress_verge(L,c);dress_props(L,c);dress_street_groups(L,c);dress_house_details()

if __name__=='__main__':
    dress_all()
