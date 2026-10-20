"""Silverbrook Northridge House 3 from docs/Game-Design/concept-art/Silverbrook/Northridge House-3.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse3.py
Original procedural geometry interpreting the elevation (concept 14 x 12 m, fitted to the
12 x 10 x 10 m slot): Gothic ashlar house under a hipped slate roof; a tall half-timbered
cross gable with a four-light traceried window over a pinnacled stone porch and pointed
double door; a round corner tower with a slate cone; a polygonal bay window; a jettied
right wing with its own timbered gable; dormer, two stacks, banners, lanterns, downpipes.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,UP,tile_plane,deck,wall,frame,window,lantern
import bpy,numpy as np
scene=kit.reset(4143)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Northridge House-3.png'
asset=bpy.data.collections.new('SilverbrookNorthridgeHouse3');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_NorthridgeHouse3_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    trim=kit.scan('SB_NorthridgeHouse3_Dressed','sandstone_blocks_08',dark=(.45,.40,.33)),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_NorthridgeHouse3_Banner','denim_fabric_04',dark=(.10,.14,.32),damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_NorthridgeHouse3_Gilt',(.80,.55,.22),.32,1))
M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
TRIMS=kit.patches('sandstone_blocks_08',(.05,.035),dark=.8,frac=.01,std=.2)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
def trim():return kit.rng.choice(TRIMS)
G=dict(stone=Geo('SM_SBNorthridgeHouse3_Masonry',asset),trim=Geo('SM_SBNorthridgeHouse3_Dressed',asset),timber=Geo('SM_SBNorthridgeHouse3_Timber',asset),
       roof=Geo('SM_SBNorthridgeHouse3_Roof',asset),detail=Geo('SM_SBNorthridgeHouse3_Details',asset))
S,T,TB,D=G['stone'],G['trim'],G['timber'],G['detail']
# ---- measured layout (metres; concept at ~0.0112 m/px across, 0.0102 m/px up) -----------
SLOT=(12,10,10)                       # town-layout slot w x d x h (concept README 'Fits')
W2,FY,RY=5.3,-2.8,3.2                 # main block half width, front and rear wall faces
EM,KM,OV=5.0,.97,.3                   # main hip roof eave, pitch, overhang
MEX,MEY,MCY=W2+OV,(RY-FY)/2+OV,(RY+FY)/2
MR=EM+MEY*KM                          # main ridge 8.2
WT=EM+OV*KM-.04                       # wall plate: just under the roof at the wall line
GX,CH,CR,KC,CW,BF=.3,2.05,9.0,1.69,1.75,-3.8   # cross wing: axis, roof half, ridge, pitch, wall half, bay front
CE=CR-CH*KC
RGX,RH,RR,KR,RW,JY,JZ=3.69,1.35,6.9,1.38,1.1,-2.95,2.8   # right wing gable, jetty face and height
TX,TY,TRAD,TTOP=-3.73,-2.3,.95,6.95   # round tower
BX,BH,BFH,BY=-3.23,1.5,.9,-4.0        # bay window: centre x, back half, front half, front face
DX,DH,DW,DA,KD=-2.25,.5,.38,7.17,1.27 # dormer
CHIM=[(-4.6,.6,7.8),(4.25,.8,7.65)]   # stacks: x, y, top
def hipz(cx,cy,ex,ey,eave,k,p):return eave+k*min(ex-abs(p.x-cx),ey-abs(p.y-cy))
main_z=lambda p:hipz(0,MCY,MEX,MEY,EM,KM,p)
main_y=lambda z:MCY-MEY+(z-EM)/KM     # front slope: y at height z
cross_z=lambda x:CR-abs(x-GX)*KC
rg_z=lambda x:RR-abs(x-RGX)*KR
JT=main_z(Vector((W2,JY,0)))-.04       # jettied wall top under the main eave
under_main=lambda p:abs(p.x)<MEX and FY-OV-.05<p.y<RY+OV+.05 and p.z<main_z(p)-.01
in_cross=lambda p:abs(p.x-GX)<CW+.02 and BF-OV-.1<p.y<RY+OV+.1 and p.z<cross_z(p.x)-.005
in_rg=lambda p:abs(p.x-RGX)<RW+.02 and p.y<-.9 and p.z<rg_z(p.x)-.005
in_tower=lambda p:math.hypot(p.x-TX,p.y-TY)<TRAD+.04 and p.z<TTOP+.35
in_chim=lambda p:any(abs(p.x-x)<.42 and abs(p.y-y)<.38 for x,y,_ in CHIM)
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.4 else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
BIG=dict(size=(.34,.48,.022),exposure=.23)
def plane(O,e,u,L,lo,hi,covered,**kw):
    u=Vector(u).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8),**kw)
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n,cover=(),w=.26):
    a,b=Vector(a),Vector(b)
    for i in range(n):
        p,q=a+(b-a)*i/n,a+(b-a)*(i+1.04)/n
        if not any(c((p+q)/2) for c in cover):cap(p,q,w)
def hip(cx,cy,ex,ey,eave,ridge,cover=(),tiles={},capw=.26):
    """Hipped slate roof; ridge along the longer axis."""
    es,rise,rl,alongx=min(ex,ey),ridge-eave,abs(ex-ey),ex>=ey;L=math.hypot(es,rise)
    for out in [Vector((0,-1,0)),Vector((0,1,0)),Vector((1,0,0)),Vector((-1,0,0))]:
        side=out.x==0;half=ex if side else ey;r=rl if side==alongx else 0
        plane(Vector((cx,cy,eave))+out*(ey if side else ex),UP.cross(out),-out*es+UP*rise,L,lerp(-half,-r,L),lerp(half,r,L),cover,**tiles)
    ends=[Vector((cx+s*rl,cy,ridge+.05)) if alongx else Vector((cx,cy+s*rl,ridge+.05)) for s in (-1,1)];n=max(2,round(L/.45))
    if rl>.05:capped(ends[0],ends[1],max(1,round(2*rl/.45)),cover,capw)
    for sx in (-1,1):
        for sy in (-1,1):capped((cx+sx*ex,cy+sy*ey,eave+.05),ends[(sx if alongx else sy)>0],n,cover,capw)
def facets(apex,eaves,cover=(),capw=.12,closed=False,**kw):
    """Slate triangles from each eave edge (counter-clockwise from above) up to one apex: cones, bay roofs."""
    A=Vector(apex);pts=[Vector(p) for p in eaves];edges=list(zip(pts,pts[1:]+pts[:1]))[:len(pts) if closed else len(pts)-1]
    for P0,P1 in edges:
        e=P1-P0;l=e.length;e.normalize();s=(A-P0).dot(e);O=P0+e*s;u=A-O;L=u.length
        plane(O,e,u,L,(lambda a0:lambda t:a0*(1-t/L))(-s),(lambda a1:lambda t:a1*(1-t/L))(l-s),cover,**kw)
    for p in pts[(0 if closed else 1):(len(pts) if closed else -1)]:capped(p+UP*.04,A+UP*.04,max(2,round((A-p).length/.4)),cover,capw)
def lathe(geo,c,profile,mat,seg=24,smooth=True):
    """Surface of revolution, UVs at the material's physical scale; r=0 rows close the ends."""
    cx,cy,cz=c;sc=kit.SCALE[mat.name]
    for (ra,za),(rb,zb) in zip(profile,profile[1:]):
        if ra<1e-6 and rb<1e-6:continue
        rm=max((ra+rb)/2,1e-3)
        for j in range(seg):
            t0,t1=math.tau*j/seg,math.tau*(j+1)/seg;q=[(ra,t0,za),(ra,t1,za),(rb,t1,zb),(rb,t0,zb)]
            if ra<1e-6:q.pop(1)
            elif rb<1e-6:q.pop(3)
            geo.poly([(cx+r*math.cos(t),cy+r*math.sin(t),cz+z) for r,t,z in q],mat,[(rm*t/sc,z/sc) for r,t,z in q],smooth=smooth)
