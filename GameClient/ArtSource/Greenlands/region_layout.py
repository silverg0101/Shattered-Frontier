"""Authored environment layout in metres, not runtime rule configuration.

Roles follow Vertical Slice Specification / Region layout and encounter envelope.
Dimensions are local authoring decisions because that design leaves them open.
"""
import math
AREAS={
 'Arrival':dict(center=(-185,-85),radius=10),
 'Settlement':dict(center=(-90,-40),radius=30),
 'Build_West':dict(center=(-25,5),radius=14),
 'Build_Central':dict(center=(5,5),radius=14),
 'Build_East':dict(center=(35,5),radius=14),
 'Resource_Grove':dict(center=(90,75),radius=22),
 'Stone_Outcrop':dict(center=(155,55),radius=16),
 'Enemy_Clearing':dict(center=(130,-135),radius=22),
 'Return_Overlook':dict(center=(-65,-150),radius=14),
}
ROUTE=[(-185,-85),(-145,-65),(-90,-40),(-70,-5),(-45,-8),(0,-15),(45,15),(90,75),(155,55),(175,0),(130,-135),(40,-165),(-65,-150),(-130,-135),(-185,-85)]
FOOTPATHS={
 'SouthernCottages':[(-104,-59),(-99,-54),(-94,-46),(-90,-40),(-80,-40),(-77,-51)],
 'NorthernCottages':[(-106,-23),(-99,-28),(-90,-40),(-78,-33),(-68,-28)],
}
# Open expansion corridors in the same world coordinates; these end at the
# present art tract boundary, not at loading gates or teleports.
CONNECTIONS={
 'WestGreenlands':[(-300,-116),(-263,-113),(-225,-99),(-185,-85)],
 'NortheastGreenlands':[(90,75),(116,125),(161,184),(207,241),(232,300)],
}
def base(x,y):return 4*math.sin(x*.012)*math.sin(y*.008)+2*math.sin(y*.03)
def height(x,y):
    z=base(x,y)
    for name,area in AREAS.items():
        ax,ay=area['center'];r=area['radius'];d=math.hypot(x-ax,y-ay)
        t=max(0,min(1,(d-r)/18));t=t*t*(3-2*t)
        target=0 if name.startswith('Build_') else (8 if name=='Return_Overlook' else base(ax,ay))
        z=target*(1-t)+z*t
    r=math.hypot(x,y);z+=max(0,r-215)*(.15+.08*math.sin(x*.022)*math.cos(y*.015))
    return z
def road_distance(x,y):
    best=1e6
    for points in [ROUTE,*CONNECTIONS.values()]:
        for (ax,ay),(bx,by) in zip(points,points[1:]):
            dx=bx-ax;dy=by-ay;t=max(0,min(1,((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy)))
            best=min(best,math.hypot(x-ax-t*dx,y-ay-t*dy))
    return best
def soil(x,y):
    f=max(0,min(1,(3-road_distance(x,y))))
    for name,a in AREAS.items():
        if name.startswith('Build_'):
            ax,ay=a['center'];d=max(abs(x-ax),abs(y-ay));f=max(f,max(0,min(.6,(11-d)*.25)))
        elif name in ('Settlement','Arrival','Enemy_Clearing'):
            ax,ay=a['center'];d=math.hypot(x-ax,y-ay);f=max(f,max(0,min(.85,(a['radius']*.7-d)/5)))
    return f
ROUTE_METRES=sum(math.dist(a,b) for a,b in zip(ROUTE,ROUTE[1:]))
