"""Silverbrook Riverside Docks from docs/Game-Design/concept-art/Silverbrook/Riverside Docks.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_riversidedocks.py
Original procedural geometry interpreting the elevation (drawn 16 m long, 6 m from the water to the
crane post top), fitted to the 30 x 10 m docks slot: a pale squared-stone quay with a stone pier and
water stairs at the west end, a timber wharf deck on banded, rope-wrapped piles with crossed bracing,
a jib crane with windlass, hand wheel, sheaves and a slung crate, and a stag banner post.
Front (-Y) is the water side, as drawn. Origin: slot centre at riverbank ground level (z=0).
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,UP,frame
import bpy
from mathutils import Matrix
scene=kit.reset(4201)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Riverside Docks.png'
asset=bpy.data.collections.new('SilverbrookRiversideDocks');scene.collection.children.link(asset)

M=dict(
    # rough_block_wall reads warm; the concept's quay stone is pale cool grey, so a blue-grey multiply at `aged` cools it.
    stone=kit.scan('SB_RiversideDocks_Masonry','rough_block_wall',dark=(.62,.70,.85),aged=.5),
    paving=kit.scan('SB_RiversideDocks_Paving','medieval_blocks_03',dark=(.62,.70,.85),damp=False,aged=.5),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    decking=kit.scan('SB_RiversideDocks_Decking','weathered_brown_planks',dark=(.32,.30,.28),damp=False,aged=.4),
    rope=kit.scan('SB_RiversideDocks_Rope','hessian_380',dark=(.42,.33,.24),damp=False,aged=.65),
    banner=kit.scan('SB_RiversideDocks_Banner','denim_fabric_03',dark=(.10,.12,.28),damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    gilt=kit.solid('SB_RiversideDocks_Gilt',(.80,.55,.18),.35,1.0))
DRESSED=kit.patches('medieval_blocks_03',(.05,.035),dark=.7,frac=.01,std=.22)
BANDS=kit.bands('weathered_brown_planks')
def rect():return kit.rng.choice(DRESSED)
def band():return kit.rng.choice(BANDS)
G=dict(stone=Geo('SM_SBRiversideDocks_Masonry',asset),deck=Geo('SM_SBRiversideDocks_Deck',asset),
       timber=Geo('SM_SBRiversideDocks_Timber',asset),crane=Geo('SM_SBRiversideDocks_Crane',asset),detail=Geo('SM_SBRiversideDocks_Details',asset))
S,T,K,D=G['stone'],G['timber'],G['crane'],G['detail']
# ---- measured layout (metres) -------------------------------------------------
# Town layout (GameClient/ArtSource/Greenlands/silverbrook-town-layout.json): docks slot 30 x 10 x 1 on the
# Riverside Harbour basin (level 0.3). heightfield.py puts the bank 1.5 m above the water (WATER_BANK);
# z=0 is that bank. The layout's 1 m height is the deck; the crane and banner post need 10 m in all.
SLOT=(30,10,10)
DECK=.5                        # walking surface: 0.5 m above the bank, 2.0 m above the water
WATER,BED=-1.5,-3.5            # water surface and the bed the piles and quay footings stand on
QF=-1.2                        # quay wall face toward the water; timber deck in front of it (y<QF)
RB=3.5                         # rear ramp foot: the paving slopes from DECK at y=RB to the bank at y=5
PX=-10.8                       # east face of the west stone pier (x<PX)
SY=-3.8                        # back of the water-stairs strip on the pier front
PF,PM=-4.6,-2.8                # front and middle pile rows
PILES=[PX+.25+i*(15-.34-(PX+.25))/8 for i in range(9)]
def orient(q,out):
    q=[Vector(p) for p in q];n=sum((a.cross(q[(i+1)%len(q)]) for i,a in enumerate(q)),Vector())
    return q if n.dot(Vector(out))>=0 else q[::-1]
def fill(geo,pts,axis,th,mat):
    """Flat slab around a planar outline, caps fanned from pts[0] (Townhouse1)."""
    pts=[Vector(p) for p in pts];axis=Vector(axis).normalized()
    if sum((a.cross(pts[(i+1)%len(pts)]) for i,a in enumerate(pts)),Vector()).dot(axis)<0:pts.reverse()
    o=axis*th/2;n=len(pts)
    for i in range(1,n-1):geo.poly([pts[0]+o,pts[i]+o,pts[i+1]+o],mat);geo.poly([pts[0]-o,pts[i+1]-o,pts[i]-o],mat)
    for i,a in enumerate(pts):b=pts[(i+1)%n];geo.poly([a-o,b-o,b+o,a+o],mat)
def lathe(geo,a,b,prof,mat,n=16):
    """Solid of revolution about a->b; prof is (t in 0..1, radius). Ends are capped where radius > 0."""
    a,b=Vector(a),Vector(b);ax=b-a;L=ax.length;ax.normalize();u=ax.orthogonal().normalized();v=ax.cross(u)
    rings=[[a+ax*(t*L)+(u*math.cos(math.tau*j/n)+v*math.sin(math.tau*j/n))*r for j in range(n)] for t,r in prof]
    for i in range(len(prof)-1):
        for j in range(n):
            q=[rings[i][j],rings[i][(j+1)%n],rings[i+1][(j+1)%n],rings[i+1][j]];c=sum(q,Vector())/4
            geo.poly(orient(q,c-(a+ax*(c-a).dot(ax))),mat,smooth=True)
    if prof[0][1]>0:geo.poly(orient(rings[0],-ax),mat)
    if prof[-1][1]>0:geo.poly(orient(rings[-1],ax),mat)
def coil(geo,a,b,r,turns,amp=.03,n=16):
    """Rope wound round a post or drum: a radius rippling once per turn."""
    k=turns*4;lathe(geo,a,b,[(i/k,r+amp*abs(math.sin(math.pi*i/4))) for i in range(k+1)],M['rope'],n)
def rope(geo,a,b,w=.04):geo.beam(a,b,w,w,M['rope'])
def swag(geo,a,b,sag,seg=8):
    a,b=Vector(a),Vector(b);pts=[a+(b-a)*(i/seg)-UP*sag*4*(i/seg)*(1-i/seg) for i in range(seg+1)]
    for p,q in zip(pts,pts[1:]):rope(geo,p,q+(q-p)*.04)
def ring(geo,c,o,r=.12):
    """Forged mooring ring on a staple plate (kit.door ring)."""
    P,R=frame(c,o);geo.box(P(0,.015,0),(.12,.03,.12),M['iron'],.004,R)
    for k in range(12):
        t=math.tau*k/12;geo.box(P(r*math.sin(t),.05,-r+r*math.cos(t)),(.075,.03,.03),M['iron'],0,R@Matrix.Rotation(-t,3,'Y'))
# ---- stone quay, west pier and water stairs -----------------------------------------------------
TOP=DECK-.15                   # masonry top under the 0.15 m paving and coping course
S.box((0,(QF+RB)/2,(BED+TOP)/2),(30,RB-QF,TOP-BED),M['stone'],0)
S.box(((-15+PX)/2,(SY+QF)/2,(BED+TOP)/2),(PX+15,QF-SY,TOP-BED),M['stone'],0)
S.box((-14.8,(-5+SY)/2,(BED+TOP)/2),(.4,SY+5,TOP-BED),M['stone'],0)   # stair-head landing
NS=10;RISE=(DECK-(WATER+.2))/NS;GOING=(PX-(-14.6))/NS
for i in range(NS):
    x0=-14.6+i*GOING;z=DECK-(i+1)*RISE
    S.box((x0+GOING/2,(-5+SY)/2,(BED+z-.1)/2),(GOING,SY+5,z-.1-BED),M['stone'],0)
# coping: single dressed stones along the quay edge and the stair edge; paving flags elsewhere
def coping(x0,x1,y0,y1):
    x=x0
    while x<x1-.01:
        L=min(kit.rng.uniform(.6,.95),x1-x)
        if x1-(x+L)<.3:L=x1-x
        G['deck'].box((x+L/2,(y0+y1)/2,TOP+.075),(L,y1-y0,.15),M['paving'],.008,rect=rect());x+=L
for i in range(NS):
    x0=-14.6+i*GOING;z=DECK-(i+1)*RISE
    G['deck'].box((x0+GOING/2,(-5+SY)/2,z-.05),(GOING,SY+5,.1),M['paving'],.01)   # dressed tread
coping(PX,15,QF,QF+.5);coping(-15,PX,SY,SY+.5);coping(-15,-14.6,-5,SY)
G['deck'].box((0,(QF+.5+RB)/2,TOP+.075),(30,RB-QF-.5,.15),M['paving'],0)
G['deck'].box(((-15+PX)/2,(SY+1+QF)/2,TOP+.075),(PX+15,QF-SY,.15),M['paving'],0)
G['deck'].prism([(0,RB,TOP),(0,RB,DECK),(0,5,0),(0,5,-.6),(0,RB,-.6)],(1,0,0),30,M['paving'])
def ramp_z(y):return DECK*(5-y)/(5-RB)
# mooring rings on the stair wall and the quay face behind the piles
for x in (-13.2,-11.6):ring(D,(x,SY,-.25),(0,-1,0))
for i in range(0,8,2):ring(D,((PILES[i]+PILES[i+1])/2,QF,-.45),(0,-1,0))
# ---- timber wharf: piles, bearers, boards, bracing, bollard tops --------------------------------------
BT=DECK-.06                    # board underside = bearer top
for x in PILES:
    T.box((x,PF,(BED+DECK+1.1)/2),(.45,.45,DECK+1.1-BED),M['timber'],.08,grain=True,band=band())   # front pile, bollard above the deck
    T.box((x,PM,(BED+BT)/2),(.4,.4,BT-BED),M['timber'],.07,grain=True,band=band())
    for z in (-1.0,-.15,DECK+.3):D.box((x,PF,z),(.48,.48,.08),M['iron'],.085)
    for z in (-1.0,-.15):D.box((x,PM,z),(.43,.43,.08),M['iron'],.075)
    D.box((x,PF,DECK+1.13),(.36,.36,.06),M['iron'],.06)                 # iron cap on the bollard
    coil(G['detail'],(x,PF,DECK+.5),(x,PF,DECK+.95),.29,5)
    T.beam((x,PF+.24,-1.1),(x,PM-.22,BT-.3),.16,.16,M['timber'],band=band())       # cross brace toward the middle row
XE=15
T.box(((PX+XE)/2,-4.89,BT-.175),(XE-PX,.18,.35),M['timber'],.012,grain=True,band=band())   # front fascia
T.box(((PX+XE)/2,PF+.33,BT-.15),(XE-PX,.2,.3),M['timber'],.012,grain=True,band=band())     # bearer behind the front piles
T.box(((PX+XE)/2,PM,BT-.15),(XE-PX,.3,.3),M['timber'],.012,grain=True,band=band())         # middle bearer on the pile heads
T.box(((PX+XE)/2,QF-.1,BT-.15),(XE-PX,.2,.3),M['timber'],.012,grain=True,band=band())      # ledger on the quay wall
for x in [PX+.1]+PILES[1:-1]+[XE-.1]:
    T.box((x,(-4.82+QF)/2,BT-.15),(.14,QF+4.82,.28),M['timber'],.01,grain=True,band=band())  # joists across the bearers
for x in PILES:
    for y in (-4.89,):D.box((x,y-.097,BT-.175),(.5,.012,.3),M['iron'],.004)                # strap over the fascia at each pile
NB=round((XE-PX)/.2);BW=(XE-PX)/NB
for i in range(NB):
    G['deck'].box((PX+(i+.5)*BW,(-4.95+QF)/2,DECK-.03),(BW,QF+4.95,.06),M['decking'],.004,grain=True,band=band())
for a,b in zip(PILES,PILES[1:]):
    T.beam((a+.1,-4.86,-1.15),(b-.1,-4.86,BT-.4),.14,.14,M['timber'],band=band())
    T.beam((b-.1,-4.85,-1.15),(a+.1,-4.85,BT-.4),.14,.14,M['timber'],band=band())
    D.box(((a+b)/2,-4.94,(-1.15+BT-.4)/2),(.3,.02,.3),M['iron'],.004)               # plate where the braces cross
    T.box(((a+b)/2,-4.87,-.3),(b-a,.16,.22),M['timber'],.01,grain=True,band=band())        # waling
    swag(D,(a+.3,PF,DECK+.82),(b-.3,PF,DECK+.82),.28)
# ---- railings at the water stairs -------------------------------------------------------------------
def railing(a,b,posts):
    a,b=Vector(a),Vector(b)
    for i in range(posts):p=a+(b-a)*(i/(posts-1));T.box((p.x,p.y,DECK+.5),(.11,.11,1.0),M['timber'],.01,grain=True,band=band())
    for z,s in ((DECK+1.0,.1),(DECK+.5,.08)):T.beam((a.x,a.y,z),(b.x,b.y,z),s,.12,M['timber'],band=band())
railing((-14.5,SY+.08,0),(PX-.06,SY+.08,0),5)
T.beam((PX+.08,PF+.2,DECK+1.0),(PX+.08,SY+.06,DECK+1.0),.1,.12,M['timber'],band=band())
T.beam((PX+.08,PF+.2,DECK+.5),(PX+.08,SY+.06,DECK+.5),.08,.12,M['timber'],band=band())
T.box((PX+.08,SY+.06,DECK+.5),(.11,.11,1.0),M['timber'],.01,grain=True,band=band())
# ---- banner post on the pier --------------------------------------------------------------------------
BX,BY=-14.3,-2.4
T.box((BX,BY,(DECK+3.85)/2),(.22,.22,3.85-DECK),M['timber'],.02,grain=True,band=band())
D.box((BX,BY,DECK+.1),(.26,.26,.2),M['iron'],.02);D.box((BX,BY,3.88),(.28,.28,.06),M['iron'],.02)
T.beam((BX-.1,BY,3.55),(BX+1.15,BY,3.55),.14,.14,M['timber'],band=band())
T.beam((BX+.05,BY,3.0),(BX+.6,BY,3.5),.1,.1,M['timber'],band=band())
def banner(c,o,ztop,zbot,w,d=.03):
    """Navy swallow-tail banner with gilt edging, rod and stag (StoneBridge banner)."""
    P,R=frame(c,o);pts=[(-w/2,ztop),(w/2,ztop),(w/2,zbot),(0,zbot+.32),(-w/2,zbot)]
    fill(D,[P(x,d,z) for x,z in pts],o,.02,M['banner'])
    for (x0,z0),(x1,z1) in zip(pts,pts[1:]+pts[:1]):D.beam(P(x0,d+.012,z0),P(x1,d+.012,z1),.03,.012,M['gilt'],grain=False)
    D.beam(P(-w/2-.06,d,ztop+.05),P(w/2+.06,d,ztop+.05),.035,.035,M['gilt'],grain=False)
    for s in (-1,1):D.beam(P(s*w*.35,d,ztop+.05),P(s*w*.35,d,3.48),.015,.015,M['iron'],grain=False)
    stag(D,P(0,d+.011,(ztop+zbot)/2+.12),R@Vector((1,0,0)),UP,Vector(o).normalized(),w*1.05)
def stag(geo,origin,right,up,normal,s):
    """Gilt stag head with antlers (Townhouse1)."""
    o=Vector(origin)+Vector(normal)*.008;Rv,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+Rv*(a*s)+U*(z*s)
    fill(geo,[p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        fill(geo,[p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
banner((BX+.75,BY,0),(0,-1,0),3.38,1.85,.62)
# ---- jib crane with windlass ----------------------------------------------------------------------------
YAW=math.radians(-35)          # the jib swings out over the water (-Y) and to the right as drawn
d=Vector((math.cos(YAW),math.sin(YAW),0));n=UP.cross(d);C=Vector((.8,.3,0));RZ=Matrix.Rotation(YAW,3,'Z')
def L(a,b,z):return C+d*a+n*b+UP*z
PT=DECK+.35;POST=4.6;JH,JT,JR=4.1,5.7,4.5   # pedestal top, post top, jib heel/tip height, reach
def jz(a):return JH+(JT-JH)*a/JR
S.box(L(0,0,DECK+.175),(1.3,1.3,.35),M['paving'],.03,RZ,rect=rect())
D.box(L(0,0,PT+.04),(.66,.66,.08),M['iron'],.02,RZ)
K.box(L(0,0,(PT+POST)/2),(.45,.45,POST-PT),M['timber'],.03,RZ,grain=True,band=band())
for z in (PT+.25,2.0,3.2,JH,POST-.12):D.box(L(0,0,z),(.49,.49,.08),M['iron'],.035,RZ)
D.box(L(0,0,POST+.04),(.55,.55,.08),M['iron'],.03,RZ)
for a,b in ((1,0),(-1,0),(0,1),(0,-1)):K.beam(L(a*.2,b*.2,PT+1.0),L(a*.55,b*.55,PT),.14,.14,M['timber'],band=band())
K.beam(L(-.1,0,JH-.05),L(JR+.15,0,jz(JR+.15)),.3,.3,M['timber'],band=band())               # jib
K.beam(L(.15,0,2.3),L(2.5,0,jz(2.5)-.12),.24,.24,M['timber'],band=band())                     # strut
K.beam(L(.15,0,3.35),L(1.0,0,jz(1.0)-.1),.18,.18,M['timber'],band=band())                     # knee
for a in (.75,2.5,4.15):D.beam(L(a-.07,0,jz(a-.07)),L(a+.07,0,jz(a+.07)),.34,.34,M['iron'],grain=False)
for a,z in ((.15,2.3),(.15,3.35)):D.box(L(.24,0,z+.05),(.02,.32,.3),M['iron'],.004,RZ)      # strut plates on the post
SH=L(JR+.02,0,JT-.4)            # tip sheave between two iron cheeks
for s in (-1,1):D.beam(L(JR+.02,s*.07,JT-.05),L(JR+.02,s*.07,JT-.52),.12,.02,M['iron'],grain=False)
lathe(D,SH-n*.045,SH+n*.045,[(0,.2),(.3,.17),(.7,.17),(1,.2)],M['iron'])
lathe(D,SH-n*.09,SH+n*.09,[(0,.03),(1,.03)],M['iron'],8)
rope(K,L(-.9,0,DECK+1.15+.25),L(-.22,0,POST+.12))                          # windlass to the post head
rope(K,L(-.22,0,POST+.12),L(JR-.15,0,JT+.2))                                # along the jib
rope(K,L(JR-.15,0,JT+.2),SH+d*(-.17)+UP*.05)                                 # down onto the sheave
BZ=3.55                         # hook block; crate hangs with 1.95 m headroom over the deck
for s in (-1,1):rope(K,SH+d*(s*.19),L(JR+.02+s*.06,0,BZ+.35))
K.box(L(JR+.02,0,BZ+.2),(.18,.24,.34),M['timber'],.02,RZ,rect=None,grain=True,band=band())
lathe(D,L(JR+.02,-.13,BZ+.25),L(JR+.02,.13,BZ+.25),[(0,.03),(1,.03)],M['iron'],8)
hook=[L(JR+.02,0,BZ+.03),L(JR+.02,0,BZ-.1),L(JR+.1,0,BZ-.2),L(JR+.02,0,BZ-.29),L(JR-.08,0,BZ-.22),L(JR-.1,0,BZ-.12)]
for p,q in zip(hook,hook[1:]):D.beam(p,q+(q-p)*.15,.035,.035,M['iron'],grain=False)
CZ=DECK+1.95;CW,CD,CH=.9,.75,.7
CC=L(JR+.02,0,CZ+CH/2)
K.box(CC,(CW-.04,CD-.04,CH-.04),M['timber'],.01,RZ,grain=True,band=band())
for sa in (-1,1):
    for sb in (-1,1):
        K.box(CC+d*(sa*(CW/2-.04))+n*(sb*(CD/2-.04)),(.08,.08,CH),M['timber'],.008,RZ,grain=True,band=band())
        rope(K,L(JR+.02,0,BZ-.24),CC+d*(sa*(CW/2-.06))+n*(sb*(CD/2-.06))+UP*CH/2)
    for sz in (-1,1):
        K.box(CC+d*(sa*(CW/2-.04))+UP*(sz*(CH/2-.04)),(.08,CD,.08),M['timber'],.008,RZ,grain=True,band=band())
        K.box(CC+n*(sa*(CD/2-.04))+UP*(sz*(CH/2-.04)),(CW,.08,.08),M['timber'],.008,RZ,grain=True,band=band())
    K.beam(CC+n*(sa*(CD/2-.01))+d*(CW/2-.08)-UP*(CH/2-.08),CC+n*(sa*(CD/2-.01))-d*(CW/2-.08)+UP*(CH/2-.08),.07,.03,M['timber'],band=band())   # face braces
    K.beam(CC+d*(sa*(CW/2-.01))+n*(CD/2-.08)-UP*(CH/2-.08),CC+d*(sa*(CW/2-.01))-n*(CD/2-.08)+UP*(CH/2-.08),.07,.03,M['timber'],band=band())
AZ=DECK+1.15;WA=-.9             # windlass axle height and position behind the post
for b in (-.55,.55):
    for s in (-1,1):K.beam(L(WA+s*.42,b,DECK),L(WA,b,AZ+.08),.12,.12,M['timber'],band=band())
    K.beam(L(WA-.32,b,DECK+.32),L(WA+.32,b,DECK+.32),.1,.1,M['timber'],band=band())
    K.box(L(WA,b,AZ+.08),(.24,.16,.18),M['timber'],.01,RZ,grain=True,band=band())
lathe(K,L(WA,-.47,AZ),L(WA,.47,AZ),[(0,.2),(1,.2)],M['timber'])
coil(D,L(WA,-.4,AZ),L(WA,.4,AZ),.22,7,.025)
lathe(D,L(WA,-.86,AZ),L(WA,.66,AZ),[(0,.045),(1,.045)],M['iron'],8)
WC=L(WA,-.78,AZ);WR=.5          # hand wheel on the water side of the windlass
rim=[WC+(d*math.cos(math.tau*k/16)+UP*math.sin(math.tau*k/16))*WR for k in range(16)]
for k in range(16):a,b=rim[k],rim[(k+1)%16];K.beam(a-(b-a)*.05,b+(b-a)*.05,.07,.07,M['timber'],band=band())
for k in range(8):t=math.tau*k/8;K.beam(WC,WC+(d*math.cos(t)+UP*math.sin(t))*(WR-.02),.045,.045,M['timber'],band=band())
lathe(D,WC-n*.06,WC+n*.06,[(0,.09),(1,.09)],M['iron'],12)
D.beam(rim[4],rim[4]-n*.22,.04,.04,M['iron'],grain=False)
lathe(K,rim[4]-n*.1,rim[4]-n*.3,[(0,.035),(1,.035)],M['timber'],8)              # crank handle
# ---- export, checks, preview scene, manifest -----------------------------------------------------
objs=[g.object() for g in G.values()]
def rewet(o):
    """The kit damps up to 0.65 m above z=0; here z=0 is the bank, so measure the wet band from the water."""
    me=o.data;col=me.color_attributes['Weathering']
    for poly in me.polygons:
        m=me.materials[poly.material_index]
        if m.name not in kit.DAMP:continue
        for li in poly.loop_indices:
            p=me.vertices[me.loops[li].vertex_index].co;damp=max(0,1-(p.z-WATER)/.65)*(.30+.15*math.sin(p.x*7.3+p.y*5.1))
            col.data[li].color=(min(.8,max(damp,kit.AGED.get(m.name,0))),col.data[li].color[1],0,1)
for o in objs:rewet(o)
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
assert max(abs(lo[0]+hi[0]),abs(lo[1]+hi[1]))<1e-3,f'not centred on the slot: {lo} {hi}'   # recentring would move the bank datum
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=200_000,f'triangle budget: {tris}'
# Walkable surface: cast down from just above the deck on a 0.25 m grid over the wharf, quay, pier, stair
# head and ramp; every hit must be at the walking height (0.04 m tolerance for chamfers and seated legs).
bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();checked=0
def want(x,y):
    if y>RB:return ramp_z(y)
    if y<SY and x<PX:return DECK if x<-14.6 else None      # stair head only; the flight steps down
    if any(abs(x-p)<.3 and abs(y-PF)<.3 for p in PILES):return None   # bollard piles rise through the deck
    return DECK
for i in range(120):
    for j in range(40):
        x,y=-14.95+i*.25+.01,-4.93+j*.25+.01;z=want(x,y)
        if z is None:continue
        hit,loc,*_,ob,_m=scene.ray_cast(dg,Vector((x,y,z+.03)),Vector((0,0,-1)))
        assert hit and abs(loc.z-z)<.04,f'not walkable at {x:.2f},{y:.2f}: {ob.name if hit else None} {loc.z if hit else None} want {z:.3f}'
        checked+=1
OUT=HERE/'SilverbrookRiversideDocks'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
def preview(name,verts,faces,mat):
    me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);ob=bpy.data.objects.new(name,me);pre.objects.link(ob);me.materials.append(mat)
# Bank at z=0 behind the quay face, harbour bed in front of it, water 1.5 m below the bank.
prof=[(-60,BED),(QF+.3,BED),(QF+.3,0),(60,0)]
preview('Ground',[(x,y,z) for y,z in prof for x in (-60,60)],[(2*i,2*i+1,2*i+3,2*i+2) for i in range(len(prof)-1)],kit.solid('Preview_Ground',(.11,.12,.07),.95))
preview('Water',[(-60,-60,WATER),(60,-60,WATER),(60,QF+.3,WATER),(-60,QF+.3,WATER)],[(0,1,2,3)],kit.solid('Preview_Water',(.03,.05,.06),.05))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-17,-24,9),(70,0,-35),lens=30);cam('Front',(0,-60,1.3),(90,0,0),31.5);cam('Side',(-60,0,1.3),(90,0,-90),21);cam('Rear',(0,60,1.3),(90,0,180),31.5)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1600;scene.render.resolution_y=800;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'RiversideDocks-reference.png')
mins=Vector(lo);maxs=Vector(hi)
manifest=dict(asset='SilverbrookRiversideDocks',reference=dict(file='RiversideDocks-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_riversidedocks.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y (water side), origin at the slot centre at riverbank ground level',bounds_m=[round(v,3) for v in maxs-mins],
    z_range_m=[round(mins.z,3),round(maxs.z,3)],
    deck=dict(walking_z_m=DECK,above_bank_m=DECK,above_water_m=round(DECK-WATER,3),bank_z_m=0.0,water_z_m=WATER,bed_z_m=BED,
              water_side='-Y',timber_deck_y_m=[-4.95,QF],stone_quay_y_m=[QF,RB],rear_ramp=dict(y_m=[RB,5.0],z_m=[DECK,0.0],grade=round(DECK/(5-RB),3)),
              water_stairs=dict(x_m=[-14.6,PX],y_m=[-5.0,SY],steps=NS,rise_m=round(RISE,3),going_m=round(GOING,3),bottom_tread_z_m=round(DECK-NS*RISE,3)),
              walk_samples=checked,check='downward rays on a 0.25 m grid; every hit at the walking height within 0.04 m'),
    height_decision='layout gives 1 m (the deck); crane post top 6 m above the water as drawn, jib head 7.4 m, so the slot height is 10 m from the pile footings (z=-3.5) to the jib head',
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookRiversideDocks',slot_m=list(SLOT),
    asset_id='BLD-GreenlandsRiversideDocks-01',outputs=['SilverbrookRiversideDocks.blend','SilverbrookRiversideDocks.glb','SilverbrookRiversideDocks.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('RIVERSIDEDOCKS_BUILT',manifest['bounds_m'],manifest['z_range_m'],tris,'walk samples',checked)
