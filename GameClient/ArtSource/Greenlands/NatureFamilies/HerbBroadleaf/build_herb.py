"""Build for VEG-GreenlandsHerbBroadleaf-01: three low broad-leaf herb patches (lily-of-the-valley-like, 20-35 cm tall, 30-45 cm across).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_herb.py

Rosettes of arching broad leaves (CC0 tree_small_02 leaf pieces, flattened, enlarged to 8-14 cm and tinted fresh deep green) on thin stalks, with a few flower spikes carrying small white bells
(original ico-sphere geometry). Leaf shapes are the harvested real ones; arrangement is procedural. Origin: patch base centre; no ground.
"""
import sys,math
import numpy as np
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import nature_kit as K,rock_family,veg_kit as G,bpy

VARIANTS=[dict(seed=4,rosettes=4,leaves=(5,7),spikes=3,spread=.14),dict(seed=7,rosettes=3,leaves=(6,9),spikes=2,spread=.12),dict(seed=10,rosettes=5,leaves=(4,6),spikes=4,spread=.17)]
K.reset();K.render_setup((2000,700),64);K.add_ground(colour=(.2,.16,.12));lib=G.leaf_library();flat=[G.flat_leaf(p) for p,_,_ in lib]
l='Sources/tree_small_02/textures/tree_small_02_leaves'
leaf_spec=dict(name='MI_HerbBroadleaf_Leaf',base=l+'_diff_2k.jpg',normal=l+'_nor_gl_2k.jpg',arm=l+'_arm_2k.jpg',alpha=l+'_alpha_2k.png',tint=[.85,1.15,.7],sat=1.0,two_sided=True,alpha_mode='MASKED')
b='Sources/pine_tree_01/textures/pine_tree_01_bark'
stem_spec=dict(name='MI_HerbBroadleaf_Stem',base=b+'_diff_2k.jpg',normal=b+'_nor_gl_2k.jpg',arm=b+'_arm_2k.jpg',tint=[.9,1.5,.7],sat=.8,two_sided=True)
flower_spec=dict(name='MI_HerbBroadleaf_Flower',base='Sources/marble_rock_01/marble_rock_01_Diffuse.jpg',normal='Sources/marble_rock_01/marble_rock_01_nor_gl.jpg',rough='Sources/marble_rock_01/marble_rock_01_Rough.jpg',tint=[2.2,2.2,2.1],sat=.1)
mats=[K.build_material(leaf_spec),K.build_material(stem_spec),K.build_material(flower_spec)];objs=[];rows=[]
Rz=lambda a:np.array([[math.cos(a),-math.sin(a),0],[math.sin(a),math.cos(a),0],[0,0,1]]);Ry=lambda a:np.array([[math.cos(a),0,math.sin(a)],[0,1,0],[-math.sin(a),0,math.cos(a)]])
for i,vc in enumerate(VARIANTS):
    rng=np.random.default_rng(vc['seed']);parts=[]  # (verts,tris,uv,mat)
    def add(v,t,uv,m):parts.append((np.asarray(v,np.float32),np.asarray(t),np.asarray(uv,np.float32),m))
    for r in range(vc['rosettes']):
        a=rng.uniform(0,6.28);c=np.array([math.cos(a),math.sin(a),0])*rng.uniform(0,vc['spread'])
        for k in range(int(rng.integers(*vc['leaves']))):
            q,Ln=flat[int(rng.integers(len(lib)))];li=int(rng.integers(len(lib)));q,Ln=flat[li];L=rng.uniform(.08,.14);w=q*(L/Ln);yaw=rng.uniform(0,6.28);tilt=math.radians(rng.uniform(20,55))
            pet=rng.uniform(.04,.09);stem=np.c_[np.linspace(0,1,5)*pet*math.cos(math.radians(70)),np.zeros(5),np.linspace(0,1,5)*pet];
            # leaf blade: arches outward, tip lowering; base at the top of the petiole
            bend=w[:,0]/Ln;ang=tilt-bend*math.radians(35);x=np.cumsum(np.r_[0,np.diff(w[:,0])*0])  # placeholder keeps shape; use simple pitch below
            P=(w@Ry(-tilt).T)+np.array([stem[-1,0],0,stem[-1,2]]);P[:,2]-=np.clip(w[:,0],0,None)**2*2.2
            P=P@Rz(yaw).T+c;add(P,lib[li][1],lib[li][2],0)
            sv,st,suv,_=G.tube(stem@Rz(yaw).T+c,np.linspace(.0016,.001,5),4,thin_brown=False);add(sv,st,suv,1)
    for s in range(vc['spikes']):
        a=rng.uniform(0,6.28);c=np.array([math.cos(a),math.sin(a),0])*rng.uniform(0,vc['spread']*1.2);H=rng.uniform(.18,.3);n=11
        t=np.linspace(0,1,n);pts=np.c_[np.zeros(n)+.04*t**2,np.zeros(n),H*t]@Rz(rng.uniform(0,6.28)).T+c;sv,st,suv,_=G.tube(pts,np.linspace(.0016,.0008,n),4,thin_brown=False);add(sv,st,suv,1)
        for j in range(int(rng.integers(5,8))):
            k=int(n*.55)+j*(n-1-int(n*.55))//7;ph=rng.uniform(0,6.28);dpos=pts[min(k,n-1)]+np.array([math.cos(ph),math.sin(ph),0])*.012+np.array([0,0,-.015])
            import bmesh
            bm=bmesh.new();bmesh.ops.create_icosphere(bm,subdivisions=2,radius=.0055);fv=np.array([x.co[:] for x in bm.verts],np.float32)*np.array([1,1,1.25]);ff=np.array([[l_.vert.index for l_ in f.loops] for f in bm.faces]);bm.free()
            add(fv+dpos,ff,np.zeros((len(ff),3,2),np.float32)+[.5,.5],2)
    V=[];T=[];UV=[];M=[];off=0
    for v,t,uv,m in parts:V.append(v);T.append(t+off);UV.append(uv);M+=[m]*len(t);off+=len(v)
    o=K.make(f'SM_HerbBroadleaf_{i:02d}',np.vstack(V),np.vstack(T),np.concatenate(UV),np.array(M,np.int32),mats);K.to_base_centre(o);objs.append(o)
    rows.append(dict(mesh=o.name,seed=vc['seed'],rosettes=vc['rosettes'],spikes=vc['spikes'],materials=[leaf_spec['name'],stem_spec['name'],flower_spec['name']]))
cfg=dict(family='HerbBroadleaf',dir=str(HERE),prefix='SM_HerbBroadleaf',asset_id='VEG-GreenlandsHerbBroadleaf-01',role='herb_broadleaf',budget=40000,
    scale_note='look profile place.scale assumed [0.8, 1.5]: patches 0.3-0.45 m across and 0.2-0.35 m tall at 1.0; sink 0.01',gap=.3,res=(2000,700),
    extra_sources=['marble_rock_01'],human_x=-.8,human_y=.5,detail=[('Detail',0,.9,.4,.1,60)])
rock_family.finish(cfg,objs,rows,[leaf_spec,stem_spec,flower_spec],['pine_tree_01','tree_small_02','marble_rock_01'])
