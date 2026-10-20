"""Build for VEG-GreenlandsPineTall-01: four Scots-pine-like trees, grey lower bark turning orange-brown above, bare lower trunk, high open crown.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_pine.py      (needs ../Sources from ../acquire_sources.py; about 8 GB RAM)

Sources are CC0 Poly Haven pines (pine_tree_01 a/b/c, pine_sapling_medium a). Their needle sprays are 4-7 M tiny cut-out meshes (about 17 triangles, 6 cm each),
far too heavy to scatter, so each variant keeps a random subset of the sprays grown about their own centres to keep the crown coverage, while trunk, branches and
dead stubs are decimated. Foliage is opaque geometry (no alpha), so it works as Nanite or as an ordinary LOD chain. Scale, mirror, yaw and the orange upper-trunk
tint (vertex colour R) are the only edits to the scans. Origin: trunk base centre. No ground.
"""
import sys,math,re
import numpy as np
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import nature_kit as K,rock_family,bpy

SPRAY_BUDGET=300000;TRUNK_TRIS=9000;BRANCH_TRIS=14000
VARIANTS=[
    dict(asset='pine_tree_01',obj='pine_tree_01_a_LOD0',scale=(1.18,1.18,1.18),mirror=False,yaw=0,grow=4.6,bare=.58,seed=1),
    dict(asset='pine_tree_01',obj='pine_tree_01_c_LOD0',scale=(1.32,1.32,1.38),mirror=True,yaw=80,grow=4.5,bare=.62,seed=2),
    dict(asset='pine_tree_01',obj='pine_tree_01_b_LOD0',scale=(1.45,1.45,1.55),mirror=False,yaw=200,grow=4.3,bare=.6,seed=3),
    dict(asset='pine_sapling_medium',obj='pine_sapling_medium_a_LOD0',scale=(2.0,2.0,2.05),mirror=True,yaw=300,grow=2.9,bare=.56,seed=4),
]
ROLES=['trunk','branch','twig','dead']          # final material slot order
ORANGE=[1.25,.9,.75]


def compact(v,t):
    used=np.unique(t);rm=-np.ones(len(v),np.int64);rm[used]=np.arange(len(used));return v[used],rm[t],used


def smooth(x):
    x=np.clip(x,0,1);return x*x*(3-2*x)


def load_variant(i,vc):
    objs=K.load_gltf(vc['asset']);o=objs[vc['obj']]
    for x in objs.values():
        if x is not o:bpy.data.objects.remove(x,do_unlink=True)
    names=[re.sub(r'\.\d+$','',m.name) for m in o.data.materials];v,t,uv,mi=K.arrays(o);bpy.data.objects.remove(o,do_unlink=True);bpy.ops.outliner.orphans_purge(do_recursive=True)
    specs=K.gltf_material_specs(vc['asset'])
    kind=[('twig' if n.endswith('twig') else 'branch' if n.endswith('bark') else 'dead' if n.endswith('dead_branches') else 'trunk') for n in names]
    # global transform: scale, mirror, yaw, then trunk base centre -> origin
    sx,sy,sz=vc['scale'];v=v*np.array([-sx if vc['mirror'] else sx,sy,sz],np.float32)
    c,s=math.cos(math.radians(vc['yaw'])),math.sin(math.radians(vc['yaw']));v=v@np.array([[c,-s,0],[s,c,0],[0,0,1]],np.float32).T
    flip=vc['mirror']
    parts={kd:(t[mi==k][:, ::-1] if flip else t[mi==k],uv[mi==k][:, ::-1] if flip else uv[mi==k]) for k,kd in enumerate(kind)}
    sp={kd:specs[n] for kd,n in zip(kind,names)}
    if 'trunk' not in parts:   # pine_sapling_medium: trunk and branches share the bark material; the largest connected piece is the trunk
        bt,buv=parts['branch'];cv,ct,_=compact(v,bt);cmp=K.components(ct,len(cv));big=np.bincount(np.unique(cmp,return_inverse=True)[1]).argmax();lab=np.unique(cmp,return_inverse=True)[1]
        parts['trunk']=(bt[lab==big],buv[lab==big]);parts['branch']=(bt[lab!=big],buv[lab!=big]);sp['trunk']=sp['branch']
    tv,_,_=compact(v,parts['trunk'][0]);z0=tv[:,2].min();base=tv[tv[:,2]<z0+.4][:,:2].mean(0)
    v=v-np.array([base[0],base[1],z0],np.float32)
    # Scots habit: lift the crown. Everything below the crown start (32% of the source height) is stretched into a bare trunk of `bare` x height, the crown is
    # squeezed vertically into the remaining top (flat, dense, umbrella-like), and branch/needle pieces below the crown start are dropped (dead stubs: 20% kept).
    H0=float(v[:,2].max());cut=.32*H0;H1=H0;zz=v[:,2];bz=vc['bare']*H1
    v=v.copy();v[:,2]=np.where(zz<cut,zz*(bz/cut),bz+(zz-cut)*((H1-bz)/(H0-cut)))
    parts['cut']=(cut,bz)
    return v,parts,sp


def filter_comps(v,t,uv,keep_fn):
    sv,st,_=compact(v,t);comp=K.components(st,len(sv));cid,inv=np.unique(comp,return_inverse=True);c=np.stack([np.bincount(inv,weights=sv[st].mean(1)[:,k])/np.bincount(inv) for k in range(3)],1)
    k=keep_fn(c)[inv];return t[k],uv[k]


