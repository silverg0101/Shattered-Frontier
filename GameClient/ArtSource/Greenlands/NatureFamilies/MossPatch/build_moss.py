"""Build for VEG-GreenlandsMossPatch-01: three flat moss mats (0.5-0.9 m across, 4-8 cm tall) for ground and rock tops.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_moss.py

Geometry is original: a dense height grid cut to an irregular noisy outline, with cushion bumps (low-frequency lumps plus fine tufts) that roll down to z=0 at the rim so the mat
blends into soil or rock. Surface: CC0 Poly Haven mossy_rock colour/normal/roughness (its moss areas), planar-projected from above, tinted fresh green. No ground in the mesh; origin at base centre.
"""
import sys,math
import numpy as np
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import nature_kit as K,rock_family,bpy

VARIANTS=[ # outline half-extents (m), cushion height (m), lump frequency, tint, seed
    dict(half=(.30,.24),h=.07,freq=7,tint=[.5,.85,.3],seed=3),
    dict(half=(.42,.32),h=.045,freq=5,tint=[.42,.78,.26],seed=7),
    dict(half=(.36,.36),h=.06,freq=9,tint=[.55,.82,.34],seed=11),
]
TILE=1.0


def smooth(x):
    x=np.clip(x,0,1);return x*x*(3-2*x)


def mat_mesh(vc,res=.012):
    rng=np.random.default_rng(vc['seed']);a,b=vc['half'];k=int(rng.integers(7,10));ang=np.sort(rng.uniform(0,2*np.pi,k));rad=rng.uniform(.72,1.1,k);ph=rng.uniform(0,6.28,(4,2))
    gx=np.arange(-a*1.2,a*1.2+res,res);gy=np.arange(-b*1.2,b*1.2+res,res);X,Y=np.meshgrid(gx,gy);ny,nx=X.shape;P=np.c_[X.ravel(),Y.ravel()]
    th=np.arctan2(P[:,1]/b,P[:,0]/a);rr=np.hypot(P[:,0]/a,P[:,1]/b)
    edge=np.interp(th,np.r_[ang-2*np.pi,ang,ang+2*np.pi],np.r_[rad,rad,rad])*(1+.07*np.sin(5*th+ph[3,0])+.04*np.sin(11*th+ph[3,1]));rho=rr/edge;out=rho>=1
    P=np.where(out[:,None],P/rho[:,None]*.999,P);rho=np.where(out,1.0,rho)
    f=vc['freq'];lump=.5+.5*np.sin(P[:,0]*f+ph[0,0])*np.sin(P[:,1]*f*1.2+ph[0,1]);tuft=.5+.5*np.sin(P[:,0]*45+ph[1,0])*np.sin(P[:,1]*52+ph[1,1])*np.sin((P[:,0]+P[:,1])*31+ph[2,0])
    z=vc['h']*(.55+.45*lump)*(.85+.15*tuft)*smooth((1-rho)/.35)**.8+.004*tuft
    V=np.c_[P,z].astype(np.float32);idx=np.arange(nx*ny).reshape(ny,nx);q=np.c_[idx[:-1,:-1].ravel(),idx[:-1,1:].ravel(),idx[1:,1:].ravel(),idx[1:,:-1].ravel()]
    on=(rho>=1.0)[q].all(1);q=q[~on];T=np.r_[q[:,[0,1,2]],q[:,[0,2,3]]];area=np.abs(np.cross(V[T[:,1],:2]-V[T[:,0],:2],V[T[:,2],:2]-V[T[:,0],:2]));T=T[area>1e-8]
    used=np.unique(T);rm=-np.ones(len(V),np.int64);rm[used]=np.arange(len(used));V=V[used];T=rm[T]
    uv=np.stack([V[T][:,:,0]/TILE+rng.uniform(0,1),V[T][:,:,1]/TILE+rng.uniform(0,1)],-1).astype(np.float32);return V,T,uv


m='Sources/mossy_rock/mossy_rock'
cfg=dict(family='MossPatch',dir=str(HERE),prefix='SM_MossPatch',asset_id='VEG-GreenlandsMossPatch-01',role='moss_patch',budget=20000,
    scale_note='look profile place.scale assumed [0.8, 1.6]: mats are 0.5-0.9 m across at 1.0, so placed 0.4-1.4 m; sink 0.005; mats are flat, drape them on rock tops with the dressing',
    gap=.3,res=(2000,600),extra_sources=['mossy_rock'],human_x=-.8,human_y=.5,detail=[('Detail',0,.9,.5,.02,60)])
K.reset();K.render_setup((2000,700),64);K.add_ground(colour=(.25,.23,.2))
objs=[];rows=[];mats=[]
for i,vc in enumerate(VARIANTS):
    V,T,uv=mat_mesh(vc);spec=dict(name=f'MI_MossPatch_{i:02d}',base=m+'_Diffuse.jpg',normal=m+'_nor_gl.jpg',rough=m+'_Rough.jpg',tint=vc['tint'],sat=.95,normal_strength=1.0)
    mat=K.build_material(spec);o=K.make(f'SM_MossPatch_{i:02d}',V,T,uv,np.zeros(len(T),np.int32),[mat]);K.decimate(o,cfg['budget']);K.to_base_centre(o);objs.append(o);mats.append(spec)
    rows.append(dict(mesh=o.name,generator='mat_mesh() height grid',seed=vc['seed'],outline_half_m=vc['half'],height_m=vc['h'],material=spec['name']))
rock_family.finish(cfg,objs,rows,mats,['mossy_rock'])
