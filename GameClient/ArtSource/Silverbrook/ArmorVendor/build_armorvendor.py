"""Silverbrook Armor Vendor from docs/Game-Design/concept-art/Silverbrook/Armor Vendor.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_armorvendor.py
Original procedural geometry interpreting the elevation (4 m between the posts, finials at 4.35 m):
two finialed oak posts with iron bands and rope lashings, a blue/cream striped shed awning with a
ragged valance, a stag banner arm (left) and a hanging lantern arm (right), and the full stock:
three plate harnesses on stands, tiered helmets, a mail hauberk, gauntlets, vambraces, greaves,
belts, a stag counter drape, fur pelts, two shields, a polearm rack, barrels and crates.
Armour is modelled here (closed thin shells); props come from CC0 Poly Haven models (kit.model).
"""
import sys,math,json,hashlib,shutil,random
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
from mathutils import Matrix
import bpy,bmesh
scene=kit.reset(4601)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Armor Vendor.png'
asset=bpy.data.collections.new('SilverbrookArmorVendor');scene.collection.children.link(asset)

M=dict(
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    blue=kit.scan('SB_ArmorVendor_AwningBlue','denim_fabric_04',dark=(.30,.33,.45),normal=.7,damp=False,aged=.6),
    cream=kit.scan('SB_ArmorVendor_AwningCream','terlenka',dark=(.58,.48,.26),normal=.8,damp=False,aged=.4),
    banner=kit.scan('SB_ArmorVendor_Banner','denim_fabric_03',dark=(.16,.19,.32),damp=False,aged=.45),
    rope=kit.scan('SB_ArmorVendor_Rope','hessian_380',dark=(.36,.27,.16),damp=False,aged=.6),
    leather=kit.scan('SB_ArmorVendor_Leather','brown_leather',dark=(.45,.36,.28),damp=False,aged=.15),
    fur=kit.scan('SB_ArmorVendor_Fur','curly_teddy_natural',dark=(.09,.085,.08),normal=2.0,damp=False,aged=.85),
    painted=kit.scan('SB_ArmorVendor_PaintedPlanks','blue_painted_planks',dark=(.20,.22,.28),damp=False,aged=.6),
    steel=kit.solid('SB_ArmorVendor_Steel',(.42,.44,.47),.34,1.0),
    worn=kit.solid('SB_ArmorVendor_SteelWorn',(.25,.25,.26),.5,1.0),
    mail=kit.solid('SB_ArmorVendor_Mail',(.26,.26,.27),.5,.8),
    maildark=kit.solid('SB_ArmorVendor_MailShadow',(.12,.12,.125),.58,.7),
    brass=kit.solid('SB_ArmorVendor_Brass',(.72,.50,.20),.38,1.0),
    gilt=kit.solid('SB_ArmorVendor_Gilt',(.80,.56,.20),.45,.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    glow=kit.solid('SB_ArmorVendor_Glow',(.30,.16,.05),.25,0,(1.0,.45,.12),3.0))
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
G=dict(timber=Geo('SM_SBArmorVendor_Timber',asset),cloth=Geo('SM_SBArmorVendor_Cloth',asset),armour=Geo('SM_SBArmorVendor_Armour',asset),detail=Geo('SM_SBArmorVendor_Details',asset))
A,D,T_,C=G['armour'],G['detail'],G['timber'],G['cloth']
R=random.Random(4601);tau=math.tau;Z=Vector((0,0,1))
# ---- measured layout (metres) -------------------------------------------------
SLOT=(6,3.5,5)                 # concept README / task: 6 x 3.5 x 5 m
PX,PS=1.95,.2                  # post centre x (4 m stall between post faces), post section
YF,YR=-.55,1.05                # front / rear post lines
ZT,ZF=3.7,3.0                  # rear + side top rails, front awning rail
FT=4.0                         # front post top; finial tip 4.35
CX=1.835                       # awning half-width (clear of the posts)
CT,CY0,CY1=1.2,-.66,.35        # counter top, front and back edge
RAIL=.09

# ---- generic thin-solid helpers ---------------------------------------------------
def tab(t,z):
    if z<=t[0][0]:return t[0][1]
    for (a,va),(b,vb) in zip(t,t[1:]):
        if z<=b:return va+(vb-va)*(z-a)/(b-a)
    return t[-1][1]
def slab(geo,P,mat,th=.003,wrap=False,disp=None,w=None,rims=(1,1,1,1)):
    """Closed thin solid from a vertex grid P[i][j] (i across, wraps if `wrap`; j along).
    Outer and inner faces are offset along the grid normal (outer optionally displaced),
    open borders get rim faces, so every part is closed and wound outward. mat: material or fn(i,j)."""
    P=[[Vector(p) for p in col] for col in P];I,J=len(P),len(P[0])
    def at(i,j):return P[i%I if wrap else min(max(i,0),I-1)][min(max(j,0),J-1)]
    N=[[None]*J for _ in range(I)]
    for i in range(I):
        for j in range(J):
            n=(at(i+1,j)-at(i-1,j)).cross(at(i,j+1)-at(i,j-1));N[i][j]=n.normalized() if n.length>1e-14 else None
    for i in range(I):
        for j in range(J):
            if N[i][j] is None:N[i][j]=next((N[i][k] for k in (j-1,j+1,j-2,j+2) if 0<=k<J and N[i][k] is not None),Z)
    h=th/2;O=[[P[i][j]+N[i][j]*(h+(disp(i,j) if disp else 0)) for j in range(J)] for i in range(I)];In=[[P[i][j]-N[i][j]*h for j in range(J)] for i in range(I)]
    cols=I+1 if wrap else I;U=[[0.0]*J for _ in range(cols)];V=[[0.0]*J for _ in range(I)]
    for j in range(J):
        for i in range(1,cols):U[i][j]=U[i-1][j]+(P[i%I][j]-P[i-1][j]).length
    for i in range(I):
        for j in range(1,J):V[i][j]=V[i][j-1]+(P[i][j]-P[i][j-1]).length
    mf=mat if callable(mat) else (lambda i,j:mat);wf=w if callable(w) else (lambda i,j:w or (0,0))
    for i in range(cols-1):
        i2=(i+1)%I
        for j in range(J-1):
            m=mf(i,j);s=kit.SCALE[m.name];q=[(i,j),(i2,j),(i2,j+1),(i,j+1)];uv=[(U[a][b]/s,V[a%I][b]/s) for a,b in [(i,j),(i+1,j),(i+1,j+1),(i,j+1)]]
            geo.poly([O[a][b] for a,b in q],m,uv,wf(i,j),smooth=True);geo.poly([In[a][b] for a,b in reversed(q)],m,uv[::-1],wf(i,j),smooth=True)
    def rim(a,b,inner,m,ww):
        quad=[In[a[0]][a[1]],In[b[0]][b[1]],O[b[0]][b[1]],O[a[0]][a[1]]]
        if (quad[1]-quad[0]).cross(quad[3]-quad[0]).dot(P[a[0]][a[1]]-P[inner[0]][inner[1]])<0:quad.reverse()
        geo.poly(quad,m,None,ww)
    for i in range(cols-1):
        i2=(i+1)%I
        if rims[0]:rim((i,0),(i2,0),(i,1),mf(i,0),wf(i,0))
        if rims[1]:rim((i,J-1),(i2,J-1),(i,J-2),mf(i,J-2),wf(i,J-2))
    if not wrap:
        for j in range(J-1):
            if rims[2]:rim((0,j),(0,j+1),(1,j),mf(0,j),wf(0,j))
            if rims[3]:rim((I-1,j),(I-1,j+1),(I-2,j),mf(I-2,j),wf(I-2,j))
def surf(geo,F,us,vs,mat,th=.003,wrap=False,**kw):
    us=[i/us for i in range(us+(0 if wrap else 1))] if isinstance(us,int) else us
    vs=[j/vs for j in range(vs+1)] if isinstance(vs,int) else vs
    slab(geo,[[F(u,v) for v in vs] for u in us],mat,th,wrap,**kw)
def lathe(geo,o,ax,prof,mat,seg=16,th=.003,ref=None,ky=1,ang=None,**kw):
    """Surface of revolution: prof [(h along ax, r)], elliptic with ky; partial with ang=(a0,a1)."""
    o=Vector(o);ax=Vector(ax).normalized();p=Vector(ref) if ref else ax.orthogonal();p=(p-ax*p.dot(ax)).normalized();q=ax.cross(p)
    ts=[ang[0]+(ang[1]-ang[0])*i/seg for i in range(seg+1)] if ang else [tau*i/seg for i in range(seg)]
    slab(geo,[[o+ax*hh+p*math.cos(t)*r+q*math.sin(t)*r*ky for hh,r in prof] for t in ts],mat,th,not ang,**kw)
def tube(geo,a,b,r0,r1,mat,seg=8,th=.003,cap=False,**kw):
    a,b=Vector(a),Vector(b);L=(b-a).length;prof=[(0,r0),(L,r1)]
    if cap:prof+=[(L+r1*math.sin(t),max(.0012,r1*math.cos(t))) for t in (.6,1.1,1.5)]
    lathe(geo,a,b-a,prof,mat,seg,th,**kw)
def rivet(geo,p,n,mat,r=.006,hgt=.004,k=6):
    n=Vector(n).normalized();a=n.orthogonal().normalized();b=n.cross(a);p=Vector(p)-n*.0005
    ring=[p+(a*math.cos(t)+b*math.sin(t))*r for t in (tau*i/k for i in range(k))];tip=p+n*hgt
    geo.poly(list(reversed(ring)),mat)
    for i in range(k):geo.poly([ring[i],ring[(i+1)%k],tip],mat)
def xf(Mx,v):return Mx@Vector(v)
class Plane:
    """Flat emblem frame: r along, u up; faces n = r x u."""
    def __init__(s,o,r,u):s.o=Vector(o);s.r=Vector(r).normalized();s.u=Vector(u).normalized();s.n=s.r.cross(s.u)
    def p(s,a,z,d=0):return s.o+s.r*a+s.u*z+s.n*d
def flat(geo,pl,pts,mat,d,th=.003):geo.prism([pl.p(a,z,d) for a,z in pts],pl.n,th,mat)
def strip(geo,pl,a0,z0,a1,z1,w0,w1,mat,d,th=.003):
    L=math.hypot(a1-a0,z1-z0);nx,nz=-(z1-z0)/L,(a1-a0)/L
    flat(geo,pl,[(a0+nx*w0/2,z0+nz*w0/2),(a0-nx*w0/2,z0-nz*w0/2),(a1-nx*w1/2,z1-nz*w1/2),(a1+nx*w1/2,z1+nz*w1/2)],mat,d,th)
def blade(geo,pl,pts,mat,th=.008):
    """Flat blade with a raised ridge: both faces are fans to a central apex, so edges are sharp and it catches light."""
    ca=sum(a for a,_ in pts)/len(pts);cz=sum(z for _,z in pts)/len(pts);ring=[pl.p(a,z) for a,z in pts]
    if sum((a0-ca)*(z1-cz)-(z0-cz)*(a1-ca) for (a0,z0),(a1,z1) in zip(pts,pts[1:]+pts[:1]))<0:ring.reverse()
    f,b=pl.p(ca,cz,th/2),pl.p(ca,cz,-th/2)
    for i in range(len(ring)):
        r0,r1=ring[i],ring[(i+1)%len(ring)];geo.poly([r0,r1,f],mat);geo.poly([r1,r0,b],mat)
def chain(geo,top,bottom,link=.045):
    top,bottom=Vector(top),Vector(bottom);n=max(1,round((top-bottom).length/(link*.8)));ax=(bottom-top).normalized()
    for k in range(n):
        c=top+(bottom-top)*(k+.5)/n;side=Vector((1,0,0)) if k%2 else Vector((0,1,0));side=(side-ax*side.dot(ax)).normalized()*.012;h=ax*link/2
        for s in (-1,1):geo.beam(c+side*s-h,c+side*s+h,.006,.006,M['iron'],grain=False)
        for e in (-1,1):geo.beam(c-side+h*e*.9,c+side+h*e*.9,.006,.006,M['iron'],grain=False)

# ---- heraldry: frontal stag head and fleur-de-lis (gilt, flat) --------------------------
def stag(geo,pl,s,mat,d0,th=.003):
    k=[0]
    def d():k[0]+=1;return d0+th/2+k[0]*.0006
    S=lambda pts:[(a*s,z*s) for a,z in pts]
    flat(geo,pl,S([(-.1,.18),(.1,.18),(.125,.08),(.075,-.12),(.045,-.3),(0,-.34),(-.045,-.3),(-.075,-.12),(-.125,.08)]),mat,d(),th)
    for sx in (-1,1):
        flat(geo,pl,S([(sx*.09,.15),(sx*.3,.25),(sx*.27,.19),(sx*.12,.07)]),mat,d(),th)
        line=[(.05,.17),(.12,.33),(.2,.46),(.25,.62),(.24,.8)]
        for (a0,z0),(a1,z1),wi in zip(line,line[1:],(.06,.052,.044,.036)):strip(geo,pl,sx*a0*s,z0*s,sx*a1*s,z1*s,wi*s,(wi-.008)*s,mat,d(),th)
        for (a0,z0),(a1,z1) in [((.1,.29),(.0,.43)),((.16,.4),(.31,.45)),((.22,.53),(.37,.62)),((.25,.67),(.37,.79)),((.245,.73),(.17,.88))]:
            strip(geo,pl,sx*a0*s,z0*s,sx*a1*s,z1*s,.038*s,.014*s,mat,d(),th)
def fleur(geo,pl,s,mat,d0,th=.003):
    k=[0]
    def d():k[0]+=1;return d0+th/2+k[0]*.0006
    S=lambda pts:[(a*s,z*s) for a,z in pts]
    flat(geo,pl,S([(0,.36),(.035,.3),(.06,.2),(.06,.11),(.035,.03),(-.035,.03),(-.06,.11),(-.06,.2),(-.035,.3)]),mat,d(),th)
    for sx in (-1,1):
        line=[(.04,.05),(.1,.07),(.16,.12),(.2,.2),(.2,.27),(.16,.3)]
        for (a0,z0),(a1,z1),wi in zip(line,line[1:],(.065,.06,.052,.042,.03)):strip(geo,pl,sx*a0*s,z0*s,sx*a1*s,z1*s,wi*s,(wi-.008)*s,mat,d(),th)
        line=[(.03,-.02),(.09,-.06),(.12,-.12)]
        for (a0,z0),(a1,z1),wi in zip(line,line[1:],(.045,.032)):strip(geo,pl,sx*a0*s,z0*s,sx*a1*s,z1*s,wi*s,(wi-.012)*s,mat,d(),th)
    flat(geo,pl,S([(-.1,-.02),(.1,-.02),(.1,.035),(-.1,.035)]),mat,d(),th)
    flat(geo,pl,S([(-.035,-.02),(.035,-.02),(0,-.14)]),mat,d(),th)

# ---- armour -------------------------------------------------------------------------
def MAIL(i,j):return .005*(j%2)+.0015*((i+j)%2)   # ring rows: alternate courses stand proud
def MAILM(i,j):return M['mail'] if (i+j)%2 else M['maildark']   # ring and gap alternate, so mail reads without a normal map
RX=[(0,.15),(.12,.17),(.26,.195),(.36,.205),(.42,.2),(.46,.17),(.5,.11),(.53,.075)]
RYF=[(0,.11),(.12,.14),(.26,.16),(.36,.15),(.42,.13),(.46,.11),(.5,.08),(.53,.065)]
RYB=[(0,.095),(.12,.105),(.26,.12),(.36,.12),(.42,.11),(.46,.095),(.5,.072),(.53,.065)]
def torso_fn(Mx,WZ,scale=1.0):
    def T(th,z,add=0):
        rx=tab(RX,z)*scale+add;ryf=tab(RYF,z)*scale+add;ryb=tab(RYB,z)*scale+add;c=math.cos(th);ry=(ryf+ryb)/2+(ryf-ryb)/2*c
        ridge=.02*scale*math.exp(-((th+math.pi)%tau-math.pi)**2/.02)*min(1,max(0,(.46-z)/.1))
        return Mx@Vector((math.sin(th)*rx,-c*ry-ridge,WZ+z))
    return T
def outward(T,th,z):
    e=1e-3;return (T(th+e,z)-T(th-e,z)).cross(T(th,z+e)-T(th,z-e)).normalized()
def pauldron(Mx,side,mat,J=(.19,0,.4),arm=None):
    J=Vector(J);J.x*=side;R3=Mx.to_3x3()
    c=Vector((side,0,.85)).normalized();a=Vector((0,1,0));b=c.cross(a)
    surf(A,lambda u,v:Mx@(J+(c*math.cos(.03+v*1.35)+(a*math.cos(u*tau)+b*math.sin(u*tau))*math.sin(.03+v*1.35))*.125),24,8,mat,.004,wrap=True)
    aa=Vector((side*.28,0,-1)).normalized();o=Vector((side,0,0));o=(o-aa*o.dot(aa)).normalized();bb=aa.cross(o)
    for k in range(3):
        t0,r=.035+.05*k,.133-.006*k
        surf(A,lambda u,v,t0=t0,r=r:Mx@(J+aa*(t0+.065*v)+(o*math.cos(-2.2+4.4*u)+bb*math.sin(-2.2+4.4*u))*(r+.008*v)),14,2,mat,.004)
        if k==2:surf(D,lambda u,v,t0=t0,r=r:Mx@(J+aa*(t0+.052+.013*v)+(o*math.cos(-2.2+4.4*u)+bb*math.sin(-2.2+4.4*u))*(r+.0145)),14,1,M['brass'],.003)
        for ph in (-1.6,0,1.6):p=J+aa*(t0+.05)+(o*math.cos(ph)+bb*math.sin(ph))*(r+.008);rivet(D,Mx@p,R3@(o*math.cos(ph)+bb*math.sin(ph)),M['brass'],.006,.004)
    if arm:   # upper arm below the pauldron: plate rerebrace or mail sleeve
        p0=J+aa*.06;fa=Vector((side*.05,-.25,-1)).normalized();el=p0+aa*.27
        if arm=='mail':
            prof=[(0,.058),(.27,.056),(.3,.054)];lathe(A,Mx@p0,R3@aa,prof,MAILM,30,.004,ref=R3@o,disp=MAIL)
            lathe(A,Mx@el,R3@fa,[(0,.054),(.24,.046),(.26,.04),(.28,.0015)],MAILM,26,.004,ref=R3@o,disp=MAIL)
        else:
            lathe(A,Mx@p0,R3@aa,[(0,.055),(.22,.05),(.3,.052)],mat,16,.004,ref=R3@o)
            lathe(A,Mx@el,R3@fa,[(0,.05),(.22,.042),(.25,.044),(.27,.0015)],mat,16,.004,ref=R3@o)    # vambrace
            lathe(A,Mx@(el-aa*.01),R3@(aa+fa).normalized(),[(-.02,.058),(.01,.064),(.04,.05),(.06,.03),(.07,.0015)],mat,14,.004,ref=R3@Vector((0,-1,0)))   # couter
            rivet(D,Mx@(el+Vector((0,-.066,0))),R3@Vector((0,-1,0)),M['brass'],.008,.005)
def gorget(Mx,WZ,mat):
    surf(A,lambda u,v:Mx@Vector((math.sin(u*tau)*(.088+.006*v),-math.cos(u*tau)*(.078+.006*v),WZ+.47+.06*v)),24,2,mat,.004,wrap=True)
    for k in range(6):t=tau*k/6;rivet(D,Mx@Vector((math.sin(t)*.095,-math.cos(t)*.085,WZ+.5)),Mx.to_3x3()@Vector((math.sin(t),-math.cos(t),0)),M['brass'],.005,.003)
def cuirass(x,y,yaw=0,st=None,skirt='cloth',strap=False,arms='plate',helm='great',tassets=3):
    st=st or M['steel'];Mx=Matrix.Translation((x,y,CT))@Matrix.Rotation(yaw,4,'Z');WZ=.42;T=torso_fn(Mx,WZ)
    T_.box(xf(Mx,(0,0,.02)),(.3,.22,.04),M['timber'],.008,Mx.to_3x3(),grain=True,band=band())
    T_.box(xf(Mx,(0,0,(.04+WZ+.74)/2)),(.05,.05,WZ+.7),M['timber'],.006,Mx.to_3x3(),grain=True,band=band())
    zs=[0,.04,.08,.12,.16,.2,.24,.28,.32,.36,.39,.42,.44,.46,.48,.5,.515,.53]
    surf(A,lambda u,v:T(u*tau,v),36,zs,st,.004,wrap=True)
    for k in range(3):   # fauld lames
        surf(A,lambda u,v,k=k:T(u*tau,-k*.055-v*.07,.006+.011*k+.022*v),36,3,st,.004,wrap=True)
        for th in (-1.25,-.65,.65,1.25,math.pi-.6,math.pi+.6):rivet(D,T(th,-k*.055-.02,.006+.011*k+.01),outward(T,th,-k*.055-.02),M['brass'],.0055,.0035)
    gorget(Mx,WZ,st)
    for s in (-1,1):pauldron(Mx@Matrix.Translation((0,0,WZ)),s,st,arm=arms)
    for side in (-1,1):   # tassets
        lo,hi=(-1.0,-.14) if side<0 else (.14,1.0)
        for k in range(tassets):
            top=-.15-.075*k
            surf(A,lambda u,v,top=top,k=k,lo=lo,hi=hi:T(lo+(hi-lo)*u,top-.09*v,.045+.008*k+.016*v),10,3,st,.004)
            for th in (lo+.08,hi-.08):rivet(D,T(th,top-.02,.045+.008*k+.008),outward(T,th,top-.02),M['brass'],.006,.004)
    if skirt=='cloth':
        cols=[]
        for i in range(27):
            th=-1.75+3.5*i/26;L=.34+R.uniform(0,.06)-(.07 if i%4==2 else 0)
            cols.append([T(th,-.12-L*j/6,.02+.03*j/6) for j in range(7)])
        slab(C,cols,M['banner'],.004)
    elif skirt=='mail':
        surf(A,lambda u,v:T(u*tau,-.12-.27*v,.022+.05*v),84,22,MAILM,.004,wrap=True,disp=MAIL)
    # leather belt with a brass buckle
    surf(D,lambda u,v:T(u*tau,-.015+.045*v,.02),36,1,M['leather'],.005,wrap=True)
    p=T(0,.007,.026);n=outward(T,0,.007);D.box(p,(.05,.008,.055),M['brass'],.003,Mx.to_3x3())
    if strap:   # diagonal baldric across the breastplate
        surf(D,lambda u,v:T(-1.25+2.2*u,.44-.4*u+(v-.5)*.045,.012+.004*math.sin(math.pi*u)),20,1,M['leather'],.005)
    surf(D,lambda u,v:T(u*tau,-.166-.014*v,.054),36,1,M['brass'],.003,wrap=True)   # gilt edge on the last lame
    surf(D,lambda u,v:T(u*tau,.5+.014*v,.006),36,1,M['brass'],.003,wrap=True)        # gilt neckline
    for th in [tau*k/14 for k in range(14)]:rivet(D,T(th,.44,.004),outward(T,th,.44),M['brass'],.005,.003)
    # helmet on the stand's head block
    helmet(helm,Mx@Matrix.Translation((0,0,WZ+.5)),st)
def helmet(kind,Mx,st=None,trim=None):
    st=st or M['steel'];trim=trim or M['brass'];dark=M['iron'];R3=Mx.to_3x3()
    keel=lambda th,amt,width=.015:1+amt*math.exp(-((th+math.pi)%tau-math.pi)**2/width)
    if kind in ('great','great2'):
        def H(th,v,e=0):
            if v<=.8:z=v/.8*.29;r=.106*(1-.05*z/.29);k=keel(th,.07)
            else:s=(v-.8)/.2;z=.29+.045*math.sin(s*math.pi/2);r=max(.0015,.1*math.cos(s*math.pi/2));k=1+(keel(th,.07)-1)*max(0,1-s*3)
            return Vector((math.sin(th)*(r*k+e),-math.cos(th)*(r*1.1*k+e),z))
        vz=lambda z:z/.29*.8
        vs=[.8*j/9 for j in range(10)]+[.8+.2*j/6 for j in range(1,7)]
        surf(A,lambda u,v:Mx@H(u*tau,v),28,vs,st,.004,wrap=True)
        for t0,t1 in ((-1.15,-.1),(.1,1.15)):surf(D,lambda u,v,t0=t0,t1=t1:Mx@H(t0+(t1-t0)*u,vz(.19+.016*v),.003),8,1,dark,.002)
        surf(D,lambda u,v:Mx@H(-1.6+3.2*u,vz(.207+.03*v),.006),20,1,trim if kind=='great' else st,.003)    # brow band
        surf(D,lambda u,v:Mx@H(-.06+.12*u,vz(.03+.16*v),.006),1,6,trim if kind=='great' else st,.003)   # nasal strip
        for th in (.42,.58,.74,.9):
            for z in (.07,.1,.13):p=H(th,vz(z),.003);rivet(D,Mx@p,R3@Vector((math.sin(th),-math.cos(th),0)),dark,.0045,.001)
        for th in (-1.4,-.95,-.5,.5,.95,1.4):p=H(th,vz(.222),.009);rivet(D,Mx@p,R3@Vector((math.sin(th),-math.cos(th),0)),trim,.005,.003)
    elif kind=='bascinet':
        def H(th,v,e=0):
            z=.36*v;s=v;r=max(.0015,.102*(1-s**2.2)**.55);k=keel(th,.05,.01)
            return Vector((math.sin(th)*(r*k+e),-math.cos(th)*(r*1.12*k+e)+.03*s*s,z))
        vs=[j/12 for j in range(12)]+[.97,.995]
        surf(A,lambda u,v:Mx@H(u*tau,v),28,vs,st,.004,wrap=True)
        snout=lambda th,v,e:H(th,v,e+.014+.055*max(0,math.cos(th))**3*math.sin(math.pi*(v-.06)/.52))
        surf(A,lambda u,v:Mx@snout(-1.5+3*u,.06+.52*v,0),20,8,st,.004)        # pig-face visor
        for t0,t1 in ((-.95,-.14),(.14,.95)):surf(D,lambda u,v,t0=t0,t1=t1:Mx@snout(t0+(t1-t0)*u,.43+.035*v,.003),8,1,dark,.002)
        for th in (-.3,-.15,0,.15,.3):
            for v in (.18,.25):p=snout(th,v,.003);rivet(D,Mx@p,R3@Vector((math.sin(th),-math.cos(th),0)),dark,.004,.001)
        for s_ in (-1,1):p=snout(s_*1.48,.45,.004);rivet(D,Mx@p,R3@Vector((s_,0,0)),trim,.012,.006,8)
    elif kind=='sallet':
        def H(th,v,e=0):
            back=max(0,-math.cos(th))**2;fl=(1-v)**3*back;ph=v*math.pi/2*.985
            r=max(.0015,.105*math.cos(ph))+.06*fl;z=.29*math.sin(ph)-.075*fl;k=1+.06*math.exp(-math.sin(th)**2/.006)*min(1,v*3)
            return Vector((math.sin(th)*(r*k+e),-math.cos(th)*(r*1.1*k+e),z))
        vs=[j/14 for j in range(15)]
        surf(A,lambda u,v:Mx@H(u*tau,v),32,vs,st,.004,wrap=True)
        surf(D,lambda u,v:Mx@H(-1.1+2.2*u,.255+.03*v,.003),12,1,dark,.002)
        surf(A,lambda u,v:Mx@H(-1.25+2.5*u,.29+.045*v,.008),14,1,st,.004)
        for th in [tau*k/12 for k in range(12)]:p=H(th,.1,.002);rivet(D,Mx@p,R3@Vector((math.sin(th),-math.cos(th),0)),trim,.0055,.003)
    elif kind=='barbute':
        def H(th,v,e=0):
            if v<.55:z=v/.55*.17;r=.103+.008*(1-v/.55)
            else:s=(v-.55)/.45;z=.17+.15*math.sin(s*math.pi/2);r=max(.0015,.103*math.cos(s*math.pi/2))
            k=keel(th,.05,.01)
            return Vector((math.sin(th)*(r*k+e),-math.cos(th)*(r*1.1*k+e),z))
        vz=lambda z:z/.17*.55
        vs=[.55*j/6 for j in range(7)]+[.55+.45*j/8 for j in range(1,9)]
        surf(A,lambda u,v:Mx@H(u*tau,v),28,vs,st,.004,wrap=True)
        surf(D,lambda u,v:Mx@H(-.85+1.7*u,vz(.15)+(vz(.165)-vz(.15))*v,.003),12,1,dark,.002)
        surf(D,lambda u,v:Mx@H(-.07+.14*u,vz(.03)+(vz(.15)-vz(.03))*v,.0031),1,4,dark,.002)
        for th in [tau*k/10 for k in range(10)]:p=H(th,.08,.002);rivet(D,Mx@p,R3@Vector((math.sin(th),-math.cos(th),0)),trim,.005,.003)
def place_helm(kind,x,y,z,yaw=0,st=None,trim=None):helmet(kind,Matrix.Translation((x,y,z))@Matrix.Rotation(yaw,4,'Z'),st,trim)
def gauntlet(Mx,st):
    R3=Mx.to_3x3()
    surf(A,lambda u,v:Mx@Vector((math.sin(u*tau)*(.07-.022*v**.7),-math.cos(u*tau)*(.058-.02*v**.7),.13*v)),20,4,st,.004,wrap=True)   # flared cuff
    HX_=[(.11,.046),(.15,.054),(.19,.055),(.205,.052),(.212,.035),(.215,.0015)];HY_=[(.11,.03),(.15,.032),(.19,.03),(.205,.027),(.212,.02),(.215,.0015)]
    surf(A,lambda u,v:Mx@Vector((math.sin(u*tau)*tab(HX_,.11+.105*v),-math.cos(u*tau)*tab(HY_,.11+.105*v),.11+.105*v)),18,8,st,.004,wrap=True)
    for k in range(4):surf(A,lambda u,v,k=k:Mx@Vector((-.056+.112*u,-.032-.003*k-.006*math.sin(math.pi*u),.125+.022*k+.026*v)),8,1,st,.003)   # metacarpal lames
    for f,xf_ in enumerate((-.0345,-.0115,.0115,.0345)):
        L=(.03,.034,.033,.028)[f]
        for s in range(3):
            a=Vector((xf_,-.008-.006*s,.2+L*s));b=a+Vector((0,-.006,L-.002))
            tube(A,Mx@a,Mx@b,.0122-.001*s,.0118-.001*s,st,10,.002,cap=(s==2),ky=.75,ref=R3@Vector((1,0,0)))
    tube(A,Mx@Vector((.045,-.005,.12)),Mx@Vector((.07,-.02,.17)),.012,.011,st,8,.002)
    tube(A,Mx@Vector((.07,-.022,.172)),Mx@Vector((.08,-.03,.21)),.011,.01,st,8,.002,cap=True)
    for th in (-1,0,1):rivet(D,Mx@Vector((math.sin(th)*.06,-math.cos(th)*.05,.02)),R3@Vector((math.sin(th),-math.cos(th),0)),M['brass'],.005,.003)
def greave(o,ax,st,ref=None):
    lathe(A,o,ax,[(0,.046),(.05,.05),(.15,.062),(.25,.062),(.36,.054),(.42,.056)],st,16,.004,ref=ref,ky=.85)
def vambrace(o,ax,st):
    o=Vector(o);ax=Vector(ax).normalized()
    lathe(A,o,ax,[(0,.042),(.18,.048),(.22,.05)],st,16,.004)
    lathe(A,o+ax*.2,ax,[(0,.055),(.03,.06),(.06,.045),(.075,.025),(.082,.0015)],st,14,.004)
    for k in (.04,.12):rivet(D,o+ax*k+Vector((0,0,.045)),Z,M['brass'],.005,.003)
def hauberk(x,y,z0):
    Mx=Matrix.Translation((x,y,z0));R3=Mx.to_3x3();SH=1.0
    T_.box((x,y,z0+.02),(.32,.24,.04),M['timber'],.008,grain=True,band=band())
    T_.box((x,y,z0+(1.18)/2),(.05,.05,1.14),M['timber'],.006,grain=True,band=band())
    T_.beam((x-.2,y,z0+SH-.02),(x+.2,y,z0+SH-.02),.05,.05,M['timber'],band=band())
    lathe(D,(x,y,z0+1.16),Z,[(0,.03),(.02,.034),(.04,.0015)],M['timber'],10,.006)
    HX=[(-.75,.26),(-.45,.22),(-.12,.195),(.08,.215),(.22,.235),(.29,.22),(.34,.15),(.38,.075)]
    HY=[(-.75,.17),(-.45,.14),(-.12,.125),(.08,.14),(.22,.13),(.29,.115),(.34,.085),(.38,.062)]
    def F(u,v):
        th=u*tau;z=-.75+1.13*v;return Mx@Vector((math.sin(th)*tab(HX,z),-math.cos(th)*tab(HY,z),SH-.38+z))
    surf(A,F,96,[j/76 for j in range(77)],MAILM,.004,wrap=True,disp=MAIL)
    for s in (-1,1):
        J=Vector((s*.21,0,SH-.1));aa=Vector((s*.62,0,-1)).normalized()
        lathe(A,Mx@(J+aa*.03),aa,[(0,.08),(.2,.088),(.34,.1)],MAILM,40,.004,ref=Vector((s,0,0)),disp=MAIL)
        pauldron(Mx@Matrix.Translation((0,0,SH-.43-.07)),s,M['worn'],J=(.2,0,.43))
    surf(D,lambda u,v:Mx@Vector((math.sin(u*tau)*.214,-math.cos(u*tau)*.138,SH-.38-.02+.05*v)),40,1,M['leather'],.006,wrap=True)
    D.box(xf(Mx,(0,-.146,SH-.375)),(.055,.01,.06),M['brass'],.003)

# ---- posts, rails, finials ----------------------------------------------------------
def post(x,y,top):T_.box((x,y,top/2),(PS,PS,top),M['timber'],.016,grain=True,band=band())
def rail(a,b,s=RAIL):T_.beam(a,b,s,s,M['timber'],band=band())
def iron_band(x,y,z,h=.1,rivets=True):
    D.box((x,y,z),(PS+.02,PS+.02,h),M['iron'],.004)
    if rivets:
        for dx in (-.055,.055):rivet(D,(x+dx,y-PS/2-.01,z),(0,-1,0),M['iron'],.012,.006,4)
def rope(x,y,z,turns=4,r=.135):
    for k in range(turns):D.vessel((x,y,z+(k-turns/2)*.03),[(r,0),(r+.012,.015),(r,.03)],M['rope'],14)
def spire(geo,x,y,rings,tip,mat):
    sq=lambda z,h:[(x-h,y-h,z),(x+h,y-h,z),(x+h,y+h,z),(x-h,y+h,z)]
    geo.poly(list(reversed(sq(*rings[0]))),mat)
    for lo,hi in zip(rings,rings[1:]):
        a,b=sq(*lo),sq(*hi)
        for i in range(4):geo.poly([a[i],a[(i+1)%4],b[(i+1)%4],b[i]],mat)
    top=sq(*rings[-1])
    for i in range(4):geo.poly([top[i],top[(i+1)%4],(x,y,tip)],mat)
for s in (-1,1):
    post(s*PX,YF,FT);post(s*PX,YR,ZT+.06)
    T_.box((s*PX,YF,FT+.04),(.25,.25,.08),M['timber'],.012,grain=True,band=band())
    spire(T_,s*PX,YF,[(FT+.08,.05),(FT+.12,.05),(FT+.17,.085),(FT+.2,.085),(FT+.23,.06)],4.35,M['timber'])
    T_.box((s*PX,YR,ZT+.08),(.23,.23,.04),M['timber'],.01,grain=True,band=band())
    rail((s*PX,YF-.1,ZT),(s*PX,YR+.1,ZT));rail((s*PX,YF,CT-.12),(s*PX,YR,CT-.12),.08)
    for y in (YF,YR):iron_band(s*PX,y,.08,.14);iron_band(s*PX,y,ZT-.13,.08);rope(s*PX,y,ZT)
    for z in (FT-.12,2.35,CT-.2):iron_band(s*PX,YF,z,.09)
    rope(s*PX,YF,ZF)
rail((-PX-.1,YR,ZT),(PX+.1,YR,ZT));rail((-PX,YF,ZF),(PX,YF,ZF))
for z in (.35,1.7,2.95):rail((-PX,YR+.045,z),(PX,YR+.045,z),.08)
# ---- awning: nine blue/cream stripes over the rails, ragged valance -------------------
TH=.006;Rr=RAIL/2+TH
path=[(YR+Rr,ZT-.08),(YR+Rr*.7,ZT+Rr*.7),(YR,ZT+Rr)]
for k in range(1,15):t=k/15;path.append((YR+(YF-YR)*t,ZT+Rr+(ZF-ZT)*t-.12*math.sin(math.pi*t)))
path+=[(YF,ZF+Rr),(YF-Rr*.7,ZF+Rr*.7),(YF-Rr,ZF)]
VAL=len(path)-1
STRIPES=['blue','cream']*4+['blue'];NS=20;SW=2*CX/len(STRIPES)
def tatters(x0,x1,base,var,seed,wmin=.06,wmax=.15):
    r=random.Random(seed);segs=[];x=x0
    while x<x1+.2:w=r.uniform(wmin,wmax);segs.append((x,x+w,base+r.uniform(-var,var),r.uniform(.06,.2)));x+=w
    def L(x):
        for a,b,l,notch in segs:
            if a<=x<=b:t=(x-a)/(b-a);return l-notch*max(0,1-min(t,1-t)/.22)+.025*(1-abs(2*t-1))
        return base
    return L
hem=tatters(-CX,CX,.43,.1,11,.05,.2)
xs=[-CX+2*CX*i/(NS*len(STRIPES)) for i in range(NS*len(STRIPES)+1)];rows=[]
for i,x in enumerate(xs):
    local=(i%NS)/NS;ring=[];L=hem(x)
    for j,(y,z) in enumerate(path):
        t=min(1,max(0,(j-3)/14));ring.append(Vector((x,y,z-.04*math.sin(math.pi*local)*math.sin(math.pi*t))))
    for k in range(1,9):
        f=k/8;fold=.02*math.sin(x*23+.7)*f;ring.append(Vector((x,YF-Rr-.004*k-fold,ZF-L*f)))
    rows.append(ring)
def grime(i,j):x=xs[i];return (.1+.12*math.sin(x*2.3+j*.4)**2+(.14 if j>VAL+3 else 0),0)
slab(C,rows,lambda i,j:M[STRIPES[min(len(STRIPES)-1,int((xs[i]+xs[i+1])/2/SW+len(STRIPES)/2))]],TH,w=grime)
def edge_z(y):t=(YR-y)/(YR-YF);return ZT+Rr+(ZF-ZT)*t-.12*math.sin(math.pi*t)
for sx in (-1,1):   # short side drops under the sloping edges, ragged
    ys=[YR-.06+(YF-YR+.12)*k/24 for k in range(25)];sh=tatters(0,2,.26,.05,20+sx,.05,.1)
    slab(C,[[Vector((sx*(CX-.004)-sx*.01*d*math.sin(y*19),y,edge_z(y)-.006-d*sh(y+1))) for d in (0,.35,.7,1)] for y in ys],M['blue'],TH)
# ---- counter: plank top on a braced frame -------------------------------------------
CW=PX-PS/2+.02
for k in range(5):w=(CY1-CY0)/5;T_.box((0,CY0+w*(k+.5),CT-.025),(2*CW,w-.006,.05),M['timber'],.008,grain=True,band=band())
for x in (-CW+.05,-.95,0,.95,CW-.05):T_.box((x,(CY0+CY1)/2,CT-.085),(.08,CY1-CY0-.04,.07),M['timber'],.008,grain=True,band=band())
T_.box((0,CY0+.03,CT-.14),(2*CW,.06,.18),M['timber'],.008,grain=True,band=band())       # front apron
T_.box((0,CY1-.03,CT-.14),(2*CW,.06,.18),M['timber'],.008,grain=True,band=band())
for x in (-.95,.95):
    T_.box((x,CY0+.06,(CT-.05)/2),(.12,.12,CT-.05),M['timber'],.01,grain=True,band=band())
    for z in (CT-.16,.12):
        D.box((x,CY0-.002,z),(.16,.012,.1),M['iron'],.003)
        for dx in (-.05,.05):rivet(D,(x+dx,CY0-.008,z),(0,-1,0),M['iron'],.01,.005,4)
for x in (-CW+.05,-.95,.95,CW-.05):T_.box((x,CY1-.05,(CT-.05)/2),(.09,.09,CT-.05),M['timber'],.01,grain=True,band=band())
for y in (CY0+.07,CY1-.05):rail((-CW,y,.1),(CW,y,.1),.07)
for a,b in ((-.89,.89),(.89,-.89)):rail((a,CY0+.07,.18),(b,CY0+.07,CT-.25),.07)       # X brace behind the drape
for s in (-1,1):D.box((s*(CW-.04),CY0-.002,CT-.14),(.12,.012,.14),M['iron'],.003)
# left under-counter shelf (gauntlets) and back wall boards
T_.box((-1.41,-.22,.535),(.88,.5,.035),M['timber'],.006,grain=True,band=band())
T_.box((-1.41,-.45,.46),(.88,.05,.11),M['timber'],.006,grain=True,band=band())
for x in (-1.83,-.99):T_.box((x,-.22,.26),(.05,.45,.52),M['timber'],.006,grain=True,band=band())
for k in range(18):
    x=-1.85+3.7*(k+.5)/18;T_.box((x,YR-.066+.012*(k%2),1.775),(3.7/18+.012,.03,3.55),M['timber'],.006,grain=True,band=band(),w=(.35,0))
# top shelf on brackets along the back wall, stepped riser on the counter
T_.box((-.4,.76,2.03),(2.9,.42,.035),M['timber'],.006,grain=True,band=band())
T_.box((-.4,.56,2.06),(2.9,.025,.03),M['timber'],.004,grain=True,band=band())
for x in (-1.6,-.4,.8):T_.prism([(x,YR-.075,1.7),(x,YR-.075,2.012),(x,.6,2.012)],(1,0,0),.05,M['timber'])
for z in (1.27,1.41,1.55):T_.box((.65,-.05,z),(.9,.03,.135),M['timber'],.006,grain=True,band=band())
T_.box((.65,.15,1.6),(.9,.43,.04),M['timber'],.006,grain=True,band=band())
for x in (.22,1.08):T_.box((x,.15,1.4),(.04,.4,.4),M['timber'],.006,grain=True,band=band())
# ---- drape over the counter front: blue stag pennant with gilt border and fringe -------
DX0,DX1=-.62,.68;DC=(DX0+DX1)/2;DY=CY0-.014
def dhem(x):t=(x-DX0)/(DX1-DX0);return .56-.38*(1-abs(2*t-1))
cols=[]
for i in range(45):
    x=DX0+(DX1-DX0)*i/44;hz=dhem(x)
    cols.append([Vector((x,y,CT+.004)) for y in (-.45,-.56,CY0-.004)]+[Vector((x,CY0-.011,CT-.018))]+[Vector((x,DY,CT-.03-(CT-.03-hz)*k/12)) for k in range(1,13)])
slab(C,cols,M['banner'],.006)
pl=Plane((DC,DY-.003,0),(1,0,0),(0,0,1))
stag(D,Plane((DC,DY-.003,.7),(1,0,0),(0,0,1)),.52,M['gilt'],0)
fleur(D,Plane((DC,DY-.003,.34),(1,0,0),(0,0,1)),.48,M['gilt'],0)
strip(D,pl,DX0+.03,CT-.07,DX1-.03,CT-.07,.025,.025,M['gilt'],.0025)
for x in (DX0+.03,DX1-.03):strip(D,pl,x,CT-.06,x,dhem(x)+.03,.025,.025,M['gilt'],.0025)
for x0,x1 in ((DX0+.03,DC),(DC,DX1-.03)):strip(D,pl,x0,dhem(x0)+.03,x1,dhem(x1)+.03,.025,.025,M['gilt'],.0025)
for k in range(66):   # gilt fringe along the hem
    x=DX0+.01+(DX1-DX0-.02)*k/65;z=dhem(x);strip(D,pl,x,z+.012,x+.002*math.sin(k),z-.045,.007,.005,M['gilt'],.001+.0003*(k%2),.002)
# ---- fur pelts over the counter edge -------------------------------------------------
def pelt(x0,x1,seed,legs=(.18,.82)):
    r=random.Random(seed);n=int((x1-x0)/.012)+1;P=[]
    for i in range(n):
        t=i/(n-1);x=x0+(x1-x0)*t;L=.22+sum(.13*math.exp(-((t-c)/.05)**2) for c in legs)+r.uniform(-.05,.05)
        top=[Vector((x+.015*math.sin(9*t+k),y,CT+.0075)) for k,y in enumerate((-.4,-.5,-.58,CY0-.002))]
        P.append(top+[Vector((x,CY0-.013,CT-.022))]+[Vector((x+.01*math.sin(z*25+i*.7),CY0-.016-.012*k/7,CT-.022-L*k/7)) for k,z in ((k,CT-L*k/7) for k in range(1,8))])
    slab(C,P,M['fur'],.014,disp=lambda i,j:r.uniform(0,.018),w=(.3,0))
pelt(-1.12,-.66,3);pelt(.72,1.46,4,(.15,.5,.86))
# ---- armour on the counter ---------------------------------------------------------
cuirass(-1.42,-.2,.06,skirt='cloth',arms='mail',helm='great')
cuirass(-.78,-.1,0,skirt='mail',arms='plate',helm='bascinet',tassets=2)
cuirass(-.14,-.2,-.06,skirt='cloth',strap=True,arms='plate',helm='great2')
for kind,x,y,z,yaw,st in [('barbute',.45,-.32,CT,.1,M['steel']),('sallet',.82,-.3,CT,-.15,M['worn']),
                          ('great',.34,.13,1.62,0,M['steel']),('bascinet',.62,.15,1.62,.1,M['steel']),('barbute',.9,.13,1.62,-.1,M['worn']),
                          ('sallet',.1,.74,2.05,.1,M['steel']),('great2',.4,.76,2.05,0,M['worn']),('bascinet',.72,.74,2.05,-.1,M['steel']),
                          ('barbute',-1.1,.74,2.05,.05,M['steel']),('sallet',-.46,.76,2.05,-.1,M['worn'])]:
    place_helm(kind,x,y,z,yaw,st)
hauberk(1.42,.2,CT)
gauntlet(Matrix.Translation((-1.6,-.26,.5525))@Matrix.Rotation(-.15,4,'Z'),M['steel'])
gauntlet(Matrix.Translation((-1.3,-.24,.5525))@Matrix.Rotation(.12,4,'Z'),M['steel'])
gauntlet(Matrix.Translation((1.56,-.3,CT))@Matrix.Rotation(-.1,4,'Z'),M['worn'])
vambrace((1.0,-.36,CT+.052),(1,.25,0),M['steel']);vambrace((1.1,-.22,CT+.052),(1,-.15,0),M['worn'])
# belts hanging from a peg bar on the right post, coiled belt on the counter
D.beam((PX-PS/2+.02,-.42,2.05),(1.55,-.42,2.05),.035,.035,M['timber'],grain=True)
for k,x in enumerate((1.6,1.66,1.72,1.78)):
    L=.3+.08*((k*7)%3);P=[]
    for xx in (x-.018,x+.018):P.append([Vector((xx,-.42+.026*math.sin(a),2.05+.026*math.cos(a))) for a in (-2.4,-1.6,-.8,0,.8,1.6)]+[Vector((xx,-.448-.004*j,2.05-L*j/6)) for j in range(1,7)])
    slab(D,P,M['leather'],.005,rims=(1,1,1,1))
    D.box((x,-.456,2.05-L+.04),(.045,.006,.035),M['brass'],.002)
r0,r1=.035,.075
P=[[Vector((1.28+(r0+(r1-r0)*t)*math.cos(t*4*math.pi),-.42+(r0+(r1-r0)*t)*math.sin(t*4*math.pi),CT+zz)) for t in [k/60 for k in range(61)]] for zz in (.002,.034)]
slab(D,P,M['leather'],.005)
# ---- under the counter: crate of helmets, metal bucket of greaves, swords ----------------
PROPS=[]
def prop(mid,loc,rz=0,s=1,rx=0,ry=0,aged=0,base=None):
    Mx=Matrix.Translation(loc)@Matrix.Rotation(rz,4,'Z')@Matrix.Rotation(ry,4,'Y')@Matrix.Rotation(rx,4,'X')@(Matrix.Diagonal((*s,1)) if isinstance(s,tuple) else Matrix.Scale(s,4))
    objs=kit.model(asset,mid,Mx,aged)
    if base is not None:
        zmin=min((o.matrix_world@v.co).z for o in objs for v in o.data.vertices)
        for o in objs:o.matrix_world=Matrix.Translation((0,0,base-zmin))@o.matrix_world
    PROPS.extend(objs);return objs
prop('wooden_crate_01',(-1.42,-.68,0),0,1,aged=.2,base=0)
place_helm('sallet',-1.62,-.7,.35,.2,M['worn']);place_helm('great',-1.22,-.68,.35,-.1,M['steel'])
bx,by=-.82,-.86
lathe(A,(bx,by,0),Z,[(0,.14),(.32,.165)],M['worn'],24,.006)
lathe(A,(bx,by,.004),Z,[(0,.0015),(0,.138)],M['worn'],24,.006)
for zz in (.04,.26):lathe(D,(bx,by,zz),Z,[(0,.14+.025*zz/.32+.006),(.035,.14+.025*(zz+.035)/.32+.006)],M['iron'],24,.004)
tube(D,(bx-.17,by,.28),(bx-.12,by,.42),.006,.006,M['iron'],6);tube(D,(bx-.12,by,.42),(bx+.12,by,.42),.006,.006,M['iron'],6);tube(D,(bx+.12,by,.42),(bx+.17,by,.28),.006,.006,M['iron'],6)
for dx,dy,tx,ty in ((-.06,-.04,-.12,-.06),(.06,-.03,.1,-.08),(0,.06,.02,.12),(-.05,.05,-.1,.1)):
    greave((bx+dx,by+dy,.012),(tx,ty,1),M['steel'],ref=(0,-1,0))
prop('wooden_bucket_01',(-.78,-.38,0),.4,1,aged=.2,base=0)
prop('wooden_handle_saber',(-.84,-.38,.12),0,1,rx=.12,ry=-.1)
prop('ornate_medieval_dagger',(-.72,-.36,.25),.5,1,rx=-.1,ry=.15)
prop('wooden_crate_02',(.84,-.78,0),math.pi/2,.95,aged=.2,base=0)
for dy,z,k in ((-.9,.105,0),(-.78,.105,1),(-.66,.105,2),(-.84,.226,3),(-.72,.226,4)):lathe(A,(.6+.02*k,dy,z),(1,.04*(k%2*2-1),0),[(0,.046),(.05,.05),(.15,.062),(.25,.062),(.36,.054),(.42,.056)],M['worn'] if k%2 else M['steel'],16,.004,ref=(0,0,1),ky=.85)
place_helm('bascinet',.72,-.8,.29,.2,M['worn']);place_helm('barbute',1.0,-.74,.29,-.2,M['steel'])
prop('wooden_crate_01',(1.42,-.38,0),math.pi/2,1,aged=.25,base=0)
D.box((1.42,-.42,.4),(.32,.22,.1),M['leather'],.02)
for dy,ang in ((-.48,.1),(-.3,-.12)):lathe(A,(1.24,dy,.508),(1,ang,0),[(0,.046),(.05,.05),(.15,.062),(.25,.062),(.36,.054),(.42,.056)],M['steel'],16,.004,ref=(0,0,1),ky=.85)
prop('ornate_medieval_dagger',(1.05,-.18,CT),1.4,1,rx=math.pi/2,base=CT)
# ---- shields ---------------------------------------------------------------------------
def heater(o,lean,yaw):
    Mx=Matrix.Translation(o)@Matrix.Rotation(yaw,4,'Z')@Matrix.Rotation(-lean,4,'X');R3=Mx.to_3x3()
    out=[(-.31,1.0),(.31,1.0)]+[(.31*math.sin(math.pi/2*(z/.55))**.75,z) for z in [.55*(1-k/10) for k in range(11)]][:-1]+[(0,0)]+[(-.31*math.sin(math.pi/2*(z/.55))**.75,z) for z in [.55*k/10 for k in range(1,11)]]
    pts=[(a,z) for a,z in out];pl=Plane(Mx@Vector((0,0,0)),R3@Vector((1,0,0)),R3@Vector((0,0,1)))
    T_.prism([pl.p(a,z+.012,.018) for a,z in pts],pl.n,.034,M['timber'])
    flat(C,pl,[(a*.985,z*.985+.022) for a,z in pts],M['banner'],.0365,.003)
    ring=pts+[pts[0]]
    for (a0,z0),(a1,z1) in zip(ring,ring[1:]):strip(D,pl,a0,z0+.012,a1,z1+.012,.035,.035,M['iron'],.039,.006)
    for a0,z0 in pts[::2]:rivet(D,pl.p(a0*.93,z0*.95+.03,.042),pl.n,M['iron'],.01,.005,6)
    stag(D,Plane(pl.p(0,.57,.039),pl.r,pl.u),.47,M['gilt'],0);fleur(D,Plane(pl.p(0,.2,.039),pl.r,pl.u),.4,M['gilt'],0)
def round_shield(o,lean,yaw,Rs=.38,t=.03):
    Mx=Matrix.Translation(o)@Matrix.Rotation(yaw,4,'Z')@Matrix.Rotation(-lean,4,'X');R3=Mx.to_3x3();n=64
    P=lambda a,r,d:Mx@Vector((math.cos(a)*r,d,Rs+math.sin(a)*r))
    for k in range(16):
        a0,a1=tau*k/16,tau*(k+1)/16;T_.poly([P(a0+(a1-a0)*j/4,Rs,-t/2) for j in range(5)]+[P(0,0,-t/2)],M['painted'] if k%2 else M['timber'])
    T_.poly([P(tau*j/n,Rs,t/2) for j in range(n)],M['timber'])
    for j in range(n):a0,a1=tau*j/n,tau*(j+1)/n;T_.poly([P(a0,Rs,-t/2),P(a0,Rs,t/2),P(a1,Rs,t/2),P(a1,Rs,-t/2)],M['timber'])
    surf(D,lambda u,v:Mx@Vector((math.cos(u*tau)*(Rs+.004*math.cos(math.pi*(v-.5))),-(t/2+.006)*math.sin(math.pi*(v-.5)),Rs+math.sin(u*tau)*(Rs+.004*math.cos(math.pi*(v-.5))))),64,4,M['iron'],.006,wrap=True)
    lathe(D,Mx@Vector((0,-t/2,Rs)),R3@Vector((0,-1,0)),[(0,.11),(.006,.105),(.012,.088)]+[(.012+.07*math.sin(a),max(.0015,.088*math.cos(a))) for a in (.4,.8,1.2,1.5)],M['steel'],24,.004)
    for k in range(16):a=tau*(k+.5)/16;rivet(D,P(a,Rs-.03,-t/2-.001),R3@Vector((0,-1,0)),M['iron'],.009,.005)
    for k in range(8):a=tau*k/8;rivet(D,P(a,.1,-t/2-.006),R3@Vector((0,-1,0)),M['iron'],.007,.004)
heater((-2.08,-1.0,.0058),math.radians(15),.12)
round_shield((1.5,-.97,.0095),math.radians(13),-.08)
# ---- polearm rack (right) --------------------------------------------------------------
RY=-.95
for x in (2.0,2.62):T_.box((x,RY,.5),(.07,.07,1.0),M['timber'],.008,grain=True,band=band());T_.box((x,RY,.025),(.08,.36,.05),M['timber'],.006,grain=True,band=band())
for z in (.45,.95):rail((1.965,RY,z),(2.655,RY,z),.06)
def polearm(x,top,kind,yaw=0):
    b=Vector((x,RY-.12,.0025));t=Vector((x+.02*math.sin(yaw*5),RY-.12+top*.115,top));ax=(t-b).normalized()
    tube(T_,b,t,.017,.015,M['timber'],8,.004)
    pl=Plane(t,Vector((1,0,0))-ax*ax.x,ax)
    pl.r=pl.r.normalized();pl.n=pl.r.cross(pl.u)
    tube(D,t-ax*.12,t+ax*.02,.021,.019,M['iron'],8,.003)
    if kind=='spear':
        blade(A,pl,[(0,.36),(.04,.2),(.034,.07),(.014,.02),(-.014,.02),(-.034,.07),(-.04,.2)],M['steel'],.012)
    elif kind=='halberd':
        blade(A,pl,[(.016,-.1),(.12,-.13),(.2,-.1),(.215,.02),(.19,.13),(.016,.06)],M['steel'],.012)
        blade(A,pl,[(-.016,-.02),(-.15,.03),(-.016,.04)],M['steel'],.01)
        blade(A,pl,[(-.013,.02),(.013,.02),(.006,.3),(0,.42),(-.006,.3)],M['steel'],.014)
    elif kind=='glaive':
        blade(A,pl,[(-.02,.0),(.035,.0),(.066,.2),(.056,.42),(0,.6),(-.02,.35)],M['steel'],.012)
    elif kind=='bardiche':
        blade(A,pl,[(.016,-.25),(.06,-.26),(.17,-.1),(.21,.08),(.19,.24),(.12,.32),(.016,.06)],M['steel'],.012)
    for k in (-.1,-.04):D.box(t+ax*k,(.046,.046,.012),M['iron'],.003,Matrix((pl.r,pl.n,ax)).transposed())
for x,top,kind in ((2.07,2.05,'halberd'),(2.18,2.2,'spear'),(2.3,1.9,'glaive'),(2.43,1.78,'bardiche'),(2.55,2.08,'spear')):polearm(x,top,kind)
prop('wooden_axe_02',(2.25,RY-.085,0),math.pi/2,1.0,ry=.07,base=0)
prop('ornate_war_hammer',(2.5,RY-.085,0),math.pi/2+.2,1.0,ry=.07,base=0)
prop('ornate_medieval_mace',(2.12,RY-.08,0),math.pi/2,.95,ry=.07,base=0)
# ---- barrels, swords against the posts ----------------------------------------------------
BS=(.74,.74,1.06)   # wine barrel slimmed to the concept's 0.55 x 0.92 m casks
prop('wine_barrel_01',(-2.4,-.42,0),.4,BS,aged=.3,base=0)
prop('wine_barrel_01',(-2.4,.3,0),1.9,BS,aged=.3,base=0)
prop('wine_barrel_01',(2.42,-.24,0),2.0,BS,aged=.3,base=0)
prop('wine_barrel_01',(2.42,.48,0),.2,BS,aged=.3,base=0)
prop('antique_estoc',(2.37,RY-.1,0),math.pi/2,1,rx=-.1,ry=math.pi,base=0)
# ---- lantern arm (right) and stag banner arm (left) --------------------------------------
AZ=3.22
for s,reach in ((-1,2.82),(1,2.62)):
    T_.beam((s*(PX+PS/2-.02),YF,AZ),(s*reach,YF,AZ),.09,.09,M['timber'],band=band())
    T_.beam((s*(PX+PS/2-.02),YF,AZ-.42),(s*(PX+.42),YF,AZ-.03),.07,.07,M['timber'],band=band())
    rope(s*PX,YF,AZ,3);D.box((s*(reach-.06),YF,AZ),(.1,.11,.11),M['iron'],.004)
# lantern: hexagonal iron frame around one glow pane prism
LX=2.5;top=Vector((LX,YF,AZ-.045));chain(D,top,top-Z*.2)
c=top-Z*(.2+.07+.16);hexo=lambda r,z:[(c.x+r*math.cos(tau*k/6),c.y+r*math.sin(tau*k/6),z) for k in range(6)]
D.prism(hexo(.082,c.z),(0,0,1),.27,M['glow'])
for p in hexo(.088,c.z):D.beam(Vector(p)-Z*.15,Vector(p)+Z*.15,.014,.014,M['iron'],grain=False)
for dz in (-.15,.15):D.prism(hexo(.108,c.z+dz),(0,0,1),.022,M['iron'])
roof=hexo(.112,c.z+.161);ap=Vector((c.x,c.y,c.z+.28))
for k in range(6):D.poly([roof[k],roof[(k+1)%6],ap],M['iron'])
D.poly(list(reversed(roof)),M['iron'])
lathe(D,ap-Z*.01,Z,[(0,.012),(.04,.0015)],M['iron'],8,.004)
D.beam(ap,top-Z*.2,.008,.008,M['iron'],grain=False)
bot=hexo(.1,c.z-.161);bp=Vector((c.x,c.y,c.z-.25))
for k in range(6):D.poly([bot[(k+1)%6],bot[k],bp],M['iron'])
D.poly(bot,M['iron'])
for k in range(6):
    a=tau*(k+.5)/6;D.beam(Vector((c.x+.1*math.cos(a),c.y+.1*math.sin(a),c.z+.16)),Vector((c.x+.13*math.cos(a),c.y+.13*math.sin(a),c.z+.2)),.01,.01,M['iron'],grain=False)
SOCKET=Vector(c)
# banner (left)
BX=-2.43;bz=[(BX-.25,3.03),(BX+.25,3.03),(BX+.25,1.86),(BX,1.62),(BX-.25,1.86)]
C.prism([(x,YF,z) for x,z in bz],(0,1,0),.014,M['banner'])
D.beam((BX-.29,YF,3.045),(BX+.29,YF,3.045),.022,.022,M['iron'],grain=False)
for x in (BX-.2,BX+.2):chain(D,(x,YF,AZ-.045),(x,YF,3.06))
for n in (-1,1):
    pb=Plane((BX,YF+n*.007,0),(-n,0,0),(0,0,1))
    ring=bz+[bz[0]]
    for (a0,z0),(a1,z1) in zip(ring,ring[1:]):strip(D,pb,-n*(a0-BX),z0,-n*(a1-BX),z1,.03,.03,M['gilt'],.0015)
    stag(D,Plane(pb.p(0,2.53,0),pb.r,pb.u),.5,M['gilt'],.0015);fleur(D,Plane(pb.p(0,2.02,0),pb.r,pb.u),.4,M['gilt'],.0015)
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
for o in objs:   # weld the shells so smooth shading runs across faces
    bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=bm.verts,dist=1e-6);bm.to_mesh(o.data);bm.free()