def spray(v,t,uv,vc,bz):
    sv,st,used=compact(v,t);comp=K.components(st,len(sv));cid,inv,cnt=np.unique(comp,return_inverse=True,return_counts=True)
    tri_c=sv[st].mean(1);cent=np.stack([np.bincount(inv,weights=tri_c[:,k],minlength=len(cid))/cnt for k in range(3)],1)
    rng=np.random.default_rng(vc['seed']);keep=np.zeros(len(cid),bool);ok=np.where(cent[:,2]>=bz)[0];keep[rng.choice(ok,min(int(SPRAY_BUDGET/(len(st)/len(cid))),len(ok)),replace=False)]=True
    k=keep[inv];st=st[k];suv=uv[k];ci=inv[k];vc_=np.zeros(len(sv),np.int64);vc_[st.ravel()]=np.repeat(ci,3)
    sv=cent[vc_]+(sv-cent[vc_])*vc['grow'];sv2,st2,_=compact(sv.astype(np.float32),st);return sv2,st2,suv,len(cid),int(keep.sum())


def build_variant(i,vc):
    v,parts,specs=load_variant(i,vc);H=float(v[:,2].max());objs=[];info={};ms=[];cut,bz=parts['cut'];rng0=np.random.default_rng(vc['seed']+9);ucx=None
    for kd in ROLES:
        sp=dict(specs[kd]);sp['name']=f'MI_PineTall_{kd.title()}_{i:02d}'
        if kd=='trunk':
            sp['tint']=[1.7,1.7,1.75];sp['sat']=.3;pb='Sources/pine_bark/pine_bark';sp['blend']=dict(base=pb+'_Diffuse.jpg',normal=pb+'_nor_gl.jpg',rough=pb+'_Rough.jpg',uv_scale=[2,6],tint=ORANGE,sat=1.1,amount=1.0,contrast=.5)
        if kd=='twig':sp['tint']=[.78,.85,.72]
        ms.append(sp)
    mats=[K.build_material(sp) for sp in ms]
    for slot,kd in enumerate(ROLES):
        t,uv=parts[kd]
        if kd=='branch':t,uv=filter_comps(v,t,uv,lambda c:c[:,2]>=bz-.01*H)
        if kd=='dead':t,uv=filter_comps(v,t,uv,lambda c:rng0.random(len(c))<.2)
        if kd=='twig':pv,pt,puv,ntw,kept=spray(v,t,uv,vc,bz);info.update(sprays_source=ntw,sprays_kept=kept)
        else:pv,pt,_=compact(v,t);puv=uv
        if kd=='trunk':   # drop the scan's root/ground lump around the foot
            zb=(pv[:,2]>1)&(pv[:,2]<2);cb0=pv[zb][:,:2].mean(0);rb0=np.median(np.hypot(*(pv[zb][:,:2]-cb0).T));bad=(pv[:,2]<.9)&(np.hypot(pv[:,0]-cb0[0],pv[:,1]-cb0[1])>rb0*1.35)
            keep=~bad[pt].any(1);pt=pt[keep];puv=puv[keep];pv,pt,_=compact(pv,pt)
        vcol=np.zeros((len(pv),4),np.float32);vcol[:,3]=1
        if kd=='trunk':vcol[:,0]=smooth((pv[:,2]/H-.22)/.5)       # grey and plated below, blending gradually into orange-brown flaky bark through the crown zone
        o=K.make(f'{kd}_{i}',pv,pt,puv,np.full(len(pt),slot,np.int32),mats,vcol=vcol)
        if kd=='trunk':
            K.decimate(o,TRUNK_TRIS);zb=(pv[:,2]>1)&(pv[:,2]<2);zt=(pv[:,2]>7)&(pv[:,2]<8);cb=pv[zb][:,:2].mean(0)
            rb_=np.median(np.hypot(*(pv[zb][:,:2]-cb).T));rt_=np.median(np.hypot(*(pv[zt][:,:2]-pv[zt][:,:2].mean(0)).T))
            ucx=K.frustum(f'UCX_SM_PineTall_{i:02d}_00',cb[0],cb[1],rb_*.9,rt_*.9,min(8.0,.5*H))
        if kd=='branch':K.decimate(o,BRANCH_TRIS)
        objs.append(o)
    o=K.join(objs,f'SM_PineTall_{i:02d}');o['ucx']=[ucx.name];return o,ms,info,ucx


K.reset();K.render_setup((2400,1300),48);K.add_ground()
objs=[];rows=[];mats_all=[];srcs=[]
for i,vc in enumerate(VARIANTS):
    o,ms,info,ucx=build_variant(i,vc);K.to_base_centre(o,centre_xy=(0,0),extras=[ucx])
    objs.append(o);mats_all+=ms;srcs.append(vc['asset'])
    rows.append(dict(mesh=f'SM_PineTall_{i:02d}',source=vc['asset'],source_object=vc['obj'],scale=list(vc['scale']),mirror=vc['mirror'],yaw=vc['yaw'],spray_growth=vc['grow'],
                     materials=[s['name'] for s in ms],**info))
cfg=dict(family='PineTall',dir=str(HERE),prefix='SM_PineTall',asset_id='VEG-GreenlandsPineTall-01',role='tree_pine_tall',budget=SPRAY_BUDGET+TRUNK_TRIS+BRANCH_TRIS+5000,
    scale_note='look profile place.scale is [0.95, 1.4]: meshes are 23-24 m tall at 1.0, so placed pines span 22-34 m (spec 20-30 m); sink 0',
    gap=2.0,res=(2400,1300),ortho=31,nanite=True,human_x=-3.0,human_y=0,extra_sources=['pine_bark'],
    detail=[('Detail-Trunk',0,16,4.5,6.0,50),('Detail-Crown',2,22,17,17,50)])
rock_family.finish(cfg,objs,rows,mats_all,srcs)
