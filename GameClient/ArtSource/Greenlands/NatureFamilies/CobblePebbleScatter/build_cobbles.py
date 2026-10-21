"""Build for ROCK-GreenlandsCobblePebbleScatter-01: six small clusters of grey-blue granite cobbles and pebbles (5-30 cm stones, 3-9 per cluster).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cobbles.py

Geometry is original: each stone is a subdivided icosphere pushed by two octaves of 3D noise plus a few planar cuts (rounded, slightly faceted granite), flattened to sit
on the soil, with its lowest point sunk a little. Stones are packed without overlap on a ~0.7 m disc. Surface: CC0 Poly Haven rock scans (see ../Sources/provenance.json)
re-graded grey-blue, box-projected at physical size per stone. Origin: cluster base centre; no ground in the mesh.
"""
import sys,math
import numpy as np
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import nature_kit as K,rock_family,bpy,bmesh
sys.path.insert(0,str(HERE.parent/'BedrockSlab'))

SETS={'rock_08':1.5,'rock_surface':2.0,'marble_rock_01':2.0,'rock_boulder_dry':2.0}
VARIANTS=[ # scan, tint, saturation, stone count, cluster radius (m), seed
    dict(scan='rock_08',tint=[.46,.5,.56],sat=.35,n=5,rad=.30,seed=2),
    dict(scan='rock_surface',tint=[.4,.44,.5],sat=.3,n=7,rad=.34,seed=5),
    dict(scan='rock_surface',tint=[.5,.53,.58],sat=.3,n=4,rad=.28,seed=9),
    dict(scan='rock_08',tint=[.38,.42,.48],sat=.3,n=9,rad=.38,seed=13),
    dict(scan='rock_08',tint=[.5,.52,.58],sat=.3,n=6,rad=.32,seed=17),
    dict(scan='rock_surface',tint=[.42,.46,.52],sat=.25,n=8,rad=.36,seed=23),
]


def stone(rng,r):
    """Returns v,t (triangles) of one stone of nominal radius r, base on z=0."""
    bm=bmesh.new();bmesh.ops.create_icosphere(bm,subdivisions=4,radius=1.0);v=np.array([x.co[:] for x in bm.verts],np.float32);f=np.array([[l.vert.index for l in fc.loops] for fc in bm.faces],np.int64);bm.free()
    sx,sy,sz=rng.uniform(.8,1.25),rng.uniform(.8,1.25),rng.uniform(.5,.8);ph=rng.uniform(0,6.28,(2,3));d=v/np.linalg.norm(v,axis=1)[:,None]
    n=.14*np.sin(d[:,0]*2.3+ph[0,0])*np.sin(d[:,1]*2.1+ph[0,1])*np.sin(d[:,2]*2.5+ph[0,2])+.05*np.sin(d[:,0]*6.1+ph[1,0])*np.sin(d[:,1]*5.7+ph[1,1])*np.sin(d[:,2]*6.3+ph[1,2])
    v=d*(1+n)[:,None]
    for _ in range(rng.integers(2,5)):    # planar cuts: facets
        nrm=rng.normal(size=3);nrm/=np.linalg.norm(nrm);off=rng.uniform(.72,.9);dist=v@nrm;v=np.where((dist>off)[:,None],v-(dist-off)[:,None]*nrm[None,:]*.85,v)
    v=v*np.array([sx,sy,sz])*r;v[:,2]=np.maximum(v[:,2],-sz*r*.55);v[:,2]-=v[:,2].min()-.0;return v.astype(np.float32),f[:,[0,1,2]] if f.shape[1]==3 else np.r_[f[:,[0,1,2]],f[:,[0,2,3]]]


def cluster(vc):
    rng=np.random.default_rng(vc['seed']);V=[];T=[];off=0;placed=[]
    sizes=sorted([rng.choice([.05,.06,.08,.1,.13,.15],p=[.25,.2,.2,.15,.12,.08]) for _ in range(vc['n'])],reverse=True);sizes[0]=max(sizes[0],.12)
    for r in sizes:
        for _ in range(200):
            a=rng.uniform(0,6.28);d=rng.uniform(0,vc['rad']);p=np.array([math.cos(a)*d,math.sin(a)*d])
            if all(np.hypot(*(p-q))>r+rr+.01 for q,rr in placed):break
        placed.append((p,r));v,t=stone(rng,r);th=rng.uniform(0,6.28);R=np.array([[math.cos(th),-math.sin(th),0],[math.sin(th),math.cos(th),0],[0,0,1]]);v=v@R.T
        v[:,0]+=p[0];v[:,1]+=p[1];v[:,2]-=r*.18;V.append(v);T.append(t+off);off+=len(v)
    V=np.vstack(V);T=np.vstack(T);return V,T


def box_uv(V,T,scale,offset):
    p0,p1,p2=V[T[:,0]],V[T[:,1]],V[T[:,2]];n=np.abs(np.cross(p1-p0,p2-p0));ax=n.argmax(1);cols={0:(1,2),1:(0,2),2:(0,1)};uv=np.empty((len(T),3,2),np.float32)
    for a,(c0,c1) in cols.items():
        m=ax==a;uv[m]=np.stack([V[T[m]][:,:,c0],V[T[m]][:,:,c1]],-1)
    return uv/scale+np.array(offset,np.float32)


cfg=dict(family='CobblePebbleScatter',dir=str(HERE),prefix='SM_CobblePebbleScatter',asset_id='ROCK-GreenlandsCobblePebbleScatter-01',role='cobble_pebble_scatter',budget=30000,
    scale_note='look profile place.scale is [0.8, 1.3]: stones 10-30 cm each, clusters 0.6-0.8 m across at scale 1.0; sink 0.02',moss=None,gap=.4,res=(2200,700),extra_sources=[],detail=[('Detail',3,1.5,.55,.05,60)],human_x=-.9,human_y=.6)
K.reset();K.render_setup((2200,800),64);K.add_ground()
objs=[];rows=[];mats=[];srcs=[]
for i,vc in enumerate(VARIANTS):
    V,T=cluster(vc);rng=np.random.default_rng(vc['seed']+100);uv=box_uv(V,T,SETS[vc['scan']],rng.uniform(0,1,2));d=f"Sources/{vc['scan']}/{vc['scan']}";name=f'SM_CobblePebbleScatter_{i:02d}'
    spec=dict(name=f'MI_CobblePebbleScatter_{i:02d}',base=d+'_Diffuse.jpg',normal=d+'_nor_gl.jpg',rough=d+'_Rough.jpg',tint=vc['tint'],sat=vc['sat'],normal_strength=1.0)
    mat=K.build_material(spec);o=K.make(name,V,T,uv,np.zeros(len(T),np.int32),[mat]);K.decimate(o,cfg['budget']);K.to_base_centre(o);objs.append(o);mats.append(spec);srcs.append(vc['scan'])
    rows.append(dict(mesh=name,source=vc['scan'],scale=1.0,generator='stone() noisy icosphere clusters',seed=vc['seed'],stones=vc['n'],cluster_radius_m=vc['rad'],material=spec['name']))
rock_family.finish(cfg,objs,rows,mats,srcs)