allo=objs+PROPS
def bounds(os):
    pts=[o.matrix_world@v.co for o in os for v in o.data.vertices];return [min(p[k] for p in pts) for k in range(3)],[max(p[k] for p in pts) for k in range(3)]
lo,hi=bounds(allo);shift=Vector((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2]))
for o in objs:o.data.transform(Matrix.Translation(shift))
for o in PROPS:o.matrix_world=Matrix.Translation(shift)@o.matrix_world
SOCKET+=shift
assert abs(lo[2])<2e-3,f'ground contact {lo[2]}'
lo,hi=bounds(allo)
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in allo)
assert tris<=350000,f'triangle budget: {tris}'
OUT=HERE/'SilverbrookArmorVendor'
for o in bpy.data.objects:o.select_set(o in allo)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
lamp=bpy.data.lights.new('Preview_LanternLight','POINT');lamp.energy=25;lamp.color=(1,.62,.3);lamp.shadow_soft_size=.05;lo_=bpy.data.objects.new('Preview_LanternLight',lamp);lo_.location=SOCKET;pre.objects.link(lo_)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c_=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c_);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c_.type='ORTHO';c_.ortho_scale=ortho
    else:c_.lens=lens
cam('Beauty',(-4.3,-6.4,2.7),(85,0,-34),lens=36);cam('Front',(0,-30,2.2),(90,0,0),6.2);cam('Side',(-30,0,2.2),(90,0,-90),6.2);cam('Rear',(0,30,2.2),(90,0,180),6.2)
cam('CloseLeft',(-1.6,-3.2,1.9),(86,0,-12),lens=40);cam('CloseRight',(1.9,-3.2,1.7),(84,0,14),lens=40);cam('CloseCounter',(.0,-2.6,2.1),(82,0,0),lens=30)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'ArmorVendor-reference.png')
lo,hi=bounds(allo)
used=sorted({o.name.rsplit('_',1)[0] for o in PROPS})
manifest=dict(asset='SilverbrookArmorVendor',asset_id='BLD-GreenlandsArmorVendor-01',reference=dict(file='ArmorVendor-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_armorvendor.py',method='procedural original geometry (concept interpretation, not image reconstruction) plus CC0 Poly Haven prop models',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(h-l,3) for l,h in zip(lo,hi)],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in allo},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans and models)',
    polyhaven_models=used,unreal_mesh='SM_SilverbrookArmorVendor',slot_m=list(SLOT),
    light_socket_m=[[round(v,3) for v in SOCKET]],light_material='SB_ArmorVendor_Glow',outputs=['SilverbrookArmorVendor.blend','SilverbrookArmorVendor.glb','SilverbrookArmorVendor.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('ARMORVENDOR_BUILT',manifest['bounds_m'],tris,manifest['light_socket_m'])
