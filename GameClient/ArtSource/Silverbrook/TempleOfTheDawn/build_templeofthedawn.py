"""Silverbrook Temple of the Dawn from docs/Game-Design/concept-art/Silverbrook/Temple of the Dawn.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_templeofthedawn.py
Original procedural geometry interpreting the front elevation (14 m wide, 20 m to the
star finial): a three-gabled hall temple in pale ashlar with navy slate roofs, a deep
pointed portal under a gabled porch, sun rose windows, banner buttresses carrying
spired turrets, round corner turrets and a central bell tower with a slate spire.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,lantern,frame
from mathutils import Matrix
import bpy
scene=kit.reset(4114)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Temple of the Dawn.png'
asset=bpy.data.collections.new('SilverbrookTempleOfTheDawn');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_TempleOfTheDawn_Slate','grey_roof_tiles_02',dark=(.08,.11,.27),damp=False,aged=.7),
    stone=kit.scan('SB_TempleOfTheDawn_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    trim=kit.scan('SB_TempleOfTheDawn_Dressed','sandstone_blocks_08',dark=(.45,.40,.33)),
    banner=kit.scan('SB_TempleOfTheDawn_Banner','denim_fabric_03',dark=(.16,.2,.42),damp=False,aged=.7),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    gilt=kit.solid('SB_TempleOfTheDawn_Gilt',(.85,.6,.22),.3,1),
    bronze=kit.solid('SB_TempleOfTheDawn_Bronze',(.5,.33,.15),.35,1),
    blue=kit.solid('SB_TempleOfTheDawn_BlueGlass',(.01,.015,.04),.15,0,(.08,.16,.85),.7))
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
DRESSED=kit.patches('sandstone_blocks_08',(.05,.035),dark=.7,frac=.01,std=.22)
def rect():return kit.rng.choice(DRESSED)
G=dict(stone=Geo('SM_SBTempleOfTheDawn_Masonry',asset),trim=Geo('SM_SBTempleOfTheDawn_Trim',asset),roof=Geo('SM_SBTempleOfTheDawn_Roof',asset),detail=Geo('SM_SBTempleOfTheDawn_Details',asset))
# ---- measured layout (metres; concept 64 px/m across, 54 px/m up) ----------------------
SLOT=(14,24,20)                # town-layout slot w x d x h (concept README 'Fits')
YF,YR=-9.0,10.0                # front and rear wall faces
NW,NE,NR=2.9,6.8,11.6          # nave half-width, eave, ridge
WI,WO,WE,WR=2.9,5.5,5.8,9.6    # wing (aisle) inner/outer x, eave, ridge; ridge over WC
WC,WH=(WI+WO)/2,(WO-WI)/2
PAR=.25                        # gable parapets stand this far above the roof plane
KN,KW=(NR-NE)/NW,(WR-WE)/WH
TY0,TY1,TH=-7.65,-4.75,1.45    # bell tower footprint (y) and half-width
FT=.6                          # front wall thickness

def pointed(w,spring):return lambda z:w/2 if z<=spring else math.sqrt(max(0,w*w-(z-spring)**2))-w/2
def lancet_hole(a,w,lo,spring):return (a,pointed(w,spring),lo,spring+w*math.sqrt(3)/2,spring)
def circle_hole(a,zc,r):return (a,lambda z:math.sqrt(max(0,r*r-(z-zc)**2)),zc-r,zc+r,zc-r)
def gable(half,eave,apex):return lambda z:(-half,half) if z<=eave else (-half*(apex-z)/(apex-eave),half*(apex-z)/(apex-eave))

def face(geo,mat,c,o,outline,z0,z1,holes=(),th=.5,breaks=()):
    """Wall slab (outer face through c) cut into horizontal courses; each course is a set of
    trapezoid prisms between the outline and any pointed or round holes (a, hw(z), lo, hi, curve_from)."""
    P,R=frame(c,o);o=Vector(o).normalized()
    ks=sorted({z0,z1,*breaks,*(v for h in holes for v in h[2:5])});ks=[k for k in ks if z0<=k<=z1];zs=[ks[0]]
    for a,b in zip(ks,ks[1:]):
        if b-a<1e-6:continue
        n=max(1,math.ceil((b-a)/.07)) if any(h[4]<b and a<h[3] for h in holes) else 1
        zs+=[a+(b-a)*i/n for i in range(1,n+1)]
    for za,zb in zip(zs,zs[1:]):
        zm=(za+zb)/2;edges=[(outline(za)[0],outline(zb)[0])]
        for a,hw,lo,hi,_ in sorted([h for h in holes if h[2]<zm<h[3]],key=lambda h:h[0]):
            f=lambda z:max(0,hw(min(max(z,lo),hi)));edges+=[(a-f(za),a-f(zb)),(a+f(za),a+f(zb))]
        edges.append((outline(za)[1],outline(zb)[1]))
        for (x0,x1),(y0,y1) in zip(edges[::2],edges[1::2]):
            pts=[]
            for p in [(x0,za),(y0,za),(y1,zb),(x1,zb)]:
                if not pts or abs(p[0]-pts[-1][0])+abs(p[1]-pts[-1][1])>1e-5:pts.append(p)
            if len(pts)>1 and abs(pts[0][0]-pts[-1][0])+abs(pts[0][1]-pts[-1][1])<1e-5:pts.pop()
            area=sum(pts[i][0]*pts[i-1][1]-pts[i-1][0]*pts[i][1] for i in range(len(pts)))/2
            if len(pts)>=3 and abs(area)>1e-5:geo.prism([P(x,-th/2,z) for x,z in pts],o,th,mat)

def arch_pts(a,w,spring,off=0,n=8):
    """Pointed (equilateral) arch, offset outward by `off`: right spring -> apex -> left spring."""
    R_=w+off;ta=math.acos(min(1,(w/2)/R_));right=[(a-w/2+R_*math.cos(t),spring+R_*math.sin(t)) for t in [ta*i/n for i in range(n+1)]]
    return right+[(2*a-x,z) for x,z in reversed(right[:-1])]
def ring(geo,P,pts,d,size,mat,**kw):
    for (x0,z0),(x1,z1) in zip(pts,pts[1:]):
        a,b=P(x0,d,z0),P(x1,d,z1);ext=(b-a).normalized()*.02;geo.beam(a-ext,b+ext,size,size,mat,grain=False,**kw)
def finial(x,y,z,s=1.0):
    orb=[(.07*s*math.sin(math.pi*k/6),.2*s-.07*s*math.cos(math.pi*k/6)) for k in range(1,6)]
    lathe(G['detail'],(x,y,z),[(0,0),(.04*s,0),(.04*s,.13*s),*orb,(.015*s,.3*s),(0,.42*s)],M['gilt'],8)
def lathe(geo,c,profile,mat,seg=16,smooth=True):
    """Surface of revolution, UVs at the material's physical scale; r=0 rows close the ends."""
    cx,cy,cz=c;S=kit.SCALE[mat.name]
    for (ra,za),(rb,zb) in zip(profile,profile[1:]):
        if ra<1e-6 and rb<1e-6:continue
        rm=max((ra+rb)/2,1e-3)
        for j in range(seg):
            t0,t1=math.tau*j/seg,math.tau*(j+1)/seg;q=[(ra,t0,za),(ra,t1,za),(rb,t1,zb),(rb,t0,zb)]
            if ra<1e-6:q.pop(1)
            elif rb<1e-6:q.pop(3)
            geo.poly([(cx+r*math.cos(t),cy+r*math.sin(t),cz+z) for r,t,z in q],mat,[(rm*t/S,z/S) for r,t,z in q],smooth=smooth)
