"""Builds a four-variant rock family (boulders, bedrock slabs) from CC0 Poly Haven scan meshes. Used by BoulderLargeMoss/ and BedrockSlab/.

Per variant: load the scan glTF -> optional levelling of its dominant face plane (slabs) -> mirror/yaw/scale -> optional organic outline crop with a rim that
sinks into the soil (slabs) -> decimate to the family budget -> origin at base centre -> moss weight in vertex colour R -> FBX + manifest row.
The scan's own UVs, normal map and roughness are kept; the colour is re-graded to grey granite in the material and moss is blended in by vertex colour.
"""
import bpy,json,math,sys
import numpy as np
from pathlib import Path
from mathutils import Vector
import nature_kit as K


def smooth(x):
    x=np.clip(x,0,1);return x*x*(3-2*x)


def level(v,tilt_deg=0.0):
    """Rotate so the least-squares plane of the (-y facing) scan face lies horizontal with the face pointing +z; optional extra tilt about x."""
    A=np.c_[v[:,0],v[:,2],np.ones(len(v))];c=np.linalg.lstsq(A,v[:,1],rcond=None)[0]
    n=Vector((c[0],-1,c[1])).normalized();R=n.rotation_difference(Vector((0,0,1))).to_matrix()
    R=np.array(R);v=v@R.T
    if tilt_deg:
        t=math.radians(tilt_deg);Rx=np.array([[1,0,0],[0,math.cos(t),-math.sin(t)],[0,math.sin(t),math.cos(t)]]);v=v@Rx.T
    return v


def outline_crop(v,t,uv,mi,half,seed,rim=.30,sink=.45):
    """Keep the region inside an irregular polygon outline (half-extents half=(a,b) metres); the outer `rim` fraction slopes down by `sink` m into the soil."""
    rng=np.random.default_rng(seed);k=rng.integers(7,10);ang=np.sort(rng.uniform(0,2*np.pi,k));rad=rng.uniform(.82,1.15,k)
    th=np.arctan2(v[:,1]/half[1],v[:,0]/half[0]);rr=np.hypot(v[:,0]/half[0],v[:,1]/half[1])
    ang2=np.r_[ang-2*np.pi,ang,ang+2*np.pi];rad2=np.r_[rad,rad,rad];edge=np.interp(th,ang2,rad2)*(1+.05*np.sin(7*th+rng.uniform(0,6)))
    rho=rr/edge;keep=rho<1.0;tk=keep[t].all(1)
    t=t[tk];uv=uv[tk];mi=mi[tk];used=np.unique(t);remap=-np.ones(len(v),np.int64);remap[used]=np.arange(len(used));v=v[used].copy();rho=rho[used];t=remap[t]
    floor=np.percentile(v[:,2],3)-sink;w=smooth((rho-(1-rim))/rim)**1.4;v[:,2]=v[:,2]*(1-w)+floor*w
    return v,t,uv,mi


def transform(v,t,uv,mi,scale,mirror,yaw,flatten=None,tilt=0.0):
    if flatten:v=level(v,tilt)
    if mirror:v=v*np.array([-1,1,1],np.float32);t=t[:,::-1];uv=uv[:,::-1]
    c,s=math.cos(math.radians(yaw)),math.sin(math.radians(yaw));v=v@np.array([[c,-s,0],[s,c,0],[0,0,1]],np.float32).T
    sc=np.array(scale if hasattr(scale,'__len__') else [scale]*3,np.float32);v=v*sc
    if (sc<0).sum()%2:t=t[:,::-1];uv=uv[:,::-1]
    return v.astype(np.float32),t,uv,mi


def scan_spec(name,src,tint,sat,normal_strength=1.0):
    d=f'Sources/{src}/textures/{src}'
    return dict(name=name,base=d+'_diff_2k.jpg',normal=d+'_nor_gl_2k.jpg',arm=d+'_arm_2k.jpg',tint=list(tint),sat=sat,normal_strength=normal_strength)


