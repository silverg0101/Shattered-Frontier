"""Build for ROCK-GreenlandsBedrockSlab-01: four flat, fractured granite slabs that emerge from the soil.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_slab.py

Geometry is original: a dense height grid cut to an irregular angular outline, split into 6-9 Voronoi fracture plates that each get their own height
and tilt, V-shaped cracks along the plate borders, weathered rounded shoulders and fine undulation, with a steep rim that drops to the soil line.
Surface: CC0 Poly Haven rock scans (see ../Sources/provenance.json) re-graded to grey granite with box-projected UVs (physical size), and patchy moss
from mossy_rock that collects in cracks and hollows (vertex colour R). No ground is part of the mesh; the origin is the base centre.
"""
import sys,math
import numpy as np
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import nature_kit as K,rock_family,bpy


def smooth(x):
    x=np.clip(x,0,1);return x*x*(3-2*x)


def slab(seed,half=(1.6,1.2),thick=.55,h=.03,plates=7,crack_depth=.1,crack_w=.04):
    """Returns verts, tris, moss weight (n,) for one slab, z from 0 (soil line) up."""
    rng=np.random.default_rng(seed);a,b=half
    k=rng.integers(6,9);ang=np.sort(rng.uniform(0,2*np.pi,k));rad=rng.uniform(.66,1.15,k)
    gx=np.arange(-a*1.25,a*1.25+h,h);gy=np.arange(-b*1.25,b*1.25+h,h);X,Y=np.meshgrid(gx,gy);ny,nx=X.shape
    P=np.c_[X.ravel(),Y.ravel()]
    th=np.arctan2(P[:,1]/b,P[:,0]/a);rr=np.hypot(P[:,0]/a,P[:,1]/b)
    edge=np.interp(th,np.r_[ang-2*np.pi,ang,ang+2*np.pi],np.r_[rad,rad,rad])*(1+.04*np.sin(9*th+rng.uniform(0,6)))
    rho=rr/edge;outside=rho>=1.0
    # snap outside vertices onto the outline so the rim is clean
    P=np.where(outside[:,None],P/rho[:,None]*.999,P);rho=np.where(outside,1.0,rho)
    # Voronoi plates (domain-warped so the cracks wander) plus a finer second set of joints
    ph=rng.uniform(0,6.28,(8,3))
    W=P+.10*np.c_[np.sin(P[:,1]*4.1+ph[0,0])+.5*np.sin(P[:,1]*9.3+ph[0,1]),np.sin(P[:,0]*3.7+ph[1,0])+.5*np.sin(P[:,0]*8.7+ph[1,1])]
    def voronoi(n,spread):
        sites=rng.uniform(-1,1,(n,2))*np.array([a,b])*spread;d=np.hypot(W[:,None,0]-sites[None,:,0],W[:,None,1]-sites[None,:,1])
        o=np.argsort(d,1);d1=np.take_along_axis(d,o[:,:1],1)[:,0];d2=np.take_along_axis(d,o[:,1:2],1)[:,0];return sites,o[:,0],(d2-d1)/2
    sites,cell,e=voronoi(plates,.9);off=rng.normal(0,.05,plates);tilt=rng.normal(0,.04,(plates,2))
    z=thick+off[cell]+((P-sites[cell])*tilt[cell]).sum(1)+P[:,0]*rng.normal(0,.025)+P[:,1]*rng.normal(0,.025)
    crack=np.clip(1-e/crack_w,0,1);z-=crack_depth*crack*(.6+.4*np.sin(P[:,0]*9+P[:,1]*5))     # V cracks along plate borders
    z-=.012*smooth(1-e/.15)                                                                      # worn plate shoulders
    _,cell2,e2=voronoi(26,1.0);crack2=np.clip(1-e2/.022,0,1);z-=.03*crack2  # fine joints
    z+=.02*(np.take(rng.normal(0,1,26),cell2))*0.5                                               # chipped micro-blocks
    for i,(wl,amp) in enumerate(((.85,.018),(.33,.012),(.13,.006))):
        f=2*np.pi/wl;z+=amp*np.sin(P[:,0]*f+ph[i+2,0])*np.sin(P[:,1]*f*1.1+ph[i+2,1])+amp*.6*np.sin((P[:,0]+P[:,1])*f*.7+ph[i+2,2])
    rim=smooth((rho-.92)/.08)**.7;z=z*(1-rim)+(-.02)*rim                                         # steep rim down to the soil line
    V=np.c_[P,z].astype(np.float32)
    idx=np.arange(nx*ny).reshape(ny,nx);q=np.c_[idx[:-1,:-1].ravel(),idx[:-1,1:].ravel(),idx[1:,1:].ravel(),idx[1:,:-1].ravel()]
    # drop cells entirely collapsed onto the outline (all four verts on the rim)
    on=(rho>=1.0)[q].all(1);q=q[~on];T=np.r_[q[:,[0,1,2]],q[:,[0,2,3]]]
    area=np.abs(np.cross(V[T[:,1],:2]-V[T[:,0],:2],V[T[:,2],:2]-V[T[:,0],:2]));T=T[area>1e-7]
    used=np.unique(T);rm=-np.ones(len(V),np.int64);rm[used]=np.arange(len(used));V=V[used];T=rm[T];crack=crack[used];rho_u=rho[used]
    blob=.5+.5*np.sin(V[:,0]*2.3+rng.uniform(0,6))*np.sin(V[:,1]*2.9+rng.uniform(0,6))+.25*np.sin(V[:,0]*7.1+V[:,1]*5.3);moss=np.clip(1.5*blob-.55,0,1)*(.4+.6*np.clip(crack*2+.3,0,1))
    return V,T,moss.astype(np.float32)