def pyramid(g,c,half,z0,apex,mat):
    cs=[Vector((c[0]+x*half,c[1]+y*half,z0)) for x,y in ((-1,-1),(1,-1),(1,1),(-1,1))];top=Vector((c[0],c[1],apex))
    for i in range(4):g.poly([cs[i],cs[(i+1)%4],top],mat)
    g.poly(cs[::-1],mat)
def finial(x,y,z0,z1,mat=None):
    """Forged fleur finial: spike, knop and two curled leaves."""
    mat=mat or M['iron'];D.beam((x,y,z0),(x,y,z1),.04,.04,mat,grain=False);h=z1-z0
    D.vessel((x,y,z0+h*.3),[(0,0),(.05,.03),(.07,.07),(.05,.11),(0,.14)],mat,10)
    for s in (-1,1):D.beam((x,y,z0+h*.55),(x+s*.11,y,z0+h*.72),.03,.03,mat,grain=False);D.beam((x+s*.11,y,z0+h*.72),(x+s*.06,y,z0+h*.8),.03,.03,mat,grain=False)
def pipe(x,y,z0,z1,wall_dir):
    """Cast-iron downpipe with hopper head, shoe and wall brackets; wall_dir points at the wall."""
    D.vessel((x,y,z0),[(.045,0),(.045,z1-z0)],M['iron'],10)
    D.box((x,y,z1+.1),(.16,.16,.2),M['iron'],.01);D.box((x,y,z0+.04),(.12,.12,.08),M['iron'],.01)
    wd=Vector(wall_dir)
    for z in np.linspace(z0+.6,z1-.2,max(2,round((z1-z0)/1.5))):D.box(Vector((x,y,z))+wd*.06,(.05+abs(wd.x)*.08,.05+abs(wd.y)*.08,.04),M['iron'],.004)