def drum(x,y,z0,z1,r,mat,seg=16,smooth=True):lathe(G['stone'] if mat is M['stone'] else G['trim'],(x,y,z0),[(0,0),(r,0),(r,z1-z0),(0,z1-z0)],mat,seg,smooth)
def spire(x,y,z0,z1,r,seg=8):lathe(G['roof'],(x,y,z0),[(0,0),(r,0),(0,z1-z0)],M['slate'],seg,False)

def lancet(c,o,a,w,lo,spring,th,lead=True,glass='glass'):
    """Leaded lancet in a hole cut by face(): glass, cames, dressed hood mould and sill."""
    P,R=frame(c,o);top=spring+w*math.sqrt(3)/2;hw=pointed(w,spring);dg=-th*.45
    out=[(a-w/2-.02,lo),(a+w/2+.02,lo)]+arch_pts(a,w,spring,.02)
    if glass:G['detail'].prism([P(x,dg,z) for x,z in out],o,.03,M[glass])
    if lead:
        dl=dg+.022;z=lo+.28
        while z<top-.12:
            h=hw(z);G['detail'].beam(P(a-h,dl,z),P(a+h,dl,z),.016,.015,M['lead'],grain=False);z+=.28
        for x in (-w/4,0,w/4):
            zt=spring+math.sqrt(max(0,w*w-(abs(x)+w/2)**2));G['detail'].beam(P(a+x,dl,lo),P(a+x,dl,zt),.016,.015,M['lead'],grain=False)
    ring(G['trim'],P,arch_pts(a,w,spring,.09),.045,.11,M['trim'],rect=rect())
    for s in (-1,1):ring(G['trim'],P,[(a+s*(w/2+.09),spring-.3),(a+s*(w/2+.09),spring)],.045,.11,M['trim'],rect=rect())
    G['trim'].box(P(a,(-th+.1)/2,lo-.06),(w+.26,th+.1,.14),M['trim'],.012,R,rect=rect())

