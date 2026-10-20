"""Silverbrook Northridge House 6 from docs/Game-Design/concept-art/Silverbrook/Northridge House-6.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse6.py
Original procedural geometry interpreting the front elevation (14 m dimensioned, ~16 m with the
end roofs and ~14 m to the tower finial, compressed to the 12 x 10 x 10 m slot): a two-storey pale
ashlar house under a steep navy slate hip roof; an octagonal corner tower with a corbelled,
crenellated parapet, four bartizans and a slate spire; a tall central cross gable over a timber
porch and a pointed double door, with paired lancets and a quatrefoil roundel; a right cross gable
carrying a canted oriel; two front dormers, a left lean-to, a copper-potted stack and stag banners.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,lantern,frame,UP
from mathutils import Matrix
import numpy as np
import bpy
scene=kit.reset(4136)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Northridge House-6.png'
asset=bpy.data.collections.new('SilverbrookNorthridgeHouse6');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_NorthridgeHouse6_Slate','grey_roof_tiles_02',dark=(.08,.11,.27),damp=False,aged=.7),
    stone=kit.scan('SB_NorthridgeHouse6_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    trim=kit.scan('SB_NorthridgeHouse6_Dressed','sandstone_blocks_08',dark=(.45,.40,.33)),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_NorthridgeHouse6_Banner','denim_fabric_03',dark=(.12,.14,.30),damp=False,aged=.75),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    copper=kit.solid('SB_NorthridgeHouse6_Copper',(.50,.24,.12),.45,1.0),
    gilt=kit.solid('SB_NorthridgeHouse6_Gilt',(.80,.55,.18),.35,1.0))
M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
DRESSED=kit.patches('sandstone_blocks_08',(.05,.035),dark=.7,frac=.01,std=.22)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
def rect():return kit.rng.choice(DRESSED)

class Geo6(Geo):
    """Geo whose prisms (gable faces, spandrels, door leaves) must come out with positive volume."""
    def prism(self,points,axis,thickness,mat,**kw):
        n=len(self.f);super().prism(points,axis,thickness,mat,**kw);vol=0
        for f in self.f[n:]:
            p=[self.v[i] for i in f];vol+=sum(p[0].dot(p[j].cross(p[j+1])) for j in range(1,len(p)-1))
        assert vol>0,'inside-out prism'
G={k:Geo6(f'SM_SBNorthridgeHouse6_{n}',asset) for k,n in (('stone','Masonry'),('trim','Dressed'),('timber','Timber'),('roof','Roof'),('detail','Details'))}

# ---- measured layout (metres; elevation read at ~95 px/m, scaled x0.72 so 16 m of roofs fit 12 m) ----
SLOT=(12,10,10)                # town-layout slot w x d x h (concept README 'Fits')
W2,YF,YR,TH=4.85,-2.6,2.6,.3   # main block: wall half width, front and rear wall faces, wall thickness
EAVE,RIDGE=4.3,7.4             # steep hip roof (the drawn end slopes are near-vertical, so the hips are steep)
EX,EY,RX=W2+.3,-YF+.3,4.2      # eave half extents, ridge half length
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
PLATE=RIDGE-(-YF)*KF-.04       # wall plate: just under the roof at the outer face of the long walls
XG,GW,GH,GE,GR,YG=.15,1.45,1.65,5.4,8.1,-2.9   # central cross gable: centre, wall half, roof half, eave, ridge, face
KG=(GR-GE)/GH
XR,RW,RH,RE,RR=2.95,1.35,1.55,4.7,7.3           # right cross gable (face flush with the front wall)
KR=(RR-RE)/RH
TX,TY,TR=-3.4,-2.6,1.13        # octagonal tower: centre on the front wall line, circumradius
TAP,THW=TR*math.cos(math.pi/8),TR*math.sin(math.pi/8)
XS,YS=4.35,-1.6                # stack, rising through the right hip beside the right gable
XD1,DW1,DH1,DE1,DR1=-1.88,.32,.4,5.35,5.95      # wall dormer between tower and central gable
XD2,DW2,DH2,DE2,DR2=-1.95,.24,.31,6.6,7.0      # small roof dormer above it
KD1,KD2=(DR1-DE1)/DH1,(DR2-DE2)/DH2
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def centre_z(x):return GR-abs(x-XG)*KG
def right_z(x):return RR-abs(x-XR)*KR
def d1_z(x):return DR1-abs(x-XD1)*KD1
def d2_z(x):return DR2-abs(x-XD2)*KD2
GC=centre_z(XG+GW)-.05;RC=right_z(XR+RW)-.05
YD2=-(RIDGE-6.3)/KF            # small dormer face sits where the main slope is at 6.3 m
in_centre=lambda p:abs(p.x-XG)<GW+.04 and p.z<centre_z(p.x)-.005
in_right=lambda p:abs(p.x-XR)<RW+.04 and p.y<0 and p.z<right_z(p.x)-.005
in_tower=lambda p:(p.x-TX)**2+(p.y-TY)**2<(TR+.06)**2 and p.z<6.9
in_stack=lambda p:abs(p.x-XS)<.5 and abs(p.y-YS)<.42
in_d1=lambda p:abs(p.x-XD1)<DW1+.02 and YF<p.y<0 and p.z<d1_z(p.x)-.005
in_d2=lambda p:abs(p.x-XD2)<DW2+.02 and YD2<p.y<0 and p.z<d2_z(p.x)-.005
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
under_centre=lambda p:abs(p.x-XG)<GH+.02 and YG-.32<p.y<YR+.32 and p.z<centre_z(p.x)-.01
inside_centre=lambda p:p.x<XG+GW and p.z<centre_z(p.x)
under_right=lambda p:abs(p.x-XR)<RH+.02 and YF-.32<p.y<0 and p.z<right_z(p.x)-.01
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered,**kw):
    kw.setdefault('size',(.34,.48,.024));kw.setdefault('exposure',.22)
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,tone=(.55,.9),**kw)
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.6,.85),0))
def capped(a,b,n,w=.26):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n,w)
def post(x,y,z0,z1,o='y',s=.18):G['timber'].box((x,y,(z0+z1)/2),(s if o=='y' else .12,.12 if o=='y' else s,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.14):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
def under(points,roof,what,margin=.03):
    """Wall tops must stay under the roof that covers them (a recurring bug: gable cheeks above hips)."""
    for p in points:assert p[2]<=roof(p[0],p[1])-margin,f'{what} pokes through the roof at {tuple(round(v,3) for v in p)}'

# ---- profile helpers (pointed arches, walls cut around them, lathes) ---------------------------------
def pointed(w,spring):return lambda z:w/2 if z<=spring else math.sqrt(max(0,w*w-(z-spring)**2))-w/2
def lancet_hole(a,w,lo,spring):return (a,pointed(w,spring),lo,spring+w*math.sqrt(3)/2,spring)
def circle_hole(a,zc,r):return (a,lambda z:math.sqrt(max(0,r*r-(z-zc)**2)),zc-r,zc+r,zc-r)
def gable(half,eave,apex):return lambda z:(-half,half) if z<=eave else (-half*(apex-z)/(apex-eave),half*(apex-z)/(apex-eave))
def flat(half):return lambda z:(-half,half)
def face(geo,mat,c,o,outline,z0,z1,holes=(),th=TH,breaks=()):
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
        a,b=P(x0,d,z0),P(x1,d,z1);ext=(b-a).normalized()*.015;geo.beam(a-ext,b+ext,size,size,mat,grain=False,**kw)
def lathe(geo,c,profile,mat,seg=16,smooth=True,phase=0):
    """Surface of revolution, UVs at the material's physical scale; r=0 rows close the ends."""
    cx,cy,cz=c;S=kit.SCALE[mat.name]
    for (ra,za),(rb,zb) in zip(profile,profile[1:]):
        if ra<1e-6 and rb<1e-6:continue
        rm=max((ra+rb)/2,1e-3)
        for j in range(seg):
            t0,t1=phase+math.tau*j/seg,phase+math.tau*(j+1)/seg;q=[(ra,t0,za),(ra,t1,za),(rb,t1,zb),(rb,t0,zb)]
            if ra<1e-6:q.pop(1)
            elif rb<1e-6:q.pop(3)
            geo.poly([(cx+r*math.cos(t),cy+r*math.sin(t),cz+z) for r,t,z in q],mat,[(rm*t/S,z/S) for r,t,z in q],smooth=smooth)
