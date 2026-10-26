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
TERRAIN_LABEL=None

def _terrain():
    import pcg_dressing
    return pcg_dressing._terrain(TERRAIN_LABEL)

def _sills():
    return {m['unreal_mesh']: m.get('sills', []) for p in ROOT.glob('*/asset-manifest.json')
            for m in [json.loads(p.read_text(encoding='utf-8'))]}

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
    scale=param(u.MaterialExpressionScalarParameter,'GlowScale',1.)   # WP6i review fix 4: lit windows clipped the jetty soffits at dusk
    one=_node(m,u.MaterialExpressionConstant,r=1.)
    k=_node(m,u.MaterialExpressionLinearInterpolate);_link(one,k,'A');_link(night,k,'B');_link(gate,k,'Alpha')
    ks=_node(m,u.MaterialExpressionMultiply);_link(k,ks,'A');_link(scale,ks,'B')
    out=_node(m,u.MaterialExpressionMultiply);_link(em,out,'A');_link(ks,out,'B')
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
        lit=any(re.search(p,mi.get_name(),re.I) for p in c['windows']['always_lit_patterns'])   # WP6i review r2 fix 2: every glow follows the sun but these
        mel.set_material_instance_scalar_parameter_value(mi,'Gate',0. if lit else 1.)
        mel.set_material_instance_scalar_parameter_value(mi,'GlowScale',val(c['windows']['glow_strength_scale']) if is_win else 1.)
        eal.save_loaded_asset(mi);(free if lit else gated).append(mi.get_name())
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
        elif isinstance(a,u.StaticMeshActor) and a.get_actor_label().split('_')[0] in ('FlowerTub','StoopTub','GardenFence','GardenFirewood','WashingLine','SillPlanter'):rects.append(pp._rect(a))  # house details (WP6d fix 4a)
        elif isinstance(a,u.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name().startswith('SM_Silverbrook') and TAG not in [str(t) for t in a.tags]:
            r=pp._rect(a)
            if max(r[2]-r[0],r[3]-r[1])<=4000:  # terrain and backdrop are not street obstacles
                rects.append(r)
    # QST-02: quest boards and givers spawn at runtime from the layout rows; keep a 3 m square around each free of dressing.
    L=json.loads(LAYOUT.read_text(encoding='utf-8'))
    for row in L.get('quest_boards',[])+L.get('quest_givers',[]):
        x,y=row['at'];rects.append((x*100-150,y*100-150,x*100+150,y*100+150))
    return pp,rects

def _clear(A):
    for a in A.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:A.destroy_actor(a)

def _cobble(L,c,x,y,extra=2.0,ramp=5.0):
    """build_silverbrook.road_field's street cobble (G) at (x, y) m: 1 to the road edge + `extra`, then a one-ramp (grid step) fall."""
    best=0.
    for _,_,pts,w in _roads(L,c):
        for a,b in zip(pts,pts[1:]):
            dx,dy=b[0]-a[0],b[1]-a[1];t=max(0.,min(1.,((x-a[0])*dx+(y-a[1])*dy)/((dx*dx+dy*dy) or 1)))
            best=max(best,min(1.,max(0.,1-(math.hypot(x-a[0]-dx*t,y-a[1]-dy*t)-w/2-extra)/ramp)))
    return best

def _wet(A):
    sys.path.insert(0,str(ROOT.parent/'Greenlands'));import water_fx;return water_fx.wet_fn(A)   # nothing stands in a river, brook, basin or the sea

def dress_verge(L,c):
    world=_world();v=c['verge'];A=_actors();pp,rects=_obstacles(A);wet=_wet(A)
    meshes=[_asset(f'/Game/Frontier/Environment/Greenlands/Nature/Meshes/{n}') for n in v['meshes']];meshes=[m for m in meshes if m]
    for path in eal.list_assets(LIB+'/Foliage',recursive=False,include_folder=False):  # also clears types dropped from the list
        ft=u.load_asset(path)
        if isinstance(ft,u.FoliageType_InstancedStaticMesh) and ft.get_name().startswith('FT_SBVerge_'):u.InstancedFoliageActor.remove_all_instances(world,ft)
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
                if z is None or wet(px*100,py*100,z):continue
                ft,tx=types[int(_h(key+'t')*len(types))%len(types)]
                if 'Pebble' in ft.get_name() and _cobble(L,c,px,py)>=.5:continue   # WP6i review fix 8: field stones stay off the carriageway
                s=lo+(hi-lo)*_h(key+'s')
                tx.append(u.Transform(location=u.Vector(px*100,py*100,z),rotation=u.Rotator(yaw=_h(key+'y')*360),scale=u.Vector(s,s,s)))
    total=0
    for ft,tx in types:
        if tx:u.InstancedFoliageActor.add_instances(world,ft,tx);total+=len(tx)
    print('VERGE_INSTANCES',total);return total

def dress_props(L,c):
    world=_world();A=_actors();pp,rects=_obstacles(A);placed={};wet=_wet(A)
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
                    if z is None or wet(px,py,z):continue
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

def _fronts(A,folder='Districts/'):
    """World (x, y) cm of every Silverbrook building: street groups stand only where a frontage has one."""
    return [(a.get_actor_location().x,a.get_actor_location().y) for a in A.get_all_level_actors()
            if isinstance(a,u.StaticMeshActor) and str(a.get_folder_path()).startswith(folder) and a.static_mesh_component.static_mesh
            and a.static_mesh_component.static_mesh.get_name().startswith('SM_Silverbrook') and '_Door' not in a.static_mesh_component.static_mesh.get_name()]
def _has_frontage(fronts,x,y,ux,uy,side,along=1200,depth=1800):
    """A building within `along` cm of the station (x, y cm) on this side of the road and within `depth` cm of it."""
    for bx,by in fronts:
        dx,dy=bx-x,by-y
        if abs(ux*dx+uy*dy)<=along and 0<side*(ux*dy-uy*dx)<=depth:return True
    return False

def dress_street_groups(L,c):
    """Small, mixed Frontier prop groups just outside the walkable road."""
    A=_actors();world=_world();pp,rects=_obstacles(A);wet=_wet(A)
    import pcg_dressing
    _,terrain=pcg_dressing._terrain()
    g=c['street_groups'];step=val(g['spacing_m']);offset=val(g['offset_m']);clear=val(g['clear_m'])*100
    names=['RainBarrel','SackPile','FirewoodStack','Bench','MarketGoods','Wheelbarrow','HayBales','HitchingPost']
    meshes=[_asset(LIB+'/Meshes/SM_SilverbrookProp'+n) for n in names]
    assert all(meshes),names
    fronts=_fronts(A);ways=[tuple(w) for w in L.get('waystations',[])]
    total=0;road_counts={}
    # Each street gets its share of max_groups by length inside the wall, so the cap is not spent on the first streets.
    share={road:sum(1 for x,y,_,_ in _stations(pts,step) if _inside(L,c,x,y)) for road,tier,pts,width in _roads(L,c)}
    quota={road:math.ceil(val(g['max_groups'])*n/max(1,sum(share.values()))) for road,n in share.items()}
    for road,tier,pts,width in _roads(L,c):
        road_counts[road]=0
        for k,(x,y,ux,uy) in enumerate(_stations(pts,step)):
            key=f'group:{road}:{k}'
            if _h(key+'skip')<val(g['skip_chance']):continue                         # irregular spacing: about a third of the stations stay empty
            j=(_h(key+'jit')-.5)*step*.8;x,y=x+ux*j,y+uy*j                           # the rest slide up to +-40 % of a step
            way=any(math.hypot(x-wx,y-wy)<15 for wx,wy in ways)
            if total>=val(g['max_groups']) or road_counts[road]>=quota[road] or not (_inside(L,c,x,y) or way):continue
            sides=[s for s in (1,-1) if way or _has_frontage(fronts,x*100,y*100,ux,uy,s)]   # only a frontage with a building
            if not sides:continue
            side=sides[int(_h(key+'side')*len(sides))%len(sides)]                     # one side per station: never a mirrored pair
            mesh=meshes[int(_h(key)*len(meshes))%len(meshes)]
            for shift in (0,0.8):
                d=width/2+offset+shift
                px=(x-side*uy*d)*100;py=(y+side*ux*d)*100
                box=(px-clear,py-clear,px+clear,py+clear)
                if not pp._free(box,rects):continue
                hit=terrain.static_mesh_component.line_trace_component(u.Vector(px,py,100000),u.Vector(px,py,-100000),True,False)
                if not hit or wet(px,py,hit[0].z):continue
                z=hit[0].z
                yaw=math.degrees(math.atan2(uy,ux))-90
                label=f'StreetGroup_{road.replace(" ", "")}_{k:03d}_{"L" if side>0 else "R"}'
                a=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(px,py,z),u.Rotator(yaw=yaw))
                a.static_mesh_component.set_static_mesh(mesh)
                a.set_actor_label(label)
                a.set_folder_path('Silverbrook/Dressing');a.tags=[TAG]
                rects.append(box);total+=1;road_counts[road]+=1
                for j in range(1+int(_h(key+'n')*2)):  # one or two barrel/sack satellites make each group a cluster
                    m2=meshes[int(_h(key+str(j))*2)]
                    sx,sy=px+side*uy*(60+40*j),py-side*ux*(60+40*j)
                    sbox=(sx-30,sy-30,sx+30,sy+30)
                    if not pp._free(sbox,rects):continue
                    hit=terrain.static_mesh_component.line_trace_component(u.Vector(sx,sy,100000),u.Vector(sx,sy,-100000),True,False)
                    if not hit or wet(sx,sy,hit[0].z):continue
                    s=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(sx,sy,hit[0].z),u.Rotator(yaw=yaw+_h(key+'y'+str(j))*360))
                    s.static_mesh_component.set_static_mesh(m2)
                    s.set_actor_label(f'{label}_{j}');s.set_folder_path('Silverbrook/Dressing');s.tags=[TAG]
                    rects.append(sbox)
    need=sum(1 for road,tier,pts,width in _roads(L,c) for x,y,ux,uy in _stations(pts,50)
             if _inside(L,c,x,y) and any(_has_frontage(fronts,x*100,y*100,ux,uy,s) for s in (1,-1)))*3
    print('STREET_GROUPS',total,'GATE',need,road_counts)
    assert total>=need,('STREET_GROUPS below 3 per fronted 50 m',total,need)
    bare=[r for r,tier,pts,width in _roads(L,c) if road_counts[r]==0 and any(_inside(L,c,x,y) and any(_has_frontage(fronts,x*100,y*100,ux,uy,s) for s in (1,-1)) for x,y,ux,uy in _stations(pts,step))]
    assert not bare,('fronted streets left bare',bare)
    return road_counts