def rose(c,o,a,zc,r,th,sun=True):
    """Round window: blue glass, gilt sun, stone spokes and a ring of dressed voussoirs."""
    P,R=frame(c,o);o_=Vector(o).normalized();dg=-th*.5
    G['detail'].prism([P(a+(r+.02)*math.cos(t),dg,zc+(r+.02)*math.sin(t)) for t in [math.tau*k/32 for k in range(32)]],o,.03,M['blue'])
    if sun:
        G['detail'].prism([P(a+r*.3*math.cos(t),dg+.04,zc+r*.3*math.sin(t)) for t in [math.tau*k/24 for k in range(24)]],o,.04,M['gilt'])
        for k in range(16):
            t=math.tau*k/16;L=r*(.86 if k%2==0 else .64)
            G['detail'].prism([P(a+r*.27*math.cos(t+s*.13),dg+.034,zc+r*.27*math.sin(t+s*.13)) for s in (-1,1)]+[P(a+L*math.cos(t),dg+.034,zc+L*math.sin(t))],o,.03,M['gilt'])
    for k in range(8):
        t=math.tau*k/8+math.pi/8;G['trim'].beam(P(a+r*.3*math.cos(t),dg+.02,zc+r*.3*math.sin(t)),P(a+(r+.03)*math.cos(t),dg+.02,zc+(r+.03)*math.sin(t)),.05,.05,M['trim'],grain=False,rect=rect())
    n=max(12,round(r*24));rm=r+.11
    for k in range(n):
        t=math.tau*(k+.5)/n;radial=R@Vector((math.cos(t),0,math.sin(t)));tang=R@Vector((-math.sin(t),0,math.cos(t)))
        G['trim'].box(P(a+rm*math.cos(t),.02,zc+rm*math.sin(t)),(math.tau*rm/n-.012,.1,.2),M['trim'],.01,Matrix((tang,o_,radial)).transposed(),rect=rect())

