"""Silverbrook street prop: IVY CLUMP. Four crossed alpha cards (1.2 m wide x 1.0 m tall) textured with a 1k RGBA atlas keyed out of the concept sheet.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propivyclump.py
Concept: concept/ivy-clump-sheet.png. Origin = ground centre, cards rise from z=0. Unreal: masked, two-sided material M_SB_Card (import_silverbrook.py).
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
import prop_common_sheet as sheet
import numpy as np,bpy
SHEET=HERE/'concept'/'ivy-clump-sheet.png';ATLAS=HERE/'PropIvyClump-atlas.png'
def make_atlas():
    """Key the flat grey background to alpha (soft edge), un-mix the grey from edge pixels, crop to the clump, 1024x1024 RGBA."""
    a=sheet.load_rgba(SHEET);bg=np.median(np.concatenate([a[:20,:,:3].reshape(-1,3),a[:,:6,:3].reshape(-1,3)]),axis=0)
    d=np.sqrt(((a[...,:3]-bg)**2).sum(-1));t=np.clip((d-.04)/(.14-.04),0,1);al=t*t*(3-2*t)
    rgb=np.where(al[...,None]>.01,(a[...,:3]-bg*(1-al[...,None]))/np.maximum(al[...,None],.01),a[...,:3]);rgb=np.clip(rgb,0,1)
    solid=al>.9;fill=rgb[solid].mean(axis=0);rgb[al<.05]=fill               # transparent texels carry leaf colour, so mips never bleed grey
    ys,xs=np.where(al>.5);y0,y1,x0,x1=max(ys.min()-6,0),ys.max()+7,max(xs.min()-6,0),xs.max()+7
    out=np.concatenate([rgb,al[...,None]],axis=-1)[y0:y1,x0:x1];sheet.save_png(sheet.resize(out,(1024,1024)),ATLAS)
    return dict(source_bbox_px=[int(x0),int(y0),int(x1),int(y1)],opaque_fraction=round(float((al>.5).mean()),3))
info=make_atlas()
scene=kit.reset(5305);asset=bpy.data.collections.new('SilverbrookPropIvyClump');scene.collection.children.link(asset)
M=dict(card=sheet.image_mat('SB_PropIvyClumpAtlas',ATLAS,.7))
G=Geo('SM_SBIvyClump_Cards',asset)
W,H,NX,NZ=1.2,1.0,6,5
for k in range(4):
    ang=math.pi/4*k;tx,ty=math.cos(ang),math.sin(ang);nx,ny=-ty,tx;lean=(.07 if k%2 else -.07)
    P=lambda i,j:Vector((tx*(i/NX-.5)*W+nx*lean*(j/NZ)**2,ty*(i/NX-.5)*W+ny*lean*(j/NZ)**2,H*j/NZ))
    for i in range(NX):
        for j in range(NZ):
            q=[P(i,j),P(i+1,j),P(i+1,j+1),P(i,j+1)];uv=[(i/NX,j/NZ),((i+1)/NX,j/NZ),((i+1)/NX,(j+1)/NZ),(i/NX,(j+1)/NZ)]
            if k%2:uv=[(1-u,v) for u,v in uv]
            G.poly(q,M['card'],uv,w=(0,0))
G.object()
sheet.finish(HERE,'PropIvyClump',scene,asset,dict(footprint_m=None,front='-Y',ground_contact=True,near=['cottage','wall','fence','ruin'],clearance_m=0.),[(1.0,1.4),(1.0,1.4),(.95,1.05)],'ivy-clump-sheet.png',
    'ground centre, cards rise from z=0',extra=dict(atlas=dict(file=ATLAS.name,size=[1024,1024],channels='RGBA (alpha = keyed grey background)',**info),
    note='masked two-sided material M_SB_Card; four cards at 0/45/90/135 degrees, odd cards mirrored in U'))
