"""Silverbrook Weapons Vendor from docs/Game-Design/concept-art/Silverbrook/Weapons Vendor.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_weaponsvendor.py
Original procedural geometry interpreting the elevation (4 m between the posts, finials to 4.6 m):
striped canvas awning on rafters with a ragged valance, two finialed posts with rope lashings
and iron bands, a plank back wall, a counter with a red drape, a hanging banner and lantern,
and the vendor's stock: modelled polearms, axes, swords, flails, arrows and a round shield,
plus many CC0 Poly Haven weapons, barrels and crates placed with kit.model.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
from mathutils import Matrix
import bpy,bmesh,numpy as np
scene=kit.reset(4606)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Weapons Vendor.png'
asset=bpy.data.collections.new('SilverbrookWeaponsVendor');scene.collection.children.link(asset)

M=dict(
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    boards=kit.scan('SB_WeaponsVendor_WallBoards','old_planks_02',dark=(.42,.36,.30),aged=.2),
    haft=kit.scan('SB_WeaponsVendor_Haft','weathered_brown_planks',dark=(.42,.24,.14),damp=False,aged=.5),
    leather=kit.scan('SB_WeaponsVendor_Leather','brown_leather',dark=(.30,.20,.14),damp=False,aged=.2),
    red=kit.scan('SB_WeaponsVendor_ClothRed','velour_velvet',dark=(.36,.11,.09),normal=.7,damp=False,aged=.7),
    cream=kit.scan('SB_WeaponsVendor_ClothCream','hessian_380',dark=(.50,.40,.28),normal=.7,damp=False,aged=.12),
    rope=kit.scan('SB_WeaponsVendor_Rope','hessian_380',dark=(.45,.36,.22),damp=False,aged=.35),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    steel=kit.solid('SB_WeaponsVendor_Steel',(.42,.42,.43),.36,1.0),
    worn=kit.solid('SB_WeaponsVendor_SteelWorn',(.11,.105,.1),.62,.85),
    gilt=kit.solid('SB_WeaponsVendor_Gilt',(.80,.55,.18),.35,1.0),
    glow=kit.solid('SB_WeaponsVendor_Glow',(.3,.2,.1),.3,0,(1.0,.45,.12),6))
BANDS=kit.bands('weathered_brown_planks');WBANDS=kit.bands('old_planks_02')
def band():return kit.rng.choice(BANDS)
G=dict(timber=Geo('SM_SBWeaponsVendor_Timber',asset),wall=Geo('SM_SBWeaponsVendor_Wall',asset),cloth=Geo('SM_SBWeaponsVendor_Cloth',asset),
       stock=Geo('SM_SBWeaponsVendor_Stock',asset),detail=Geo('SM_SBWeaponsVendor_Details',asset))
R_=kit.rng
# ---- measured layout (metres) ------------------------------------------------------
SLOT=(6,3.5,5)                 # concept README 'Fits' (w x d x h)
PX,YP,PS,PT=2.0,-.45,.18,4.15  # main posts: centre x, y, section, top (finial tip 4.6)
YB,BT=.75,3.82                 # rear posts / back wall line, rear post top
WY=.71                         # back wall front face
RX=1.80                        # rafters (under the awning edges)
ZR0,YR0,ZR1,YR1=3.80,.80,2.84,-1.48   # rafter: rear end (on the top rail) -> front end
YF=-1.42                       # front batten (carries the valance)
CX=1.87                        # awning half-width (clear of the posts)
CT,CY0,CY1,CW=.95,-1.05,-.45,1.25   # counter top, front, back, half-width
TH=.006
def rz(y):return ZR0+(y-YR0)*(ZR1-ZR0)/(YR1-YR0)     # rafter centre line

# ---- geometry helpers ------------------------------------------------------------------------
def V(*a):return Vector(a)
def M4(x,y,z,o):
    m=Matrix.Identity(4)
    for i in range(3):m[i][0],m[i][1],m[i][2],m[i][3]=x[i],y[i],z[i],o[i]
    return m
def aim(bottom,top,face=(1,0,0)):
    """Frame at `bottom`: local z toward `top`, local x toward `face` (blade side), y the flat normal."""
    b,t=Vector(bottom),Vector(top);z=(t-b).normalized();f=Vector(face);x=(f-z*f.dot(z)).normalized();return M4(x,z.cross(x),z,b),(t-b).length
def loft(geo,rings,mat,closed=False,smooth=False,w=(0,0),caps=(True,True)):
    """Closed solid through rings of equal size; winding is fixed by signed volume (works under mirrors)."""
    rings=[[Vector(p) for p in r] for r in rings];k=len(rings[0]);faces=[]
    for a,b in list(zip(rings,rings[1:]))+([(rings[-1],rings[0])] if closed else []):
        for i in range(k):faces.append([a[i],a[(i+1)%k],b[(i+1)%k],b[i]])
    if not closed:
        if caps[0]:faces.append(list(reversed(rings[0])))
        if caps[1]:faces.append(list(rings[-1]))
    c=sum((p for r in rings for p in r),Vector())/(len(rings)*k)
    vol=sum((f[0]-c).dot((f[j]-c).cross(f[j+1]-c)) for f in faces for j in range(1,len(f)-1))
    assert abs(vol)>1e-12,'flat loft'
    for f in faces:geo.poly(f if vol>0 else f[::-1],mat,w=w,smooth=smooth)
def blade(geo,W,st,mat,single=False):
    """Blade along local z. st: (z, xa, xb, half thickness); diamond section, or single-edged (back xa, edge xb)."""
    rings=[]
    for z,xa,xb,t in st:
        pts=[(xa,-t,z),(xb,0,z),(xa,t,z)] if single else [(xa,0,z),((xa+xb)/2,-t,z),(xb,0,z),((xa+xb)/2,t,z)]
        rings.append([W@V(*p) for p in pts])
    loft(geo,rings,mat)
def head(geo,W,keys,mat,ti=.011,te=.0015,edge=.035):
    """Axe bit: keys (z, inner x, edge x) up the haft; wedge section thinning to the edge.
    Dark forged body (SteelWorn) with a bright ground edge band of width `edge` (mat)."""
    xm=lambda xi,xo:max(xi+.4*(xo-xi),xo-edge)
    tm=lambda xi,xo:ti+(te-ti)*(xm(xi,xo)-xi)/max(xo-xi,1e-6)
    loft(geo,[[W@V(xi,-ti,z),W@V(xm(xi,xo),-tm(xi,xo),z),W@V(xm(xi,xo),tm(xi,xo),z),W@V(xi,ti,z)] for z,xi,xo in keys],M['worn'])
    loft(geo,[[W@V(xm(xi,xo),-tm(xi,xo),z),W@V(xo,-te,z),W@V(xo,te,z),W@V(xm(xi,xo),tm(xi,xo),z)] for z,xi,xo in keys],mat)
def wbox(geo,W,c,size,mat,bevel=.004,**kw):geo.box(W@V(*c),size,mat,bevel,W.to_3x3().normalized(),**kw)
def wbeam(geo,W,a,b,wd,dp,mat,**kw):geo.beam(W@V(*a),W@V(*b),wd,dp,mat,**kw)
def ring(n,r,z=0,c=(0,0,0),phase=0):return [V(c[0]+r*math.cos(phase+math.tau*i/n),c[1]+r*math.sin(phase+math.tau*i/n),c[2]+z) for i in range(n)]
def hoop(geo,centre,u,v,n,radius,section,mat,seg=32):
    """Closed ring swept around a circle in the u-v plane; section: (radial, normal) offsets."""
    centre,u,v,n=Vector(centre),Vector(u),Vector(v),Vector(n)
    loft(geo,[[centre+(u*math.cos(a)+v*math.sin(a))*(radius+dr)+n*dn for dr,dn in section] for a in (math.tau*i/seg for i in range(seg))],mat,closed=True,smooth=True)
def rect(w,t):return [(-w/2,-t/2),(w/2,-t/2),(w/2,t/2),(-w/2,t/2)]
def ball(geo,c,r,mat,nlat=7,nlon=12):
    c=Vector(c);loft(geo,[[c+V(r*math.sin(t)*math.cos(math.tau*j/nlon),r*math.sin(t)*math.sin(math.tau*j/nlon),-r*math.cos(t)) for j in range(nlon)] for t in (math.pi*i/nlat for i in range(1,nlat))],mat,smooth=True)
def basis(d):
    d=Vector(d).normalized();a=V(1,0,0) if abs(d.x)<.9 else V(0,1,0);u=d.cross(a).normalized();return u,d.cross(u)
def spike(geo,base,d,length,r,mat):
    base,d=Vector(base),Vector(d).normalized();u,v=basis(d)
    loft(geo,[[base+(u*math.cos(a)+v*math.sin(a))*r for a in (math.tau*i/4 for i in range(4))],[base+d*length+(u*math.cos(a)+v*math.sin(a))*.0012 for a in (math.tau*i/4 for i in range(4))]],mat)
def chain(geo,top,bottom,mat,link=.045,wire=.007):
    top,bottom=Vector(top),Vector(bottom);D=(bottom-top).length;n=max(2,round(D/(link*.78)));d=(bottom-top).normalized();u,v=basis(d)
    for k in range(n):
        c=top+d*(link/2+k*(D-link)/(n-1));side=u if k%2 else v;hw=.014
        for s in (-1,1):geo.beam(c+side*s*hw-d*link/2,c+side*s*hw+d*link/2,wire,wire,mat,grain=False)
        for s in (-1,1):geo.beam(c-side*(hw+wire/2)+d*s*(link/2-wire/2),c+side*(hw+wire/2)+d*s*(link/2-wire/2),wire,wire,mat,grain=False)
def rope(x,y,z,r=.13,turns=4,geo=None):
    for k in range(turns):(geo or G['detail']).vessel((x,y,z+(k-turns/2)*.028),[(r,0),(r+.009,.014),(r,.028)],M['rope'],12)
def spire(geo,x,y,rings,tip,mat):
    """Square-section finial: rings [(z, half)] bottom-up, then a point at z=tip."""
    sq=lambda z,h:[(x-h,y-h,z),(x+h,y-h,z),(x+h,y+h,z),(x-h,y+h,z)]
    geo.poly(list(reversed(sq(*rings[0]))),mat)
    for lo,hi in zip(rings,rings[1:]):
        a,b=sq(*lo),sq(*hi)
        for i in range(4):geo.poly([a[i],a[(i+1)%4],b[(i+1)%4],b[i]],mat)
    top=sq(*rings[-1])
    for i in range(4):geo.poly([top[i],top[(i+1)%4],(x,y,tip)],mat)

# ---- modelled weapons (local: z along the haft/blade, x the edge side, y the flat normal) ---------------
S=G['stock']
def sword(W,L=.86,w=.052,mat=None):
    """Arming sword with the guard at the origin and the blade up local z."""
    blade(S,W,[(0,-w/2,w/2,.0055),(L*.5,-w*.42,w*.42,.0048),(L*.82,-w*.3,w*.3,.004),(L,-.002,.002,.0015)],mat or M['steel'])
    q=[(-.115,-.004),(-.07,-.012),(0,-.015),(.07,-.012),(.115,-.004)]
    for (a0,z0),(a1,z1) in zip(q,q[1:]):wbeam(S,W,(a0,0,z0),(a1,0,z1),.02,.026,M['worn'],grain=False)
    wbox(S,W,(0,0,-.012),(.05,.03,.028),M['worn'],.004)
    wbeam(S,W,(0,0,-.024),(0,0,-.215),.03,.026,M['leather'])
    loft(S,[[W@V(.03*math.cos(a),y,-.24+.03*math.sin(a)) for a in (math.tau*i/8 for i in range(8))] for y in (-.013,.013)],M['worn'])
def knife(W,L=.15):
    blade(S,W,[(0,-.006,.016,.003),(L*.6,-.006,.014,.0026),(L*.9,-.001,.008,.002),(L,.004,.0045,.001)],M['steel'],single=True)
    wbox(S,W,(.005,0,-.006),(.032,.018,.012),M['worn'],.003)
    wbeam(S,W,(.005,0,-.012),(.005,0,-.115),.022,.016,M['haft'])
def haft(W,z0,z1,d=.032):
    wbeam(S,W,(0,0,z0),(0,0,z1),d,d,M['haft'])
    wbeam(S,W,(0,0,0),(0,0,.06),d+.006,d+.006,M['iron'],grain=False)            # butt ferrule
def spear(W,L):
    haft(W,.05,L-.26);wbeam(S,W,(0,0,L-.3),(0,0,L-.2),.03,.03,M['worn'],grain=False)
    blade(S,W,[(L-.21,-.011,.011,.008),(L-.15,-.033,.033,.008),(L-.08,-.026,.026,.007),(L,-.001,.001,.002)],M['steel'])
def halberd(W,L):
    haft(W,.05,L-.36)
    for s in (-1,1):wbox(S,W,(s*.019,0,L-.5),(.006,.02,.3),M['iron'],.002)       # langets
    wbox(S,W,(0,0,L-.29),(.05,.045,.17),M['worn'],.006)
    blade(S,W,[(L-.21,-.012,.012,.007),(L-.1,-.009,.009,.006),(L,-.001,.001,.002)],M['steel'])
    head(S,W,[(L-.37,.02,.2),(L-.33,.02,.235),(L-.26,.02,.25),(L-.19,.02,.24),(L-.15,.02,.16),(L-.125,.02,.07)],M['steel'])
    Wb=W@Matrix.Translation((-.02,0,L-.27))@Matrix.Rotation(-math.pi/2,4,'Y')        # rear fluke, drooping
    blade(S,Wb,[(0,-.022,.022,.008),(.09,-.012,.006,.006),(.16,-.03,-.027,.002)],M['worn'])
def glaive(W,L):
    haft(W,.05,L-.5);wbeam(S,W,(0,0,L-.62),(0,0,L-.48),.034,.034,M['worn'],grain=False)
    blade(S,W,[(L-.5,-.016,.032,.006),(L-.38,-.016,.046,.006),(L-.24,-.01,.054,.0055),(L-.12,.006,.052,.005),(L-.04,.03,.05,.004),(L,.05,.052,.0015)],M['steel'],single=True)
    Wb=W@Matrix.Translation((-.014,0,L-.36))@Matrix.Rotation(-math.pi/2,4,'Y')
    blade(S,Wb,[(0,-.012,.016,.005),(.06,.004,.01,.004),(.09,.02,.022,.0015)],M['worn'])
def bearded(W,L):
    haft(W,.05,L+.02)
    wbox(S,W,(0,0,L-.08),(.05,.046,.12),M['worn'],.006)
    head(S,W,[(L-.31,.12,.17),(L-.25,.06,.19),(L-.18,.024,.2),(L-.11,.024,.198),(L-.05,.024,.178),(L-.025,.024,.11)],M['steel'])
def double_axe(W,L,h=.17,reach=.24):
    """Crescent double-bit axe on a tall haft; L to the top spike."""
    zc=L-.24;haft(W,.05,zc+.06)
    wbox(S,W,(0,0,zc),(.056,.05,.15),M['worn'],.006)
    for sx in (1,-1):
        Wm=W@Matrix.Scale(sx,4,(1,0,0))
        head(S,Wm,[(zc+h*u,.024+(reach-.03)*u*u,reach+.05*(1-u*u)) for u in np.linspace(-.97,.97,11)],M['steel'],ti=.012)
    blade(S,W,[(zc+.07,-.012,.012,.008),(zc+.16,-.01,.01,.006),(L,-.001,.001,.002)],M['steel'])
def morning_star(c,r=.085):
    ball(S,c,r,M['worn'])
    gold=math.pi*(3-math.sqrt(5))
    for i in range(14):
        z=1-2*(i+.5)/14;rr=math.sqrt(1-z*z);d=V(rr*math.cos(gold*i),rr*math.sin(gold*i),z);spike(S,Vector(c)+d*r*.85,d,r*.15+.045,.016,M['iron'])
def arrow(bottom,top,fl):
    W,L=aim(bottom,top,(1,0,0))
    blade(S,W,[(.0,-.0005,.0005,.0005),(.05,-.009,.009,.003),(.065,-.004,.004,.003)],M['worn'])
    wbeam(S,W,(0,0,.06),(0,0,L),.009,.009,M['haft'])
    for k in range(3):
        a=math.tau*k/3+.4;R=Matrix.Rotation(a,4,'Z');wbox(S,W@R,(.011,0,L-.1),(.014,.0016,.11),fl,0)
def shield(c,r=.33,th=.026):
    """Round board shield facing -Y: five boards, iron rim, boss and straps."""
    c=Vector(c);n=5;xs=np.linspace(-r,r,n+1)
    for x0,x1 in zip(xs,xs[1:]):
        pts=[]
        for x in np.linspace(x0+.003,x1-.003,5):h=math.sqrt(max(r*r-x*x,1e-6))*.995;pts.append(c+V(x,0,h))
        for x in np.linspace(x1-.003,x0+.003,5):h=math.sqrt(max(r*r-x*x,1e-6))*.995;pts.append(c+V(x,0,-h))
        G['timber'].prism(pts,(0,1,0),th,M['timber'])
    hoop(G['detail'],c,(1,0,0),(0,0,1),(0,1,0),r,[(-.012,-th/2-.006),(.006,-th/2-.006),(.006,th/2),(-.012,th/2)],M['iron'],40)
    f=c-V(0,th/2,0)
    loft(G['detail'],[[f+V(p.x,-z,p.y) for p in ring(12,rr)] for rr,z in ((.105,-.002),(.105,.008),(.075,.012),(.072,.04),(.055,.07),(.03,.085),(.008,.09))],M['worn'],smooth=True)
    for k in range(4):
        a=math.pi/4+k*math.pi/2;d=V(math.cos(a),0,math.sin(a));R=Matrix((d,V(0,1,0),d.cross(V(0,1,0)))).transposed()
        G['detail'].box(f+d*(r+.1)/2-V(0,.004,0),(r-.1,.008,.035),M['iron'],.002,R)
        for t in (.16,.27):G['detail'].box(f+d*t-V(0,.01,0),(.014,.008,.014),M['iron'],.003,R)

# ---- Poly Haven models -----------------------------------------------------------
PH=[];PHN={}
def rot(rx=0,ry=0,rzz=0):return (Matrix.Rotation(math.radians(rzz),4,'Z')@Matrix.Rotation(math.radians(ry),4,'Y')@Matrix.Rotation(math.radians(rx),4,'X'))
def ph(mid,loc,R=None,s=1,rest=None,anchor='centre',keep=None,aged=.12):
    """Place a Poly Haven model: model anchor -> loc after rotation R and scale s; rest drops its lowest point to z=rest."""
    PHN[mid]=PHN.get(mid,0)+1
    objs=kit.model(asset,mid,Matrix(),aged=aged,name=f'SM_SBWeaponsVendor_PH_{mid}_{PHN[mid]:02d}')
    if keep is not None:
        for k,o in enumerate(objs):
            if k not in keep:bpy.data.objects.remove(o,do_unlink=True)
        objs=[o for k,o in enumerate(objs) if k in keep]
    P=np.concatenate([np.array([v.co[:] for v in o.data.vertices]) for o in objs]);lo,hi=P.min(0),P.max(0)
    if anchor=='centre':A=(lo+hi)/2
    elif anchor=='origin':A=np.zeros(3)
    elif anchor=='guard':   # the crossbar: the widest verts in x
        cx=(lo[0]+hi[0])/2;half=(hi[0]-lo[0])/2;A=np.array([cx,0,P[np.abs(P[:,0]-cx)>.8*half][:,2].mean()])
    W=Matrix.Translation(loc)@(R or Matrix())@Matrix.Scale(s,4)@Matrix.Translation(-Vector(A))
    if rest is not None:
        Q=np.c_[P,np.ones(len(P))]@np.array(W).T;W=Matrix.Translation((0,0,rest-Q[:,2].min()))@W
    for o in objs:o.matrix_world=W
    PH.extend(objs);return objs

# ---- posts, rafters, rails, finials ------------------------------------------------------------------
T=G['timber']
def iron_band(x,y,z,h=.1,s=PS):G['detail'].box((x,y,z),(s+.022,s+.022,h),M['iron'],.004)
for s in (-1,1):
    x=s*PX
    T.box((x,YP,PT/2),(PS,PS,PT),M['timber'],.016,grain=True,band=band())
    T.box((x,YP,PT+.03),(.24,.24,.06),M['timber'],.012,grain=True,band=band())
    spire(T,x,YP,[(PT+.06,.04),(PT+.1,.04),(PT+.18,.085),(PT+.22,.085)],4.6,M['timber'])
    for z,h in ((.1,.14),(1.0,.08),(2.2,.08),(3.62,.08),(4.02,.1)):iron_band(x,YP,z,h)
    rope(x,YP,rz(YP)+.02,.135,5)                                                  # rafter lashing
    T.box((s*(RX+.065),YP,rz(YP)),(.11,.12,.14),M['timber'],.01,grain=True,band=band())   # bolster post -> rafter
    T.beam((s*1.87,YP-PS/2+.01,2.4),(s*1.87,-1.04,rz(-1.04)-.03),.07,.07,M['timber'],band=band())  # knee brace
    # rear posts
    T.box((x,YB,BT/2),(.16,.16,BT),M['timber'],.014,grain=True,band=band())
    T.box((x,YB,BT+.02),(.2,.2,.04),M['timber'],.01,grain=True,band=band())
    for z in (.1,3.6):iron_band(x,YB,z,.08,.16)
    # rafter from the rear top rail to past the front batten
    T.beam((s*RX,YR0+.06,rz(YR0+.06)),(s*RX,YR1,ZR1),.08,.1,M['timber'],band=band())
    rope(x,YB,3.72,.115,3)
rope(-PX,YP,1.38,.135,4)                                                          # left post: polearms tied here
T.beam((-PX-.06,YB,3.72),(PX+.06,YB,3.72),.1,.1,M['timber'],band=band())          # rear top rail
T.beam((-RX-.04,YF,rz(YF)-.085),(RX+.04,YF,rz(YF)-.085),.07,.07,M['timber'],band=band())   # front batten under the rafters
# ---- back wall: vertical boards, rear battens ---------------------------------------------------------
x=-1.92;step=False
while x<1.92-.02:
    w=min(R_.uniform(.17,.24),1.92-x);top=3.67-R_.uniform(0,.03);step=not step   # boards overlap 1 cm, alternately stepped back 4 mm
    G['wall'].box((x+w/2,WY+.02+(.004 if step else 0),top/2),(w+.01,.04,top),M['boards'],.006,grain=True,band=R_.choice(WBANDS));x+=w
for z in (.35,1.8,3.25):G['wall'].box((0,WY+.06,z),(3.84,.04,.12),M['boards'],.008,grain=True,band=R_.choice(WBANDS))
G['wall'].beam((-1.88,WY+.105,.5),(1.88,WY+.105,3.1),.1,.035,M['boards'],grain=True,band=R_.choice(WBANDS))     # rear diagonal brace
# ---- awning: seven stripes over the rafters, ragged valance ------------------------------------------
Rr=.05
def ztop(y):return rz(y)+.05+TH
path=[(YR0+.13,3.70),(YR0+.13,3.79),(YR0+.09,ztop(YR0+.09)+.03)]
for k in range(15):t=k/14;y=YR0+.06+(YF-.03-(YR0+.06))*t;path.append((y,ztop(y)))
path+=[(YR1+.01,ZR1+.045),(YR1-.03,ZR1+.005),(YR1-.04,ZR1-.05)]
VAL=len(path)-1
for k in range(1,7):path.append((YR1-.04-.004*k,ZR1-.05-.058*k))
MAIN=(3,3+14)
def sheet(rows,xs,mats,grime=lambda x,v:0,geo=None):
    """Two-sided cloth from a vertex grid rows[i][j]; mats[i] per column (same as the Market Stall)."""
    geo=geo or G['cloth'];I,J=len(rows),len(rows[0]);arc=[0]
    for a,b in zip(rows[0],rows[0][1:]):arc.append(arc[-1]+(b-a).length)
    def nrm(i,j):return (rows[min(i+1,I-1)][j]-rows[max(i-1,0)][j]).cross(rows[i][min(j+1,J-1)]-rows[i][max(j-1,0)]).normalized()
    N=[[nrm(i,j) for j in range(J)] for i in range(I)]
    for i in range(I-1):
        mat=mats[i];sc=kit.SCALE[mat.name]
        for j in range(J-1):
            ij=[(i,j),(i+1,j),(i+1,j+1),(i,j+1)];uv=[(xs[a]/sc,arc[b]/sc) for a,b in ij];w=(grime(xs[i],arc[j]),0)
            geo.poly([rows[a][b]-N[a][b]*TH/2 for a,b in reversed(ij)],mat,list(reversed(uv)),w)
            geo.poly([rows[a][b]+N[a][b]*TH/2 for a,b in ij],mat,uv,w)
STRIPES=['red','cream']*3+['red'];NS=14;xs=[-CX+2*CX*i/(NS*len(STRIPES)) for i in range(NS*len(STRIPES)+1)];rows=[]
notch=set(R_.sample(range(len(xs)),22))
for i,x in enumerate(xs):
    fold=.02*math.sin(x*23+.7);local=(i%NS)/NS;ring_=[];sag=1-(x/CX)**2
    for j,(y,z) in enumerate(path):
        f=max(0,j-VAL)/(len(path)-1-VAL);t=min(1,max(0,(j-MAIN[0])/(MAIN[1]-MAIN[0])))
        ring_.append(V(x,y-fold*f,z-(.07*sag+.02*math.sin(math.pi*local))*math.sin(math.pi*t)))
    y,z=path[-1];hem=ZR1-.44-R_.uniform(0,.06)-(.06*R_.random() if R_.random()<.4 else 0)+(.13 if i in notch else 0)+.03*math.sin(i*.9)
    ring_.append(V(x,y-.012-fold,hem));rows.append(ring_)
def grime(x,v):return .12+.12*math.sin(x*2.3+v*3.1)**2+(.14 if v>2.3 else 0)
sheet(rows,xs,[M[STRIPES[i//NS]] for i in range(len(xs)-1)],grime)
for sx in (-1,1):    # short side drops along the rafters
    ys=[YR0-.02+(YR1+.04-(YR0-.02))*k/16 for k in range(17)][::sx]
    rows=[[V(sx*(CX-.003)-sx*.01*d*math.sin(y*19),y,ztop(y)-.004-d*(.2+(R_.uniform(0,.07) if d==1 else 0))) for d in (0,.4,.75,1)] for y in ys]
    sheet(rows,[-sx*y for y in ys],[M['red']]*16)
# ---- counter: plank top, legs, braced end bays, red drape -----------------------------------------
for k in range(4):
    w=(CY1-CY0)/4;T.box((0,CY0+w*(k+.5),CT-.03),(2*CW,w-.006,.06),M['timber'],.008,grain=True,band=band())
for x in (-CW+.06,-.82,.82,CW-.06):
    for y in (CY0+.06,CY1-.06):T.box((x,y,(CT-.06)/2),(.1,.1,CT-.06),M['timber'],.012,grain=True,band=band())
    for z in (.16,CT-.16):G['detail'].box((x,CY0+.005,z),(.12,.012,.12),M['iron'],.003)
T.box((0,CY0+.03,CT-.13),(2*CW-.02,.05,.14),M['timber'],.008,grain=True,band=band())       # apron
for y in (CY0+.06,CY1-.06):T.box((0,y,.12),(2*CW-.02,.06,.08),M['timber'],.008,grain=True,band=band())
for k in range(3):T.box((0,CY0+.15+k*.15,.18),(2*CW-.24,.14,.035),M['timber'],.006,grain=True,band=band())   # low shelf
for s in (-1,1):
    a,b=s*(CW-.11),s*.87
    T.beam((a,CY0+.03,.18),(b,CY0+.03,CT-.22),.06,.04,M['timber'],band=band());T.beam((b,CY0+.01,.18),(a,CY0+.01,CT-.22),.06,.04,M['timber'],band=band())
    for y0,y1 in ((CY0+.11,CY1-.11),):
        T.beam((s*(CW-.03),y0,.18),(s*(CW-.03),y1,CT-.22),.06,.04,M['timber'],band=band());T.beam((s*(CW-.01),y1,.18),(s*(CW-.01),y0,CT-.22),.06,.04,M['timber'],band=band())
cc=[(CY0+.34,CT+TH),(CY0+.1,CT+TH),(CY0-.006,CT-.004),(CY0-.016,CT-.04)]+[(CY0-.018,CT-.04-k*.1) for k in range(1,7)]
cx=[-.8+1.6*i/40 for i in range(41)]
def drape_y(x,y,z):return y-(.012*math.sin(x*14) if abs(x)>.36 and z<CT-.03 else 0)
rows=[[V(x,drape_y(x,y,z),z) for y,z in cc]+[V(x,drape_y(x,CY0-.02,.3),.3-R_.uniform(0,.08)-(.08 if R_.random()<.25 else 0)+.02*math.sin(x*9))] for x in cx]
sheet(rows,cx,[M['red']]*40,lambda x,v:.6+.15*math.sin(x*4)**2+(.1 if v>.55 else 0))
def crossed_swords(geo,o,right,up,n,s,th=.008):
    """Two gilt sword silhouettes crossed in an X (points up), on a cloth face."""
    o,right,up,n=Vector(o),Vector(right),Vector(up),Vector(n)
    for sg in (-1,1):
        a=math.radians(sg*38);d=right*math.sin(a)+up*math.cos(a);p=right*math.cos(a)-up*math.sin(a)
        def P(u,v):return o+(d*(v-.1)+p*u)*s     # cross at mid-blade
        for poly in ([(-.022,-.12),(.022,-.12),(.022,.3),(0,.36),(-.022,.3)],[(-.085,-.145),(.085,-.145),(.085,-.12),(-.085,-.12)],[(-.014,-.24),(.014,-.24),(.014,-.145),(-.014,-.145)],[(0,-.29),(.03,-.262),(0,-.234),(-.03,-.262)]):
            geo.prism([P(u,v) for u,v in poly],n,th,M['gilt'])
crossed_swords(G['detail'],(0,CY0-.016-TH/2-.001,.66),(1,0,0),(0,0,1),(0,-1,0),.7)
# ---- counter stock: display cases, tray, lantern ----------------------------------------------------------
def case(x0,x1,y0,y1,h,lid=True):
    """Open wooden display case on the counter, red-lined; returns the lining top z."""
    z0=CT;xm,ym,w,d=(x0+x1)/2,(y0+y1)/2,x1-x0,y1-y0
    T.box((xm,ym,z0+.01),(w,d,.02),M['timber'],.004,grain=True,band=band())
    for y in (y0+.01,y1-.01):T.box((xm,y,z0+.02+h/2),(w,.02,h),M['timber'],.004,grain=True,band=band())
    for x in (x0+.01,x1-.01):T.box((x,ym,z0+.02+h/2),(.02,d-.04,h),M['timber'],.004,grain=True,band=band())
    G['detail'].box((xm,ym,z0+.025),(w-.04,d-.04,.01),M['red'],0)
    if lid:
        hz=z0+.02+h;Rl=Matrix.Rotation(math.radians(-100),3,'X');hinge=V(xm,y1,hz)
        def L(p):return hinge+Rl@(Vector(p)-hinge)
        T.box(L((xm,ym,hz+.01)),(w,d,.02),M['timber'],.004,Rl,grain=True,band=band())
        for x in (x0+.08,x1-.08):G['detail'].box(V(x,y1+.003,hz),(.05,.012,.05),M['iron'],.002)
    for x in (x0+.022,x1-.022):G['detail'].box(V(x,y0-.002,z0+.02+h-.02),(.04,.006,.04),M['iron'],.002)
    return z0+.03
def lantern(base,r=.085,h=.24,hang=None):
    """Hexagonal forged-iron lantern (glass on SB_WeaponsVendor_Glow); returns the glass centre."""
    b=Vector(base);D=G['detail'];hx=lambda z,rr:[b+p for p in ring(6,rr,z,phase=math.pi/6)]
    if hang is None:
        for p in ring(3,r*.8,0,phase=.3):D.box(b+p+V(0,0,.012),(.03,.03,.024),M['iron'],.004)
        z0=.024
    else:
        loft(D,[hx(-.06,.012),hx(0,r*.7),hx(.02,r+.012)],M['iron']);z0=.02
    loft(D,[hx(z0,r+.014),hx(z0+.025,r+.014)],M['iron'])
    loft(D,[hx(z0+.025,r-.006),hx(z0+.025+h,r-.006)],M['glow'])
    for p,q in zip(hx(z0+.025,r),hx(z0+.025+h,r)):D.beam(p,q,.012,.012,M['iron'],grain=False)
    for z in (z0+.025+h*.5,):loft(D,[hx(z-.006,r+.002),hx(z+.006,r+.002)],M['iron'])
    zt=z0+.025+h;loft(D,[hx(zt,r+.018),hx(zt+.025,r+.018),hx(zt+.03,r+.006),hx(zt+.1,r*.45),hx(zt+.14,.012)],M['iron'])
    top=b+V(0,0,zt+.14)
    for s in (-1,1):D.beam(top+V(s*.03,0,0),top+V(s*.03,0,.06),.008,.008,M['iron'],grain=False)
    D.beam(top+V(-.035,0,.06),top+V(.035,0,.06),.008,.008,M['iron'],grain=False)
    return b+V(0,0,z0+.025+h/2),top+V(0,0,.06)
SOCKETS=[]
zl=case(-1.17,-.66,-1.0,-.68,.09)
for k,(x,y,a) in enumerate([(-.92,-.94,4),(-.9,-.84,-3),(-.93,-.75,6)]):ph('ornate_medieval_dagger',(x,y,0),rot(90,0,90+a),rest=zl,keep=[0])
zr=case(.36,1.06,-1.0,-.62,.1)
ph('ornate_medieval_dagger',(.55,-.92,0),rot(90,0,82),rest=zr,keep=[0]);ph('ornate_medieval_dagger',(.55,-.82,0),rot(90,0,95),rest=zr,keep=[0])
ph('ornate_medieval_dagger',(.85,-.9,0),rot(90,0,-70),rest=zr)
ph('ornate_medieval_dagger',(.5,-.72,0),rot(90,0,100),rest=zr,keep=[0]);ph('ornate_medieval_dagger',(.72,-.75,0),rot(90,0,-95),rest=zr,keep=[0])
for x,y,a in ((.86,-.72,8),(.95,-.78,-12)):knife(aim((x,y,zr+.017),(x-.15*math.cos(math.radians(a)),y-.15*math.sin(math.radians(a)),zr+.017),(0,-1,0))[0])
# middle tray: low box with fanned knives and daggers
T.box((-.06,-.86,CT+.01),(.6,.3,.02),M['timber'],.004,grain=True,band=band())
for y in (-1.0,-.72):T.box((-.06,y,CT+.035),(.6,.02,.05),M['timber'],.004,grain=True,band=band())
for x in (-.35,.23):T.box((x,-.86,CT+.035),(.02,.26,.05),M['timber'],.004,grain=True,band=band())
G['detail'].box((-.06,-.86,CT+.025),(.56,.26,.01),M['red'],0)
for k,x in enumerate((-.27,-.17,-.07,.03,.13)):
    if k%2:ph('ornate_medieval_dagger',(x,-.86,0),rot(90,0,180+(k-2)*6),rest=CT+.03,keep=[0])
    else:knife(aim((x,-.8,CT+.042),(x+.02*(k-2),-.95,CT+.042),(1,0,0))[0])
c,_=lantern((-.48,-.57,CT));SOCKETS.append(c)
ph('ornate_medieval_mace',(-.92,-.55,0),rot(0,87,4),rest=CT)
ph('wooden_handle_saber',(.58,-.54,0),rot(0,90,0),rest=CT)
ph('ornate_war_hammer',(1.16,-.74,0),rot(90,0,0),rest=CT)
# ---- back wall stock -----------------------------------------------------------------------------
# sword rack: two stiles, three rails, pegs; six swords hang point-down, one lies across the top
RY=WY-.05
for x in (-.62,.5):T.box((x,WY-.03,1.68),(.07,.06,1.5),M['timber'],.008,grain=True,band=band())
for z in (1.05,1.9,2.18):T.box((-.06,RY,z),(1.22,.04,.06),M['timber'],.006,grain=True,band=band())
HZ=1.94;SY=RY-.06
for k,x in enumerate((-.5,-.33,-.16,.01,.18,.35)):
    for dx in (-.03,.03):G['detail'].beam(V(x+dx,RY-.02,HZ-.03),V(x+dx,SY-.03,HZ-.03),.016,.016,M['iron'],grain=False)
    if k in (0,2,5):ph('antique_estoc',(x,SY,HZ-.012),rot(0,180,0),s=.7,anchor='guard')
    elif k==3:ph('wooden_handle_saber',(x,SY,HZ-.012),rot(0,180,90),anchor='origin')
    else:sword(M4(V(-1,0,0),V(0,1,0),V(0,0,-1),V(x,SY,HZ+.003)),L=.84)
for x in (-.4,.3):G['detail'].beam(V(x,RY-.02,2.24),V(x,SY-.03,2.24),.016,.016,M['iron'],grain=False)
ph('antique_estoc',(-.46,SY,2.29),rot(0,90,0),s=.7,anchor='guard')
# shelf with morning stars and a mace
T.box((.8,WY-.1,2.13),(.74,.2,.035),M['timber'],.006,grain=True,band=band())
for x in (.52,1.08):T.beam((x,WY-.01,1.98),(x,WY-.17,2.11),.04,.05,M['timber'],band=band())
for x,a in ((.6,-25),(1.02,205)):
    c=V(x,WY-.11,2.1475+.085+.04);morning_star(c)
    d=V(math.cos(math.radians(a)),.25,0).normalized();e=c+d*.36;e.z=2.1475+.016
    wbeam(S,aim(c+d*.08,e)[0],(0,0,0),(0,0,(e-c).length-.08),.03,.03,M['haft'])
ph('ornate_medieval_mace',(.82,WY-.17,0),rot(0,90,-3),rest=2.1475)
shield((.79,WY-.016,1.6))
G['detail'].beam(V(.79,WY,1.95),V(.79,WY-.04,1.95),.02,.02,M['iron'],grain=False)
# large double-bit axes and a spear leaning on the wall
for x,top,L in ((-1.36,2.42,2.35),(-.95,2.3,2.2),(1.42,2.4,2.3)):
    W,_=aim((x,WY-.13,0),(x+.02,WY-.04,L*.998),(1,0,0));double_axe(W,L)
W,L=aim((1.22,WY-.12,0),(1.21,WY-.04,2.5));spear(W,L)
# ---- left: polearm rack, crates, arrows, barrel ---------------------------------------------------------------
for x in (-1.86,-1.28):T.box((x,-.05,.72),(.07,.07,1.44),M['timber'],.008,grain=True,band=band())
T.box((-1.57,-.05,1.36),(.66,.08,.06),M['timber'],.006,grain=True,band=band())
T.box((-1.57,-.05,.1),(.66,.16,.08),M['timber'],.006,grain=True,band=band())
for (xb,xt,tip,kind,face) in ((-1.78,-1.95,2.25,'halberd',-1),(-1.65,-1.7,2.5,'spear',1),(-1.53,-1.46,2.35,'glaive',1),(-1.4,-1.24,2.28,'halberd',1)):
    W,L=aim((xb,-.05+R_.uniform(-.02,.02),.14),(xt,-.03,tip),(face,0,0));{'halberd':halberd,'spear':spear,'glaive':glaive}[kind](W,L)
def crate(cx,cy,w,d,h,z0=0,brace=True):
    """Open slatted crate: corner posts, three boards a side, floor on runners, X brace and iron corners."""
    for sx in (-1,1):
        for sy in (-1,1):T.box((cx+sx*(w/2-.025),cy+sy*(d/2-.025),z0+h/2),(.05,.05,h),M['timber'],.006,grain=True,band=band())
    bh=(h-.03)/3
    for k in range(3):
        z=z0+.02+bh*(k+.5)
        for sy in (-1,1):T.box((cx,cy+sy*(d/2-.01),z),(w-.1,.02,bh-.012),M['timber'],.004,grain=True,band=band())
        for sx in (-1,1):T.box((cx+sx*(w/2-.01),cy,z),(.02,d-.1,bh-.012),M['timber'],.004,grain=True,band=band())
    for sy in (-1,1):T.box((cx,cy+sy*(d/2-.06),z0+.02),(w-.1,.05,.04),M['timber'],.004,grain=True,band=band())
    T.box((cx,cy,z0+.05),(w-.04,d-.04,.02),M['timber'],.004,grain=True,band=band())
    if brace:
        y=cy-d/2-.012
        for sg in (-1,1):T.beam((cx-sg*(w/2-.06),y-sg*.004,z0+.05),(cx+sg*(w/2-.06),y-sg*.004,z0+h-.05),.045,.016,M['timber'],band=band())
    for sx in (-1,1):
        for z in (z0+.06,z0+h-.06):G['detail'].box((cx+sx*(w/2-.03),cy-d/2-.004,z),(.06,.008,.08),M['iron'],.002)
    return z0+.06
fz=crate(-1.66,-.97,.54,.44,.48)
for (x,y,a,kind) in ((-1.81,-1.06,4,'estoc'),(-1.7,-.9,-3,'sword'),(-1.6,-1.05,5,'saber'),(-1.5,-.92,-5,'estoc'),(-1.75,-.98,-6,'sword')):
    if kind=='estoc':ph('antique_estoc',(x,y,0),rot(0,180+a,0),s=.66,rest=fz,anchor='guard')
    elif kind=='saber':ph('wooden_handle_saber',(x,y,0),rot(0,180+a,90),rest=fz,anchor='origin')
    else:sword(aim((x,y,fz+.86),(x+math.sin(math.radians(a))*.86,y,fz),(1,0,0))[0],L=.86)
ph('wooden_axe',(-1.47,-.83,0),rot(0,-4,-90),s=1.3,rest=fz)
ph('wooden_crate_01',(-2.45,-.05,0),rest=0)
az=crate(-2.45,-.05,.5,.36,.4,z0=.342,brace=False)
for k in range(36):
    a=R_.uniform(0,math.tau);r=math.sqrt(R_.random())*.11;b=V(-2.45+r*math.cos(a)*1.4,-.05+r*math.sin(a)*.95,az)
    tdir=V(math.cos(a)*r*1.1,math.sin(a)*r*.6,1).normalized();arrow(b,b+tdir*R_.uniform(.88,.98),M['red'] if k%3 else M['cream'])
rope(-2.45,-.05,az+.42,.125,2,S)
ph('wine_barrel_01',(-2.55,-.86,0),rest=0)
ph('wooden_axe_02',(-2.56,-.84,0),rot(0,90,-20),rest=.872)
# ---- left banner (gold crossed swords, fleur-de-lis) ---------------------------------------------------------
AZ,AX=3.42,-2.86
T.beam((-PX-PS/2+.02,YP,AZ),(AX,YP,AZ),.08,.08,M['timber'],band=band())
T.beam((-PX-PS/2+.02,YP,AZ-.36),(-PX-.36,YP,AZ-.03),.06,.06,M['timber'],band=band())
rope(-PX,YP,AZ,.135,3)
BX=-2.5;bz=[(BX-.23,3.26),(BX+.23,3.26),(BX+.23,1.95),(BX,1.72),(BX-.23,1.95)]
G['detail'].prism([(x,YP,z) for x,z in bz],(0,1,0),.016,M['red'])
G['detail'].beam((BX-.27,YP,3.275),(BX+.27,YP,3.275),.024,.024,M['iron'],grain=False)
for x in (BX-.17,BX+.17):chain(G['detail'],(x,YP,AZ-.045),(x,YP,3.29),M['iron'])
def fleur(geo,o,s,n):
    """Gilt fleur-de-lis: tall centre petal, side petals curling out and down, band, three tails."""
    o=Vector(o);n=Vector(n)
    def P(u,v):return o+V(u*s,0,v*s)
    def curl(pts,ws):
        for (a,wa),(b,wb) in zip(zip(pts,ws),list(zip(pts,ws))[1:]):
            t=Vector((b[0]-a[0],b[1]-a[1])).normalized();q=Vector((-t.y,t.x))
            geo.prism([P(a[0]+q.x*wa,a[1]+q.y*wa),P(a[0]-q.x*wa,a[1]-q.y*wa),P(b[0]-q.x*wb,b[1]-q.y*wb),P(b[0]+q.x*wb,b[1]+q.y*wb)],n,.006,M['gilt'])
    geo.prism([P(u,v) for u,v in [(0,.38),(.075,.22),(.06,.06),(0,.0),(-.06,.06),(-.075,.22)]],n,.006,M['gilt'])
    for sx in (-1,1):
        curl([(sx*.04,.0),(sx*.1,.1),(sx*.17,.16),(sx*.23,.14),(sx*.25,.07),(sx*.22,.03)],[.03,.032,.03,.024,.014,.004])
        curl([(sx*.05,-.08),(sx*.11,-.13),(sx*.17,-.13),(sx*.19,-.09)],[.022,.02,.012,.003])
    geo.prism([P(u,v) for u,v in [(-.15,-.015),(.15,-.015),(.15,-.07),(-.15,-.07)]],n,.006,M['gilt'])
    geo.prism([P(u,v) for u,v in [(-.04,-.07),(.04,-.07),(0,-.22)]],n,.006,M['gilt'])
for sg in (-1,1):
    yb=YP+sg*.008
    for a,b in zip(bz,bz[1:]+bz[:1]):G['detail'].beam(V(a[0],yb+sg*.002,a[1]),V(b[0],yb+sg*.002,b[1]),.016,.022,M['gilt'],grain=False)
    crossed_swords(G['detail'],(BX,yb+sg*.001,2.82),(-sg,0,0),(0,0,1),(0,sg,0),.62,.005)
    fleur(G['detail'],(BX,yb+sg*.001,2.18),.62,(0,sg,0))
# ---- right: crates, polearms, open barrel, hanging lantern ---------------------------------------------------
fz=crate(1.66,-.97,.54,.44,.5)
W,L=aim((1.82,-1.02,fz),(1.9,-1.04,2.2),(1,0,0));halberd(W,L)
W,L=aim((1.5,-.9,fz),(1.5,-.88,2.0),(1,0,0));double_axe(W,L,h=.15,reach=.21)
W,L=aim((1.65,-1.06,fz),(1.62,-1.08,2.42),(1,0,0));spear(W,L)
W,L=aim((1.58,-.98,fz),(1.36,-1.0,2.12),(-1,0,0));glaive(W,L)
W,L=aim((1.45,-1.1,fz),(1.41,-1.12,1.45),(-1,0,0));bearded(W,L)
W,L=aim((1.76,-.84,fz),(1.8,-.82,1.38),(1,0,0));bearded(W,L)
ph('wooden_crate_02',(1.62,-.05,0),rest=0)
ph('wooden_axe_03',(1.6,-.2,0),rot(0,90,20),rest=.453)
ph('ornate_war_hammer',(1.58,.2,0),rot(90,0,-75),rest=.453)
# open barrel of staves and hoops
BXc,BYc,BH,BR=2.54,-.86,.86,.3
def br(z):return BR*(1+.12*math.sin(math.pi*z/BH))
n=18
for k in range(n):
    a0,a1=math.tau*(k+.04)/n,math.tau*(k+.96)/n
    T_=[[V(BXc+rr*math.cos(a),BYc+rr*math.sin(a),z) for rr,a in ((br(z),a0),(br(z),a1),(br(z)-.022,a1),(br(z)-.022,a0))] for z in np.linspace(0,BH,7)]
    loft(T,T_,M['timber'],smooth=False)
T.prism([V(BXc,BYc,.09)+p for p in ring(18,br(.09)-.02)],(0,0,1),.03,M['timber'])
for z in (.1,.3,.56,.76):
    hoop(G['detail'],(BXc,BYc,z),(1,0,0),(0,1,0),(0,0,1),br(z),[(0,-.022),(.007,-.022),(.007,.022),(0,.022)],M['iron'],36)
bz0=.105
W,L=aim((2.6,-.82,bz0),(2.7,-.8,1.62),(1,0,0));bearded(W,L)
W,L=aim((2.42,-.9,bz0),(2.38,-.95,1.55),(-1,0,0));double_axe(W,L,h=.14,reach=.2)
ph('ornate_war_hammer',(2.58,-.98,0),rot(0,-6,0),s=1.6,rest=bz0)
ph('wooden_axe',(2.48,-.74,0),rot(0,5,90),s=1.7,rest=bz0)
ph('ornate_medieval_mace',(2.66,-.92,0),rot(0,8,0),s=1.25,rest=bz0)
LZ=3.06
T.beam((PX+PS/2-.02,YP,LZ),(2.68,YP,LZ),.08,.08,M['timber'],band=band())
T.beam((PX+PS/2-.02,YP,LZ-.34),(PX+.34,YP,LZ-.03),.06,.06,M['timber'],band=band())
rope(PX,YP,LZ,.135,3)
lb=V(2.55,YP,2.3)
c,top=lantern(lb,.09,.26,hang=True);SOCKETS.append(c)
chain(G['detail'],(2.55,YP,LZ-.045),top,M['iron'])

# ---- bake, export, preview scene, manifest -------------------------------------------------------------------
objs=[g.object() for g in G.values()]
cl=next(o for o in objs if o.name=='SM_SBWeaponsVendor_Cloth');bm=bmesh.new();bm.from_mesh(cl.data);bmesh.ops.remove_doubles(bm,verts=bm.verts,dist=1e-5);bm.to_mesh(cl.data);bm.free()
for p in cl.data.polygons:p.use_smooth=True
for o in PH:o.data.transform(o.matrix_world);o.matrix_world=Matrix()
objs+=PH
def bounds():
    P=np.concatenate([np.array([v.co[:] for v in o.data.vertices]) for o in objs]);return P.min(0),P.max(0)
lo,hi=bounds();off=Vector((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2]))
for o in objs:o.data.transform(Matrix.Translation(off))
SOCKETS=[[round(v,3) for v in (c+off)] for c in SOCKETS]
lo,hi=bounds();size=hi-lo
assert abs(lo[2])<1e-4,f'ground contact {lo[2]}'
assert all(s<=l+1e-3 for s,l in zip(size,SLOT)),f'exceeds slot {SLOT}: {[round(v,3) for v in size]}'
OUT=HERE/'SilverbrookWeaponsVendor'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
for k,s in enumerate(SOCKETS):
    lamp=bpy.data.lights.new(f'Preview_Lantern{k}','POINT');lamp.energy=25;lamp.color=(1,.62,.3);lamp.shadow_soft_size=.05;lo_=bpy.data.objects.new(f'Preview_Lantern{k}',lamp);lo_.location=s;pre.objects.link(lo_)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.8;scene.world=world
def cam(name,loc,target=None,rot=None,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;pre.objects.link(o)
    if target:o.rotation_euler=(Vector(target)-Vector(loc)).to_track_quat('-Z','Y').to_euler()
    else:o.rotation_euler=[math.radians(a) for a in rot]
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cz=hi[2]/2
cam('Beauty',(-5.6,-8.2,2.6),(0,0,1.9),lens=34);cam('Front',(0,-30,cz),rot=(90,0,0),ortho=6.4);cam('Side',(-30,0,cz),rot=(90,0,-90),ortho=6.4);cam('Rear',(0,30,cz),rot=(90,0,180),ortho=6.4)
o=Vector(off)
cam('CloseCounter',o+V(.1,-2.9,1.75),o+V(0,-.6,1.1),lens=36);cam('CloseWall',o+V(.2,-2.6,1.95),o+V(-.05,WY,1.75),lens=30)
cam('CloseLeft',o+V(-3.6,-3.0,1.7),o+V(-1.9,-.4,1.3),lens=32);cam('CloseRight',o+V(3.6,-3.0,1.7),o+V(1.9,-.4,1.3),lens=32)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'WeaponsVendor-reference.png')
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=350000,f'budget: {tris} triangles'
meshes={}
for o in objs:
    key=o.name if not o.name.startswith('SM_SBWeaponsVendor_PH_') else 'PolyHaven:'+o.name[len('SM_SBWeaponsVendor_PH_'):].rsplit('_',2)[0]
    e=meshes.setdefault(key,dict(objects=0,triangles=0,materials=[]));e['objects']+=1;e['triangles']+=sum(len(p.vertices)-2 for p in o.data.polygons)
    e['materials']=sorted(set(e['materials'])|{m.name for m in o.data.materials})
manifest=dict(asset='SilverbrookWeaponsVendor',asset_id='BLD-GreenlandsWeaponsVendor-01',reference=dict(file='WeaponsVendor-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_weaponsvendor.py',method='procedural original geometry (concept interpretation, not image reconstruction) plus placed CC0 Poly Haven models',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(float(v),3) for v in size],
    meshes=meshes,triangles=tris,poly_haven_models={k:v for k,v in sorted(PHN.items())},
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans and models)',unreal_mesh='SM_SilverbrookWeaponsVendor',slot_m=list(SLOT),
    light_socket_m=SOCKETS,light_material='SB_WeaponsVendor_Glow',outputs=['SilverbrookWeaponsVendor.blend','SilverbrookWeaponsVendor.glb','SilverbrookWeaponsVendor.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('WEAPONSVENDOR_BUILT',manifest['bounds_m'],tris,SOCKETS)
