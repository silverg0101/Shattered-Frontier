"""Silverbrook Northridge House 4 from docs/Game-Design/concept-art/Silverbrook/Northridge House-4.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse4.py
Original procedural geometry interpreting the elevation (14 m wide, ~14 m to the finials;
widths x0.8, heights compressed ~0.7 to fit the 12 x 10 x 10 m slot): H-plan Gothic manor in
pale rough-faced ashlar. Two steep front cross wings with heavy timber gable framing, tall
traceried lancets, canted bay windows with hipped slate roofs; a recessed centre with a
pointed-arch porch, stone balcony and balustrade, double door up a flight of steps, a
central dormer, four stacks with clay pots, banners and lanterns.
"""
import sys,math,json,hashlib,shutil
import numpy as np
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,UP,tile_plane,deck,frame,lantern
import bpy
scene=kit.reset(4904)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Northridge House-4.png'
asset=bpy.data.collections.new('SilverbrookNorthridgeHouse4');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_NorthridgeHouse4_Slate','grey_roof_tiles_02',dark=(.08,.11,.27),lichen='lichen_rock',damp=False,aged=.6),
    stone=kit.scan('SB_NorthridgeHouse4_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_NorthridgeHouse4_Banner','denim_fabric_03',dark=(.10,.12,.30),normal=.6,damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_NorthridgeHouse4_Gilt',(.80,.62,.30),.35,1.0))
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_05',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.0)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
def stone():return kit.rng.choice(STONES)
G=dict(stone=Geo('SM_SBNorthridgeHouse4_Masonry',asset),timber=Geo('SM_SBNorthridgeHouse4_Timber',asset),roof=Geo('SM_SBNorthridgeHouse4_Roof',asset),detail=Geo('SM_SBNorthridgeHouse4_Details',asset))
S,T,D=G['stone'],G['timber'],G['detail']
# ---- measured layout (metres; elevation ~65 px/m, widths x0.8, heights ~x0.7) ----------
SLOT=(12,10,10)                # town-layout slot w x d x h (concept README 'Fits')
XW,WH=3.5,2.05                 # cross wings: centre |x|, wall half width
YF,YB=-3.3,3.3                 # wing front / rear wall faces
CF,CB,XC=-2.7,2.7,1.45         # recessed centre block front / rear faces, half width
WE,WR,WO=6.2,9.1,.3            # wing roofs: eave (at the overhang), ridge, side overhang
KW=(WR-WE)/(WH+WO);GY0,GY1=YF-.35,YB+.35
WP=WR-KW*WH-.05;WA=WR-.05      # wing wall plate and gable-wall apex (just under the roof plane)
CE,CR,CY=6.2,8.4,3.0           # centre roof: eave, ridge, eave half-depth
KC=(CR-CE)/CY;CP=CR-KC*(-CF)-.05
FLOOR=4.2                      # first-floor bressumer / balcony level
def wing_z(x):return WR-KW*abs(abs(x)-XW)
def centre_z(y):return CR-KC*abs(y)
STACKS=[(s*4.85,.5,.4,.35,9.0) for s in (-1,1)]+[(s*1.97,.9,.3,.3,8.75) for s in (-1,1)]
in_stacks=[(lambda p,x=x,y=y,hx=hx,hy=hy:abs(p.x-x)<hx+.02 and abs(p.y-y)<hy+.02) for x,y,hx,hy,_ in STACKS]
in_wing=lambda p:abs(abs(p.x)-XW)<WH+.02 and abs(p.y)<GY1+.02 and p.z<wing_z(p.x)-.005
under_centre=lambda p:abs(p.x)<XW-.1 and abs(p.y)<CY and p.z<centre_z(p.y)-.01
def lichen(p):return kit.rng.uniform(.4,.8) if kit.rng.random()<.25 else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
BIG=dict(size=(.34,.46,.022),exposure=.22)
def plane(O,e,toward,L,lo,hi,covered,**kw):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.55,.9),**kw)
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.6,.9),.2))
def capped(a,b,n,w=.26):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n,w)
def post(x,y,z0,z1,s=.16,d=.12):T.box((x,y,(z0+z1)/2),(s,d,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):T.beam(a,b,.12,s,M['timber'],band=band())
def came(A,B):
    A,B=Vector(A),Vector(B);D.box((A+B)/2,(.009,.012,(B-A).length),M['lead'],0,(B-A).to_track_quat('Z','Y').to_matrix())
def cames(P,ac,pw,h,d,z0):
    """Diamond quarries clipped to one pane (as kit.window); unbevelled cames keep the budget."""
    lo,hi=ac-pw/2,ac+pw/2;slope=1.7;step=.10*slope
    for sign in (1,-1):
        for k in range(-40,41):
            b=k*step;pts=[]
            for a in (lo,hi):
                z=sign*slope*(a-ac)+b
                if 0<=z<=h:pts.append((a,z))
            for z in (0,h):
                a=ac+(z-b)/(sign*slope)
                if lo<a<hi:pts.append((a,z))
            if len(pts)>=2:
                (a0,za),(a1,zb)=pts[0],pts[1]
                if math.hypot(a1-a0,zb-za)>.02:came(P(a0,d,z0+za),P(a1,d,z0+zb))
# ---- pointed (two-centred) arches, after TownHall --------------------------------------
def arcpts(w,spring,off=0,n=9):
    """Right-hand arc of a pointed arch (radius .75 w), springing to apex, as (offset from axis, z)."""
    r,h=.75*w,w/2;c=h-r;R_=r+off;t1=math.acos(-c/R_)
    return [(c+R_*math.cos(t),spring+R_*math.sin(t)) for t in np.linspace(0,t1,n)]
def apex(w,spring,off=0):return arcpts(w,spring,off,2)[-1][1]
def seg(g,A,B,o,radial,depth,mat):
    t=B-A;L=t.length;t.normalize();n=Vector(o).normalized();rot=Matrix((n.cross(t),n,t)).transposed()
    g.box((A+B)/2,(radial,depth,L+.012),mat,.01,rot,rect=stone())
def ring(P,R,o,a,w,spring,off,radial,dc,depth,jamb=None,n=9):
    pts=arcpts(w,spring,off,n)
    for s in (1,-1):
        for (x0,z0),(x1,z1) in zip(pts,pts[1:]):seg(S,P(a+s*x0,dc,z0),P(a+s*x1,dc,z1),o,radial,depth,M['stone'])
        if jamb is not None:S.box(P(a+s*(w/2+off),dc,(jamb+spring)/2),(radial,depth,spring-jamb),M['stone'],.012,R,rect=stone())
def pwall(c,o,half,height,openings,th=.3):
    """kit.wall with pointed heads: openings are (a, width, bottom, spring)."""
    P,R=frame(c,o);holes=sorted((b,a,w,apex(w,s)) for a,w,b,s in openings)
    cuts=sorted({-half,half,*[a+d*w/2 for _,a,w,_ in holes for d in (-1,1)]})
    for a0,a1 in zip(cuts,cuts[1:]):
        z=0;spans=[]
        for b,a,w,top in holes:
            if abs(a-(a0+a1)/2)<w/2:spans.append((z,b));z=top
        for z0,z1 in spans+[(z,height)]:
            if z1-z0>.005:S.box(P((a0+a1)/2,-th/2,(z0+z1)/2),(a1-a0,th,z1-z0),M['stone'],0,R)
    for a,w,b,s in openings:
        top=apex(w,s)
        for sd in (1,-1):S.prism([P(a+sd*w/2,-th/2,top)]+[P(a+sd*x,-th/2,z) for x,z in arcpts(w,s)],o,th,M['stone'])
    return P,R
def gwall(c,o,half,eave,top,openings,th=.3):
    """Gable wall: pwall to the eave, then vertical strips under the rake (and over any arch that
    rises past the eave), so no stone stands above the roof planes."""
    P,R=pwall(c,o,half,eave,openings,th);g=lambda a:top-(top-eave)*abs(a)/half
    cuts=sorted({-half,0,half,*[a+d*w/2 for a,w,_,_ in openings for d in (-1,1)]})
    for a0,a1 in zip(cuts,cuts[1:]):
        hole=next((h for h in openings if abs(h[0]-(a0+a1)/2)<h[1]/2),None);z0=max(eave,apex(hole[1],hole[3])) if hole else eave
        pts=[(a0,z0),(a1,z0)]+[(a,g(a)) for a in (a1,a0) if g(a)>z0+.003]
        if len(pts)>=3:S.prism([P(a,-th/2,z) for a,z in pts],o,th,M['stone'])
    return P,R
def lancet(c,o,a,w,bottom,spring,th=.3,lights=2,decor=False,sill=True,hood=True):
    """Leaded Gothic window (TownHall's, with diamond quarries): lit glass, stone mullions,
    tracery when lights >= 4, moulded hood and sill. decor=True lays it on a solid face."""
    P,R=frame(c,o);dg=.03 if decor else -th/2;pts=arcpts(w,spring)
    outline=[P(a-w/2,dg,bottom),P(a+w/2,dg,bottom)]+[P(a+x,dg,z) for x,z in pts]+[P(a-x,dg,z) for x,z in pts[::-1][1:]]
    D.prism(outline,o,.02,M['glass'])
    def zarc(x):r,h=.75*w,w/2;return spring+math.sqrt(max(0,r*r-(abs(x)+r-h)**2))
    for i in range(1,lights):
        x=-w/2+i*w/lights;zt=zarc(x)
        if lights>=4:zt=apex(w/2,spring) if i!=lights//2 else spring+.37*w
        S.box(P(a+x,dg,(bottom+zt)/2),(.11,.16,zt-bottom),M['stone'],.012,R,rect=stone())
    if lights>=4:
        for s in (-1,1):
            q=arcpts(w/2,spring,0,7)
            for (x0,z0),(x1,z1) in zip(q,q[1:]):
                for m in (1,-1):seg(S,P(a+s*w/4+m*x0,dg,z0),P(a+s*w/4+m*x1,dg,z1),o,.09,.14,M['stone'])
        oc=spring+.53*w;rr=.16*w;k=14
        for j in range(k):
            t0,t1=math.tau*j/k,math.tau*(j+1)/k;seg(S,P(a+rr*math.cos(t0),dg,oc+rr*math.sin(t0)),P(a+rr*math.cos(t1),dg,oc+rr*math.sin(t1)),o,.08,.14,M['stone'])
    lw=w/lights
    for i in range(lights):cames(P,a-w/2+(i+.5)*lw,lw-(.11 if lights>1 else .02),spring-bottom,dg+.02,bottom)
    if hood:ring(P,R,o,a,w,spring,.09,.15,.03 if not decor else .06,.1 if not decor else .08,jamb=bottom)
    if sill:S.box(P(a,.05 if not decor else .08,bottom-.06),(w+.36,.2 if not decor else .14,.12),M['stone'],.012,R,rect=stone())
def planter(P,R,a,w,z,d=.26):
    """Timber window box with soil, resting on the ledge below it at z."""
    T.box(P(a,d,z+.13),(w,.24,.26),M['timber'],.006,R)
    D.box(P(a,d,z+.25),(w-.06,.19,.02),M['soil'],0,R)
def finial(x,y,z0,spike):
    post(x,y,z0,z0+.42,.15,.15);T.box((x,y,z0+.45),(.22,.22,.06),M['timber'],.015,grain=True,band=band())
    D.beam((x,y,z0+.48),(x,y,spike),.035,.035,M['iron'],grain=False);D.box((x,y,z0+.62),(.08,.08,.08),M['iron'],.01)
    for s in (-1,1):D.beam((x,y,spike-.2),(x+s*.09,y,spike-.08),.02,.02,M['iron'],grain=False)   # fleur-de-lis arms
def stag(origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R_,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R_*(a*s)+U*(z*s)
    D.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        D.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            D.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def banner(c,o,a,ztop,zbot,w,d=.14):
    """Navy cloth banner with a pointed foot, gilt edging, rod on iron brackets and a gilt stag."""
    P,R=frame(c,o);pts=[(-w/2,ztop),(w/2,ztop),(w/2,zbot+.3),(0,zbot),(-w/2,zbot+.3)]
    D.prism([P(a+x,d,z) for x,z in pts],o,.02,M['banner'])
    for (x0,z0),(x1,z1) in zip(pts,pts[1:]+pts[:1]):D.beam(P(a+x0,d+.012,z0),P(a+x1,d+.012,z1),.03,.012,M['gilt'],grain=False)
    D.beam(P(a-w/2-.08,d,ztop+.05),P(a+w/2+.08,d,ztop+.05),.04,.04,M['gilt'],grain=False)
    for s in (-1,1):D.box(P(a+s*(w/2+.04),d/2,ztop+.05),(.04,d+.04,.04),M['iron'],.004,R)
    stag(P(a,d+.012,(ztop+zbot)/2+.05),R@Vector((1,0,0)),UP,o,w*.75)

# ---- cross-wing roofs ---------------------------------------------------------------------
LW=math.hypot(WH+WO,WR-WE)
for s in (-1,1):
    cx=s*XW
    plane((cx-WH-WO,0,WE),(0,-1,0),(WH+WO,0,WR-WE),LW,lambda t:-GY1,lambda t:-GY0,[under_centre]+in_stacks,**BIG)
    plane((cx+WH+WO,0,WE),(0,1,0),(-(WH+WO),0,WR-WE),LW,lambda t:GY0,lambda t:GY1,[under_centre]+in_stacks,**BIG)
    capped((cx,GY0,WR+.05),(cx,GY1,WR+.05),16)
    xo=cx+s*(WH+WO+.02)
    T.beam((xo,GY0,WE-.06),(xo,GY1,WE-.06),.04,.2,M['timber'],band=band())             # outer fascia
    D.beam((xo+s*.03,GY0+.1,WE-.12),(xo+s*.03,GY1-.1,WE-.12),.1,.08,M['iron'],grain=False)   # gutter
    for y in (GY0,GY1):                                                                   # bargeboards
        yb=y-.04 if y<0 else y+.04
        for sd in (-1,1):
            T.beam((cx+sd*(WH+WO),yb,WE-.08),(cx-sd*.16,yb,WR+.16*KW+.03),.09,.26,M['timber'],band=band())
            T.box((cx+sd*(WH+WO-.05),yb,WE-.3),(.18,.18,.36),M['timber'],.015,grain=True,band=band())   # barge feet
    finial(cx,GY0-.06,WR+.05,9.95);finial(cx,GY1+.06,WR+.05,9.7)
# ---- centre roof and dormer --------------------------------------------------------------
roof_y=lambda z:-(CR-z)/KC
def dormer(x,yf,base,half,pitch,apex_z):
    """Gabled stone dormer: face, slate slabs and cheeks running back into the centre roof."""
    zt=apex_z-half*pitch;ov=.2;zs=lambda u:apex_z+.06-abs(u-x)*pitch
    S.prism([(x-half,yf+.15,base),(x+half,yf+.15,base),(x+half,yf+.15,zt),(x,yf+.15,apex_z),(x-half,yf+.15,zt)],(0,1,0),.3,M['stone'])
    y0,y1=yf-.2,roof_y(apex_z+.06)
    for s in (-1,1):
        e=x+s*(half+ov);G['roof'].prism([(e,y0,zs(e)),(x,y0,zs(x)),(x,y1,zs(x)),(e,y1,zs(e))],(s*pitch,0,1),.1,M['slate'])
        S.prism([(x+s*(half-.1),yf+.3,base),(x+s*(half-.1),yf+.3,zt),(x+s*(half-.1),roof_y(zt+.1),zt)],(1,0,0),.2,M['stone'])
        T.beam((e+s*.02,yf-.23,zs(e)-.05),(x-s*.06,yf-.23,apex_z+.12+.06*pitch),.07,.2,M['timber'],band=band())
    return lambda p:abs(p.x-x)<half+ov+.02 and yf<p.y<y1+.05 and p.z<zs(p.x)-.005
DYF=-1.7;in_dormer=dormer(0,DYF,centre_z(DYF),.42,1.3,8.3)
lancet((0,DYF,0),(0,-1,0),0,.3,7.27,7.5,lights=1,decor=True,sill=False)
D.beam((0,DYF-.23,8.42),(0,DYF-.23,9.05),.03,.03,M['iron'],grain=False);D.box((0,DYF-.23,8.62),(.07,.07,.07),M['iron'],.01)
LC=math.hypot(CY,CR-CE)
plane((0,-CY,CE),(1,0,0),(0,CY,CR-CE),LC,lambda t:-3.4,lambda t:3.4,[in_wing,in_dormer]+in_stacks,**BIG)
plane((0,CY,CE),(-1,0,0),(0,-CY,CR-CE),LC,lambda t:-3.4,lambda t:3.4,[in_wing]+in_stacks,**BIG)
XR=XW-(WR-CR)/KW               # where the centre ridge runs into the wing roofs
capped((-XR,0,CR+.05),(XR,0,CR+.05),13)
for y in (-CY-.02,CY+.02):
    sy=1 if y>0 else -1
    T.beam((-XC,y,CE-.06),(XC,y,CE-.06),.04,.2,M['timber'],band=band())
    D.beam((-XC+.05,y+sy*.06,CE-.12),(XC-.05,y+sy*.06,CE-.12),.1,.08,M['iron'],grain=False)
# ---- stacks ---------------------------------------------------------------------------------
for x,y,hx,hy,top in STACKS:
    S.box((x,y,(6.0+top-.22)/2),(2*hx,2*hy,top-.22-6.0),M['stone'],.01)
    S.box((x,y,top-.16),(2*hx+.08,2*hy+.08,.12),M['stone'],.015,rect=stone())
    S.box((x,y,top-.05),(2*hx+.02,2*hy+.02,.1),M['stone'],.015,rect=stone(),w=(.45,0))
    for dx in ((-.17,.17) if hx>.35 else (0,)):D.vessel((x+dx,y,top),[(.1,0),(.095,.3),(.105,.38),(.12,.42),(.09,.42),(.075,.36),(0,.36)],M['pot'])

# ---- cross wings: walls, windows, timbered gables ----------------------------------------
GL=(0,1.15,4.95,6.25)          # gable lancet (a, w, bottom, spring); its head rises past the eave
SL=[(s*1.15,.3,4.7,5.45) for s in (-1,1)]
for s in (-1,1):
    cx=s*XW
    P,R=gwall((cx,YF,0),(0,-1,0),WH,WP,WA,[GL]+SL)
    lancet((cx,YF,0),(0,-1,0),*GL,lights=4)
    for o_ in SL:lancet((cx,YF,0),(0,-1,0),*o_,lights=1)
    planter(P,R,0,1.3,FLOOR+.22)
    gwall((cx,YB,0),(0,1,0),WH,WP,WA,[(0,.9,1.2,2.5),(0,.9,5.0,6.1)])
    lancet((cx,YB,0),(0,1,0),0,.9,1.2,2.5);lancet((cx,YB,0),(0,1,0),0,.9,5.0,6.1)
    xo=cx+s*WH
    pwall((xo,0,0),(s,0,0),-YF-.3,WP,[(a,.7,1.2,2.5) for a in (-1.4,1.4)]+[(a,.6,4.8,5.6) for a in (-1.4,1.4)])
    for a in (-1.4,1.4):lancet((xo,0,0),(s,0,0),a,.7,1.2,2.5);lancet((xo,0,0),(s,0,0),a,.6,4.8,5.6)
    for y in (-2.85,2.85):pwall((s*XC,y,0),(-s,0,0),.15,WP,[])
    # corner buttresses at the outer front and rear corners, weathered tops
    for y in (YF+.25,YB-.25):
        S.box((xo+s*.17,y,1.5),(.34,.5,3.0),M['stone'],.015)
        S.prism([(xo,y,3.0),(xo+s*.34,y,3.0),(xo,y,3.45)],(0,1,0),.5,M['stone'])
    # plinth course (front stops at the bay)
    for a0,a1 in ((-WH-.04,-1.3),(1.3,WH+.04)):S.box((cx+(a0+a1)/2,YF-.04,.22),(a1-a0,.1,.44),M['stone'],.01)
    S.box((cx,YB+.04,.22),(2*WH+.08,.1,.44),M['stone'],.01)
    S.box((xo+s*.04,0,.22),(.1,2*YB,.44),M['stone'],.01)
    # front gable timber: jettied bressumer on corbels, posts, split tie beam, king post, collar, struts
    y=YF-.05
    T.box((cx,YF-.01,FLOOR+.11),(2*WH+.16,.3,.22),M['timber'],.015,grain=True,band=band())
    for dx in (-1.85,-.95,.95,1.85):T.box((cx+dx,YF-.08,FLOOR-.15),(.14,.16,.3),M['timber'],.01,grain=True,band=band())
    for sd in (-1,1):
        post(cx+sd*(WH-.08),y,FLOOR+.22,WP-.12)
        rail((cx+sd*(WH-.02),y,WP-.1),(cx+sd*.8,y,WP-.1),.16)
        rail((cx+sd*(WH-.15),y,WP-.6),(cx+sd*(WH-.45),y,WP-.2),.1)
        zc=7.85;hx=(WA-zc)/KW-.05
        rail((cx+sd*hx,y,zc),(cx+sd*.09,y,zc),.14)
        rail((cx+sd*1.55,y,WP-.02),(cx+sd*.95,y,zc-.08),.1)
    post(cx,y,7.32,WA-.1)
    # side and rear timber: floor band, rear tie and king post
    T.box((xo+s*.04,0,FLOOR+.11),(.1,2*YB+.1,.22),M['timber'],.015,grain=True,band=band())
    y=YB+.05
    T.box((cx,YB+.04,FLOOR+.11),(2*WH+.1,.1,.22),M['timber'],.015,grain=True,band=band())
    for sd in (-1,1):post(cx+sd*(WH-.08),y,FLOOR+.22,WP-.12);rail((cx+sd*(WH-.02),y,WP-.1),(cx+sd*.62,y,WP-.1),.16)
    post(cx,y,7.0,WA-.1)
    # downpipes: outer gutter to ground at the side walls
    for yy in (-2.6,2.6):
        D.beam((xo+s*.28,yy,WE-.12),(xo+s*.13,yy,WE-.45),.07,.07,M['iron'],grain=False)
        D.beam((xo+s*.13,yy,WE-.45),(xo+s*.13,yy,.12),.07,.07,M['iron'],grain=False);D.box((xo+s*.13,yy,.08),(.1,.1,.08),M['iron'],.008)

# ---- canted bay windows with hipped slate roofs --------------------------------------------
def meet(p,d,q,f):
    den=d.x*f.y-d.y*f.x;s=((q.x-p.x)*f.y-(q.y-p.y)*f.x)/den;return p+d*s
def offset(pts,off,yend):
    """Bay outline pushed outward by `off`, its ends run back to the line y = yend."""
    E=[(b-a).normalized() for a,b in zip(pts,pts[1:])];L=[(a+Vector((e.y,-e.x,0))*off,e) for a,e in zip(pts,E)];X=Vector((1,0,0))
    return [meet(*L[0],Vector((0,yend,0)),X)]+[meet(*L[i-1],*L[i]) for i in range(1,len(L))]+[meet(*L[-1],Vector((0,yend,0)),X)]
def slab(pts,off,z0,z1,g,mat):
    o=offset(pts,off,YF+.1);g.prism([p+UP*(z0+z1)/2 for p in o],(0,0,1),z1-z0,mat)
def bay_face(A,B,z0,z1,n):
    """Timber-framed glazing between two bay corners: n pointed lights with diamond cames."""
    o=(B-A).cross(UP).normalized();P,R=frame((A+B)/2,o);w=(B-A).length-.14;lw=(w-.07*(n-1))/n;top=z1-.1;gb=z0+.08
    T.box(P(0,-.05,z0+.04),(w,.1,.08),M['timber'],.006,R,grain=True,band=band());T.box(P(0,-.05,z1-.05),(w,.1,.1),M['timber'],.006,R,grain=True,band=band())
    sp=top-(apex(lw,0));arc=arcpts(lw,sp)
    for i in range(n):
        ac=-w/2+lw/2+i*(lw+.07)
        if i:T.box(P(ac-lw/2-.035,-.05,(gb+top)/2),(.07,.1,top-gb),M['timber'],.006,R,grain=True,band=band())
        D.prism([P(ac-lw/2,-.09,gb),P(ac+lw/2,-.09,gb)]+[P(ac+x,-.09,z) for x,z in arc]+[P(ac-x,-.09,z) for x,z in arc[::-1][1:]],o,.01,M['glass'])
        for sd in (1,-1):T.prism([P(ac+sd*lw/2,-.05,top)]+[P(ac+sd*x,-.05,z) for x,z in arc],o,.1,M['timber'])
        cames(P,ac,lw,sp-gb,-.08,gb)
def bay_roof(E,z,k):
    """Hipped slate roof on the canted outline E (wall end, corners, wall end) at eave height z."""
    cs=1/math.sqrt(1+k*k);n=len(E)-1;Ev=[(b-a).normalized() for a,b in zip(E,E[1:])];V=[UP.cross(e) for e in Ev];X=Vector((1,0,0))
    H=[X]+[(V[j-1]+V[j]).normalized() for j in range(1,n)]+[X]       # wall line at the ends, hip bisectors between
    def cr(a,b):return a.x*b.y-a.y*b.x
    for i in range(n):
        A,B,e,v=E[i],E[i+1],Ev[i],V[i];Lp=(B-A).length
        aA,aB=-cr(v,H[i])/cr(e,H[i]),-cr(v,H[i+1])/cr(e,H[i+1]);dmax=Lp/(aA-aB)
        if 0<i<n-1:dmax=min(dmax,(YF-A.y)/v.y)
        u=(v+UP*k).normalized();lo=lambda t,aA=aA:aA*t*cs;hi=lambda t,aA=aA,aB=aB,Lp=Lp:max(Lp+aB*t*cs,aA*t*cs)
        tile_plane(G['roof'],A+UP*z,e,u,dmax/cs,lo,hi,M['slate'],SLATES,[],size=(.22,.3,.018),exposure=.14,lichen=lichen,tone=(.55,.9))
        deck(G['roof'],A+UP*z,e,u,dmax/cs,lo,hi,M['timber'],[],cell=.3)
    for j in range(1,n):
        r=(YF-E[j].y)/H[j].y;d=r*H[j].dot(V[j]);capped(E[j]+UP*(z+.05),E[j]+H[j]*r+UP*(z+k*d+.03),3,.18)
for s in (-1,1):
    cx=s*XW;pts=[Vector((cx-1.3,YF,0)),Vector((cx-.7,YF-.6,0)),Vector((cx+.7,YF-.6,0)),Vector((cx+1.3,YF,0))]
    slab(pts,0,0,1.0,S,M['stone']);slab(pts,.06,1.0,1.1,S,M['stone']);slab(pts,.08,2.85,3.05,T,M['timber'])
    for p in pts:post(p.x,p.y,1.1,2.85,.14,.14)
    for i,(A,B) in enumerate(zip(pts,pts[1:])):bay_face(A,B,1.1,2.85,3 if i==1 else 1)
    bay_roof(offset(pts,.2,YF),3.05,.9)
    P,R=frame((cx,YF-.6,0),(0,-1,0));planter(P,R,0,1.35,.61,d=.1)

# ---- recessed centre: porch, balcony, door, window -----------------------------------------
PF=-3.6                                                    # porch front face
P,R=pwall((0,PF,0),(0,-1,0),1.5,FLOOR,[(0,1.9,.75,2.65)],.92)   # landing is the opening's sill
ring(P,R,(0,-1,0),0,1.9,2.65,.09,.15,.03,.1,jamb=.75)
DOOR=(0,1.5,.75,2.45);WIN=(0,1.0,4.6,5.25)
P,R=pwall((0,CF,0),(0,-1,0),XC,CP,[DOOR,WIN])
lancet((0,CF,0),(0,-1,0),*WIN,lights=4)
arc=arcpts(1.5,2.45);top=arc[-1][1]
# double door: each leaf is its own mesh (opened at runtime); the cover strip rides on the right leaf
LEAF={-1:Geo('SM_SBNorthridgeHouse4_Door1',asset),1:Geo('SM_SBNorthridgeHouse4_Door2',asset)};G.update(door1=LEAF[-1],door2=LEAF[1])
for sd in (-1,1):
    L=LEAF[sd];L.prism([P(sd*.745,-.2,.75),P(0,-.2,.75)]+[P(sd*x*.995,-.2,z) for x,z in arc[::-1]],(0,-1,0),.07,M['door'])
    for z in (1.15,2.2):L.box(P(sd*.4,-.15,z),(.56,.012,.05),M['iron'],.004,R)
    for k in range(10):
        t=math.tau*k/10;L.box(P(sd*.16+.06*math.cos(t),-.13,1.65+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
    L.box(P(sd*.16,-.155,1.71),(.07,.01,.07),M['iron'],.004,R)
LEAF[1].box(P(0,-.155,(.75+top)/2),(.04,.02,top-.8),M['iron'],0,R)
pwall((0,CB,0),(0,1,0),XC,CP,[(0,.8,1.2,2.4),(0,.8,4.6,5.5)])
lancet((0,CB,0),(0,1,0),0,.8,1.2,2.4);lancet((0,CB,0),(0,1,0),0,.8,4.6,5.5)
S.box((0,CB+.04,.22),(2*XC,.1,.44),M['stone'],.01)
# balcony: slab on corbels, Gothic balustrade with newels
S.box((0,(PF-.18+CF)/2,FLOOR+.09),(3.1,CF-(PF-.18),.18),M['stone'],.015)
for x in (-1.3,-.75,.75,1.3):S.box((x,PF-.07,FLOOR-.13),(.18,.16,.26),M['stone'],.012,rect=stone())
BY=PF-.08;BT=FLOOR+.95
for a,b in (((-1.36,BY),(1.36,BY)),((-1.36,BY),(-1.36,CF)),((1.36,BY),(1.36,CF))):
    for z,h,wd in ((FLOOR+.24,.12,.2),(BT-.05,.1,.18)):S.beam((a[0],a[1],z),(b[0],b[1],z),wd,h,M['stone'],grain=False,rect=stone())
    L=math.hypot(b[0]-a[0],b[1]-a[1])
    for k in range(1,round(L/.24)):
        p=Vector((a[0],a[1],0)).lerp(Vector((b[0],b[1],0)),k/round(L/.24));S.box((p.x,p.y,(FLOOR+.3+BT-.1)/2),(.07,.07,BT-.1-FLOOR-.3),M['stone'],.008,rect=stone())
for x in (-1.36,1.36):
    S.box((x,BY,(FLOOR+.18+BT+.12)/2),(.22,.22,BT+.12-FLOOR-.18),M['stone'],.012,rect=stone())
    S.box((x,BY,BT+.16),(.28,.28,.08),M['stone'],.012,rect=stone())
# flight of steps between pedestals with cheek walls
for k in range(1,5):S.box((0,PF-.3*k+.16,(.75-.15*k)/2),(3.04,.32,.75-.15*k),M['stone'],.015)
for s in (-1,1):
    S.box((s*1.75,-4.55,.55),(.5,.5,1.1),M['stone'],.015);S.box((s*1.75,-4.55,1.15),(.6,.6,.1),M['stone'],.012,rect=stone())
    D.vessel((s*1.75,-4.55,1.2),[(0,0),(.1,0),(.06,.08),(.08,.14),(.2,.24),(.22,.36),(.18,.36),(.17,.3),(0,.3)],M['stone'],16)
    S.box((s*1.72,-3.95,.375),(.4,.72,.75),M['stone'],.012)
# lanterns on the porch face, banners on brackets and over the balustrade
for s in (-1,1):lantern(G,M,(s*1.25,PF-.42,2.75),arm=((s*1.25,PF-.09,2.75),(0,-1,0)))
for s in (-1,1):banner((0,YF,0),(0,-1,0),s*1.95,3.4,2.0,.4,d=.32)
banner((0,BY,0),(0,-1,0),0,BT-.04,FLOOR-.3,.55,d=.16)
# centre gutters and downpipes beside the porch
for s in (-1,1):
    D.beam((s*1.4,-CY-.08,CE-.12),(s*1.63,-3.5,CE-.6),.07,.07,M['iron'],grain=False)
    D.beam((s*1.63,-3.5,CE-.6),(s*1.63,-3.5,.12),.07,.07,M['iron'],grain=False);D.box((s*1.63,-3.5,.08),(.1,.1,.08),M['iron'],.008)
    D.beam((s*1.3,CY+.08,CE-.12),(s*1.3,CB+.1,CE-.5),.07,.07,M['iron'],grain=False)
    D.beam((s*1.3,CB+.1,CE-.5),(s*1.3,CB+.1,.12),.07,.07,M['iron'],grain=False)

# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'not grounded: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
def leaf_part(obj,name,s):
    """Front (-Y) door leaf hinged on its s side (-1 left, +1 right): pivot at the bottom of that edge on the inner
    (+Y) face; travel opens it inward. Checked by swinging it: it must end inside the building, clear of the jamb."""
    vs=[obj.matrix_world@v.co for v in obj.data.vertices];hx=(min if s<0 else max)(v.x for v in vs)
    pv=Vector((hx,max(v.y for v in vs),min(v.z for v in vs)));travel=90 if s<0 else -90
    sw=[pv+Matrix.Rotation(math.radians(travel),3,'Z')@(v-pv) for v in vs]
    assert min(v.y for v in sw)>pv.y-1e-3 and min(s*(pv.x-v.x) for v in sw)>-1e-3,f'{name} does not swing inward'
    print('DOOR_SWING',name,[round(c,3) for c in pv],travel,'open depth',round(max(v.y for v in sw)-pv.y,3))
    return (obj,name,tuple(pv),(0,0,1),'swing',travel,'closed')
OUT=HERE/'SilverbrookNorthridgeHouse4'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
PARTS=kit.export_parts([leaf_part(bpy.data.objects[G['door1'].name],'Door1',-1),leaf_part(bpy.data.objects[G['door2'].name],'Door2',1)],str(OUT),'SM_SilverbrookNorthridgeHouse4')
for o in (bpy.data.objects[G['door1'].name],bpy.data.objects[G['door2'].name]):o.select_set(False)   # leaves ship as their own meshes, not in the house FBX
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-40,-40,0),(40,-40,0),(40,40,0),(-40,40,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-14,-19,6.5),(83,0,-36),lens=38);cam('Front',(0,-30,5.0),(90,0,0),12.6);cam('Side',(-30,0,5.0),(90,0,-90),12.6);cam('Rear',(0,30,5.0),(90,0,180),12.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'NorthridgeHouse4-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'house triangle budget: {tris}'
manifest=dict(asset='SilverbrookNorthridgeHouse4',town_asset='BLD-GreenlandsNorthridgeHouse-01',variant=4,reference=dict(file='NorthridgeHouse4-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_northridgehouse4.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookNorthridgeHouse4',parts=PARTS,slot_m=list(SLOT),outputs=['SilverbrookNorthridgeHouse4.blend','SilverbrookNorthridgeHouse4.glb','SilverbrookNorthridgeHouse4.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('NORTHRIDGEHOUSE4_BUILT',manifest['bounds_m'],tris)