def coping(c,o,half,eave,apex,th):
    """Dressed coping along both rakes of a gable parapet, plus a gilt apex finial."""
    P,R=frame(c,o)
    for s in (-1,1):
        off=Vector((s*(apex-eave),0,half)).normalized()*.075
        G['trim'].beam(P(s*half+off.x,-th/2,eave+off.z),P(off.x,-th/2,apex+off.z),.15,th+.1,M['trim'],grain=False,rect=rect())
    finial(*P(0,-th/2,apex+.1),1.1)

def roof(xr,half,eave,ridge,y0,y1,covered=()):
    """Gable roof along y: slates and boarding on both planes, ridge caps."""
    k=(ridge-eave)/half;ov=.15 if half>2 else .2;L=(half+ov)*math.hypot(1,k);ze=eave-ov*k
    for s in (-1,1):
        O=(xr+s*(half+ov),0,ze);e=(0,s,0);u=Vector((-s*(half+ov),0,ridge-ze))
        lo,hi=(lambda t:-y1,lambda t:-y0) if s<0 else (lambda t:y0,lambda t:y1)
        tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,size=(.36,.5,.025),exposure=.23,tone=(.55,.9))
        deck(G['roof'],O,e,u.normalized(),L,lo,hi,M['slate'],covered)
    n=round((y1-y0)/1.0)
    for i in range(n):
        a=y0+(y1-y0)*i/n;b=y0+(y1-y0)*(i+1.04)/n;G['roof'].beam((xr,a,ridge+.06),(xr,b,ridge+.06),.3,.06,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.6,.85),0))

# ---- roofs ----------------------------------------------------------------------------
in_tower=lambda p:abs(p.x)<TH-.05 and TY0+.05<p.y<TY1-.05
roof(0,NW,NE,NR,YF+.4,YR-.4,[in_tower])
for s in (-1,1):roof(s*WC,WH,WE,WR,YF+.4,YR-.4)
# ---- front: nave gable, wing gables, porch ------------------------------------------------
NA=NR+PAR;WA=WR+PAR
DOOR=(0,1.7,.6,3.43)           # a, width, sill (= landing top), spring
front_nave=[lancet_hole(DOOR[0],DOOR[1],DOOR[2],DOOR[3]),circle_hole(0,8.5,.95),lancet_hole(0,.3,10.1,10.64),lancet_hole(-1.45,.36,6.0,6.99),lancet_hole(1.45,.36,6.0,6.99)]
face(G['stone'],M['stone'],(0,YF,0),(0,-1,0),gable(NW,NE+PAR,NA),0,NA,front_nave,FT,[NE+PAR])
rose((0,YF,0),(0,-1,0),0,8.5,.95,FT)
lancet((0,YF,0),(0,-1,0),0,.3,10.1,10.64,FT,lead=False)
for x in (-1.45,1.45):lancet((0,YF,0),(0,-1,0),x,.36,6.0,6.99,FT)
coping((0,YF,0),(0,-1,0),NW,NE+PAR,NA,FT)
WIN_F=[(-.45,.55,1.9,4.02),(.45,.55,1.9,4.02)]
for s in (-1,1):
    c=(s*WC,YF,0)
    face(G['stone'],M['stone'],c,(0,-1,0),gable(WH,WE+PAR,WA),0,WA,[lancet_hole(*h) for h in WIN_F]+[circle_hole(0,6.6,.55)],.5,[WE+PAR])
    for h in WIN_F:lancet(c,(0,-1,0),*h,.5)
    rose(c,(0,-1,0),0,6.6,.55,.5);coping(c,(0,-1,0),WH,WE+PAR,WA,.5)
    G['trim'].box((s*(WC-.12),YF-.06,5.2),(2*WH-.24,.14,.16),M['trim'],.012)       # string course