# ---- pointed (two-centred) arches, from the Town Hall builder ----------------------------
def arcpts(w,spring,off=0,n=9):
    """Right-hand arc of a pointed arch (radius .75 w), springing to apex, as (offset from axis, z)."""
    r,h=.75*w,w/2;c=h-r;R=r+off;t1=math.acos(-c/R)
    return [(c+R*math.cos(t),spring+R*math.sin(t)) for t in np.linspace(0,t1,n)]
def apex(w,spring,off=0):return arcpts(w,spring,off,2)[-1][1]
def zarc(w,spring,x):r,h=.75*w,w/2;return spring+math.sqrt(max(0,r*r-(abs(x)+r-h)**2))
def seg(g,A,B,o,radial,depth,mat):
    t=B-A;L=t.length;t.normalize();n=Vector(o).normalized();rot=Matrix((n.cross(t),n,t)).transposed()
    g.box((A+B)/2,(radial,depth,L+.012),mat,.01,rot,rect=trim())
def ring(P,R,o,a,w,spring,off,radial,dc,depth,jamb=None,n=9):
    pts=arcpts(w,spring,off,n)
    for s in (1,-1):
        for (x0,z0),(x1,z1) in zip(pts,pts[1:]):seg(T,P(a+s*x0,dc,z0),P(a+s*x1,dc,z1),o,radial,depth,M['trim'])
        if jamb is not None:T.box(P(a+s*(w/2+off),dc,(jamb+spring)/2),(radial,depth,spring-jamb),M['trim'],.012,R,rect=trim())
def pwall(g,c,o,half,height,openings,th):
    """kit.wall with pointed heads: openings are (a, width, bottom, spring)."""
    P,R=frame(c,o);holes=sorted((b,a,w,apex(w,s)) for a,w,b,s in openings)
    cuts=sorted({-half,half,*[a+d*w/2 for _,a,w,_ in holes for d in (-1,1)]})
    for a0,a1 in zip(cuts,cuts[1:]):
        z=0;spans=[]
        for b,a,w,top in holes:
            if abs(a-(a0+a1)/2)<w/2:spans.append((z,b));z=top
        for z0,z1 in spans+[(z,height)]:
            if z1-z0>.005:g.box(P((a0+a1)/2,-th/2,(z0+z1)/2),(a1-a0,th,z1-z0),M['stone'],0,R)
    for a,w,b,s in openings:
        top=apex(w,s)
        for sd in (1,-1):g.prism([P(a+sd*w/2,-th/2,top)]+[P(a+sd*x,-th/2,z) for x,z in arcpts(w,s)],o,th,M['stone'])
def diamonds(P,d,a,w,lo,hi,inside,step=.16,slope=1.7):
    """Diamond-quarry lead cames clipped to a convex light outline."""
    xs=np.linspace(a-w/2,a+w/2,90);K=int((hi-lo+slope*w)/(step*slope))+3
    for sign in (1,-1):
        for k in range(-K,K):
            b=lo+k*step*slope;pts=[(x,sign*slope*(x-a)+b) for x in xs if inside(x,sign*slope*(x-a)+b)]
            if len(pts)>=2 and math.hypot(pts[-1][0]-pts[0][0],pts[-1][1]-pts[0][1])>.03:
                D.beam(P(pts[0][0],d,pts[0][1]),P(pts[-1][0],d,pts[-1][1]),.011,.011,M['lead'],grain=False)
def pointed_glass(P,o,a,w,bottom,spring,dg):
    pts=arcpts(w,spring);D.prism([P(a-w/2,dg,bottom),P(a+w/2,dg,bottom)]+[P(a+x,dg,z) for x,z in pts]+[P(a-x,dg,z) for x,z in pts[::-1][1:]],o,.016,M['glass'])
    diamonds(P,dg+.016,a,w,bottom,apex(w,spring),lambda x,z:z>=bottom+.01 and abs(x-a)<=w/2-.01 and z<=zarc(w,spring,x-a)-.012)