def drum(geo,x,y,z0,z1,r,mat,seg=16,smooth=True,phase=0):lathe(geo,(x,y,z0),[(0,0),(r,0),(r,z1-z0),(0,z1-z0)],mat,seg,smooth,phase)
def spire(x,y,z0,z1,r,seg=8,phase=0):lathe(G['roof'],(x,y,z0),[(0,0),(r,0),(0,z1-z0)],M['slate'],seg,False,phase)
def finial(x,y,z,s=1.0,mat=None):
    """Forged finial: socket, orb and spike (the drawn finials are dark iron)."""
    orb=[(.07*s*math.sin(math.pi*k/6),.2*s-.07*s*math.cos(math.pi*k/6)) for k in range(1,6)]
    lathe(G['detail'],(x,y,z),[(0,0),(.04*s,0),(.04*s,.13*s),*orb,(.015*s,.3*s),(.035*s,.36*s),(0,.55*s)],mat or M['iron'],8)

# ---- lancet windows: glass, diamond quarries, dressed hood and jambs, sill ---------------------------
def quarries(P,a,w,lo,spring,d,step=.1,slope=1.7):
    hw=pointed(w,spring);top=spring+w*math.sqrt(3)/2;st=step*slope
    inside=lambda x,z:lo+.005<=z<=top-.005 and abs(x-a)<=hw(z)-.003
    for sign in (1,-1):
        for k in range(-40,41):
            b=lo+k*st;pts=[(x,sign*slope*(x-a)+b) for x in np.linspace(a-w/2,a+w/2,48)];pts=[p for p in pts if inside(*p)]
            if len(pts)>=2 and math.dist(pts[0],pts[-1])>.03:G['detail'].beam(P(pts[0][0],d,pts[0][1]),P(pts[-1][0],d,pts[-1][1]),.009,.012,M['lead'],grain=False)