# porch: a deep stone gable with a pointed archway onto the landing; colonnettes and archivolts
PY0,PY1,PH,PE,PA=-10.2,YF+.02,1.95,5.0,6.6
PW,PS=2.6,3.25                 # archway width and spring
face(G['stone'],M['stone'],(0,PY0,0),(0,-1,0),gable(PH,PE,PA),0,PA,[lancet_hole(0,PW,.6,PS)],PY1-PY0,[PE])
kp=(PA-PE)/PH;LPo=math.hypot(PH+.12,(PH+.12)*kp)
for s in (-1,1):
    O=(s*(PH+.12),0,PE-.12*kp);e=(0,s,0);u=Vector((-s*(PH+.12),0,PA-O[2]))
    lo,hi=(lambda t:-YF-.1,lambda t:-PY0+.05) if s<0 else (lambda t:PY0-.05,lambda t:YF+.1)
    tile_plane(G['roof'],O,e,u,LPo,lo,hi,M['slate'],SLATES,size=(.36,.5,.025),exposure=.23,tone=(.55,.9));deck(G['roof'],O,e,u.normalized(),LPo,lo,hi,M['slate'])
for i in range(3):G['roof'].beam((0,PY0-.05+i*.42,PA+.06),(0,PY0-.05+(i+1.04)*.42,PA+.06),.3,.06,M['slate'],grain=False,rect=kit.rng.choice(SLATES))
finial(0,PY0+.1,PA+.04,1.2)
Pp,Rp=frame((0,0,0),(0,-1,0))
for y in (-10.0,-9.6,-9.2):
    ring(G['trim'],Pp,arch_pts(0,PW,PS,-.08),-y,.16,M['trim'],rect=rect())
    for s in (-1,1):
        x=s*1.21;drum(x,y,.6,.82,.15,M['trim'],12);drum(x,y,.82,PS-.2,.1,M['trim'],12);drum(x,y,PS-.2,PS,.16,M['trim'],12)
for s in (-1,1):                # porch corner pinnacles
    G['trim'].box((s*(PH-.12),PY0+.12,PE+.2),(.26,.26,.4),M['trim'],.01,rect=rect());spire(s*(PH-.12),PY0+.12,PE+.4,PE+1.25,.16,4);finial(s*(PH-.12),PY0+.12,PE+1.2,.6)