def lancet(c,o,a,w,bottom,spring,th=.4,lights=1,decor=False,sill=True,hood=True):
    """Leaded Gothic window: lit glass, dressed mullions (tracery when lights >= 4), diamond
    cames, hood mould and sill. decor=True lays it on a solid face instead of in a hole."""
    P,R=frame(c,o);dg=.03 if decor else -th/2
    pointed_glass(P,o,a,w,bottom,spring,dg)
    for i in range(1,lights):
        x=-w/2+i*w/lights;zt=zarc(w,spring,x)
        if lights>=4:zt=apex(w/2,spring) if i!=lights//2 else spring+.37*w
        T.box(P(a+x,dg,(bottom+zt)/2),(.1,.16,zt-bottom),M['trim'],.012,R,rect=trim())
    if lights>=4:
        for s in (-1,1):
            q=arcpts(w/2,spring,0,7)
            for (x0,z0),(x1,z1) in zip(q,q[1:]):
                for m in (1,-1):seg(T,P(a+s*w/4+m*x0,dg,z0),P(a+s*w/4+m*x1,dg,z1),o,.09,.14,M['trim'])
        # quatrefoil in the head: four lobes round the oculus
        oc=spring+.53*w;rr=.16*w;k=14
        for j in range(k):
            t0,t1=math.tau*j/k,math.tau*(j+1)/k;seg(T,P(a+rr*math.cos(t0),dg,oc+rr*math.sin(t0)),P(a+rr*math.cos(t1),dg,oc+rr*math.sin(t1)),o,.08,.14,M['trim'])
        for j in range(4):
            t=math.pi/4+math.pi/2*j
            D.beam(P(a,dg+.03,oc),P(a+rr*.9*math.cos(t),dg+.03,oc+rr*.9*math.sin(t)),.05,.03,M['trim'],grain=False,rect=trim())
    if hood:ring(P,R,o,a,w,spring,.09,.15,.03 if not decor else .06,.1 if not decor else .08,jamb=bottom)
    if sill:T.box(P(a,.05 if not decor else .08,bottom-.06),(w+.36,.2 if not decor else .14,.12),M['trim'],.012,R,rect=trim())
def lights(c,o,a,w,bottom,top,n,dg,panel=True,fr=.07):
    """Timber-framed casement of n pointed leaded lights (bay faces, jetty and gable windows)."""
    P,R=frame(c,o)
    if panel:TB.box(P(a,dg-.03,(bottom+top)/2),(w,.04,top-bottom),M['timber'],.004,R,grain=True,band=band())
    lw=(w-(n+1)*fr)/n;sp=top-.04-(apex(lw,0))
    for i in range(n):pointed_glass(P,o,a-w/2+fr+lw/2+i*(lw+fr),lw,bottom,sp,dg)
    for i in range(n+1):TB.box(P(a-w/2+fr/2+i*(lw+fr),dg+.03,(bottom+top)/2),(fr,.1,top-bottom),M['timber'],.008,R,grain=True,band=band())
    TB.box(P(a,dg+.03,top+fr/2),(w+.04,.08,fr),M['timber'],.008,R,grain=True,band=band())
    TB.box(P(a,dg+.05,bottom-fr/2),(w+.12,.12,fr),M['timber'],.008,R,grain=True,band=band())
def planter(c,o,a,z,w):
    """Timber window box on two iron brackets (the flowers are settlement dressing)."""
    P,R=frame(c,o);TB.box(P(a,.13,z+.1),(w,.24,.2),M['timber'],.006,R,grain=True,band=band());D.box(P(a,.13,z+.195),(w-.06,.19,.02),M['soil'],0,R)
    for s in (-1,1):D.beam(P(a+s*(w/2-.12),.01,z-.28),P(a+s*(w/2-.12),.2,z+.01),.03,.03,M['iron'],grain=False)
def banner(c,o,a,ztop,zbot,w,d=.14):
    """Hanging cloth banner with a pointed foot, gilt edging and rod on iron brackets."""
    P,R=frame(c,o);pts=[(-w/2,ztop),(w/2,ztop),(w/2,zbot+.3),(0,zbot),(-w/2,zbot+.3)]
    D.prism([P(a+x,d,z) for x,z in pts],o,.02,M['banner'])
    for (x0,z0),(x1,z1) in zip(pts,pts[1:]+pts[:1]):D.beam(P(a+x0,d+.012,z0),P(a+x1,d+.012,z1),.03,.012,M['gilt'],grain=False)
    # stag device, simplified to a gilt roundel with a fleur below
    D.prism([P(a+.1*math.cos(t),d+.014,(ztop+zbot)/2+.2+.1*math.sin(t)) for t in np.linspace(0,math.tau,13)[:-1]],o,.008,M['gilt'])
    D.prism([P(a+x,d+.014,z) for x,z in ((0,(ztop+zbot)/2-.25),(.06,(ztop+zbot)/2-.08),(0,(ztop+zbot)/2),(-.06,(ztop+zbot)/2-.08))],o,.008,M['gilt'])
    D.beam(P(a-w/2-.08,d,ztop+.04),P(a+w/2+.08,d,ztop+.04),.04,.04,M['gilt'],grain=False)
    for s in (-1,1):D.box(P(a+s*(w/2+.03),d/2,ztop+.04),(.035,d+.04,.035),M['iron'],.004,R)
def post(P,R,a,d,z0,z1,w=.14,dep=.12):TB.box(P(a,d,(z0+z1)/2),(w,dep,z1-z0),M['timber'],.012,R,grain=True,band=band())
def rail(P,a0,a1,d,z0,z1,s=.14,dep=.12):TB.beam(P(a0,d,z0),P(a1,d,z1),dep,s,M['timber'],band=band())
def barge(x,y,half,eave,ridge,k):
    for s in (-1,1):TB.beam((x+s*(half+.05),y,eave-.08),(x-s*.16,y,ridge+.16*k+.03),.08,.26,M['timber'],band=band())