def _outside_houses(houses,q,band,pad=50):
    """True when world point q (cm) is outside every house's wall-base box grown by `pad` cm."""
    for o in houses:
        op=o.get_actor_location();oy=math.radians(o.get_actor_rotation().yaw);dx,dy=q[0]-op.x,q[1]-op.y
        lx,ly=math.cos(oy)*dx+math.sin(oy)*dy,-math.sin(oy)*dx+math.cos(oy)*dy
        if abs(lx)>3000 or abs(ly)>3000:continue
        x0,x1,y0,y1=_base_box(o.static_mesh_component.static_mesh,band)
        if x0-pad<lx<x1+pad and y0-pad<ly<y1+pad:return False
    return True

def dress_house_details(folder='Districts/'):
    """Fenced side gardens with crops and firewood, flower tubs at the door and on the stoop, chimney smoke plumes."""
    A=_actors();world=_world();import pcg_dressing
    _,terrain=_terrain()
    fence=_asset(LIB+'/Meshes/SM_SilverbrookPropFenceSection')
    tub=_asset(LIB+'/Meshes/SM_SilverbrookPropFlowerTub')  # oak half-barrel with geraniums (WP6c review fix 7)
    crop=_asset('/Game/Frontier/Environment/Greenlands/Nature/Meshes/SM_HerbBroadleaf_00')
    firewood=_asset(LIB+'/Meshes/SM_SilverbrookPropFirewoodStack')
    assert all((fence,tub,crop,firewood)),'import PropFlowerTub (import_silverbrook.py PropFlowerTub)'
    # Chimney smoke (WP6g review): one Niagara sprite system per smoking chimney, tuned through its User parameters.
    ns=_asset('/Game/Frontier/VFX/ChimneySmoke/NS_ChimneySmoke');assert ns,'author NS_ChimneySmoke (docs/blueprints/vfx.md section 2)'
    cs=cfg()['chimney_smoke']
    sign,line,ladder,planter,ivy=(_asset(LIB+'/Meshes/SM_SilverbrookProp'+n) for n in ('ShopSign','WashingLine','Ladder','SillPlanter','IvyClump'))
    assert all((sign,line,ladder,planter,ivy)),'import the five wall-dressing props (import_silverbrook.py PropShopSign ...)'
    boards=[_asset(f'{LIB}/Materials/MI_SB_PropShopSignBoard{v}') for v in ('','_Boots','_Ale')]
    blooms=[_asset(f'{LIB}/Materials/MI_SB_GeraniumBloom{v}') for v in ('','_Pink')]
    band=val(cfg()['footing']['base_band_cm'])
    # Bay-window sills (WP6d review fix 3), mesh-local cm traced on the imported meshes (the import recentres each mesh on its
    # bounds, so Blender coordinates are off by up to 26 cm): x = bay centre, y = outermost frame/sill face + 2 cm (the planter's
    # origin is the back edge of its trough), z = apron top / sill line.
    sills=_sills()
    houses=[x for x in A.get_all_level_actors() if isinstance(x,u.StaticMeshActor) and str(x.get_folder_path()).startswith(folder)
            and x.static_mesh_component.static_mesh and x.static_mesh_component.static_mesh.get_name().startswith('SM_Silverbrook')
            and '_Door' not in x.static_mesh_component.static_mesh.get_name() and 'static' not in [str(t) for t in x.tags]]  # static parts (street barrels) are not houses
    lined=set()
    stoops={x.get_actor_label():x for x in A.get_all_level_actors() if isinstance(x,u.StaticMeshActor) and x.get_actor_label().endswith('_Stoop0')}
    stacks={1:(2.2,1.5,8.85),2:(-2.05,0,8.7),3:(2.45,-.55,8.75),4:(-2.1,.3,8.65),5:(2.25,.55,8.7),6:(-2.15,1.2,8.55)}
    counts=dict(gardens=0,crops=0,pots=0,stoop_pots=0,firewood=0,smoking=0,signs=0,washing_lines=0,ladders=0,sill_planters=0,ivy=0)
    def spawn(label,mesh,x,y,z,yaw,scale):
        a=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x,y,z),u.Rotator(yaw=yaw))
        a.static_mesh_component.set_static_mesh(mesh)
        a.set_actor_scale3d(u.Vector(*scale))
        a.set_actor_label(label);a.set_folder_path('Silverbrook/Dressing');a.tags=[TAG]
        return a
    def smoke(label,x,y,z,yaw):
        a=A.spawn_actor_from_class(u.NiagaraActor,u.Vector(x,y,z),u.Rotator(yaw=yaw))
        c=a.get_editor_property('niagara_component');c.set_asset(ns)
        name=lambda k:'User.'+''.join(w.title() for w in k.split('_'))
        for k in ('spawn_rate','lifetime','opacity'):c.set_variable_float(name(k),float(val(cs[k])))
        for k in ('rise_speed','start_size','end_size'):c.set_variable_float(name(k),float(val(cs[k]))*100)   # m -> cm
        c.set_variable_vec3('User.Wind',u.Vector(*[w*100 for w in val(cs['wind'])]))
        c.set_variable_linear_color('User.Tint',u.LinearColor(*val(cs['tint']),1))
        a.set_actor_label(label);a.set_folder_path('Silverbrook/Dressing');a.tags=[TAG]
        return a
    for a in A.get_all_level_actors():
        if not isinstance(a,u.StaticMeshActor) or not str(a.get_folder_path()).startswith(folder):continue
        mesh=a.static_mesh_component.static_mesh
        if not mesh or not mesh.get_name().startswith('SM_Silverbrook') or '_Door' in mesh.get_name() or 'static' in [str(t) for t in a.tags]:continue
        label=a.get_actor_label();h=_h(label);b=mesh.get_bounding_box();p=a.get_actor_location()
        x0,x1,y0,y1=_base_box(mesh,band)
        yaw=a.get_actor_rotation().yaw;co,si=math.cos(math.radians(yaw)),math.sin(math.radians(yaw))
        def pt(lx,ly):return p.x+co*lx-si*ly,p.y+si*lx+co*ly
        def terrain_z(x,y):
            hit=terrain.static_mesh_component.line_trace_component(u.Vector(x,y,100000),u.Vector(x,y,-100000),True,False)
            return hit[0].z if hit else None
        for part in a.get_attached_actors():  # 'static' package parts (barrels, benches) stand on the street, not on the lifted house
            if 'static' in [str(t) for t in part.tags]:
                q=part.get_actor_location();z=terrain_z(q.x,q.y)
                if z is not None:part.set_actor_location(u.Vector(q.x,q.y,z),False,False)
        def face_y(lx,lz):
            """Local y of the street (+Y) face at (lx, height lz): bounds include eaves and bays, so trace the mesh."""
            x0,y0=pt(lx,b.max.y+300);x1,y1=pt(lx,0)
            hit=a.static_mesh_component.line_trace_component(u.Vector(x0,y0,p.z+lz),u.Vector(x1,y1,p.z+lz),True,False)
            return -si*(hit[0].x-p.x)+co*(hit[0].y-p.y) if hit else b.max.y
        if h<.75:
            side=-1 if _h(label+'side')<.5 else 1
            sx=side*((b.max.x-b.min.x)*.42+140)
            run=(fence.get_bounding_box().max.x-fence.get_bounding_box().min.x)*.82/2  # half a section along its run (X)
            # An L of three sections: two along the house axis (X) and one turned 90 degrees at the far corner.
            for i,(fx,fy,fyaw) in enumerate(((sx,0,0),(sx+side*160,0,0),(sx+side*(160+run),run,90))):
                x,y=pt(fx,fy);z=terrain_z(x,y)
                if z is not None:spawn(f'GardenFence_{label}_{i}',fence,x,y,z,yaw+fyaw,(.82,.82,.72))
            for i in range(10):
                cx,cy=pt(sx+side*(55+(i%2)*85),25+(i//2)*25)  # inside the fence L (sections along y 0, return toward +y)
                cz=terrain_z(cx,cy)
                if cz is not None:
                    spawn(f'GardenCrop_{label}_{i}',crop,cx,cy,cz,yaw,(1.15,1.15,1.15));counts['crops']+=1
            if h<.5:  # firewood at the rear corner on the other side
                x,y=pt(-side*((b.max.x-b.min.x)/2+60),b.min.y+60);z=terrain_z(x,y)  # the street face is local +Y
                if z is not None:spawn('GardenFirewood_'+label,firewood,x,y,z,yaw+90,(.8,.8,.8));counts['firewood']+=1
            counts['gardens']+=1
        if h<.5:
            side=-1 if _h(label+'pot')<.5 else 1
            lx=side*(b.max.x-b.min.x)*.35;x,y=pt(lx,face_y(lx,40)+45);z=terrain_z(x,y)  # beside the door on the street face (+Y)
            if z is not None:
                t_=spawn('FlowerTub_'+label,tub,x,y,z,yaw,(1,1,1));counts['pots']+=1
                t_.static_mesh_component.set_material_by_name('SB_GeraniumBloom',blooms[int(_h(label+'bloom')*2)%2])
            st=stoops.get(label+'_Stoop0')
            if st:  # second tub on the top step, off to one side of the tread
                c0,ext=st.get_actor_bounds(False);r=st.get_actor_rotation().yaw;sc=st.get_actor_scale3d()
                ang=math.radians(r if sc.x>sc.y else r+90);off=max(sc.x,sc.y)*50-25
                x,y=c0.x+math.cos(ang)*off*side,c0.y+math.sin(ang)*off*side;z=c0.z+ext.z
                t_=spawn('StoopTub_'+label,tub,x,y,z,yaw,(.75,.75,.75));counts['stoop_pots']+=1  # 0.42 m: fits the 0.4 m tread
                t_.static_mesh_component.set_material_by_name('SB_GeraniumBloom',blooms[int(_h(label+'stoopbloom')*2)%2])
        for lx,ly,lz in sills.get(mesh.get_name(),[]):  # a planter on every bay-window sill (WP6c review fix 6)
            x,y=pt(lx,ly);pl=spawn(f'SillPlanter_{label}_{lz}',planter,x,y,p.z+lz,yaw,(1,1,1))
            pl.static_mesh_component.set_material(0,blooms[int(_h(label+'bloom'+str(lz))*2)%2]);counts['sill_planters']+=1
            fx,fy=pt(lx,ly+150)  # a hit means the house wall stands between the street and the planter
            assert not a.static_mesh_component.line_trace_component(u.Vector(fx,fy,p.z+lz+15),u.Vector(x,y,p.z+lz+15),True,False),('sill planter inside the wall',label,lz)
        baked_sign=any('SignBoard' in str(s.material_slot_name) for s in mesh.static_materials)  # buildings.md §5: never both signs on one building
        if _h(label+'wall')<.4 or folder!='Districts/':  # a dev-map review always has a sign view
            doors=[d for d in a.get_attached_actors() if 'Door' in d.get_actor_label()]
            dlx=(co*(doors[0].get_actor_location().x-p.x)+si*(doors[0].get_actor_location().y-p.y)) if doors else 0
            lx=dlx+(110 if dlx<0 else -110)  # shop sign beside the door, its bracket 2.6 m up the street face
            x,y=pt(lx,face_y(lx,260))
            if not baked_sign:
                s_=spawn('ShopSign_'+label,sign,x,y,p.z+260,yaw,(1,1,1))
                s_.static_mesh_component.set_material(1,boards[int(_h(label+'board')*3)%3]);counts['signs']+=1
            side=-1 if _h(label+'ladder')<.5 else 1  # ladder leaning at 75 degrees against a side wall, at mid depth
            wx=x1 if side>0 else x0;foot=320*math.sin(math.radians(15))+10
            x,y=pt(wx+side*foot,(y0+y1)/2);z=terrain_z(x,y)
            if z is not None:
                ld=spawn('Ladder_'+label,ladder,x,y,z,yaw+90,(1,1,1))
                for roll in (15,-15):
                    ld.set_actor_rotation(u.Rotator(roll=roll,yaw=yaw+90),False)
                    c,_=ld.get_actor_bounds(False)
                    if (co*(c.x-x)+si*(c.y-y))*side<0:break  # top leans toward the wall
                counts['ladders']+=1
            cx=x0 if _h(label+'ivy')<.5 else x1  # ivy clump at a front plinth corner
            x,y=pt(cx+(10 if cx>0 else -10),y1+10);z=terrain_z(x,y)
            if z is not None:spawn('IvyClump_'+label,ivy,x,y,z,yaw+45,(1,1,1));counts['ivy']+=1
        if _h(label+'line')<.5:  # washing line on half the houses (WP6c review fix 5)
            gside=(-1 if _h(label+'side')<.5 else 1) if h<.75 else 0   # this house's garden side
            spot=None
            for o in houses:  # a free gap to a same-row neighbour: lots are 5.1 m apart, so the line (3.7 m) scales to the gap
                if o==a:continue
                op=o.get_actor_location();dx,dy=op.x-p.x,op.y-p.y;ox,oy=co*dx+si*dy,-si*dx+co*dy
                if abs(oy)>200 or abs(((o.get_actor_rotation().yaw-yaw)+180)%360-180)>15:continue
                toward=1 if ox>0 else -1;ol=o.get_actor_label()
                if toward==gside or ol in lined:continue                                   # our garden fills that gap
                if _h(ol)<.75 and (-1 if _h(ol+'side')<.5 else 1)==-toward:continue        # the neighbour's garden fills it
                ob=_base_box(o.static_mesh_component.static_mesh,band)
                gap=(ox+ob[0]-x1) if ox>0 else (x0-(ox+ob[1]))
                if not 300<=gap<=800:continue
                gx=(x1+ox+ob[0])/2 if ox>0 else (x0+ox+ob[1])/2
                spot=(gx,(y0+y1)/2,yaw,max(.8,min(1.3,(gap-100)/370)));lined.add(ol);break
            if spot is None:spot=(-gside*100,y0-230,yaw,1.0)   # rear yard on its own post pair, parallel to the back wall
            gx,gy,lyaw,sx_=spot;x,y=pt(gx,gy);z=terrain_z(x,y)
            # Posts and mid-point clear of every house's own wall base (+50 cm): world AABBs of turned houses cover the gaps.
            ends=[pt(gx+d*185*sx_,gy) for d in (-1,0,1)]
            if z is not None and all(_outside_houses(houses,q,band) for q in ends):
                spawn('WashingLine_'+label,line,x,y,z,lyaw,(sx_,1,1));lined.add(label);counts['washing_lines']+=1
        if mesh.get_name().startswith('SM_SilverbrookTownhouse') and h<.4:
            number=int(mesh.get_name().removeprefix('SM_SilverbrookTownhouse'))
            sx,sy,sz=stacks[number];x,y=pt(sx*100,sy*100)
            smoke(f'ChimneySmoke_{label}',x,y,p.z+sz*100+20,yaw)   # pot top + 20 cm
            counts['smoking']+=1
    print('HOUSE_DETAILS',counts)
    return counts

def vary_house_finishes():
    """Five shared roof/plaster tones (>= 20 % luminance spread) assigned deterministically per house."""
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
    palettes={'MI_SB_LimePlaster':[(1.00,.93,.80),(.84,.86,.88),(1.00,.86,.66),(.93,.95,.84),(.78,.74,.70)],
              'MI_SB_Slate':[(.78,.86,1.02),(1.06,.90,.74),(.86,.98,.88),(.66,.68,.72),(1.00,1.00,1.00)]}
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
        if not mesh or not mesh.get_name().startswith('SM_Silverbrook') or '_Door' in mesh.get_name() or 'static' in [str(t) for t in a.tags]:continue
        i=int(_h(a.get_actor_label()+'tone')*5)%5
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
def _base_cm():
    """Opt-in wall-base boxes from package manifests (`base_cm`, mesh-local cm): for buildings whose baked street props
    (barrels, benches under an overhang) would otherwise become the lowest-band edge."""
    if '_' not in _BASE_CM:_BASE_CM['_']={m['unreal_mesh']:m['base_cm'] for p in ROOT.glob('*/asset-manifest.json')
                                           for m in [json.loads(p.read_text(encoding='utf-8'))] if 'base_cm' in m}
    return _BASE_CM['_']
_BASE_CM={}
def _footings_cm():
    """Opt-in extra footing boxes per mesh from package manifests (`footings_cm`, mesh-local cm [[x0,x1,y0,y1], ...])."""
    if 'f' not in _BASE_CM:_BASE_CM['f']={m['unreal_mesh']:m['footings_cm'] for p in ROOT.glob('*/asset-manifest.json')
                                           for m in [json.loads(p.read_text(encoding='utf-8'))] if 'footings_cm' in m}
    return _BASE_CM['f']
def _base_box(mesh,band):
    """(xmin,xmax,ymin,ymax) in mesh space (cm) of the vertices within `band` cm of the mesh's lowest point,
    or the manifest `base_cm` when the package declares one."""
    key=(mesh.get_name(),band)
    if key not in _BASE and mesh.get_name() in _base_cm():_BASE[key]=tuple(_base_cm()[mesh.get_name()])
    if key not in _BASE:
        dm=u.DynamicMesh()
        dm,out=u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(mesh,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD())
        assert out==u.GeometryScriptOutcomePins.SUCCESS,mesh.get_name()
        pts=u.GeometryScript_List.convert_vector_list_to_array(u.GeometryScript_MeshQueries.get_all_vertex_positions(dm,False)[1])
        low=min(p.z for p in pts);near=[p for p in pts if p.z<=low+band]
        _BASE[key]=(_edge(sorted(p.x for p in near),True),_edge(sorted(p.x for p in near),False),
                    _edge(sorted(p.y for p in near),True),_edge(sorted(p.y for p in near),False))
    return _BASE[key]

_SHELL={}
def _wall_shell(a,step=20.):
    """Ground-floor wall shell of a placed house, from horizontal traces against its own mesh (owner: the lowest-band box
    covered only the stoop/porch strip at the front and the rest of the house floated). Returns mesh-space cm
    (x0,x1,y0,y1,bottom,stoops): per side the outermost wall plane hit by >= 30 % of the rays at 1.0-1.8 m above the mesh's lowest
    point (bays, stoops, sills, posts and signs are minorities; the foot-level base course up to 15 cm proud is included; rays through doors and windows that reach the far inside are
    dropped), `bottom` = the lowest wall foot along those planes, found by stepping the hitting rays down 2 cm at a time, and
    `stoops` = [(side, lateral from, lateral to, outer depth)] runs of the mesh's own foot-level steps/porch outside the walls.
    Cached per mesh and scale; the actor's yaw and position do not change the result."""
    c=a.static_mesh_component;m=c.static_mesh;s=a.get_actor_scale3d()
    key=(m.get_name(),round(s.x,3),round(s.y,3),round(s.z,3))
    if key in _SHELL:return _SHELL[key]
    b=m.get_bounding_box();T=a.get_actor_transform()
    W=lambda q:T.transform_location(u.Vector(*q));L=lambda q:T.inverse_transform_location(q)
    def ray(ax,sg,t,z):  # ray from outside the bounds toward the centre line; returns the hit depth along `ax` or None
        out=(b.max if sg>0 else b.min).x if ax==0 else (b.max if sg>0 else b.min).y
        p0=[0,0,z];p1=[0,0,z];p0[ax]=out+sg*100;p0[1-ax]=t;p1[1-ax]=t
        h=c.line_trace_component(W(p0),W(p1),True,False)
        return None if not h else (L(h[0]).x if ax==0 else L(h[0]).y)
    low=b.min.z;planes={};bottoms=[];stoops=[]
    for side,(ax,sg) in {'x0':(0,-1),'x1':(0,1),'y0':(1,-1),'y1':(1,1)}.items():
        lo,hi=((b.min.y,b.max.y) if ax==0 else (b.min.x,b.max.x));out=(b.max if sg>0 else b.min).x if ax==0 else (b.max if sg>0 else b.min).y
        hits=[]
        for z in (low+100,low+140,low+180):
            t=lo+step/2
            while t<hi:
                d=ray(ax,sg,t,z)
                if d is not None and abs(out-d)<=150:hits.append((d,t))
                t+=step
        assert hits,(key,side)
        ds=sorted(hits,key=lambda h:-sg*h[0]);groups=[[ds[0]]]
        for h in ds[1:]:
            (groups[-1].append(h) if abs(h[0]-groups[-1][-1][0])<=12 else groups.append([h]))
        g=next(g for g in groups if len(g)>=.3*len(hits))   # main wall: a canted bay or porch is < 30 % of a side
        plane=max(sg*h[0] for h in g)*sg
        # The stone base course at the wall foot stands a few cm proud of the wall: take the outermost foot-level hit
        # within 15 cm outside the wall plane (stoops and steps stand further out and are skipped).
        for z in (low+4,low+16,low+30):
            t=lo+step/4
            while t<hi:   # at a quarter of the step: a narrow chimney-breast foot is easy to miss
                d=ray(ax,sg,t,z)
                if d is not None and 0<sg*(d-plane)<=15:plane=d
                t+=step/2
        planes[side]=plane
        # The mesh's own doorstep / porch: foot-level geometry 15-150 cm out of the wall, grouped into runs along the side.
        t=lo+step/2;run=None
        while t<hi+step:
            ds_=[q for q in (ray(ax,sg,t,low+4),ray(ax,sg,t,low+10)) if q is not None] if t<hi else []
            d=max(ds_,key=lambda q:sg*q) if ds_ else None
            if d is not None and 15<sg*(d-plane)<=150:
                run=[t,t,d] if run is None else [run[0],t,d if sg*(d-run[2])>0 else run[2]]
            elif run is not None:
                stoops.append((side,run[0]-step/2,run[1]+step/2,run[2]));run=None
            t+=step
        for d,t in g[::3]:   # wall foot: step the ray down while it still hits this plane
            z=low+100
            while z>low-2 and (lambda d2:d2 is not None and abs(d2-d)<=8)(ray(ax,sg,t,z-2)):z-=2
            bottoms.append(z)
    _SHELL[key]=(planes['x0'],planes['x1'],planes['y0'],planes['y1'],min(bottoms),stoops)
    return _SHELL[key]

def dress_footings(c, folder='Districts/'):
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
    _,terrain=_terrain()
    A=_actors();actors=A.get_all_level_actors();by_label={a.get_actor_label():a for a in actors}
    cube=_asset('/Engine/BasicShapes/Cube');n=0;created=0
    def ground(x,y):
        hit=terrain.static_mesh_component.line_trace_component(u.Vector(x,y,100000),u.Vector(x,y,-100000),True,False)
        assert hit,(x,y)
        return hit[0].z
    for building in actors:
        if not isinstance(building,u.StaticMeshActor) or not str(building.get_folder_path()).startswith(folder):
            continue
        mesh=building.static_mesh_component.static_mesh
        if not mesh or not mesh.get_name().startswith('SM_Silverbrook') or '_Door' in mesh.get_name() or 'static' in [str(t) for t in building.tags]:continue
        b=mesh.get_bounding_box();s=building.get_actor_scale3d();loc=building.get_actor_location()
        # Footprint = the ground-floor wall shell traced on the house's own mesh, plus a few cm of proud edge. (Whole-mesh
        # bounds include eaves, bays and signs, which left one-sided ledges; the lowest-band box covered only the front strip.)
        x0,x1,y0,y1,_,stoop_runs=_wall_shell(building)
        pad=val(f['proud_cm'])
        w=(x1-x0+2*pad)*s.x;d=(y1-y0+2*pad)*s.y;cx=(x0+x1)/2*s.x;cy=(y0+y1)/2*s.y
        yaw=math.radians(building.get_actor_rotation().yaw);co,si=math.cos(yaw),math.sin(yaw)
        px=loc.x+co*cx-si*cy;py=loc.y+si*cx+co*cy
        # Terrain under the whole footprint, every 50 cm along its edge and on a 2 m grid inside (corners alone missed dips).
        nx,ny=max(1,math.ceil(w/50)),max(1,math.ceil(d/50))
        edge=[(-w/2+w*i/nx,sy_) for i in range(nx+1) for sy_ in (-d/2,d/2)]+[(sx_,-d/2+d*j/ny) for j in range(ny+1) for sx_ in (-w/2,w/2)]
        grid=[(-w/2+w*i/max(1,math.ceil(w/200)),-d/2+d*j/max(1,math.ceil(d/200))) for i in range(math.ceil(w/200)+1) for j in range(math.ceil(d/200)+1)]
        heights=[ground(px+co*x-si*y,py+si*x+co*y) for x,y in edge+grid]
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
        bottom=min(heights)-val(f['burial_cm'])-2   # 2 cm: terrain between the 50 cm samples can sit a little lower
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
        # Opt-in extra footings (manifest footings_cm, mesh-local cm boxes): wall-base parts outside base_cm, e.g. an external chimney.
        for k,(fx0,fx1,fy0,fy1) in enumerate(_footings_cm().get(mesh.get_name(),[])):
            lab=f'{building.get_actor_label()}_Footing{k}';ft=by_label.get(lab)
            fcx,fcy=(fx0+fx1)/2*s.x,(fy0+fy1)/2*s.y;fx,fy=loc.x+co*fcx-si*fcy,loc.y+si*fcx+co*fcy
            if not ft:
                ft=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(fx,fy,0),building.get_actor_rotation())
                ft.static_mesh_component.set_static_mesh(cube);ft.set_actor_label(lab);ft.set_folder_path(building.get_folder_path())
            fb=ground(fx,fy)-val(f['burial_cm'])
            ft.set_actor_location(u.Vector(fx,fy,(fb+wall_base)/2),False,False);ft.set_actor_rotation(building.get_actor_rotation(),False)
            ft.set_actor_scale3d(u.Vector((fx1-fx0)*s.x/100,(fy1-fy0)*s.y/100,(wall_base-fb)/100));ft.static_mesh_component.set_material(0,m)
        # Stoop: equal low risers from the doorstep down to the street, so the raised doorway stays walkable.
        for old in [x for x in by_label.values() if x.get_actor_label().startswith(building.get_actor_label()+'_Stoop')]:A.destroy_actor(old)
        # The mesh's own doorsteps/porch outside the walls stand on stone too (owner: floating houses), same top and burial.
        beyond={}   # per side: how far the stone base reaches past the plinth face, so the risers start in front of it
        for k,(side,t0,t1,dout) in enumerate(stoop_runs):
            ax_=0 if side[0]=='x' else 1;sg_=1 if side[1]=='1' else -1
            plane={'x0':x0,'x1':x1,'y0':y0,'y1':y1}[side]
            a0,a1=sorted((plane,dout+sg_*pad));b0,b1=t0-pad,t1+pad
            lx_,ly_=((a0+a1)/2,(b0+b1)/2) if ax_==0 else ((b0+b1)/2,(a0+a1)/2)
            sx_,sy_=((a1-a0),(b1-b0)) if ax_==0 else ((b1-b0),(a1-a0))
            wx,wy=loc.x+co*lx_*s.x-si*ly_*s.y,loc.y+si*lx_*s.x+co*ly_*s.y
            under=[ground(loc.x+co*qx*s.x-si*qy*s.y,loc.y+si*qx*s.x+co*qy*s.y) for qx in (lx_-sx_/2,lx_,lx_+sx_/2) for qy in (ly_-sy_/2,ly_,ly_+sy_/2)]
            low_=min(under)-val(f['burial_cm'])-2
            sb=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(wx,wy,(low_+wall_base)/2),building.get_actor_rotation())
            sb.static_mesh_component.set_static_mesh(cube);sb.static_mesh_component.set_material(0,m)
            sb.set_actor_scale3d(u.Vector(sx_*s.x/100,sy_*s.y/100,(wall_base-low_)/100))
            sb.set_actor_label(f'{building.get_actor_label()}_StoopBase{k}');sb.set_folder_path(building.get_folder_path())
            key_=(ax_,sg_);beyond[key_]=max(beyond.get(key_,0),abs(dout+sg_*pad-plane-sg_*pad)*(s.x if ax_==0 else s.y))
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
                off=face+beyond.get((0 if ax else 1,ax or ay),0)+20+40*k;cxl,cyl=(ax*off+(0 if ax else lat),ay*off+(lat if ax else 0))
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
        # Every graybox authoring box in Places/Plots (markers, waystone, yards, ruins, plots), whatever its colour (WP6c fix 2).
        marker=isinstance(a,u.StaticMeshActor) and str(a.get_folder_path()) in ("Places","Plots") and a.static_mesh_component.get_material(0) is not None and a.static_mesh_component.get_material(0).get_name().startswith("MI_Graybox")
        if marker or a.get_class().get_name() in ("TextRenderActor","ZoneShape","ZoneGraphData") or str(a.get_folder_path()) in ("Labels","Townsfolk/Lanes"):
            a.set_actor_hidden_in_game(True);n+=1
    print("DEBUG_HIDDEN_IN_GAME",n)

def dress_all(folder='Districts/'):
    c=cfg();L=json.loads(LAYOUT.read_text(encoding='utf-8'));_clear(_actors())
    dress_footings(c,folder)
    if folder!='Districts/':
        dress_house_details(folder)
        assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
        return
    hide_debug_actors();vary_house_finishes();dress_roads(c);gate_windows(c);dress_verge(L,c);dress_props(L,c);dress_house_details();dress_street_groups(L,c)  # groups keep clear of tubs (WP6d fix 4a)
    # Lamp lights and flame cards last, on the glass where dress_footings left the houses (owner: lights from nowhere).
    sys.path.insert(0,str(ROOT.parent/'Greenlands'));import lamp_fx,importlib;importlib.reload(lamp_fx)
    if lamp_fx.ZONE in _world().get_path_name():lamp_fx.apply()   # build_silverbrook.py runs dress_all in its unsaved new world and calls lamp_fx.apply() itself after saving

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('--folder',default='Districts/');parser.add_argument('--terrain')
    args=parser.parse_args();TERRAIN_LABEL=args.terrain
    dress_all(args.folder)
