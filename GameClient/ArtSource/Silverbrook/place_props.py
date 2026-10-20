"""Place Silverbrook street props (Prop*/ packages) around the placed buildings of the open level.

python tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/place_props.py   (re-run replaces earlier props)

Each prop manifest's `placement` names the building kinds it belongs near (`near`), its footprint and clearance.
For every building actor of a matching kind, candidate spots along its sides and front corners (never in front
of the doorway) are tried in a stable per-building order; a prop lands on the first spot whose footprint plus
clearance is free of other actors and has ground under it, facing away from the wall. Props are tagged
'SilverbrookProp' in folder Silverbrook/Props. build_silverbrook.py can call place_all() after spawning buildings.
"""
import unreal as u,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent;TAG='SilverbrookProp';LIB='/Game/Frontier/Environment/Greenlands/Silverbrook/Meshes'
# Building mesh name -> place kinds a prop's `near` list may name.
KINDS={'Cottage':['cottage','home'],'Townhouse':['townhouse','home','road'],'NorthridgeHouse':['northridge','home'],'Farmhouse':['farmhouse','farm','home'],
       'Stables':['stables','farm'],'Granary':['granary','farm'],'Mill':['mill','farm'],'Blacksmith':['blacksmith','workshop'],'Workshop':['workshop'],
       'TownHall':['square','townhall'],'AdventurersGuild':['square','guild'],'GoldenStagInn':['square','inn'],'TempleOfTheDawn':['square','temple'],
       'MarketStall':['market'],'ArmorVendor':['market','armor vendor'],'WeaponsVendor':['market','weapons vendor'],'FishermansWharf':['wharf','river'],'RiversideDocks':['docks','river'],
       'Watchtower':['watchtower','road'],'Gatehouse':['gate','gatehouse','road']}
PER_BUILDING=2       # props per building at most
SHARE=.55            # chance a matching building gets a given prop (stable per building/prop)
CAP={'Well':4,'Signpost':6,'NoticeBoard':4,'HayCart':6,'MarketGoods':8,'Grindstone':4}  # town-wide limits; others DEFAULT_CAP
DEFAULT_CAP=14

def _h(s):  # stable 0..999; md5, not crc32 (crc is linear, so same-length labels ranked props identically)
    return int(hashlib.md5(s.encode()).hexdigest()[:8],16)%1000

def _props():
    out=[]
    for f in sorted(ROOT.glob('Prop*/asset-manifest.json')):
        m=json.loads(f.read_text());p=m.get('placement')
        if not p:continue
        mesh=u.load_asset(f"{LIB}/{m['unreal_mesh']}")
        if mesh:out.append(dict(name=m['unreal_mesh'],mesh=mesh,near=set(p.get('near',[])),fp=p.get('footprint_m',[1,1]),clear=p.get('clearance_m',.5)))
    return out

def _kind(mesh_name):
    base=mesh_name.replace('SM_Silverbrook','').rstrip('0123456789')
    return KINDS.get(base)

def _rect(a):
    o,e=a.get_actor_bounds(False);return (o.x-e.x,o.y-e.y,o.x+e.x,o.y+e.y)

def _free(r,rects):
    return not any(r[0]<q[2] and r[2]>q[0] and r[1]<q[3] and r[3]>q[1] for q in rects)

def _ground(world,x,y,z):
    res=u.SystemLibrary.line_trace_single(world,u.Vector(x,y,z+800),u.Vector(x,y,z-1500),u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE,True)
    t=res.to_tuple() if res else None
    return t[5].z if t and t[0] else None  # blocking_hit, impact_point

def place_all():
    A=u.get_editor_subsystem(u.EditorActorSubsystem);world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    actors=A.get_all_level_actors()
    for a in actors:
        if TAG in [str(t) for t in a.tags]:A.destroy_actor(a)
    actors=[a for a in A.get_all_level_actors() if isinstance(a,u.StaticMeshActor) and a.static_mesh_component.static_mesh]
    rects=[];buildings=[]
    for a in actors:
        r=_rect(a)
        if max(r[2]-r[0],r[3]-r[1])>4000:continue            # terrain, backdrop, roads: not obstacles
        rects.append(r);k=_kind(a.static_mesh_component.static_mesh.get_name())
        if k:buildings.append((a,k))
    props=_props();placed=0;used={}
    for b,kinds in sorted(buildings,key=lambda t:_h(t[0].get_actor_label()+'b')):  # hashed order spreads capped props over the town
        xf=b.get_actor_transform();loc=b.get_actor_location();yaw=b.get_actor_rotation().yaw;own=_rect(b)
        o,e=b.get_actor_bounds(False);half=max(e.x,e.y)
        mine=[p for p in props if p['near']&set(kinds) and _h(b.get_actor_label()+p['name'])<SHARE*1000 and used.get(p['name'],0)<CAP.get(p['name'].replace('SM_SilverbrookProp',''),DEFAULT_CAP)]
        mine.sort(key=lambda p:_h(b.get_actor_label()+p['name']+'o'));n=0
        for p in mine:
            if n>=PER_BUILDING:break
            if used.get(p['name'],0)>=CAP.get(p['name'].replace('SM_SilverbrookProp',''),DEFAULT_CAP):continue
            w,d=p['fp'][0]*100,p['fp'][1]*100;gap=p['clear']*100+60
            # local frame: +Y is the building's front (FBX flip), sides at +-X; doorway zone |x| < 180 on the front is skipped
            mesh_box=b.static_mesh_component.static_mesh.get_bounding_box();hx=(mesh_box.max.x-mesh_box.min.x)/2;hy=(mesh_box.max.y-mesh_box.min.y)/2
            # yaw t turns local +Y (prop front) to (-sin t, cos t): -90 faces +X, 90 faces -X, 0 front, 180 back
            spots=[(hx+d/2+gap,0,-90),(-hx-d/2-gap,0,90),(hx*.7,hy+d/2+gap,0),(-hx*.7,hy+d/2+gap,0),(hx+d/2+gap,-hy*.6,-90),(-hx-d/2-gap,-hy*.6,90),(0,-hy-d/2-gap,180)]
            start=_h(b.get_actor_label()+p['name']+'s')%len(spots)
            for i in range(len(spots)):
                lx,ly,face=spots[(start+i)%len(spots)]
                c=xf.transform_location(u.Vector(lx,ly,0));z=_ground(world,c.x,c.y,loc.z)
                if z is None:continue
                rad=max(w,d)/2+p['clear']*100;r=(c.x-rad,c.y-rad,c.x+rad,c.y+rad)
                if not _free(r,[q for q in rects if q!=own]):continue  # own AABB swells when rotated; spots are already outside the mesh
                a=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(c.x,c.y,z),u.Rotator(roll=0,pitch=0,yaw=yaw+face))
                a.static_mesh_component.set_static_mesh(p['mesh']);a.tags=[TAG];a.set_folder_path('Silverbrook/Props')
                a.set_actor_label(f"{b.get_actor_label()}_{p['name'].replace('SM_SilverbrookProp','')}")
                rects.append(_rect(a));placed+=1;n+=1;used[p['name']]=used.get(p['name'],0)+1;break
    return placed

if __name__=='__main__':
    print('SILVERBROOK_PROPS',place_all())