# ---- main hipped roof -------------------------------------------------------------
YD=main_y(5.65);DE=DA-DH*KD;DB=5.55
dormer_z=lambda x:DA-abs(x-DX)*KD
in_dorm=lambda p:abs(p.x-DX)<DW+.02 and YD-.2<p.y<0 and p.z<dormer_z(p.x)-.005
hip(0,MCY,MEX,MEY,EM,MR,[in_cross,in_rg,in_tower,in_dorm,in_chim],BIG)
# ---- cross wing: full-depth gable roof, taller than the main ridge -----------------------
LC=math.hypot(CH,CR-CE)
plane((GX-CH,0,CE),(0,-1,0),(CH,0,CR-CE),LC,lambda t:-(RY+OV),lambda t:-(BF-OV),[under_main],**BIG)
plane((GX+CH,0,CE),(0,1,0),(-CH,0,CR-CE),LC,lambda t:BF-OV,lambda t:RY+OV,[under_main],**BIG)
capped((GX,BF-OV,CR+.05),(GX,RY+OV,CR+.05),18)
for yb in (BF-OV-.02,RY+OV+.02):barge(GX,yb,CH,CE,CR,KC)
finial(GX,BF-OV-.02,CR+.2,9.92)
# side walls of the wing (visible as cheeks above the main roof at front and rear)
for s in (-1,1):pwall(S,(GX+s*CW,(BF+.4+RY-.4)/2,0),(s,0,0),(RY-BF-.8)/2,cross_z(GX+CW)-.06,[],.4)
# front: bay wall with the door and great window, gable face above the wall plate
WIN=(0,2.13,3.7,5.3);DOOR=(0,1.2,0,1.78);WA=apex(WIN[1],WIN[3])
pwall(S,(GX,BF,0),(0,-1,0),CW,WT,[DOOR,WIN],.4)
gx=lambda z:(CR-.05-z)/KC;gzw=CR-.05-CW*KC;hw=WIN[1]/2
for piece in ([(-CW,WT),(-hw,WT),(-hw,WA),(-gx(WA),WA),(-CW,gzw)],[(hw,WT),(CW,WT),(CW,gzw),(gx(WA),WA),(hw,WA)],[(-gx(WA),WA),(gx(WA),WA),(0,CR-.05)]):
    S.prism([(GX+x,BF+.2,z) for x,z in piece],(0,1,0),.4,M['stone'])
lancet((GX,BF,0),(0,-1,0),0,WIN[1],WIN[2],WIN[3],lights=4)
planter((GX,BF,0),(0,-1,0),0,3.36,2.3)
P,R=frame((GX,BF,0),(0,-1,0));d=.06
for s in (-1,1):
    post(P,R,s*1.66,d,2.95,gzw-.04)
    rail(P,s*1.73,s*1.33,d,5.35,5.35)
    TB.box(P(s*1.86,.12,CE-.2),(.16,.36,.22),M['timber'],.012,R,grain=True,band=band())     # eave corbels
    rail(P,s*.9,s*.1,d,7.3,8.3,.12)
z=7.2;rail(P,-gx(z)+.02,gx(z)-.02,d,z,z,.16);post(P,R,0,d,z+.08,CR-.12,.16)
# rear gable
S.prism([(GX+x,RY-.2,z) for x,z in [(-CW,WT),(CW,WT),(CW,gzw),(0,CR-.05),(-CW,gzw)]],(0,1,0),.4,M['stone'])
lancet((GX,RY,0),(0,1,0),0,.55,6.0,6.5,decor=True)
Pr,Rr=frame((GX,RY,0),(0,1,0))
z=7.25;rail(Pr,-gx(z)+.02,gx(z)-.02,d,z,z,.16);post(Pr,Rr,0,d,z+.08,CR-.12,.16)
for s in (-1,1):post(Pr,Rr,s*1.66,d,WT-.3,gzw-.04)
TB.box(Pr(0,d,WT-.2),(2*CW+.1,.14,.2),M['timber'],.012,Rr,grain=True,band=band())
# ---- porch: pinnacled stone hood over the pointed double door -----------------------------
PY0,PHW,PT,PGA=-4.4,1.35,2.85,3.2
pwall(S,(GX,PY0,0),(0,-1,0),PHW,PT,[(0,1.5,0,1.75)],.62)
Pp,Rp=frame((GX,PY0,0),(0,-1,0))
ring(Pp,Rp,(0,-1,0),0,1.5,1.75,.1,.2,.05,.1,jamb=.48,n=11)
T.prism([(GX+x,PY0+.31,z) for x,z in [(-PHW,PT),(PHW,PT),(0,PGA)]],(0,1,0),.62,M['trim'])
for s in (-1,1):T.beam((GX+s*(PHW+.08),PY0+.3,PT-.04),(GX,PY0+.3,PGA+.04),.16,.7,M['trim'],grain=False,rect=trim())
T.box((GX,PY0+.3,PGA+.12),(.3,.74,.26),M['trim'],.012,rect=trim())   # apex block over the mitre
for s in (-1,1):
    bx=GX+s*1.5;T.box((bx,-4.215,1.65),(.34,.87,3.3),M['trim'],.015)
    T.box((bx,-4.215,3.36),(.42,.95,.12),M['trim'],.012,rect=trim())
    T.box((bx,-4.3,3.55),(.26,.26,.26),M['trim'],.012,rect=trim());pyramid(T,(bx,-4.3),.15,3.68,4.0,M['trim']);finial(bx,-4.3,3.98,4.3)
    banner((bx,-4.65,0),(0,-1,0),0,2.65,1.25,.44,d=.1)
    lantern(G,M,(GX+s*1.05,PY0-.42,2.2),arm=((GX+s*1.05,PY0-.09,2.2),(0,-1,0)))
