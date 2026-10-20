"""Silverbrook hay cart: two-wheeled oak farm cart, iron-tyred spoked wheels, shafts resting on a trestle prop, loaded with hay.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_prophaycart.py
Designed to match Silverbrook; no owner concept.
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
import hay_props_common as hp
import bpy
scene=kit.reset(5101);rng=kit.rng
asset=bpy.data.collections.new('SilverbrookPropHayCart');scene.collection.children.link(asset)
M=dict(timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
       iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
       straw=kit.scan('SB_PropHayCart_Straw','reed_roof_04',dark=(.55,.45,.3),damp=False,normal=1.0))
BANDS=kit.bands('weathered_brown_planks')
def band():return rng.choice(BANDS)
G=dict(timber=Geo('SM_SBPropHayCart_Timber',asset),iron=Geo('SM_SBPropHayCart_Iron',asset),hay=Geo('SM_SBPropHayCart_Hay',asset),stalks=Geo('SM_SBPropHayCart_Stalks',asset))
SLOT=(2.2,3.6,2.2)   # bounds rounded up to 0.1 m (asserted in finish)
T,I=M['timber'],M['iron']
def tb(c,size,rot=None,**kw):G['timber'].box(c,size,T,.008,rot,grain=True,band=band(),**kw)
def beam(a,b,w,d=None,**kw):G['timber'].beam(a,b,w,d,T,band=band(),**kw)
def ib(c,size,rot=None):G['iron'].box(c,size,I,.003,rot)

# ---- wheels: 1.3 m, 12 spokes, 12 felloes, nave, iron tyre; axle at z=R, y=0 --------------
R=.65;AX=.82
def wheel(xw):
    z=R;ex=Vector((1,0,0))
    hp.cyl_x(G['timber'],T,(0,0,z),0,.10,xw-.13,xw+.13,14)
    hp.cyl_x(G['timber'],T,(0,0,z),0,.118,xw-.055,xw+.055,14)
    for s in (-1,1):hp.cyl_x(G['iron'],I,(0,0,z),.10,.112,xw+s*.13-(.03 if s>0 else 0),xw+s*.13+(0 if s>0 else .03),14)
    for k in range(12):
        a=math.tau*k/12;d=Vector((0,math.cos(a),math.sin(a)))
        beam(Vector((xw,0,z))+d*.09,Vector((xw,0,z))+d*.56,.052,.04)
    for k in range(12):
        a=math.tau*(k+.5)/12;d=Vector((0,math.cos(a),math.sin(a)));tan=Vector((0,-math.sin(a),math.cos(a)))
        tb(Vector((xw,0,z))+d*.578,(.08,.05,2*.578*math.sin(math.pi/12)+.008),Matrix((ex,d,tan)).transposed())
    hp.cyl_x(G['iron'],I,(0,0,z),.60,R,xw-.04,xw+.04,48)
    for k in range(12):  # tyre nail heads
        a=math.tau*(k+.25)/12;d=Vector((0,math.cos(a),math.sin(a)));tan=Vector((0,-math.sin(a),math.cos(a)))
        for s in (-.02,.02):ib(Vector((xw+s,0,z))+d*(R+.002),(.018,.006,.018),Matrix((ex,d,tan)).transposed())
for xw in (-AX,AX):wheel(xw)
# axle-tree, stub ends with linch washers and pins
tb((0,0,R),(2.0,.1,.1))
for s in (-1,1):
    hp.cyl_x(G['iron'],I,(0,0,R),0,.05,s*.955-(.012 if s>0 else 0),s*.955+(0 if s>0 else .012),10)
    ib((s*.985,0,R),(.02,.016,.11))

# ---- bed: sills, floor boards, planked box (three plank courses, battens, iron straps) -------------
Z0=.86;BL=1.0;BH=.56
for s in (-1,1):tb((s*.62,0,.76),(.09,2*BL+.1,.12))
for s in (-1,1):beam((s*.42,.8,.76),(s*.42,-2.3,.76),.075,.075)      # shafts (thills)
for i in range(7):tb((-.63+i*.21,0,Z0-.0175),(.19,2*BL,.035))
for s in (-1,1):
    for k in range(3):tb((s*.67,0,Z0+.09+k*.18),(.05,2*BL+.05,.175))
    tb((s*.67,0,Z0+BH+.02),(.09,2*BL+.1,.04))
    for y in (-.95,-.5,0,.5,.95):tb((s*.705,y,Z0+BH/2),(.035,.07,BH))
    for y in (-.75,.75):ib((s*.727,y,Z0+BH/2),(.006,.06,BH+.02))
for sy in (-1,1):
    for k in range(3):tb((0,sy*(BL+.025),Z0+.09+k*.18),(1.34,.05,.175))
    tb((0,sy*(BL+.025),Z0+BH+.02),(1.44,.09,.04))
# iron: axle-tree straps, shaft ferrules and strap ties
for s in (-1,1):
    ib((s*.62,0,.76),(.1,.07,.14));ib((s*.42,-2.275,.76),(.085,.05,.085));ib((s*.42,-2.306,.76),(.09,.012,.09))
    ib((s*.42,.0,.76),(.09,.06,.09))

# ---- prop: trestle under the shaft tips ---------------------------------------------
PY=-1.95
tb((0,PY,.69),(1.0,.07,.07))
for s in (-1,1):
    for sy in (-1,1):beam((s*.42,PY,.69),(s*.55,PY+sy*.35,0),.065,.055)
    ib((s*.42,PY,.76),(.09,.12,.012));ib((s*.42,PY,.725),(.09,.012,.07))   # strap hooking the shaft to the prop

# ---- hay load: lumpy heaped mound + loose stalks -----------------------------------------
PROF=[(-.64,0),(-.64,.56),(-.8,.68),(-.86,.8),(-.78,.9),(-.48,.96),(0,.98)];PROF=PROF+[(-x,z) for x,z in reversed(PROF[:-1])]
HY=1.0
def lump(x,y):return .04*math.sin(x*6.1+y*3.3)+.03*math.sin(x*11.7-y*7.9+1)+.02*math.sin(x*21+y*17)
def surf(th,i,m,noise=1):
    """th in [-pi/2,pi/2] along the cart; i profile index (can be fractional)"""
    y=HY*math.sin(th);s=abs(math.sin(th));e=(1-s**6)**.35;j=int(i);f=i-j;(x0,z0),(x1,z1)=PROF[j],PROF[min(j+1,len(PROF)-1)]
    x,z=x0+(x1-x0)*f,z0+(z1-z0)*f;e2=e**.3
    p=Vector((x*e2,y,Z0+z*e));
    if noise and 0<i<len(PROF)-1:p.z+=lump(p.x,y)*e
    return p
NY=40;NP=len(PROF)-1
UW=(.05,.05,.9,.75)   # window of the straw scan (no moss band, no tiling)
def uv(th,i):return (UW[0]+(i/NP)*UW[2],UW[1]+((th/math.pi+.5))*UW[3])
def qn(q):pts=[surf(t,i,0) for t,i in q];return (pts[2]-pts[0]).cross(pts[3]-pts[1])
FLIP=qn([(0,NP//2),(0,NP//2+1),(.1,NP//2+1),(.1,NP//2)]).z<0
for a in range(NY):
    t0,t1=-math.pi/2+math.pi*a/NY,-math.pi/2+math.pi*(a+1)/NY
    for b in range(NP):
        q=[(t0,b),(t0,b+1),(t1,b+1),(t1,b)];pts=[surf(t,i,1) for t,i in q]
        if all(p.z<=Z0+.001 for p in pts):continue
        if FLIP:q=q[::-1];pts=pts[::-1]
        G['hay'].poly(pts,M['straw'],[uv(t,i) for t,i in q],smooth=True)
# loose stalks poking out of the surface and trailing over the edges
def stalk(a,b,w=.006):
    rot=(b-a).to_track_quat('Z','Y').to_matrix();G['stalks'].box((a+b)/2,(w,w,(b-a).length),M['straw'],0,rot,rect=(.3+rng.random()*.3,.3+rng.random()*.3,.02,.02))
for _ in range(300):
    th=rng.uniform(-1.35,1.35);i=rng.uniform(.3,NP-.3);p=surf(th,i,1)
    if p.z<Z0+.45:continue
    # lean outward along the surface tangent, droop with gravity
    n=Vector((p.x*.6,rng.uniform(-.8,.8),.45)).normalized();d=(n+Vector((rng.uniform(-.7,.7),rng.uniform(-.7,.7),rng.uniform(-.35,.25)))).normalized()
    L=rng.uniform(.12,.26);a=p-d*.05;b=p+d*L;stalk(a,b)
for _ in range(60):   # wisps dragging over the sides and ends
    th=rng.choice((-1,1))*rng.uniform(1.0,1.5);i=rng.choice((1,2,4,5));p=surf(th,i,1)
    a=p;b=p+Vector((rng.uniform(-.1,.1),rng.choice((-1,1))*rng.uniform(.0,.15),-rng.uniform(.15,.4)));stalk(a-(b-a)*.2,b)

hp.finish(HERE,'PropHayCart',G,SLOT,dict(footprint_m=[2.2,3.6],front='-Y',ground_contact=True,near=['stables','farmhouse','granary','mill','road','market','cottage'],clearance_m=2.6),60000,weld=('SM_SBPropHayCart_Hay',),beauty=(-6,-8,3.6),crop=(860,270,1110,520))
