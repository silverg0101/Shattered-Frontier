"""Shared helpers for the owner-concept street props (HitchingPost, RainBarrel, SackPile); wraps prop_common_wns.finalize."""
import math,json,hashlib,shutil
from pathlib import Path
import bpy
from silverbrook_kit import Vector,Matrix
import prop_common_wns as wns
CONCEPT=Path(__file__).resolve().parents[3]/'docs/Game-Design/concept-art/Silverbrook/Street Props.png'

def torus(geo,c,u,v,R,r,mat,seg=20,tube=8):
    """Ring of tube radius r around circle radius R centred c, circle plane spanned by u,v (unit); smooth quads, outward winding."""
    c,u,v=Vector(c),Vector(u),Vector(v);w=u.cross(v)
    P=lambda i,j:(lambda t,p:c+(u*math.cos(t)+v*math.sin(t))*(R+r*math.cos(p))+w*r*math.sin(p))(math.tau*i/seg,math.tau*j/tube)
    for i in range(seg):
        for j in range(tube):
            q=[P(i,j),P(i+1,j),P(i+1,j+1),P(i,j+1)];ctr=c+(u*math.cos(math.tau*(i+.5)/seg)+v*math.sin(math.tau*(i+.5)/seg))*R
            n=(q[1]-q[0]).cross(q[3]-q[0])
            if n.dot(sum(q,Vector())/4-ctr)<0:q.reverse()
            geo.poly(q,mat,smooth=True)

def slot_of(asset):
    pts=[o.matrix_world@Vector(c) for o in asset.objects if o.type=='MESH' for c in o.bound_box]
    return tuple(math.ceil(round((max(p[k] for p in pts)-min(p[k] for p in pts))*10,4))/10 for k in range(3))

def finish(here,pkg,scene,asset,placement,expect):
    """Assert real-world size, finalize with slot = bounds rounded up to 0.1 m, then record the owner concept sheet in the manifest."""
    slot=slot_of(asset);pts=[o.matrix_world@Vector(c) for o in asset.objects if o.type=='MESH' for c in o.bound_box]
    size=[max(p[k] for p in pts)-min(p[k] for p in pts) for k in range(3)]
    assert all(a<=s<=b for s,(a,b) in zip(size,expect)),f'size {size} outside {expect}'
    placement['footprint_m']=list(slot[:2]);wns.finalize(here,pkg,pkg,scene,asset,slot,placement)
    dst=here/f'{pkg}-reference.png';shutil.copyfile(CONCEPT,dst)
    mf=here/'asset-manifest.json';m=json.loads(mf.read_text())
    m['reference']=dict(file=dst.name,sha256=hashlib.sha256(dst.read_bytes()).hexdigest(),note='owner sheet Street Props.png, item: '+pkg[4:])
    m['method']='procedural original geometry (concept interpretation, not image reconstruction)';m.pop('note',None)
    mf.write_text(json.dumps(m,indent=2)+'\n')