def box_uv(V,T,scale,offset):
    """Per-triangle box projection by dominant normal axis; UV in physical units / scale."""
    p0,p1,p2=V[T[:,0]],V[T[:,1]],V[T[:,2]];n=np.abs(np.cross(p1-p0,p2-p0));ax=n.argmax(1)
    cols={0:(1,2),1:(0,2),2:(0,1)};uv=np.empty((len(T),3,2),np.float32)
    for a,(c0,c1) in cols.items():
        m=ax==a;uv[m]=np.stack([V[T[m]][:,:,c0],V[T[m]][:,:,c1]],-1)
    return uv/scale+np.array(offset,np.float32)


SETS={'marble_rock_01':2.0,'rock_08':1.5,'rock_surface':2.0}   # physical size of the scan tile in metres (Poly Haven)
MOSS=dict(base='Sources/mossy_rock/mossy_rock_Diffuse.jpg',normal='Sources/mossy_rock/mossy_rock_nor_gl.jpg',rough='Sources/mossy_rock/mossy_rock_Rough.jpg',
          uv_scale=[.6,.6],tint=[.24,.4,.11],sat=1.0,amount=.9,contrast=.35)
VARIANTS=[ # scan set, tint, saturation, outline half-extents, thickness, plate count, seed
    dict(scan='marble_rock_01',tint=[.42,.43,.45],sat=.3,half=(1.55,1.15),thick=.3,plates=7,seed=3),
    dict(scan='rock_08',tint=[.5,.5,.52],sat=.35,half=(1.45,1.25),thick=.4,plates=8,seed=21),
    dict(scan='marble_rock_01',tint=[.36,.37,.4],sat=.25,half=(1.65,1.05),thick=.28,plates=6,seed=8),
    dict(scan='rock_surface',tint=[.52,.52,.54],sat=.25,half=(1.5,1.2),thick=.36,plates=9,seed=14),
]
cfg=dict(family='BedrockSlab',dir=str(HERE),prefix='SM_BedrockSlab',asset_id='ROCK-GreenlandsBedrockSlab-01',role='bedrock_slab',budget=30000,
    scale_note='look profile place.scale is [1.5, 2.6]: meshes are 2.4-3.4 m across at 1.0, so placed slabs span 3.6-8.8 m (spec 3-8 m); sink 0.35',
    moss=MOSS,gap=1.0,res=(2000,700))
K.reset();K.render_setup((2000,800),64);K.add_ground()
objs=[];rows=[];mats=[];srcs=[]
for i,vc in enumerate(VARIANTS):
    V,T,moss=slab(vc['seed'],vc['half'],vc['thick'],plates=vc['plates'])
    S=SETS[vc['scan']];rng=np.random.default_rng(vc['seed']+100);uv=box_uv(V,T,S,rng.uniform(0,1,2))
    d=f"Sources/{vc['scan']}/{vc['scan']}";name=f"SM_BedrockSlab_{i:02d}"
    spec=dict(name=f'MI_BedrockSlab_{i:02d}',base=d+'_Diffuse.jpg',normal=d+'_nor_gl.jpg',rough=d+'_Rough.jpg',tint=vc['tint'],sat=vc['sat'],normal_strength=1.0,blend=dict(MOSS))
    mat=K.build_material(spec);rgba=np.zeros((len(V),4),np.float32);rgba[:,0]=moss;rgba[:,3]=1
    o=K.make(name,V,T,uv,np.zeros(len(T),np.int32),[mat],vcol=rgba);K.decimate(o,cfg['budget']);K.to_base_centre(o)
    objs.append(o);mats.append(spec);srcs.append(vc['scan'])
    rows.append(dict(mesh=name,source=vc['scan'],scale=1.0,generator='slab() height grid',seed=vc['seed'],outline_half_m=vc['half'],plates=vc['plates'],thickness_m=vc['thick'],
                     material=spec['name'],moss_mean=round(float(moss.mean()),3)))
rock_family.finish(cfg,objs,rows,mats,srcs)
