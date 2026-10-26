"""Silverbrook hay bales: a small stack of three twine-tied rectangular straw bales (owner playtest 2026-10-26: the old cubes
were noise-textured boxes with stiff card stalks; these are rounded, bulged and flaked, strands lie along the bale, the cut ends
show stalk ends, the twine sits in a pinched groove and only a few short frayed strands droop from the edges).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_prophaybales.py
Run make_straw_side.py first (the side scan). Designed to match Silverbrook; no owner concept.
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
import hay_props_common as hp
import bpy
scene=kit.reset(5201);rng=kit.rng
asset=bpy.data.collections.new('SilverbrookPropHayBales');scene.collection.children.link(asset)
M=dict(end=kit.scan('SB_PropHayBales_Straw','sb_straw_face',dark=(.55,.45,.3),damp=False,normal=1.0),     # make_straw_scan.py: the cut ends
       side=kit.scan('SB_PropHayBales_StrawSide','sb_straw_side',dark=(.55,.46,.32),damp=False,normal=1.6),  # make_straw_side.py: strands along the bale
       twine=kit.solid('SB_PropHayBales_Twine',(.12,.08,.04),.9))                                          # dark tarred jute
G=dict(bales=Geo('SM_SBPropHayBales_Bales',asset),twine=Geo('SM_SBPropHayBales_Twine',asset),stalks=Geo('SM_SBPropHayBales_Stalks',asset))
SLOT=(2.2,0.9,1.2)   # bounds rounded up to 0.1 m (asserted in finish)
L,W,H=.95,.6,.56
RAD=.04;NX,NY,NZ=36,24,22;TWINE_X=(-.25,.25);FLAKES=9
HALF=Vector((L/2,W/2,H/2))
def hash1(i):return (math.sin(i*127.1+311.7)*43758.5453)%1.0
def surface(p,seed):
    """Rounded box point -> displaced point: pillow bulge, flake seams and layers on the long faces, dished ends, pinched twine grooves."""
    q=Vector([max(-(HALF[k]-RAD),min(HALF[k]-RAD,p[k])) for k in range(3)]);d=p-q;n=d.normalized() if d.length>1e-9 else Vector((0,0,1))
    r=q+n*RAD;x=r.x;long_=max(abs(n.y),abs(n.z));endness=abs(n.x)
    disp=long_*(.012*(1-(2*x/L)**2))                       # a loaded bale bulges between the strings
    for xs in TWINE_X:disp-=long_*.007*math.exp(-((x-xs)/.014)**2)   # the twine pinches the straw
    k=min(FLAKES-1,max(0,int((x+L/2)/L*FLAKES)));disp+=long_*(hash1(k+seed)-.5)*.008   # each flake sits a little proud or sunk
    seam=abs(((x+L/2)/L*FLAKES)%1.0-.5)*2;disp-=long_*.006*math.exp(-((1-seam)/.12)**2)    # and is parted from the next by a seam
    disp+=long_*.0025*math.sin(r.y*37+seed)*math.sin(r.z*29+seed*2)+endness*(-.008*(1-min(1,(r.y/(W/2))**2+(r.z/(H/2))**2)))
    return r+n*disp,n
def face(G_,R,C,a,sgn,nb,nc,seed,mat,uvfn):
    b,c=[k for k in range(3) if k!=a]
    def pt(i,j):
        p=Vector((0,0,0));p[a]=sgn*HALF[a];p[b]=(i/nb*2-1)*HALF[b];p[c]=(j/nc*2-1)*HALF[c];w,_=surface(p,seed);return w,(i/nb,j/nc)
    grid=[[pt(i,j) for j in range(nc+1)] for i in range(nb+1)]
    for i in range(nb):
        for j in range(nc):
            quad=[grid[i][j],grid[i+1][j],grid[i+1][j+1],grid[i][j+1]];pts=[C+R@w for w,_ in quad]
            if (pts[1]-pts[0]).cross(pts[2]-pts[0]).dot(R@(Vector((sgn if a==0 else 0,sgn if a==1 else 0,sgn if a==2 else 0))))<0:quad=quad[::-1];pts=pts[::-1]
            G_.poly(pts,mat,[uvfn(w) for w,_ in quad],smooth=True)
def loop(G_,R,C,x):
    """Twine: a 6-sided tube around the cross-section at local x, following the rounded outline (clamp-and-push, as the faces do)."""
    hy,hz=W/2-RAD,H/2-RAD;per=2*(W+H);n=64;ring=[]
    for i in range(n):
        t=per*i/n
        if t<W:y,z=-W/2+t,H/2
        elif t<W+H:y,z=W/2,H/2-(t-W)
        elif t<2*W+H:y,z=W/2-(t-W-H),-H/2
        else:y,z=-W/2,-H/2+(t-2*W-H)
        q=Vector((0,max(-hy,min(hy,y)),max(-hz,min(hz,z))));nrm=(Vector((0,y,z))-q).normalized()
        ring.append((Vector((x,0,0))+q+nrm*(RAD+.0015),nrm))
    seg=6;rad=.005;tube=[]
    for pos,nrm in ring:
        tang=Vector((1,0,0));tube.append([pos+(nrm*math.cos(math.tau*k/seg)+tang*math.sin(math.tau*k/seg))*rad for k in range(seg)])
    ctr=C+R@Vector((x,0,0))
    for i in range(n):
        A,B=tube[i],tube[(i+1)%n]
        for k in range(seg):
            pts_=[C+R@p for p in (A[k],B[k],B[(k+1)%seg],A[(k+1)%seg])]
            if (pts_[1]-pts_[0]).cross(pts_[2]-pts_[0]).dot(sum(pts_,Vector())/4-(C+R@Vector((x,ring[i][0].y,ring[i][0].z))))<0:pts_=pts_[::-1]
            G_.poly(pts_,M['twine'],[(0,0),(1,0),(1,1),(0,1)],smooth=True)
def frays(G_,R,C,count):
    """A few short strands drooping from the edges and ends (width 4 mm, both sides, close to the bale)."""
    for _ in range(count):
        f=rng.random()
        if f<.6:s=rng.choice((-1,1));p=Vector((s*L/2,rng.uniform(-W/2+.05,W/2-.05),rng.uniform(-H/2+.05,H/2-.05)));d=Vector((s,rng.uniform(-.4,.4),rng.uniform(-.3,.5)))
        else:s=rng.choice((-1,1));p=Vector((rng.uniform(-L/2+.08,L/2-.08),s*W/2*.97,rng.uniform(-H/2+.06,H/2-.06)));d=Vector((rng.uniform(-.7,.7),s*.5,rng.uniform(-.2,.6)))
        d.normalize();ln=rng.uniform(.07,.15);steps=5;pts=[p];cur=p.copy()
        for k in range(steps):
            d=(d+Vector((0,0,-.22))).normalized();cur=cur+d*(ln/steps);pts.append(cur.copy())
        side=(R@d).cross(Vector((0,0,1)));side=(side if side.length>1e-4 else Vector((1,0,0))).normalized()*.002
        v0=rng.uniform(0,.9);u0=rng.uniform(0,.8)
        for k in range(steps):
            a,b=C+R@pts[k],C+R@pts[k+1];quad=[a-side,b-side,b+side,a+side]
            uv=[(u0+k/steps*.15,v0),(u0+(k+1)/steps*.15,v0),(u0+(k+1)/steps*.15,v0+.012),(u0+k/steps*.15,v0+.012)]
            G_.poly(quad,M['side'],uv,smooth=False);G_.poly(quad[::-1],M['side'],uv[::-1],smooth=False)
def bale(c,yaw,tilt=0):
    R=Matrix.Rotation(math.radians(yaw),3,'Z')@Matrix.Rotation(math.radians(tilt),3,'X');C=Vector(c);seed=rng.randint(1,999)
    ou,ov=rng.random(),rng.random()
    for a,sgn in ((0,-1),(0,1)):   # cut ends
        face(G['bales'],R,C,a,sgn,NY,NZ,seed,M['end'],lambda w,sg=sgn:(w.y/.6+ou,w.z/.6+ov))
    for a,sgn in ((1,-1),(1,1)):   # long faces: strands along x
        face(G['bales'],R,C,a,sgn,NX,NZ,seed,M['side'],lambda w:(w.x/.6+ou,w.z/.6+ov))
    for a,sgn in ((2,-1),(2,1)):
        face(G['bales'],R,C,a,sgn,NX,NY,seed,M['side'],lambda w:(w.x/.6+ou,w.y/.6+ov))
    for x in TWINE_X:loop(G['twine'],R,C,x)
    frays(G['stalks'],R,C,130)
bale((-.49,0,H/2),1.5);bale((.49,.01,H/2),-1.0)                       # two on the ground, end to end, a 2 cm wider shadowed gap
bale((.02,.02,H*1.5-.01),-3.5)                                      # one on top, centred over the joint
hp.finish(HERE,'PropHayBales',G,SLOT,dict(footprint_m=[2.2,.9],front='-Y',ground_contact=True,near=['stables','farmhouse','granary','market','cottage','road','square'],clearance_m=.8),40000,beauty=(-2.2,-3.4,2.0),ortho=3.0,crop=(20,680,250,830),weld=('SM_SBPropHayBales_Bales','SM_SBPropHayBales_Twine'))