def build(cfg):
    K.reset();K.render_setup((2000,900),64);K.add_ground()
    objs=[];rows=[];mats=[];srcs=[]
    for i,vc in enumerate(cfg['variants']):
        loaded=K.load_gltf(vc['source']);o=max(loaded.values(),key=lambda x:len(x.data.polygons))
        for other in loaded.values():
            if other is not o:bpy.data.objects.remove(other,do_unlink=True)
        v,t,uv,mi=K.arrays(o);bpy.data.objects.remove(o,do_unlink=True)
        v,t,uv,mi=transform(v,t,uv,mi,vc['scale'],vc.get('mirror',False),vc.get('yaw',0),vc.get('flatten'),vc.get('tilt',0))
        if vc.get('crop'):
            v=v-np.array([(v[:,0].min()+v[:,0].max())/2+vc.get('crop_dx',0),(v[:,1].min()+v[:,1].max())/2+vc.get('crop_dy',0),0],np.float32)
            v,t,uv,mi=outline_crop(v,t,uv,mi,vc['crop'],vc.get('seed',i))
        spec=scan_spec(f"MI_{cfg['prefix'][3:]}_{i:02d}",vc['source'],vc['tint'],vc['sat'],vc.get('normal_strength',1.0));spec['blend']=dict(cfg['moss'],**vc.get('moss_spec',{}))
        mat=K.build_material(spec);name=f"{cfg['prefix']}_{i:02d}"
        o=K.make(name,v,t,uv,np.zeros(len(t),np.int32),[mat]);K.decimate(o,cfg['budget']);K.to_base_centre(o)
        m=K.moss_mask(o,seed=100+i,**vc.get('moss',cfg['moss_mask']));mats.append(spec);srcs.append(vc['source'])
        objs.append(o);rows.append(dict(mesh=name,source=vc['source'],scale=vc['scale'],mirror=vc.get('mirror',False),yaw=vc.get('yaw',0),levelled_plane=bool(vc.get('flatten')),
                                        outline_half_m=vc.get('crop'),material=spec['name'],moss_mean=round(m,3)))
    finish(cfg,objs,rows,mats,srcs)


def finish(cfg,objs,rows,mats,srcs):
    out=Path(cfg['dir']);x=0;gap=cfg.get('gap',1.0)
    for o in objs:
        lo,hi=K.verts(o).min(0),K.verts(o).max(0);w=hi[0]-lo[0];o.location.x=x+w/2;x+=w+gap
    for o,r in zip(objs,rows):
        ux=[bpy.data.objects[n] for n in o.get('ucx',[])]
        for q in [o]+ux:q.location=(0,0,0)
        K.export_fbx(o,out/(r['mesh']+'.fbx'),ux);v=K.verts(o);lo,hi=v.min(0),v.max(0)
        r.update(triangles=K.tricount(o),vertices=len(v),size_m=[round(float(hi[i]-lo[i]),3) for i in range(3)],fbx=r['mesh']+'.fbx',fbx_sha256=K.sha256(out/(r['mesh']+'.fbx')),
                 origin='base centre (footprint centre, lowest point z=0)',collision_ucx=list(o.get('ucx',[])))
    x=0
    for o in objs:
        lo,hi=K.verts(o).min(0),K.verts(o).max(0);w=hi[0]-lo[0];o.location.x=x+w/2;x+=w+gap
        for n in o.get('ucx',[]):bpy.data.objects[n].location.x=o.location.x
    total=x-gap;cx=(total-1.6)/2;width=total+2.0
    K.add_human(cfg.get('human_x',-1.0),cfg.get('human_y',0.6))
    sc=bpy.context.scene;sc.render.resolution_x,sc.render.resolution_y=cfg.get('res',(2000,800))
    if cfg.get('ortho'):  # tall subjects: orthographic front view, height = cfg['ortho'] metres of frame
        H=cfg['ortho'];K.camera((cx,-300,H/2-1),(cx,0,H/2-1),ortho=max(width,H*sc.render.resolution_x/sc.render.resolution_y));K.render(out/(cfg['family']+'-Variants.png'))
    else:
        K.frame(cx,width,look_z=cfg.get('look_z',.7),lens=60,elev_deg=12);K.render(out/(cfg['family']+'-Variants.png'))
    names=[cfg['family']+'-Variants.png']
    for nm,idx,dist,cz,tz,lens in cfg.get('detail',[]):
        ox=objs[idx].location.x;K.camera((ox,-dist,cz),(ox,0,tz),lens=lens);K.render(out/(cfg['family']+'-'+nm+'.png'));names.append(cfg['family']+'-'+nm+'.png')
    if not cfg.get('detail') and not cfg.get('ortho'):
        K.frame(cx,width,look_z=.3,lens=60,elev_deg=50,side=1);K.render(out/(cfg['family']+'-Variants-Top.png'));names.append(cfg['family']+'-Variants-Top.png')
    bpy.ops.wm.save_as_mainfile(filepath=str(out/(cfg['family']+'.blend')))
    tri=[r['triangles'] for r in rows]
    man=dict(family=cfg['family'],asset_id=cfg['asset_id'],look_profile_role=cfg['role'],variant_count=len(rows),blender=bpy.app.version_string,generator='build_'+cfg['family'].lower()+'.py',
             units='metres, Z up, front -Y, origin base centre, no ground in any mesh',budget_triangles_per_variant=cfg['budget'],triangles=tri,
             placement_scale_note=cfg['scale_note'],unreal_meshes=[r['mesh'] for r in rows],nanite=cfg.get('nanite',True),materials=mats,variants=rows,
             sources=K.provenance(sorted(set(srcs)|set(cfg.get('extra_sources',['mossy_rock'])))),renders=names,
             concept_reference='docs/Game-Design/concept-art/Greenlands-Forest-Look-v1.webp')
    (out/'asset-manifest.json').write_text(json.dumps(man,indent=2)+'\n');print('FAMILY',cfg['family'],'tris',tri,'sizes',[r['size_m'] for r in rows])
