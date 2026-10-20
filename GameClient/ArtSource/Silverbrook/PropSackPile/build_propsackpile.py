"""Silverbrook street prop: SACK PILE. Stuffed, slumped hessian grain sacks with pinched, corded necks, stacked in four layers on a low oak pallet.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propsackpile.py
Owner concept: docs/Game-Design/concept-art/Silverbrook/Street Props.png (1.6 m x 1.0 m). Sacks are procedural soft bodies (superellipse rings, sagging belly, gathered frill at the tie).
"""
import sys,math,random
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
from prop_common_street import torus,finish
import bpy
ZP=.12
scene=kit.reset(5203);asset=bpy.data.collections.new('SilverbrookPropSackPile');scene.collection.children.link(asset)
M=dict(sack=kit.scan('SB_PropSackPile_Hessian','hessian_380',aged=.5,dark=(.5,.47,.42),normal=2.5,damp=False),timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    cord=kit.solid('SB_PropSackPile_Cord',(.17,.12,.065),.9))
BANDS=kit.bands('weathered_brown_planks');band=lambda:kit.rng.choice(BANDS)
G=dict(sacks=Geo('SM_SBSackPile_Sacks',asset),cord=Geo('SM_SBSackPile_Cord',asset),pallet=Geo('SM_SBSackPile_Pallet',asset))
R=random.Random(11)
# sack profile along its length u in [0,1]: (u, radius factor). Blunt seamed bottom at 0, belly, pinched neck at .92, gathered frill at 1.
PROFILE=[(0,0),(.012,.55),(.04,.82),(.09,.94),(.2,1),(.45,1),(.65,.96),(.78,.84),(.86,.5),(.91,.17),(.935,.12),(.955,.13),(.975,.22),(.99,.3),(1,.22),(1,0)]
NECK=.935;N=26
K=.3                                  # UV scale: the scan tile is 0.27 m (2 mm threads, which mip to flat colour at 2-3 m); K=.3 enlarges it to 0.9 m so the weave (about 6 mm threads) reads at game distance
H={}                                  # settle heightfield: max sack surface z per 4 cm cell
cell=lambda p:(round(p.x/.04),round(p.y/.04))
def top(p):return max(H.get((p_[0]+dx,p_[1]+dy),ZP) for p_ in [cell(p)] for dx in (-1,0,1) for dy in (-1,0,1))
def sack(c,yaw,L,W,T,pitch=0,roll=0):
    """Sack centre xy, long axis along local x, neck at +x. Built high, dropped onto the heightfield, then draped over what it rests on."""
    rot=Matrix.Rotation(yaw,3,'Z')@Matrix.Rotation(pitch,3,'Y')@Matrix.Rotation(roll,3,'X');ph=R.uniform(0,6.28);s=kit.SCALE[M['sack'].name]
    rings=[];loc=[];c0=Vector((c[0],c[1],2.0))
    for u,f in PROFILE:
        gather=max(0,(u-.9)/.1);amp=.035+.3*gather;ring=[];lr=[]
        for j in range(N):
            t=math.tau*j/N;rr=f*(1+amp*math.sin(7*t+ph+u*5)*(1 if f>0 else 0)+.05*math.sin(3*t+ph*2+u*9)+.025*math.sin(5*t+ph*3+u*13))
            y,z=W/2*rr*math.cos(t),T/2*rr*math.sin(t)
            z=z*1.08 if z>0 else z*.62                              # slump: flat sagging underside, rounded top
            z+=.05*max(0,u-.8)**1.5*L*4*(1 if f>0 else 0)           # neck cocks upward
            ring.append(rot@Vector(((u-.5)*L,y,z))+c0);lr.append((y,z))
        rings.append(ring);loc.append(lr)
    body=[(i,j) for i,(u,f) in enumerate(PROFILE) for j in range(N) if u<.85 and f>0]
    low=[(rings[i][j].z-top(rings[i][j]),top(rings[i][j])) for i,j in body if loc[i][j][1]<0];gaps=sorted(g for g,t in low if t>ZP+.01) or sorted(g for g,t in low);drop=min(gaps[len(gaps)//7]+.02,gaps[0]+.045)   # soft sacks sink into each other: rest near the support peaks, at most 4.5 cm into any of them
    for r in rings:
        for p in r:p.z-=drop
    for i,j in body:   # drape: underside verts that float within 8 cm of a support sink onto it
        p=rings[i][j];z0=loc[i][j][1]
        if z0<0:
            g=p.z-top(p)
            if 0<g<.07 and .12<PROFILE[i][0]<.8:p.z-=g*.6*min(1,-z0/(T*.25))
    for i,j in body:
        k=cell(rings[i][j]);H[k]=max(H.get(k,ZP),rings[i][j].z)
    per=math.pi*(W+T)/2;nr=len(PROFILE)
    for i in range(nr-1):
        for j in range(N):
            k=(j+1)%N;q=[rings[i][j],rings[i+1][j],rings[i+1][k],rings[i][k]];d=[];keep=[]
            for idx,p in zip((0,1,2,3),q):
                if not d or (p-d[-1]).length>1e-6:d.append(p);keep.append(idx)
            if (d[0]-d[-1]).length<1e-6:d.pop();keep.pop()
            if len(d)<3:continue
            cc=sum(rings[i],Vector())/N;cc=cc.lerp(sum(rings[i+1],Vector())/N,.5)
            idxs=[(i,j),(i+1,j),(i+1,k),(i,k)]
            if i in (0,1,nr-3,nr-2):   # closed ends: planar UVs from the cross-section, so no radial fan
                uv=[(loc[a][b][0]*K/s+ph,loc[a][b][1]*K/s) for a,b in (idxs[m] for m in keep)]
            else:uv=[(PROFILE[a][0]*L*K/s,(b if b>=j else N)/N*per*K/s) for a,b in (idxs[m] for m in keep)]
            nrm=sum(((a.cross(d[(m+1)%len(d)])) for m,a in enumerate(d)),Vector())
            if (nrm.dot(rot@Vector((1,0,0)))<0) if i==nr-2 else (nrm.dot(sum(d,Vector())/len(d)-cc)<0):d.reverse();uv.reverse()
            G['sacks'].poly(d,M['sack'],uv,smooth=True)
    nc=sum(rings[PROFILE.index(next(q for q in PROFILE if q[0]==NECK))],Vector())/N;ay,az=rot@Vector((0,1,0)),rot@Vector((0,0,1));rn=.12*(W+T)/4+.004
    for d in (-.012,.012):torus(G['cord'],nc+rot@Vector((d,0,0)),ay,az,rn*1.1,.0095,M['cord'],seg=14,tube=6)
# ---- pallet: two rough oak bearers along X with loose deck planks across them ------------------------
oak=lambda c,s,**k:G['pallet'].box(c,s,M['timber'],.008,grain=True,band=band(),**k)
for y in (-.33,.33):oak((0,y,.045),(1.5,.13,.09))
for i in range(6):
    x=-.62+.248*i+R.uniform(-.012,.012);ln=R.uniform(.92,1.0);oak((x,R.uniform(-.02,.02),.105),(.19,ln,.03),rot=Matrix.Rotation(R.uniform(-.025,.025),3,'Z'))
T=.235
def add(x,y,z,yaw,L=.78,W=.5,**k):sack((x+R.uniform(-.015,.015),y+R.uniform(-.015,.015),0),yaw+R.uniform(-.07,.07),L,W,T,roll=R.uniform(-.06,.06),**k)
# layer 1: four sacks along X, necks outward; layer 2: three across Y; layer 3: two along X; layer 4: one across Y
for sx in (-1,1):
    for y in (-.255,.255):add(sx*.39,y,T/2,0 if sx>0 else math.pi)
for i,x in enumerate((-.5,0,.5)):add(x,.0,T*1.5-.03,-math.pi/2 if i%2==0 else math.pi/2)
for sx,y in ((-1,-.14),(1,.14)):add(sx*.37,y,T*2.5-.055,0 if sx>0 else math.pi,W=.48)
add(0,0,T*3.5-.085,-math.pi/2,W=.46,pitch=.0)
import bmesh
for k,g in G.items():
    o=g.object()
    if k!='pallet':   # weld the duplicated ring vertices so smooth shading rounds the sacks (UVs stay per loop)
        bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=bm.verts,dist=1e-5);bm.to_mesh(o.data);bm.free();o.data.update()
finish(HERE,'PropSackPile',scene,asset,dict(footprint_m=None,front='-Y',ground_contact=True,near=['granary','farmhouse','market','mill','stables','square'],clearance_m=1.1),[(1.4,1.8),(.8,1.2),(.8,1.2)])