for (hx,y0,y1,h) in ((1.15,-3.45,-4.75,.48),(1.2,-4.75,-5.05,.32),(1.25,-5.05,-5.35,.16)):T.box((GX,(y0+y1)/2,h/2),(2*hx,y0-y1,h),M['trim'],.012)
# door: pointed ledged double leaf in the bay wall, iron straps and ring pulls
Pd,Rd=frame((GX,BF,0),(0,-1,0));w,sp=DOOR[1],DOOR[3];pts=arcpts(w,sp);top=pts[-1][1]
for s in (-1,1):
    leaf=[(0,.48),(s*w/2,.48),(s*w/2,sp)]+[(s*x,z) for x,z in pts[1:-1]]+[(0,top)]
    D.prism([Pd(x+s*.005,-.2,z) for x,z in leaf],(0,-1,0),.07,M['door'])
    for z in (.85,1.75):D.box(Pd(s*w*.27,-.155,z),(w*.42,.014,.06),M['iron'],.004,Rd)
    for k in range(10):t=math.tau*k/10;D.box(Pd(s*.13+.055*math.cos(t),-.15,1.25+.055*math.sin(t)),(.035,.012,.012),M['iron'],0,Rd@Matrix.Rotation(t,3,'Y'))
    D.box(Pd(s*.13,-.158,1.31),(.06,.008,.06),M['iron'],.004,Rd)
# ---- round corner tower with slate cone ----------------------------------------------
lathe(S,(TX,TY,0),[(0,0),(TRAD,0),(TRAD,TTOP),(0,TTOP)],M['stone'],24)
lathe(T,(TX,TY,0),[(TRAD-.02,TTOP-.1),(TRAD+.13,TTOP),(TRAD+.13,TTOP+.3),(0,TTOP+.3)],M['trim'],24,False)
for j in range(16):
    t=math.tau*(j+.5)/16;TB.box((TX+(TRAD+.06)*math.cos(t),TY+(TRAD+.06)*math.sin(t),TTOP-.2),(.14,.1,.2),M['timber'],.01,Matrix.Rotation(t+math.pi/2,3,'Z'),grain=True,band=band())
CONE=[(TX+1.25*math.cos(math.tau*j/16),TY+1.25*math.sin(math.tau*j/16),TTOP+.3) for j in range(16)]
facets((TX,TY,9.05),CONE,closed=True,capw=.1,size=(.22,.32,.02),exposure=.14)
finial(TX,TY,9.0,9.92)
for lo,sp in ((3.7,4.45),(5.6,6.2)):lancet((TX,TY-TRAD,0),(0,-1,0),0,.4,lo,sp,decor=True)
lancet((TX-TRAD,TY,0),(-1,0,0),0,.4,5.6,6.2,decor=True)
# ---- bay window: stone base, oak-framed pointed lights, hipped half-cone of slate -----------
BW=[(BX-BH,FY+.05),(BX-BFH,BY),(BX+BFH,BY),(BX+BH,FY+.05)]
def offset(poly,dist):
    """Offset a convex counter-clockwise outline outward by dist (inward if negative)."""
    out=[];n=len(poly)
    for i in range(n):
        lines=[]
        for a,b in ((poly[i-1],poly[i]),(poly[i],poly[(i+1)%n])):
            ex,ey=b[0]-a[0],b[1]-a[1];L=math.hypot(ex,ey);nx,ny=ey/L,-ex/L;lines.append(((a[0]+nx*dist,a[1]+ny*dist),(ex,ey)))
        (p1,d1),(p2,d2)=lines;den=d1[0]*d2[1]-d1[1]*d2[0];t=((p2[0]-p1[0])*d2[1]-(p2[1]-p1[1])*d2[0])/den;out.append((p1[0]+d1[0]*t,p1[1]+d1[1]*t))
    return out