# door: pointed ledged double leaf, iron straps and rings, dressed hood ring
Pd,Rd=frame((0,YF,0),(0,-1,0));a,w,lo,sp=DOOR;hw=pointed(w,sp)
# One mesh per leaf so each opens; hinges on the outer jambs, leaves meet on the centre line.
arc=arch_pts(a,w,sp,.02);half=len(arc)//2   # right spring -> apex (index half) -> left spring
for s in (-1,1):
    L=Geo(f'SM_SBTempleOfTheDawn_Door{len(kit.DOORS)+1}',asset);G[L.name]=L
    kit.DOORS.append(dict(geo=L,r=Vector((-s,0,0)),o=Vector((0,-1,0)),width=w/2))   # r points from the hinge jamb to the meeting line
    outline=[(-w/2-.02,lo),(a,lo)]+arc[half:] if s<0 else [(a,lo),(w/2+.02,lo)]+arc[:half+1]
    L.prism([Pd(x,-.35,z) for x,z in outline],(0,-1,0),.08,M['door'])
    L.box(Pd(s*.0125,-.305,(lo+sp+w*.86)/2-.02),(.025,.012,sp+w*.86-lo-.06),M['iron'],.003,Rd)   # meeting strap, half per leaf
    for z in (1.0,2.2,3.3):L.box(Pd(s*hw(z)/2,-.303,z),(hw(z)-.1,.012,.06),M['iron'],.003,Rd)
    for k in range(10):
        t=math.tau*k/10;L.box(Pd(s*.18+.06*math.cos(t),-.29,1.55+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,Rd@Matrix.Rotation(t,3,'Y'))
    L.box(Pd(s*.18,-.302,1.61),(.07,.01,.07),M['iron'],.004,Rd)
ring(G['trim'],Pd,arch_pts(a,w,sp,.1),.04,.13,M['trim'],rect=rect())
lantern(G,M,(0,-9.7,PS+PW*.866-.02))
# landing steps and lantern pedestals
G['trim'].box((0,PY0-.2,.2),(4.0,.4,.4),M['trim'],.012);G['trim'].box((0,PY0-.6,.1),(4.0,.4,.2),M['trim'],.012)
for s in (-1,1):
    x,y=s*2.45,-10.3
    G['trim'].box((x,y,.7),(.6,.8,1.4),M['trim'],.015,rect=rect());G['trim'].box((x,y,1.45),(.72,.92,.1),M['trim'],.015,rect=rect())
    G['detail'].beam((x,y,1.5),(x,y,2.25),.06,.06,M['iron'],grain=False);lantern(G,M,(x,y,2.62))
# ---- front buttresses with banners, carrying spired turrets ------------------------------
for s in (-1,1):
    x=s*2.45
    G['stone'].box((x,(-9.8-8.5)/2,3.2),(.8,1.3,6.4),M['stone'],.01)
    G['stone'].box((x,(-9.7-8.5)/2,8.0),(.7,1.2,3.2),M['stone'],.01)
    for z,d in ((6.4,-9.8),(9.6,-9.7)):G['trim'].box((x,(d-.06-8.5)/2,z+.06),(.84 if z<7 else .74,-8.5-d+.06,.12),M['trim'],.01,rect=rect())
    drum(x,-9.1,9.66,12.0,.45,M['stone'],8,False);drum(x,-9.1,12.0,12.15,.53,M['trim'],8,False)
    spire(x,-9.1,12.15,14.2,.5);finial(x,-9.1,14.15,.9)
    Pb,Rb=frame((x,-9.8,0),(0,-1,0))
    for k in (0,1):G['detail'].prism([Pb(xx,.009,z) for xx,z in ([(-.27,6.26),(-.27,3.6),(0,3.85),(0,6.26)] if k==0 else [(0,6.26),(0,3.85),(.27,3.6),(.27,6.26)])],(0,-1,0),.02,M['banner'])
    G['detail'].prism([Pb(.12*math.cos(t),.025,5.2+.12*math.sin(t)) for t in [math.tau*k/16 for k in range(16)]],(0,-1,0),.012,M['gilt'])
    for k in range(8):
        t=math.tau*k/8;G['detail'].prism([Pb(.1*math.cos(t+q),.024,5.2+.1*math.sin(t+q)) for q in (-.25,.25)]+[Pb(.2*math.cos(t),.024,5.2+.2*math.sin(t))],(0,-1,0),.01,M['gilt'])
    G['detail'].beam(Pb(-.33,.04,6.25),Pb(.33,.04,6.25),.03,.03,M['iron'],grain=False)
    for xx in (-.3,.3):G['detail'].box(Pb(xx,.02,6.25),(.03,.05,.03),M['iron'],0,Rb)
# ---- round corner turrets ------------------------------------------------------------------
for s in (-1,1):
    x=s*WO;drum(x,YF,0,5.6,.5,M['stone'],20);drum(x,YF,5.6,5.75,.6,M['trim'],20);drum(x,YF,5.75,7.0,.42,M['stone'],16);drum(x,YF,7.0,7.1,.5,M['trim'],16)
    spire(x,YF,7.1,8.9,.52,12);finial(x,YF,8.85,.8)
# ---- side walls, nave clerestory, rear ---------------------------------------------------
WIN_S=[-6.6,-3.0,.6,4.2,7.8]
for s in (-1,1):
    c=(s*WO,0,0);o=(s,0,0)
    face(G['stone'],M['stone'],c,o,lambda z:(-8.5,9.5) if s>0 else (-9.5,8.5),0,WE,[lancet_hole(s*y,.7,1.6,3.8) for y in WIN_S],.5)
    for y in WIN_S:lancet(c,o,s*y,.7,1.6,3.8,.5)
    for y in (-4.8,-1.2,2.4,6.0,9.6):
        G['stone'].box((s*(WO+.25),y,2.4),(.7,.5,4.8),M['stone'],.01);G['trim'].box((s*(WO+.27),y,4.86),(.76,.56,.12),M['trim'],.01,rect=rect())
    G['trim'].box((s*(WO+.04),(YF+YR)/2+.25,5.39),(.18,YR-YF-.5,.18),M['trim'],.012)          # eave cornice
    G['trim'].box((s*(WO+.03),(YF+YR)/2+.25,.22),(.16,YR-YF-.5,.44),M['trim'],.012)           # plinth
    G['stone'].box((s*(NW-.25),(YF+YR)/2,NE/2),(.5,YR-YF-1.0,NE),M['stone'],.01)            # nave arcade walls
    c=(s*WC,YR,0);face(G['stone'],M['stone'],c,(0,1,0),gable(WH,WE+PAR,WA),0,WA,[lancet_hole(0,.6,1.8,4.0)],.5,[WE+PAR])
    lancet(c,(0,1,0),0,.6,1.8,4.0,.5);coping(c,(0,1,0),WH,WE+PAR,WA,.5)
    G['trim'].box((s*4.0,YF-.03,.22),(2.1,.16,.44),M['trim'],.012)
face(G['stone'],M['stone'],(0,YR,0),(0,1,0),gable(NW,NE+PAR,NA),0,NA,[lancet_hole(0,1.4,2.0,5.5),circle_hole(0,8.6,.7)],.5,[NE+PAR])
lancet((0,YR,0),(0,1,0),0,1.4,2.0,5.5,.5);rose((0,YR,0),(0,1,0),0,8.6,.7,.5,sun=False);coping((0,YR,0),(0,1,0),NW,NE+PAR,NA,.5)
G['trim'].box((0,YR+.03,.22),(2*WO,.16,.44),M['trim'],.012)
for x in (-WO+.3,-NW,NW,WO-.3):G['stone'].box((x,YR+.3,2.4),(.5,.6,4.8),M['stone'],.01)
# ---- bell tower ----------------------------------------------------------------------------
TC=(TY0+TY1)/2
G['stone'].box((0,TC,6.7),(2*NW-.2,TY1-TY0+.2,.2),M['stone'],.01)                         # belfry floor on the nave walls
T1,T2,T3=6.8,14.5,14.8;tw=.35
for o,c,half in (((0,-1,0),(0,TY0,0),TH),((0,1,0),(0,TY1,0),TH),((-1,0,0),(-TH,TC,0),TH-tw),((1,0,0),(TH,TC,0),TH-tw)):
    face(G['stone'],M['stone'],c,o,lambda z,h=half:(-h,h),T1,T2,[lancet_hole(0,.8,11.9,13.1)],tw);lancet(c,o,0,.8,11.9,13.1,tw,lead=False,glass=None)
G['trim'].box((0,TC,(T2+T3)/2),(2*TH+.3,TY1-TY0+.3,T3-T2),M['trim'],.015)
for k in range(6):
    for o,c in (((0,-1,0),(0,TY0,0)),((0,1,0),(0,TY1,0))):
        P,R=frame(c,o);G['trim'].box(P(-1.1+k*.44,.07,T2-.12),(.16,.14,.24),M['trim'],.01,R,rect=rect())
S2=.8;T4=16.7
for o,c,half in (((0,-1,0),(0,TC-S2,0),S2),((0,1,0),(0,TC+S2,0),S2),((-1,0,0),(-S2,TC,0),S2-.25),((1,0,0),(S2,TC,0),S2-.25)):
    face(G['stone'],M['stone'],c,o,lambda z,h=half:(-h,h),T3,T4,[lancet_hole(0,.45,15.0,15.95)],.25);lancet(c,o,0,.45,15.0,15.95,.25,lead=False,glass=None)
G['trim'].box((0,TC,T4+.1),(2*S2+.3,2*S2+.3,.2),M['trim'],.015)
spire(0,TC,T4+.2,19.05,.92);finial(0,TC,18.98,1.0)
# gilt star of the dawn, facing the front
Ps,Rs=frame((0,TC,0),(0,-1,0));ZS=19.62
G['detail'].prism([Ps(.08*math.cos(t),0,ZS+.08*math.sin(t)) for t in [math.tau*k/16 for k in range(16)]],(0,-1,0),.06,M['gilt'])
for k in range(8):
    t=math.tau*k/8+math.pi/2;L=.32 if k%2==0 else .2
    G['detail'].prism([Ps(.07*math.cos(t+q),0,ZS+.07*math.sin(t+q)) for q in (-.4,.4)]+[Ps(L*math.cos(t),0,ZS+L*math.sin(t))],(0,-1,0),.04,M['gilt'])
for k in range(16):
    t0,t1=math.tau*k/16,math.tau*(k+1.1)/16;G['detail'].beam(Ps(.15*math.cos(t0),0,ZS+.15*math.sin(t0)),Ps(.15*math.cos(t1),0,ZS+.15*math.sin(t1)),.025,.025,M['gilt'],grain=False)
for sx in (-1,1):
    for sy in (-1,1):
        x,y=sx*1.15,TC+sy*1.15;drum(x,y,T3,16.2,.22,M['stone'],8,False);drum(x,y,16.2,16.3,.27,M['trim'],8,False);spire(x,y,16.3,17.5,.26);finial(x,y,17.45,.6)
# bells on iron beams
BELL=[(0,.06),(.4,0),(.38,.08),(.29,.25),(.26,.55),(.19,.66),(0,.69)]
for zb,sc,half in ((12.75,1.0,TH-tw),(15.95,.55,S2-.25)):
    lathe(G['detail'],(0,TC,zb),[(r*sc,z*sc) for r,z in BELL],M['bronze'],20)
    G['detail'].beam((-half-.05,TC,zb+.69*sc+.06),(half+.05,TC,zb+.69*sc+.06),.08,.08,M['iron'],grain=False)
    G['detail'].box((0,TC,zb+.69*sc+.01),(.06,.06,.06),M['iron'],0)
# ---- export, preview scene, manifest -----------------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'model base at z={lo[2]:.3f}, not on the ground'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookTempleOfTheDawn'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookTempleOfTheDawn')  # door leaves: own meshes, hinged; deselected for the main FBX
# Door leaves are their own meshes hinged at the jamb; check each opens inward (+Y, away from the street) above its sill.
for d,row in zip(kit.DOORS,DOORS):
    ob_=bpy.data.objects[d['geo'].name];vs=[ob_.matrix_world@v.co for v in ob_.data.vertices]
    pv=Vector(row['pivot_m']);rot=Matrix.Rotation(math.radians(row['travel']),3,'Z');op=[pv+rot@(v-pv) for v in vs]
    dy=sum(v.y for v in op)/len(op)-sum(v.y for v in vs)/len(vs);print('DOOR_SWING',row['name'],row['pivot_m'],row['travel'],round(dy,3))
    assert dy>.3 and min(v.z for v in op)>pv.z-1e-3,f"{row['name']} does not swing inward"
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-60,-60,0),(60,-60,0),(60,60,0),(-60,60,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o);c.clip_end=300
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-21,-31,7),(86,0,-34),lens=36);cam('Front',(0,-60,10.1),(90,0,0),21);cam('Side',(-60,0,10.1),(90,0,-90),25);cam('Rear',(0,60,10.1),(90,0,180),21)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'TempleOfTheDawn-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=400000,f'{tris} triangles exceeds the landmark budget'
manifest=dict(asset='SilverbrookTempleOfTheDawn',reference=dict(file='TempleOfTheDawn-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_templeofthedawn.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookTempleOfTheDawn',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookTempleOfTheDawn.blend','SilverbrookTempleOfTheDawn.glb','SilverbrookTempleOfTheDawn.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('TEMPLE_BUILT',manifest['bounds_m'],tris)
