"""Build for VEG-GreenlandsSaplingBroadleaf-01: three birch/alder-like saplings, 1.6-2.8 m at scale 1.0 (look profile: 1-3 m).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_sapling.py      (needs ../Sources from ../acquire_sources.py)

Same procedural method and CC0 sources as ../BroadleafYoung (veg_kit.py): tapering stem, thin ascending branches, twigs, harvested tree_small_02 leaf pieces tinted fresh green,
hackberry bark lifted to pale grey. A sapling has one thin stem, a sparse crown and larger leaves relative to the stem. Origin: base centre of the stem. No ground.
"""
import sys
import numpy as np
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import nature_kit as K,rock_family,veg_kit as G,bpy
G.BARK_TILE=1.89;G.LEAF_BUDGET=40000;G.DROP=.2;G.TW=.12
VARIANTS=[ # stems: (x, y, lean_x, lean_y, height, base radius)
    dict(seed=5,stems=[(0,0,.15,.05,2.8,.018)],lmax=.32,leaf_scale=1.25),
    dict(seed=8,stems=[(0,0,-.2,.12,2.2,.015)],lmax=.27,leaf_scale=1.2),
    dict(seed=12,stems=[(0,0,.1,-.25,1.6,.012)],lmax=.22,leaf_scale=1.15),
]
K.reset();K.render_setup((2000,1100),48);K.add_ground();lib=G.leaf_library()
leaf_spec=dict(name='MI_SaplingBroadleaf_Leaf',base='Sources/tree_small_02/textures/tree_small_02_leaves_diff_2k.jpg',normal='Sources/tree_small_02/textures/tree_small_02_leaves_nor_gl_2k.jpg',
               arm='Sources/tree_small_02/textures/tree_small_02_leaves_arm_2k.jpg',alpha='Sources/tree_small_02/textures/tree_small_02_leaves_alpha_2k.png',
               tint=[1.35,1.6,.7],sat=1.05,two_sided=True,alpha_mode='MASKED')
h='Sources/japanese_hackberry_bark/japanese_hackberry_bark'
bark_spec=dict(name='MI_SaplingBroadleaf_Bark',base=h+'_Diffuse.jpg',normal=h+'_nor_gl.jpg',rough=h+'_Rough.jpg',tint=[2.5,2.5,2.4],sat=.1,
               blend=dict(base=h+'_Diffuse.jpg',normal=h+'_nor_gl.jpg',rough=h+'_Rough.jpg',tint=[.12,.09,.06],sat=.6,amount=1.0,contrast=.2))
mats=[K.build_material(bark_spec),K.build_material(leaf_spec)];objs=[];rows=[]
for i,vc in enumerate(VARIANTS):
    tubes,attach=G.skeleton(vc);Vs=[];Ts=[];Us=[];Rs=[];off=0
    for j,(P,r,sides) in enumerate(tubes):
        v,t,uv,R=G.tube(P,r,sides,mark_phase=vc['seed']+j*.37);Vs.append(v);Ts.append(t+off);Us.append(uv);Rs.append(R);off+=len(v)
    wv=np.vstack(Vs);wt=np.vstack(Ts);wuv=np.concatenate(Us);wR=np.concatenate(Rs)
    lv,lt,luv=G.leaves(attach,lib,vc['leaf_scale'],vc['seed']);V=np.vstack([wv,lv]);T=np.vstack([wt,lt+len(wv)]);UV=np.concatenate([wuv,luv])
    mi=np.r_[np.zeros(len(wt)),np.ones(len(lt))].astype(np.int32);vcol=np.zeros((len(V),4),np.float32);vcol[:len(wv),0]=wR;vcol[:,3]=1
    o=K.make(f'SM_SaplingBroadleaf_{i:02d}',V,T,UV,mi,mats,vcol=vcol);ux=[]
    for k,(P,r,sd) in enumerate([x for x in tubes if x[2]==8]):ux.append(K.frustum(f'UCX_SM_SaplingBroadleaf_{i:02d}_{k:02d}',P[0][0],P[0][1],r[0]*.95,r[10]*.95,min(1.0,P[10][2])))
    o['ucx']=[x.name for x in ux];K.to_base_centre(o,centre_xy=(vc['stems'][0][0],vc['stems'][0][1]),extras=ux)
    objs.append(o);rows.append(dict(mesh=o.name,seed=vc['seed'],stems=1,wood_triangles=int(len(wt)),leaf_triangles=int(len(lt)),leaf_scale=vc['leaf_scale'],materials=[bark_spec['name'],leaf_spec['name']]))
cfg=dict(family='SaplingBroadleaf',dir=str(HERE),prefix='SM_SaplingBroadleaf',asset_id='VEG-GreenlandsSaplingBroadleaf-01',role='sapling_broadleaf',budget=G.LEAF_BUDGET+20000,
    scale_note='look profile place.scale assumed [0.7, 1.1]: meshes are 1.6-2.8 m tall at 1.0 (spec 1-3 m); sink 0; doubles as GAT-01 growth stage',
    gap=1.0,res=(2000,1000),ortho=4.2,nanite=True,human_x=-1.2,human_y=0,extra_sources=['tree_small_02','japanese_hackberry_bark'])
rock_family.finish(cfg,objs,rows,[bark_spec,leaf_spec],['tree_small_02','japanese_hackberry_bark'])
