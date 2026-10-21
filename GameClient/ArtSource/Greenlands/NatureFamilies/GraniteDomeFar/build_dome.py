"""Build for TERR-GreenlandsGraniteDomeFar-01: two rounded granite hills for the distance, 420 x 330 m and 360 x 300 m at 1.0, 90-120 m tall.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_dome.py

Original geometry: a radial heightfield (smooth dome profile broken by ridge noise, shoulders and a few exposed granite ribs), decimated to ~45k triangles. Vertex colour R marks the forested lower
flanks (by height and slope): layer A is CC0 Poly Haven rock_08 re-graded to pale grey granite, layer B is forest_ground_04 tinted dark conifer green. Meant to be seen beyond the playable radius:
no collision, origin at the base centre, rim flush with the ground so it sinks into the terrain backdrop.
"""
import sys,math
import numpy as np
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import nature_kit as K,rock_family,bpy

VARIANTS=[dict(half=(210,165),h=115,seed=4,rot=.3),dict(half=(180,150),h=92,seed=9,rot=1.1)]


def smooth(x):
    x=np.clip(x,0,1);return x*x*(3-2*x)


def dome(vc,n=260):
    rng=np.random.default_rng(vc['seed']);a,b=vc['half'];ph=rng.uniform(0,6.28,(6,2));gx=np.linspace(-a*1.1,a*1.1,n);gy=np.linspace(-b*1.1,b*1.1,n);X,Y=np.meshgrid(gx,gy);P=np.c_[X.ravel(),Y.ravel()]
    c,s=math.cos(vc['rot']),math.sin(vc['rot']);Q=P@np.array([[c,-s],[s,c]]);th=np.arctan2(Q[:,1]/b,Q[:,0]/a);rr=np.hypot(Q[:,0]/a,Q[:,1]/b)
    edge=1+.12*np.sin(3*th+ph[0,0])+.07*np.sin(5*th+ph[0,1])+.04*np.sin(9*th+ph[1,0]);rho=rr/edge
    prof=smooth(1-rho)**.62*(1+.12*np.sin(Q[:,0]/a*5+ph[2,0])*np.sin(Q[:,1]/b*4+ph[2,1]))   # rounded dome + ridges
    prof=prof+.06*smooth(1-rho*1.3)*np.sin(Q[:,0]/a*11+ph[3,0])*np.sin(Q[:,1]/b*9+ph[3,1])
    prof=np.clip(prof,0,None);z=vc['h']*prof*(1+.03*np.sin(Q[:,0]*.21+ph[4,0])*np.sin(Q[:,1]*.18+ph[4,1]));z=np.where(rho>=1,0,z)
    V=np.c_[P,z].astype(np.float32);idx=np.arange(n*n).reshape(n,n);q=np.c_[idx[:-1,:-1].ravel(),idx[:-1,1:].ravel(),idx[1:,1:].ravel(),idx[1:,:-1].ravel()]
    keep=(rho[q]<1.0).all(1);q=q[keep];T=np.r_[q[:,[0,1,2]],q[:,[0,2,3]]];used=np.unique(T);rm=-np.ones(len(V),np.int64);rm[used]=np.arange(len(used));V=V[used];T=rm[T]
    return V,T,rho[used]


def forest_weight(V,T,rho,seed):
    p0,p1,p2=V[T[:,0]],V[T[:,1]],V[T[:,2]];nrm=np.cross(p1-p0,p2-p0);nrm/=np.linalg.norm(nrm,axis=1)[:,None]+1e-9
    acc=np.zeros(len(V),np.float32);cnt=np.zeros(len(V),np.float32)
    for k in range(3):np.add.at(acc,T[:,k],nrm[:,2]);np.add.at(cnt,T[:,k],1)
    nz=acc/np.maximum(cnt,1);rng=np.random.default_rng(seed);ph=rng.uniform(0,6.28,3);hz=V[:,2]/V[:,2].max()
    noise=.5+.5*np.sin(V[:,0]*.03+ph[0])*np.sin(V[:,1]*.027+ph[1])
    return np.clip(smooth((.58-hz+.2*noise)/.3)*smooth((nz-.6)/.2),0,1)   # low and not steep


ROCK='Sources/rock_08/rock_08';FOR='Sources/forest_ground_04/forest_ground_04'
_cam=K.camera
def _cam_far(*a,**k):
    c=_cam(*a,**k);c.data.clip_end=20000;c.data.clip_start=1;return c
K.camera=_cam_far
objs=[];rows=[];mats=[]
K.reset();K.render_setup((2200,900),64)
for i,vc in enumerate(VARIANTS):
    V,T,rho=dome(vc);fw=forest_weight(V,T,rho,vc['seed']);uv=np.stack([V[T][:,:,0]/45.,V[T][:,:,1]/45.],-1).astype(np.float32)
    spec=dict(name=f'MI_GraniteDomeFar_{i:02d}',base=ROCK+'_Diffuse.jpg',normal=ROCK+'_nor_gl.jpg',rough=ROCK+'_Rough.jpg',tint=[.5,.52,.56],sat=.3,uv_scale=[1,1],normal_strength=1.0,
              blend=dict(base=FOR+'_Diffuse.jpg',normal=FOR+'_nor_gl.jpg',rough=FOR+'_Rough.jpg',tint=[.22,.32,.16],sat=1.1,uv_scale=[1,1],amount=1.0,contrast=.2))
    mat=K.build_material(spec);rgba=np.zeros((len(V),4),np.float32);rgba[:,0]=fw;rgba[:,3]=1
    o=K.make(f'SM_GraniteDomeFar_{i:02d}',V,T,uv,np.zeros(len(T),np.int32),[mat],vcol=rgba);K.decimate(o,45000);K.to_base_centre(o);objs.append(o);mats.append(spec)
    rows.append(dict(mesh=o.name,generator='dome() radial heightfield',seed=vc['seed'],half_extents_m=vc['half'],height_m=vc['h'],forest_mean=round(float(fw.mean()),3),material=spec['name']))
cfg=dict(family='GraniteDomeFar',dir=str(HERE),prefix='SM_GraniteDomeFar',asset_id='TERR-GreenlandsGraniteDomeFar-01',role='granite_dome_far',budget=45000,
    scale_note='backdrop only, beyond the playable radius; place 1.5-4 km out at scale 3-8 as needed (silhouette family); no collision; rim is at z=0',gap=60,res=(2200,800),ortho=None,
    extra_sources=['forest_ground_04'],human_x=-280,human_y=-200,nanite=True)
rock_family.finish(cfg,objs,rows,mats,['rock_08','forest_ground_04'])