def lancet(c,o,a,w,lo,spring,th=TH,dg=None,trim='trim',planter=False,sill=True):
    """Leaded lancet; dg is the glass depth (default: inside the hole face() cut)."""
    P,R=frame(c,o);dg=-th*.45 if dg is None else dg;mat=M[trim];geo=G[trim]
    G['detail'].prism([P(x,dg,z) for x,z in [(a-w/2-.02,lo),(a+w/2+.02,lo)]+arch_pts(a,w,spring,.02)],o,.03,M['glass'])
    quarries(P,a,w,lo,spring,dg+.022)
    kw=dict(rect=rect()) if trim=='trim' else {}
    ring(geo,P,arch_pts(a,w,spring,.07,5),.035,.09,mat,**kw)
    for s in (-1,1):ring(geo,P,[(a+s*(w/2+.07),lo),(a+s*(w/2+.07),spring)],.035,.09,mat,**kw)
    if sill:G['trim'].box(P(a,(-th+.1)/2,lo-.06),(w+.2,th+.1,.14),M['trim'],.012,R,rect=rect())
    if planter:
        G['timber'].box(P(a,.16,lo-.24),(w+.3,.22,.18),M['planks'],.006,R)
        G['detail'].box(P(a,.16,lo-.15),(w+.24,.17,.02),M['soil'],0,R)
def wall(c,o,outline,z0,z1,wins=(),th=TH,breaks=(),extra=()):
    """Ashlar wall with lancets: wins are (a, width, sill, spring[, planter])."""
    face(G['stone'],M['stone'],c,o,outline,z0,z1,[lancet_hole(*w[:4]) for w in wins]+list(extra),th,breaks)
    for w in wins:lancet(c,o,*w[:4],th=th,planter=len(w)>4 and w[4])