def slab(geo,poly,z0,z1,mat):geo.prism([(x,y,(z0+z1)/2) for x,y in poly],(0,0,1),z1-z0,mat)
slab(S,BW,0,.9,M['stone']);slab(T,offset(BW,.05),.86,.98,M['trim']);slab(TB,offset(BW,-.16),.98,2.42,M['timber']);slab(TB,BW,2.42,2.72,M['timber'])
for i in range(3):
    a,b=Vector((*BW[i],0)),Vector((*BW[i+1],0));c=(a+b)/2;o=Vector(((b-a).y,-(b-a).x,0)).normalized()
    lights(c,o,0,(b-a).length-.3,1.02,2.38,2,-.15,panel=False)
for p in BW[1:3]:TB.box((*p,1.7),(.16,.16,1.44),M['timber'],.012,grain=True,band=band())
BE=offset(BW,.2);BE=[(BE[0][0],FY+.25),BE[1],BE[2],(BE[3][0],FY+.25)]
facets((BX,FY+.2,3.6),[(x,y,2.66) for x,y in BE],[in_tower],capw=.12,size=(.26,.36,.02),exposure=.17)
finial(BX,-3.2,3.3,3.95)
# ---- main walls -------------------------------------------------------------------
A0,A1=-W2,1.65;ML=(-2.16,.5,3.65,4.32)
pwall(S,((A0+A1)/2,FY,0),(0,-1,0),(A1-A0)/2,WT,[(ML[0]-(A0+A1)/2,*ML[1:])],.4)
lancet((0,FY,0),(0,-1,0),*ML);planter((0,FY,0),(0,-1,0),ML[0],3.25,.75)
B0,B1=GX+CW,W2;bc=(B0+B1)/2;LOW=[(x,.56,.85,1.72) for x in (3.16,4.22)]
pwall(S,(bc,FY,0),(0,-1,0),(B1-B0)/2,JZ,[(x-bc,*r) for x,*r in LOW],.4)
for h in LOW:lancet((0,FY,0),(0,-1,0),*h)
planter((0,FY,0),(0,-1,0),RGX,.5,1.7)
wall(S,M['stone'],(bc,JY,JZ),(0,-1,0),(B1-B0)/2,JT-JZ,[(RGX-bc,1.06,3.35-JZ,4.47-JZ)],th=.4)
lights((RGX,JY,0),(0,-1,0),0,1.06,3.35,4.47,2,-.2)
planter((RGX,JY,0),(0,-1,0),0,3.08,1.3)
pwall(S,(0,RY,0),(0,1,0),W2,WT,[(-x,.6,lo,sp) for x in (-3.6,GX,3.6) for lo,sp in ((1.0,2.0),(3.3,4.1))],.4)
for x in (-3.6,GX,3.6):
    for lo,sp in ((1.0,2.0),(3.3,4.1)):lancet((0,RY,0),(0,1,0),-x,.6,lo,sp)
for s in (-1,1):
    c=(s*W2,MCY,0);o=(s,0,0);ys=(-1.0,1.6);pwall(S,c,o,(RY-FY)/2-.4,WT,[(s*(y-MCY),.6,lo,sp) for y in ys for lo,sp in ((1.0,2.0),(3.3,4.1))],.4)
    for y in ys:
        for lo,sp in ((1.0,2.0),(3.3,4.1)):lancet(c,o,s*(y-MCY),.6,lo,sp)
