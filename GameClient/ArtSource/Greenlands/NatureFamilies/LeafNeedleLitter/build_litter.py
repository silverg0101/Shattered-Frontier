"""Build for VEG-GreenlandsLeafNeedleLitter-01: three flat litter patches (~0.55 m across) of fallen pine needles and brown leaves, as thin small meshes.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_litter.py

Needles are thin tilted strips (pine_bark texture, tinted straw-brown); leaves are the CC0 tree_small_02 leaf pieces (veg_kit.leaf_library), laid flat and tinted autumn brown.
Everything stacks within ~2 cm of the ground. Origin: patch base centre, no ground.
"""
import sys,math
import numpy as np
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import nature_kit as K,rock_family,veg_kit as G,bpy

VARIANTS=[dict(seed=3,needles=520,leaves=26,rad=.27,leaf_len=.07),dict(seed=6,needles=380,leaves=40,rad=.3,leaf_len=.06),dict(seed=9,needles=650,leaves=14,rad=.25,leaf_len=.08)]
K.reset();K.render_setup((2000,700),64);K.add_ground(colour=(.18,.14,.1));lib=G.leaf_library()
flat=[G.flat_leaf(p) for p,_,_ in lib]
b='Sources/pine_tree_01/textures/pine_tree_01_bark';l='Sources/tree_small_02/textures/tree_small_02_leaves'
needle_spec=dict(name='MI_LeafNeedleLitter_Needle',base=b+'_diff_2k.jpg',normal=b+'_nor_gl_2k.jpg',arm=b+'_arm_2k.jpg',tint=[2.1,1.35,.65],sat=.9,two_sided=True)
leaf_spec=dict(name='MI_LeafNeedleLitter_Leaf',base=l+'_diff_2k.jpg',normal=l+'_nor_gl_2k.jpg',arm=l+'_arm_2k.jpg',alpha=l+'_alpha_2k.png',tint=[3.0,.5,.12],sat=.75,two_sided=True,alpha_mode='MASKED')
mats=[K.build_material(needle_spec),K.build_material(leaf_spec)];objs=[];rows=[]
for i,vc in enumerate(VARIANTS):
    rng=np.random.default_rng(vc['seed']);V=[];T=[];UV=[];M=[];off=0
    def disc():
        while True:
            p=rng.uniform(-1,1,2)*vc['rad']
            if np.hypot(*p)<vc['rad']*rng.uniform(.8,1.0):return p
    for _ in range(vc['needles']):     # needle: strip of length 6-10 cm, width 2.5 mm, small random tilt and height
        c=disc();L=rng.uniform(.06,.1);th=rng.uniform(0,np.pi);d=np.array([math.cos(th),math.sin(th)]);n=np.array([-d[1],d[0]]);z=rng.uniform(.002,.016);dz=rng.uniform(-.004,.004);w=.0015
        v=np.array([[*(c-d*L/2-n*w),z-dz],[*(c-d*L/2+n*w),z-dz],[*(c+d*L/2+n*w),z+dz],[*(c+d*L/2-n*w),z+dz]],np.float32);u0=rng.uniform(0,.8)
        V.append(v);T.append(np.array([[0,1,2],[0,2,3]])+off);UV.append(np.array([[(u0,0),(u0+.02,0),(u0+.02,.1)],[(u0,0),(u0+.02,.1),(u0,.1)]],np.float32));M+=[0,0];off+=4
    for _ in range(vc['leaves']):
        k=int(rng.integers(len(lib)));q,Ln=flat[k];s=vc['leaf_len']*rng.uniform(.8,1.3)/Ln;th=rng.uniform(0,6.28);R=np.array([[math.cos(th),-math.sin(th),0],[math.sin(th),math.cos(th),0],[0,0,1]])
        t=math.radians(rng.uniform(-14,14));Rx=np.array([[1,0,0],[0,math.cos(t),-math.sin(t)],[0,math.sin(t),math.cos(t)]]);w=(q*s)@Rx.T@R.T;c=disc();w[:,0]+=c[0];w[:,1]+=c[1];w[:,2]+=rng.uniform(.006,.022)-w[:,2].min()
        V.append(w.astype(np.float32));T.append(lib[k][1]+off);UV.append(lib[k][2]);M+=[1]*len(lib[k][1]);off+=len(w)
    Va=np.vstack(V);Ta=np.vstack(T);o=K.make(f'SM_LeafNeedleLitter_{i:02d}',Va,Ta,np.concatenate(UV),np.array(M,np.int32),mats);K.to_base_centre(o);objs.append(o)
    rows.append(dict(mesh=o.name,seed=vc['seed'],needles=vc['needles'],leaves=vc['leaves'],radius_m=vc['rad'],materials=[needle_spec['name'],leaf_spec['name']]))
cfg=dict(family='LeafNeedleLitter',dir=str(HERE),prefix='SM_LeafNeedleLitter',asset_id='VEG-GreenlandsLeafNeedleLitter-01',role='leaf_needle_litter',budget=40000,
    scale_note='look profile place.scale assumed [0.8, 1.6]: patches 0.5-0.6 m across at 1.0; sink 0; thin enough to lay straight on terrain',gap=.3,res=(2000,600),
    extra_sources=['pine_bark','tree_small_02'],human_x=-.8,human_y=.5,detail=[('Detail',0,.9,.5,.02,60)])
rock_family.finish(cfg,objs,rows,[needle_spec,leaf_spec],['pine_tree_01','tree_small_02'])
