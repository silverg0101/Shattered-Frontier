"""Check the actual saved world's door and arrival-road capsule clearance."""
import unreal as u
import math,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import height,ROUTE
MAP='/Game/Frontier/Maps/Test/Greenlands_ArtReview'
assert '-game' in u.SystemLibrary.get_command_line(),'Run in the live preview; commandlet editor worlds have no active physics scene'
world=u.load_asset(MAP)
checks=[]
def sweep(label,a,b,z_offset):
    start=u.Vector(a[0]*100,a[1]*100,(height(*a)+z_offset)*100)
    end=u.Vector(b[0]*100,b[1]*100,(height(*b)+z_offset)*100)
    result=u.SystemLibrary.capsule_trace_single(world,start,end,42,88,u.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[],u.DrawDebugTrace.NONE,True)
    if isinstance(result,tuple):
        flags=[v for v in result if isinstance(v,bool)]
        assert flags,repr(result)
        blocked=flags[-1]
    elif result is None:blocked=False
    elif isinstance(result,u.HitResult):blocked=True
    else:raise TypeError(repr(result))
    checks.append(dict(label=label,start=a,end=b,blocked=blocked,hit=str(result) if blocked else None))
# A known solid wall must block the same query; otherwise clear sweeps prove nothing.
sweep('Positive control - solid house wall',(-103,-62),(-105,-62),1.16)
assert checks.pop()['blocked'],'Physics positive control did not block'
for i,(cx,cy,yaw) in enumerate([(-107,-59,90),(-77,-55,180),(-109,-23,90),(-68,-24,0)]):
    a=math.radians(yaw)
    def point(y):return (cx-y*math.sin(a),cy+y*math.cos(a))
    sweep('House %d entry'%(i+1),point(-5),point(-1),1.16)
for segment,((ax,ay),(bx,by)) in enumerate(zip(ROUTE[:2],ROUTE[1:3])):
    n=math.ceil(math.dist((ax,ay),(bx,by))/2)
    for i in range(n):
        a=(ax+(bx-ax)*i/n,ay+(by-ay)*i/n);b=(ax+(bx-ax)*(i+1)/n,ay+(by-ay)*(i+1)/n)
        if math.dist(a,(-185,-85))<8 or math.dist(b,(-90,-40))<8:continue
        sweep('Arrival road %d/%d'%(segment,i),a,b,1.35)
report=dict(capsule_radius_cm=42,capsule_half_height_cm=88,checks=checks,
            scope='Saved-world clearance sweeps for four doorways and the arrival road; not a full route gameplay or network test')
(ROOT/'village-clearance.json').write_text(json.dumps(report,indent=2))
assert not any(c['blocked'] for c in checks),[c for c in checks if c['blocked']]
print('VILLAGE_CLEARANCE',len(checks),'clear')
