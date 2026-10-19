"""Seeded ecological patches and deliberately placed garden beds, in metres."""
from dataclasses import dataclass
from pathlib import Path
import json,math,random
from region_layout import AREAS,road_distance,FOOTPATHS

@dataclass(frozen=True)
class PlantingSettings:
    seed:int
    bounds_metres:list
    wild_patch_count:int
    patch_radius_metres:list
    wild_spacing_metres:float
    road_clearance_metres:float
    flower_height_metres:list
    blooming_share:float
    bloom_height_metres:list
    garden_bloom_height_metres:list
    meadow_bounds_metres:list
    meadow_spacing_metres:float
    meadow_height_metres:list
    garden_spacing_metres:float
    garden_size_metres:list
    cottages_metres_yaw:list
    featured_wild_patches_metres:list

def load_settings():
    s=PlantingSettings(**json.loads((Path(__file__).parent/'planting-settings.json').read_text()))
    assert 0<=s.seed<2**32 and 0<s.wild_patch_count<=150
    assert .35<=s.wild_spacing_metres<=2 and .2<=s.garden_spacing_metres<=1
    assert 2.8<=s.road_clearance_metres<=5 and .1<=s.flower_height_metres[0]<=s.flower_height_metres[1]<=1.2
    assert 0<=s.blooming_share<=1
    for bounds in [s.bloom_height_metres,s.garden_bloom_height_metres]:assert .2<=bounds[0]<=bounds[1]<=.65
    assert .45<=s.meadow_spacing_metres<=1.5 and .1<=s.meadow_height_metres[0]<=s.meadow_height_metres[1]<=.5
    assert len(s.meadow_bounds_metres)==4 and s.meadow_bounds_metres[0]<s.meadow_bounds_metres[2] and s.meadow_bounds_metres[1]<s.meadow_bounds_metres[3]
    return s

def local_point(cx,cy,yaw,x,y):
    a=math.radians(yaw);return cx+x*math.cos(a)-y*math.sin(a),cy+x*math.sin(a)+y*math.cos(a)

def allowed(x,y,s,garden=False):
    if road_distance(x,y)<s.road_clearance_metres:return False
    for points in FOOTPATHS.values():
        for (ax,ay),(bx,by) in zip(points,points[1:]):
            dx,dy=bx-ax,by-ay;t=max(0,min(1,((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy)))
            if math.hypot(x-ax-t*dx,y-ay-t*dy)<1.3:return False
    for name,area in AREAS.items():
        cx,cy=area['center']
        if name.startswith('Build_') and abs(x-cx)<12 and abs(y-cy)<12:return False
        if name in ['Arrival','Enemy_Clearing','Stone_Outcrop'] and math.dist((x,y),(cx,cy))<area['radius']:return False
    if math.dist((x,y),(-90,-40))<14:return False
    for cx,cy,yaw in s.cottages_metres_yaw:
        lx,ly=local_point(0,0,-yaw,x-cx,y-cy)
        if abs(lx)<4.6 and abs(ly)<3.6:return False
        if abs(lx)<1.3 and -6.7<ly<-2.9:return False
    return True

def generate():
    s=load_settings();rng=random.Random(s.seed);patches=list(s.featured_wild_patches_metres)
    xmin,ymin,xmax,ymax=s.bounds_metres
    for _ in range(s.wild_patch_count):
        for attempt in range(40):
            x=rng.uniform(xmin,xmax);y=rng.uniform(ymin,ymax)
            if allowed(x,y,s) and all(math.dist((x,y),p[:2])>p[2]+4 for p in patches):break
        else:continue
        patches.append([x,y,rng.uniform(*s.patch_radius_metres)])
    flowers=[];beds=[]
    for patch,(cx,cy,radius) in enumerate(patches):
        rng=random.Random(s.seed+104729*(patch+1))
        # Smooth density pockets leave low-density gaps instead of a uniform carpet.
        phase=rng.uniform(0,math.tau);n=math.ceil(2*radius/s.wild_spacing_metres)
        for i in range(n):
            for j in range(n):
                x=cx-radius+(i+rng.random())*s.wild_spacing_metres;y=cy-radius+(j+rng.random())*s.wild_spacing_metres
                d=math.dist((x,y),(cx,cy))/radius
                density=max(0,1-d*d)*(.62+.3*math.sin(x*.47+phase)*math.sin(y*.34-phase))
                if rng.random()>density or not allowed(x,y,s):continue
                family=0 if rng.random()<(.86 if patch%3 else .22) else 1
                flowers.append([x,y,family*2+rng.randrange(2),rng.uniform(*s.flower_height_metres),rng.uniform(0,360),'wild'])
    for cottage,(cx,cy,yaw) in enumerate(s.cottages_metres_yaw):
        for side in [-1,1]:
            rng=random.Random(s.seed+1000003+cottage*17+side)
            x,y=local_point(cx,cy,yaw,side*2.55,-5.5);length,width=s.garden_size_metres
            if not allowed(x,y,s,True):continue
            beds.append([x,y,yaw,length,width])
            for i in range(math.ceil(length/s.garden_spacing_metres)):
                for j in range(math.ceil(width/s.garden_spacing_metres)):
                    lx=-length/2+(i+.5)*s.garden_spacing_metres;ly=-width/2+(j+.5)*s.garden_spacing_metres
                    if (lx/(length/2))**2+(ly/(width/2))**2>.88:continue
                    px,py=local_point(x,y,yaw,lx,ly)
                    if allowed(px,py,s,True):flowers.append([px,py,((cottage+int(abs(lx)<.6))%2)*2+rng.randrange(2),rng.uniform(.4,.6),rng.uniform(0,360),'garden'])
    # Independent stream preserves each authored position while replacing sparse
    # stems with full-bloom scan clumps. Larger variants form pocket interiors.
    for index,flower in enumerate(flowers):
        rng=random.Random(s.seed+2000003+index*37)
        if flower[5]=='garden':
            flower[2]=rng.choice([5,6]);flower[3]=rng.uniform(*s.garden_bloom_height_metres)
        elif rng.random()<s.blooming_share:
            flower[2]=rng.choices([4,5,6],weights=[1,4,4])[0];flower[3]=rng.uniform(*s.bloom_height_metres)
    return s,patches,flowers,beds

def generate_meadow(s):
    """Lower connecting grass, with a soft outer edge and mown cottage yards."""
    xmin,ymin,xmax,ymax=s.meadow_bounds_metres;spacing=s.meadow_spacing_metres
    for i in range(math.ceil((xmax-xmin)/spacing)):
        for j in range(math.ceil((ymax-ymin)/spacing)):
            rng=random.Random(s.seed+3000007+i*100003+j*17)
            x=xmin+(i+rng.random())*spacing;y=ymin+(j+rng.random())*spacing
            if not allowed(x,y,s) or road_distance(x,y)<12:continue
            if any(math.dist((x,y),(cx,cy))<8 for cx,cy,yaw in s.cottages_metres_yaw):continue
            edge=min(x-xmin,xmax-x,y-ymin,ymax-y)
            density=min(1,max(0,edge/8))*(.8+.15*math.sin(x*.12)*math.cos(y*.17))
            if rng.random()>density:continue
            yield x,y,rng.randrange(4),rng.uniform(*s.meadow_height_metres),rng.uniform(0,360)