# ---- banners: navy cloth, gilt border, stag and fleur-de-lis on an iron rod ----------------------------
def stag(geo,origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R*(a*s)+U*(z*s)
    geo.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        geo.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def fleur(geo,origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R*(a*s)+U*(z*s)
    geo.prism([p(a,z) for a,z in [(0,.5),(-.08,.3),(-.05,.06),(.05,.06),(.08,.3)]],N,.02,M['gilt'])
    for sx in (-1,1):geo.prism([p(sx*a,z) for a,z in ([(.06,.1),(.3,.24),(.2,.32),(.08,.2)] if sx>0 else [(.08,.2),(.2,.32),(.3,.24),(.06,.1)])],N,.02,M['gilt'])
    geo.prism([p(a,z) for a,z in [(-.16,-.02),(.16,-.02),(.16,.05),(-.16,.05)]],N,.024,M['gilt'])
    geo.prism([p(a,z) for a,z in [(-.035,-.24),(.035,-.24),(.04,-.02),(-.04,-.02)]],N,.02,M['gilt'])
def trim(geo,pts,normal,w=.03):
    n=Vector(normal)*.008
    for a,b in zip(pts,pts[1:]+pts[:1]):geo.beam(Vector(a)+n,Vector(b)+n,.02,w,M['gilt'],grain=False)
def banner(c,o,a,ztop,zbot,w):
    """Banner hung flat against a wall face from an iron rod on two brackets."""
    P,R=frame(c,o);o=Vector(o).normalized();r=R@Vector((1,0,0))
    bn=[(a-w/2,ztop),(a+w/2,ztop),(a+w/2,zbot+.35*w),(a,zbot),(a-w/2,zbot+.35*w)]
    G['detail'].prism([P(x,.03,z) for x,z in bn],o,.02,M['banner'])
    trim(G['detail'],[P(x,.04,z) for x,z in bn],o)
    h=ztop-zbot;stag(G['detail'],P(a,.04,ztop-.5*h),r,UP,o,w*.95);fleur(G['detail'],P(a,.04,zbot+.3*w+.05),r,UP,o,w*.5)
    G['detail'].beam(P(a-w/2-.09,.06,ztop+.02),P(a+w/2+.09,.06,ztop+.02),.03,.03,M['iron'],grain=False)
    for x in (a-w/2-.04,a+w/2+.04):G['detail'].box(P(x,.03,ztop+.02),(.03,.06,.03),M['iron'],0,R)
    for x in (a-w/2-.1,a+w/2+.1):G['detail'].box(P(x,.06,ztop+.02),(.04,.04,.04),M['iron'],.004,R)

def barge(a,b,y_out,w=.08,d=.24):
    """Bargeboard with a row of iron studs on its outer face."""
    a,b=Vector(a),Vector(b);G['timber'].beam(a,b,w,d,M['timber'],band=band());n=max(2,int((b-a).length/.38))
    for i in range(1,n):
        p=a+(b-a)*i/n;p.y+=y_out*(w/2+.006);G['detail'].box(p,(.035,.012,.035),M['iron'],.003)
def cheek(x,inward,y0,y1,top,bottom_fn,roof,th=.1):
    """Side wall closing a gable or dormer down to the main slope; outer face exactly on x."""
    yq=y1;pts=[(x+inward*th/2,y0,bottom_fn(y0)),(x+inward*th/2,y0,top),(x+inward*th/2,yq,top)]
    under([(x,y,z) for _,y,z in pts]+[(x+inward*th,y,z) for _,y,z in pts],roof,'cheek')
    G['stone'].prism(pts,(1,0,0),th,M['stone'])

# ---- main hip roof -------------------------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
front_cut=[in_centre,in_right,in_tower,in_stack,in_d1,in_d2]
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),front_cut)
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_centre])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_tower])
GXc=(GR-RIDGE)/KG              # the main ridge disappears into the central gable roof within GXc of XG
capped((-RX,0,RIDGE+.05),(XG-GXc,0,RIDGE+.05),10);capped((XG+GXc,0,RIDGE+.05),(RX,0,RIDGE+.05),10)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),8)
for sx in (-1,1):finial(sx*RX,0,RIDGE+.08,.9)
for a,b in ((-EX,XG-GH),(XR+RH,EX)):G['timber'].beam((a,-EY-.02,EAVE-.06),(b,-EY-.02,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((-EX,EY+.02,EAVE-.06),(EX,EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- central cross gable: runs front to back, above the main ridge ----------------------------------
LGc=math.hypot(GH,GR-GE)
plane((XG-GH,0,GE),(0,-1,0),(GH,0,GR-GE),LGc,lambda t:-(YR+.3),lambda t:-(YG-.3),[under_main])
plane((XG+GH,0,GE),(0,1,0),(-GH,0,GR-GE),LGc,lambda t:YG-.3,lambda t:YR+.3,[under_main,under_right])
capped((XG,YG-.3,GR+.05),(XG,YR+.3,GR+.05),18)
for y,so in ((YG-.34,-1),(YR+.34,1)):
    for s in (-1,1):barge((XG+s*(GH+.05),y,GE-.08),(XG-s*.16,y,GR+.16*KG+.03),so,.08,.26)
    G['timber'].box((XG,y,GR+.3),(.14,.14,.42),M['timber'],.02,grain=True,band=band());finial(XG,y,GR+.5,1.3)
for y0,sy in ((YF,-1),(YR-TH,1)):   # cheek walls close the gable faces down to the main slopes (start behind each face)
    for s in (-1,1):
        x=XG+s*GW;yq=sy*(RIDGE-GC)/KF
        cheek(x,-s,y0,yq,GC,lambda y,x=x:main_z(x,y)-.04,lambda px,py:centre_z(px)+0*py)
# ---- right cross gable: runs back into the main slope ---------------------------------------------
LGr=math.hypot(RH,RR-RE)
plane((XR-RH,0,RE),(0,-1,0),(RH,0,RR-RE),LGr,lambda t:0,lambda t:-(YF-.3),[under_main,under_centre,inside_centre])
plane((XR+RH,0,RE),(0,1,0),(-RH,0,RR-RE),LGr,lambda t:YF-.3,lambda t:0,[under_main,in_stack])
capped((XR,YF-.3,RR+.05),(XR,-.1,RR+.05),10)
xm=(GR+XG*KG-RR+XR*KR)/(KG+KR)  # where the right gable's left slope emerges from under the central roof
barge((xm+.03,YF-.34,right_z(xm+.03)+.02),(XR+.16,YF-.34,RR+.16*KR+.03),-1,.08,.24)
barge((XR+RH+.05,YF-.34,RE-.08),(XR-.16,YF-.34,RR+.16*KR+.03),-1,.08,.24)
G['timber'].box((XR,YF-.34,RR+.28),(.13,.13,.38),M['timber'],.02,grain=True,band=band());finial(XR,YF-.34,RR+.46,1.1)
for s in (-1,1):
    x=XR+s*RW;yq=-(RIDGE-RC)/KF
    cheek(x,-s,YF+TH,yq,RC,lambda y,x=x:main_z(x,y)-.04,lambda px,py:right_z(px)+0*py)
# ---- dormers ---------------------------------------------------------------------------------------
def dormer(xd,dw,dh,de,dr,kd,yface,base,win,zfn):
    L=math.hypot(dh,dr-de);ytop=-(RIDGE-dr)/KF+.3
    plane((xd-dh,0,de),(0,-1,0),(dh,0,dr-de),L,lambda t:-ytop,lambda t:-yface+.14,[under_main,under_centre],size=(.22,.32,.02),exposure=.15)
    plane((xd+dh,0,de),(0,1,0),(-dh,0,dr-de),L,lambda t:yface-.14,lambda t:ytop,[under_main,under_centre],size=(.22,.32,.02),exposure=.15)
    capped((xd,yface-.14,dr+.04),(xd,ytop-.3,dr+.04),3,.2)
    for s in (-1,1):barge((xd+s*(dh+.03),yface-.17,de-.05),(xd-s*.1,yface-.17,dr+.1*kd+.02),-1,.06,.16)
    finial(xd,yface-.17,dr+.1,.75)
    top=zfn(xd+dw)-.05
    wall((xd,yface,0),(0,-1,0),gable(dw,top,dr-.05),base,dr-.05,[win],.2,[top])
    yq=-(RIDGE-top)/KF
    for s in (-1,1):cheek(xd+s*dw,-s,yface+.2,yq,top,lambda y,x=xd+s*dw:main_z(x,y)-.04,lambda px,py,f=zfn:f(px)+0*py)
dormer(XD1,DW1,DH1,DE1,DR1,KD1,YF,PLATE,(0,.3,4.68,5.04),d1_z)
dormer(XD2,DW2,DH2,DE2,DR2,KD2,YD2,main_z(XD2,YD2)-.1,(0,.2,6.3,6.46),d2_z)
# ---- stack with copper pots and a banner ------------------------------------------------------------
G['stone'].box((XS,YS,6.1),(.9,.75,3.6),M['stone'],.01)
G['trim'].box((XS,YS,7.95),(.98,.83,.1),M['trim'],.015,rect=rect())
G['trim'].box((XS,YS,8.08),(1.02,.87,.16),M['trim'],.015,rect=rect(),w=(.45,0))
for dx in (-.2,.2):G['detail'].vessel((XS+dx,YS,8.16),[(.1,0),(.1,.06),(.085,.08),(.085,.42),(.11,.46),(.11,.5),(.075,.5),(.075,.46),(0,.46)],M['copper'])
banner((XS,YS-.375,0),(0,-1,0),0,7.35,5.75,.5)
# ---- front walls ----------------------------------------------------------------------------------
STEP,DS,DOOR=.3,1.7,1.2        # door sill = landing top; spring above ground; width (2.4 m clear under the point)
cl=(-W2+(XG-GW))/2;hl=(XG-GW+W2)/2
wall((cl,YF,0),(0,-1,0),flat(hl),0,PLATE,[(-1.88-cl,.36,.75,1.3),(-1.88-cl,.4,2.95,3.45,True)])
wall(((XR+RW+W2)/2,YF,0),(0,-1,0),flat((W2-XR-RW)/2),0,PLATE,[(0,.26,3.0,3.4)])
banner(((XR+RW+W2)/2,YF,0),(0,-1,0),0,2.65,1.05,.34)
wall((XR,YF,0),(0,-1,0),gable(RW,RC,RR-.05),0,RR-.05,[(x,.3,.75,1.15) for x in (-.85,0,.85)]+[(0,.3,5.4,5.75)],breaks=[RC])
centre_wins=[(-.32,.4,4.15,5.2),(.32,.4,4.15,5.2),(-.95,.28,4.15,4.95),(.95,.28,4.15,4.95)]
wall((XG,YG,0),(0,-1,0),gable(GW,GC,GR-.05),0,GR-.05,centre_wins,breaks=[GC],extra=[lancet_hole(0,DOOR,STEP,DS),circle_hole(0,6.3,.34)])
P,R=frame((XG,YG,0),(0,-1,0))
G['trim'].box(P(0,.05,4.0),(2*GW,.12,.1),M['trim'],.012,R)                       # sill course over the porch
# quatrefoil roundel: lit glass, four cusped lobes, voussoir ring
G['detail'].prism([P(.36*math.cos(t),-.135,6.3+.36*math.sin(t)) for t in [math.tau*k/24 for k in range(24)]],(0,-1,0),.03,M['glass'])
for k in range(4):
    t=math.tau*k/4+math.pi/4;cx,cz=.15*math.cos(t),6.3+.15*math.sin(t)
    ring(G['trim'],P,[(cx+.15*math.cos(u),cz+.15*math.sin(u)) for u in np.linspace(t-2.2,t+2.2,8)],-.1,.035,M['trim'],rect=rect())
for k in range(20):
    t=math.tau*(k+.5)/20;rm=.45;radial=R@Vector((math.cos(t),0,math.sin(t)));tang=R@Vector((-math.sin(t),0,math.cos(t)))
    G['trim'].box(P(rm*math.cos(t),.02,6.3+rm*math.sin(t)),(math.tau*rm/20-.012,.1,.2),M['trim'],.01,Matrix((tang,Vector((0,-1,0)),radial)).transposed(),rect=rect())
# central gable timbering over the stone
y=YG-.03;fit=lambda z,top,k:(top-z-.09)/k-.07
for s in (-1,1):
    post(XG+s*1.3,y,4.06,centre_z(XG+1.37)-.09,'y',.14)
    rail((XG+s*1.15,y,5.8),(XG+s*fit(6.9,GR,KG),y,6.85),.1)
rail((XG-fit(6.96,GR,KG),y,6.9),(XG+fit(6.96,GR,KG),y,6.9),.12);post(XG,y,6.96,GR-.2,'y',.14)
# pointed double door: ledged leaves, iron straps and rings, dressed hood
hw=pointed(DOOR,DS)
G['detail'].prism([P(x,-.2,z) for x,z in [(-DOOR/2-.02,STEP),(DOOR/2+.02,STEP)]+arch_pts(0,DOOR,DS,.02)],(0,-1,0),.08,M['door'])
G['detail'].box(P(0,-.155,(STEP+DS+DOOR*.866)/2),(.04,.012,DS+DOOR*.866-STEP-.06),M['iron'],.003,R)
for z in (.75,1.5,2.25):
    for s in (-1,1):G['detail'].box(P(s*hw(z)/2,-.153,z),(hw(z)-.1,.012,.06),M['iron'],.003,R)
for s in (-1,1):
    for k in range(10):
        t=math.tau*k/10;G['detail'].box(P(s*.16+.06*math.cos(t),-.14,1.25+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
    G['detail'].box(P(s*.16,-.152,1.31),(.07,.01,.07),M['iron'],.004,R)
ring(G['trim'],P,arch_pts(0,DOOR,DS,.1),.04,.13,M['trim'],rect=rect())
for s in (-1,1):ring(G['trim'],P,[(s*(DOOR/2+.1),STEP),(s*(DOOR/2+.1),DS)],.04,.13,M['trim'],rect=rect())
# corner piers of the central gable with pinnacles and lanterns
for s in (-1,1):
    x=XG+s*(GW-.15)
    G['stone'].box((x,YG-.15,1.275),(.3,.3,2.55),M['stone'],.01)
    G['trim'].box((x,YG-.15,2.59),(.36,.36,.08),M['trim'],.012,rect=rect())
    spire(x,YG-.15,2.63,3.3,.19,4,math.pi/4);finial(x,YG-.15,3.27,.45)
    lantern(G,M,(x,YG-.63,2.15),arm=((x,YG-.3,2.15),(0,-1,0)))
# ---- porch: gabled slate roof on clustered colonnettes, open timber truss --------------------------
PH,PE,PA,PY=1.1,2.7,3.85,-4.05
kp=(PA-PE)/PH;LP=math.hypot(PH,PA-PE)
plane((XG-PH,0,PE),(0,-1,0),(PH,0,PA-PE),LP,lambda t:-YG,lambda t:-PY,[],size=(.26,.36,.02),exposure=.17)
plane((XG+PH,0,PE),(0,1,0),(-PH,0,PA-PE),LP,lambda t:PY,lambda t:YG,[],size=(.26,.36,.02),exposure=.17)
capped((XG,PY,PA+.04),(XG,YG,PA+.04),4,.22)
for s in (-1,1):barge((XG+s*(PH+.04),PY-.03,PE-.07),(XG-s*.1,PY-.03,PA+.1*kp+.02),-1,.07,.2)
finial(XG,PY-.03,PA+.12,.8)
G['trim'].box((XG,(YG+PY+.05)/2,STEP/2),(2.0,YG-PY-.05,STEP),M['trim'],.012)         # landing at the door sill
G['trim'].box((XG,PY-.11,.075),(1.6,.32,.15),M['trim'],.012)                          # step
for s in (-1,1):
    x=XG+s*.85
    for dx in (-.07,.07):drum(G['trim'],x+dx,-3.8,STEP+.12,2.5,.055,M['trim'],10)
    drum(G['trim'],x,-3.8,STEP,STEP+.12,.17,M['trim'],10);drum(G['trim'],x,-3.8,2.5,2.62,.17,M['trim'],10)
    G['timber'].beam((x,-3.95,2.69),(x,YG,2.69),.12,.14,M['timber'],band=band())     # side plates
rail((XG-.9,-3.85,2.69),(XG+.9,-3.85,2.69),.14);post(XG,-3.85,2.76,PA-.16,'y',.12)
for s in (-1,1):rail((XG+s*.75,-3.85,2.8),(XG+s*.1,-3.85,3.45),.09)
lantern(G,M,(XG,-3.45,PA-.12))
# ---- right gable: canted oriel on brackets, timbering above -----------------------------------------
def trap(cx,wy,fh,d,o=0):
    """Plan outline of a 45-degree canted bay on the wall line wy, offset outward by o: BL, FL, FR, BR."""
    return [Vector((cx-fh-d-1.414*o,wy,0)),Vector((cx-fh-.414*o,wy-d-o,0)),Vector((cx+fh+.414*o,wy-d-o,0)),Vector((cx+fh+d+1.414*o,wy,0))]
def slab(geo,pts,z0,z1,mat,tuck=Vector((0,.05,0))):
    pts=[pts[0]+tuck]+pts+[pts[-1]+tuck];geo.prism([p+Vector((0,0,(z0+z1)/2)) for p in pts],(0,0,1),z1-z0,mat)
def hipped(chain,ze,k,r_max,inwall,size=(.2,.28,.018),exposure=.13):
    """Hipped slate roof over a bay: one plane per footprint edge rising inward at slope k until it
    reaches the wall (chain runs wall -> ... -> wall with the bay on its left)."""
    q=math.sqrt(1+k*k);n=len(chain)
    def inset(j):
        if j in (0,n-1):return None
        d0=(chain[j]-chain[j-1]).normalized();d1=(chain[j+1]-chain[j]).normalized();return math.tan(math.acos(max(-1,min(1,d0.dot(d1))))/2)
    for i in range(n-1):
        A,B=chain[i],chain[i+1];e=(B-A).normalized();Le=(B-A).length;inward=UP.cross(e);ia,ib=inset(i),inset(i+1)
        lo=(lambda t:-t/q) if ia is None else (lambda t,c=ia:c*t/q)
        hi=(lambda t,Le=Le:Le+t/q) if ib is None else (lambda t,Le=Le,c=ib:Le-c*t/q)
        plane(A+UP*ze,e,inward+UP*k,r_max*q,lo,hi,[inwall],size=size,exposure=exposure)
        if ib is not None:
            e1=(chain[i+2]-B).normalized();top=B+e1*ib*r_max+UP.cross(e1)*r_max
            cap(B+UP*(ze+.04),top+UP*(ze+k*r_max+.04),.14)
    return ze+k*r_max
FH,FD=.6,.45                   # oriel front half width, projection
fb=trap(XR,YF,FH,FD)
for x in (XR-.5,XR,XR+.5):     # carved brackets
    G['timber'].beam((x,YF-.02,1.72),(x,YF-FD-.06,2.12),.1,.12,M['timber'],band=band())
    G['timber'].box((x,YF-.08,1.95),(.12,.16,.46),M['timber'],.012,grain=True,band=band())
slab(G['trim'],trap(XR,YF,FH,FD,.05),2.15,2.3,M['trim'])
slab(G['stone'],fb,2.3,4.0,M['stone'])
for a,b in zip(fb,fb[1:]):
    nrm=(b-a).cross(UP).normalized();mid=(a+b)/2
    if abs(a.y-b.y)<1e-6:
        for off in (-.3,.3):lancet((mid.x,mid.y,0),nrm,off,.34,2.55,3.3,dg=.015,trim='timber',sill=False)
        G['timber'].box((mid.x,mid.y-.13,2.2),(2*FH+.1,.22,.16),M['planks'],.006)
        G['detail'].box((mid.x,mid.y-.13,2.285),(2*FH+.04,.17,.02),M['soil'],0)
    else:lancet((mid.x,mid.y,0),nrm,0,.26,2.55,3.35,dg=.015,trim='timber',sill=False)
for p in fb[1:3]:G['timber'].box((p.x,p.y,3.15),(.12,.12,1.7),M['timber'],.012,grain=True,band=band())
slab(G['timber'],trap(XR,YF,FH,FD,.05),4.0,4.1,M['timber'])
o=.15;ztop=hipped(trap(XR,YF,FH,FD,o),4.1-1.5*(o-.05)-.01,1.5,FD+o,lambda p:p.y>YF+.02)
bl,_,_,br=trap(XR,YF,FH,FD,o);G['timber'].beam(bl+Vector((0,-.05,ztop+.03)),br+Vector((0,-.05,ztop+.03)),.08,.1,M['timber'],band=band())
y=YF-.03
rail((XR-fit(5.28,RR,KR),y,5.22),(XR+fit(5.28,RR,KR),y,5.22),.12);rail((XR-fit(6.45,RR,KR),y,6.39),(XR+fit(6.45,RR,KR),y,6.39),.12);post(XR,y,6.45,RR-.2,'y',.14)
for s in (-1,1):rail((XR+s*(fit(5.35,RR,KR)-.05),y,5.3),(XR+s*fit(6.38,RR,KR),y,6.33),.1)
# ---- octagonal tower: corbel table, crenellated parapet, bartizans, slate spire ------------------------
ZT,ZP=6.75,7.4
tw=[[(0,.32,.75,1.2),(0,.42,2.9,3.45,True)],[(0,.26,5.0,5.4)],[],[],[],[],[(0,.34,2.95,3.45)],[(0,.26,5.0,5.4)]]
for k in range(8):
    phi=-math.pi/2+k*math.pi/4;o=Vector((math.cos(phi),math.sin(phi),0));c=Vector((TX,TY,0))+o*TAP
    wall(c,o,flat(THW),0,ZT,tw[k],.35)
    P,R=frame(c,o)
    for a in (-.26,0,.26):G['trim'].box(P(a,.08,6.6),(.13,.2,.3),M['trim'],.01,R,rect=rect())
RP=1.3;oct_=lambda r:[Vector((TX+r*math.cos(math.pi/8+k*math.pi/4),TY+r*math.sin(math.pi/8+k*math.pi/4),0)) for k in range(8)]
G['trim'].prism([p+Vector((0,0,.2)) for p in oct_(TR+.07)],(0,0,1),.4,M['trim'])     # plinth
outer,inner=oct_(RP),oct_(RP-.2/math.cos(math.pi/8))
for k in range(8):
    j=(k+1)%8;G['stone'].prism([p+Vector((0,0,(ZT+ZP)/2)) for p in (outer[k],outer[j],inner[j],inner[k])][::-1],(0,0,1),ZP-ZT,M['stone'])
    phi=-math.pi/2+k*math.pi/4;o=Vector((math.cos(phi),math.sin(phi),0));P,R=frame(Vector((TX,TY,0))+o*RP*math.cos(math.pi/8),o)
    for a in (-.24,.24):G['stone'].box(P(a,-.1,ZP+.21),(.22,.2,.3),M['stone'],.01,R)              # merlons
co,ci=oct_(RP+.03),oct_(RP-.23/math.cos(math.pi/8))
for k in range(8):j=(k+1)%8;G['trim'].prism([p+Vector((0,0,ZP+.03)) for p in (co[k],co[j],ci[j],ci[k])],(0,0,1),.06,M['trim'])   # coping
spire(TX,TY,6.8,9.05,1.075,8,math.pi/8);finial(TX,TY,9.0,1.15)
for ang in (math.pi/8,-math.pi/8,math.pi-math.pi/8,math.pi+math.pi/8):
    x,y=TX+1.22*math.cos(ang),TY+1.22*math.sin(ang)
    lathe(G['trim'],(x,y,6.2),[(0,0),(.12,0),(.21,.55),(0,.55)],M['trim'],12)
    drum(G['stone'],x,y,6.75,7.75,.21,M['stone'],12);drum(G['trim'],x,y,7.75,7.83,.25,M['trim'],12)
    spire(x,y,7.83,8.5,.24,8);finial(x,y,8.46,.55)
banner((TX,TY-TAP,0),(0,-1,0),0,6.2,4.45,.5)
# ---- left lean-to under a hipped slate pent roof ------------------------------------------------------
LX,LY0,LY1,LZ=-5.45,YF,-.6,1.95
wall(((LX-W2)/2,LY0,0),(0,-1,0),flat((W2+LX)/-2),0,LZ,[(0,.26,.7,1.1)])
wall((LX,(LY0+LY1)/2,0),(-1,0,0),flat((LY1-LY0)/2-TH),0,LZ,[(0,.34,.75,1.2)])
wall(((LX-W2)/2,LY1,0),(0,1,0),flat((W2+LX)/-2),0,LZ)
o=.15;chain=[Vector((-W2,LY1+o,0)),Vector((LX-o,LY1+o,0)),Vector((LX-o,LY0-o,0)),Vector((-W2,LY0-o,0))]
lz=hipped(chain,LZ-.1,1.1,-W2-LX+o,lambda p:p.x>-W2-.02,size=(.24,.34,.02),exposure=.16)
G['timber'].beam((-W2-.05,LY0-o,lz+.02),(-W2-.05,LY1+o,lz+.02),.1,.1,M['timber'],band=band())
# ---- side and rear walls (inferred) -------------------------------------------------------------------
YS_=-YF-TH
for s,wins in ((-1,[(1.3,.4,3.0,3.5),(-1.3,.4,3.0,3.5),(-1.3,.4,.75,1.3)]),(1,[(-1.2,.4,.75,1.3),(1.2,.4,.75,1.3),(-1.2,.4,2.95,3.45),(1.2,.4,2.95,3.45)])):
    wall((s*W2,0,0),(s,0,0),flat(YS_),0,PLATE,wins)
    top=[(y,main_z(W2,y)-.05) for y in np.linspace(-YS_,YS_,41)]
    pts=[(s*(W2-TH/2),y,z) for y,z in [(YS_,PLATE),(-YS_,PLATE)]+top];under([(s*W2,y,z) for _,y,z in pts],main_z,'side wall')
    G['stone'].prism(pts,(1,0,0),TH,M['stone'])
cr=(XR+RW+W2)/2
wall((cl,YR,0),(0,1,0),flat(hl),0,PLATE,[(-(x-cl),.4,z,z+.5) for x in (-3.6,-2.2) for z in (.75,2.95)])
wall(((XG+GW+W2)/2,YR,0),(0,1,0),flat((W2-XG-GW)/2),0,PLATE,[(-(x-(XG+GW+W2)/2),.4,z,z+.5) for x in (2.4,4.0) for z in (.75,2.95)])
wall((XG,YR,0),(0,1,0),gable(GW,GC,GR-.05),0,GR-.05,[(0,.5,.75,1.35),(0,.5,2.95,3.55),(0,.45,4.3,5.0)],breaks=[GC])
# plinth course, side and rear timbering, downpipes
for a0,a1 in ((-W2-.04,XG-GW),(XG+GW,W2+.04)):G['trim'].box(((a0+a1)/2,YF-.02,.175),(a1-a0,.1,.35),M['trim'],.01)
G['trim'].box((0,YR+.02,.175),(2*W2+.08,.1,.35),M['trim'],.01)
for s in (-1,1):G['trim'].box((s*(W2+.02),0,.17),(.1,2*-YF,.34),M['trim'],.01)
for s in (-1,1):
    x=s*(W2+.03)
    for yy in (YF+.15,0,YR-.15):post(x,yy,2.75,PLATE-.05,'x',.16)
    rail((x,YR,2.75),(x,-.4 if s<0 else YF,2.75));rail((x,YF,4.45),(x,YR,4.45))
for x,z1 in ((-2.3,4.12),):
    G['detail'].beam((x,YF-.08,0),(x,YF-.08,z1),.08,.08,M['iron'],grain=False);G['detail'].box((x,YF-.1,z1+.06),(.16,.14,.14),M['iron'],.01)
    for z in (.5,1.7,2.9,4.0):
        if z<z1:G['detail'].box((x,YF-.04,z),(.11,.08,.04),M['iron'],.004)
# ---- export, preview scene, manifest -------------------------------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'not grounded: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookNorthridgeHouse6'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-40,-40,0),(40,-40,0),(40,40,0),(-40,40,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-15,-20,6.8),(81,0,-37),lens=38);cam('Front',(0,-30,5.0),(90,0,0),12.6);cam('Side',(-30,0,5.0),(90,0,-90),12.6);cam('Rear',(0,30,5.0),(90,0,180),12.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'NorthridgeHouse6-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'house triangle budget: {tris} '+str({o.name:sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs})
manifest=dict(asset='SilverbrookNorthridgeHouse6',reference=dict(file='NorthridgeHouse6-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_northridgehouse6.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookNorthridgeHouse6',slot_m=list(SLOT),outputs=['SilverbrookNorthridgeHouse6.blend','SilverbrookNorthridgeHouse6.glb','SilverbrookNorthridgeHouse6.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('NORTHRIDGEHOUSE6_BUILT',manifest['bounds_m'],tris)