# ---- right wing: jetty, timber framing and its own gable --------------------------------
Pj,Rj=frame((0,JY,0),(0,-1,0))
TB.box(Pj(bc,.1,JZ),(B1-B0+.1,.2,.2),M['timber'],.015,Rj,grain=True,band=band())        # bressumer
for x in (B0+.15,RGX,B1-.12):TB.beam(Pj(x,-.15,JZ-.5),Pj(x,.12,JZ-.1),.12,.12,M['timber'],band=band())
for x in (B0+.12,RGX-.6,RGX+.6,B1-.1):post(Pj,Rj,x,.06,JZ+.1,JT-.12)
TB.box(Pj(bc,.06,JT-.08),(B1-B0,.12,.16),M['timber'],.012,Rj,grain=True,band=band())
for s in (-1,1):rail(Pj,RGX+s*(RW+.5),RGX+s*.7,.06,JZ+.15,JT-.2,.12)
rgw=rg_z(RGX+RW)-.05
S.prism([(RGX+x,JY+.2,z) for x,z in [(-RW,JT),(RW,JT),(RW,rgw),(0,RR-.05),(-RW,rgw)]],(0,1,0),.4,M['stone'])
for s in (-1,1):post(Pj,Rj,RGX+s*(RW-.07),.06,JT,rgw-.02)
z=6.08;rail(Pj,RGX-(RR-.05-z)/KR+.03,RGX+(RR-.05-z)/KR-.03,.06,z,z,.14);post(Pj,Rj,RGX,.06,z+.07,RR-.12,.12)
lights((RGX,JY,0),(0,-1,0),0,.4,5.35,5.85,1,.04)
LR=math.hypot(RH,RR-rg_z(RGX+RH));RE=rg_z(RGX+RH);YB=main_y(RR+.15)
plane((RGX-RH,0,RE),(0,-1,0),(RH,0,RR-RE),LR,lambda t:-YB,lambda t:-(JY-OV),[under_main],**BIG)
plane((RGX+RH,0,RE),(0,1,0),(-RH,0,RR-RE),LR,lambda t:JY-OV,lambda t:YB,[under_main],**BIG)
capped((RGX,JY-OV,RR+.05),(RGX,YB,RR+.05),5,[under_main])
barge(RGX,JY-OV-.02,RH,RE,RR,KR);finial(RGX,JY-OV-.02,RR+.18,RR+.75)
# ---- dormer on the left slope ---------------------------------------------------------
LD=math.hypot(DH,DA-DE);YDB=main_y(DA+.12)
plane((DX-DH,0,DE),(0,-1,0),(DH,0,DA-DE),LD,lambda t:-YDB,lambda t:-YD+.16,[under_main])
plane((DX+DH,0,DE),(0,1,0),(-DH,0,DA-DE),LD,lambda t:YD-.16,lambda t:YDB,[under_main])
capped((DX,YD-.16,DA+.04),(DX,YDB,DA+.04),4,[under_main],.2)
barge(DX,YD-.18,DH,DE,DA,KD);finial(DX,YD-.18,DA+.15,7.85)
wall(S,M['stone'],(DX,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.54,.21,.84)],th=.12)
S.prism([(DX-DW,YD+.06,DE+.02),(DX+DW,YD+.06,DE+.02),(DX+DW,YD+.06,dormer_z(DX+DW)-.04),(DX,YD+.06,DA-.04),(DX-DW,YD+.06,dormer_z(DX+DW)-.04)],(0,1,0),.12,M['stone'])
ym=main_y(dormer_z(DX+DW)-.04)
for s in (-1,1):S.prism([(DX+s*DW,YD,DB),(DX+s*DW,YD,dormer_z(DX+DW)-.04),(DX+s*DW,ym,dormer_z(DX+DW)-.04)],(1,0,0),.08,M['stone'])
for s in (-1,1):TB.box((DX+s*(DW-.05),YD-.02,(DB+DE)/2),(.1,.1,DE-DB),M['timber'],.01,grain=True,band=band())
TB.box((DX,YD-.03,DE-.02),(2*DW+.08,.13,.12),M['timber'],.01,grain=True,band=band())
window(G,M,(DX,YD,DB+.3),(0,-1,0),.36,.45,shutters=False,wall_th=.12,band=band())
# ---- stacks ---------------------------------------------------------------------------
for x,y,top in CHIM:
    base=main_z(Vector((x,y,0)))-.3;S.box((x,y,(base+top)/2),(.78,.68,top-base),M['stone'],.01,w=(.3,0))
    T.box((x,y,top-.55),(.84,.74,.1),M['trim'],.015,rect=trim());T.box((x,y,top+.06),(.9,.8,.12),M['trim'],.015,rect=trim(),w=(.45,0))
    for dx in (-.17,.17):D.vessel((x+dx,y,top+.12),[(.1,0),(.095,.3),(.105,.38),(.12,.42),(.09,.42),(.075,.36),(0,.36)],M['pot'])
# ---- downpipes --------------------------------------------------------------------------
for x,y,top,wd in ((-W2-.06,FY+.2,EM-.25,(1,0,0)),(-2.62,FY-.08,EM-.25,(0,1,0)),(GX-CW-.06,-3.55,CE-.25,(1,0,0)),(GX+CW+.06,-3.55,CE-.25,(-1,0,0)),(W2+.06,FY+.2,EM-.25,(-1,0,0))):
    pipe(x,y,0,top,wd)
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'ground not at z=0: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'house triangle budget: {tris}'
OUT=HERE/'SilverbrookNorthridgeHouse3'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a 60 MB GLB.
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-40,-40,0),(40,-40,0),(40,40,0),(-40,40,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,target,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=(Vector(target)-Vector(loc)).to_track_quat('-Z','Y').to_euler();pre.objects.link(o);c.clip_end=300
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-13,-19,5.5),(0,0,4.6),lens=36);cam('Front',(0,-50,5.0),(0,0,5.0),12.4);cam('Side',(-50,0,5.0),(0,0,5.0),12.4);cam('Rear',(0,50,5.0),(0,0,5.0),12.4)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'NorthridgeHouse3-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
manifest=dict(asset='SilverbrookNorthridgeHouse3',reference=dict(file='NorthridgeHouse3-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_northridgehouse3.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookNorthridgeHouse3',slot_m=list(SLOT),outputs=['SilverbrookNorthridgeHouse3.blend','SilverbrookNorthridgeHouse3.glb','SilverbrookNorthridgeHouse3.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('NORTHRIDGEHOUSE3_BUILT',manifest['bounds_m'],tris,{o.name:sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs})
