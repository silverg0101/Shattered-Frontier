"""Shared helpers for the PropWell / PropNoticeBoard / PropSignpost builders (new file; silverbrook_kit.py untouched)."""
import math,json,hashlib,shutil
from pathlib import Path
import bpy
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix

def revolve(geo,origin,axis,profile,mat,seg=8,phase=0,smooth=False):
    """Convex solid of revolution about `axis` from (radius, distance) rings, faces wound outward from the centroid."""
    o,ax=Vector(origin),Vector(axis).normalized();u=ax.orthogonal().normalized();v=ax.cross(u)
    rings=[[o+ax*d+(u*math.cos(phase+math.tau*j/seg)+v*math.sin(phase+math.tau*j/seg))*r for j in range(seg)] for r,d in profile]
    faces=[list(reversed(rings[0]))] if profile[0][0]>0 else []
    if profile[-1][0]>0:faces.append(rings[-1])
    for (ra,_),(rb,_),A,B in zip(profile,profile[1:],rings,rings[1:]):
        for j in range(seg):
            k=(j+1)%seg;f=[A[j],A[k],B[k],B[j]]
            if ra==0:f=[A[j],B[k],B[j]]
            elif rb==0:f=[A[j],A[k],B[j]]
            faces.append(f)
    c=sum((p for r in rings for p in r),Vector())/(len(rings)*seg);vol=0
    for f in faces:
        n=sum((a.cross(f[(i+1)%len(f)]) for i,a in enumerate(f)),Vector())
        if n.dot(sum(f,Vector())/len(f)-c)<0:f.reverse()
        geo.poly(f,mat,smooth=smooth);vol+=sum((f[0]-c).dot((f[i]-c).cross(f[i+1]-c)) for i in range(1,len(f)-1))
    assert vol>0,'inside-out revolve'

def finalize(here,pkg,asset_id,scene,asset,slot,placement,cam_h=None):
    """Bake object transforms, recentre on the ground, assert slot fit, export, build preview scene, write manifest."""
    name=f'Silverbrook{pkg}';objs=[o for o in asset.objects if o.type=='MESH']
    for o in objs:o.data.transform(o.matrix_world);o.matrix_world=Matrix()
    pts=[Vector(c) for o in objs for c in o.bound_box]
    lo=[min(p[k] for p in pts) for k in range(3)];hi=[max(p[k] for p in pts) for k in range(3)]
    shift=Vector((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2]))
    for o in objs:o.data.transform(Matrix.Translation(shift))
    size=[h-l for l,h in zip(lo,hi)]
    assert all(s<=t+1e-6 for s,t in zip(size,slot)),f'exceeds slot {slot}: {[round(s,3) for s in size]}'
    assert all(t-s<.1+1e-6 for s,t in zip(size,slot)),f'slot {slot} not rounded to bounds {[round(s,3) for s in size]}'
    out=here/name
    REF=here.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Street Props.png';shutil.copy2(REF,here/f'{pkg}-reference.png')
    for o in bpy.data.objects:o.select_set(o in objs)
    bpy.context.view_layer.objects.active=objs[0]
    bpy.ops.export_scene.gltf(filepath=str(out)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
    bpy.ops.export_scene.fbx(filepath=str(out)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
    pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
    gm=bpy.data.meshes.new('Ground');gm.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',gm);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
    sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
    world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
    H=size[2];tgt=bpy.data.objects.new('Target',None);tgt.location=(0,0,H*.5);pre.objects.link(tgt);big=max(size)
    def cam(n,loc,ortho=None,lens=34):
        c=bpy.data.cameras.new(n);o=bpy.data.objects.new(n,c);o.location=loc;pre.objects.link(o)
        t=o.constraints.new('TRACK_TO');t.target=tgt;t.track_axis='TRACK_NEGATIVE_Z';t.up_axis='UP_Y'
        if ortho:c.type='ORTHO';c.ortho_scale=ortho
        else:c.lens=lens
    d=big*1.35
    cam('Beauty',(-d*.7,-d*.9,H*.75),lens=34);cam('Front',(0,-40,H/2),big*1.3);cam('Side',(-40,0,H/2),big*1.3);cam('Rear',(0,40,H/2),big*1.3)
    scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
    bpy.ops.wm.save_as_mainfile(filepath=str(out)+'.blend',relative_remap=True)
    tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=40000,f'small-prop budget: {tris} triangles'
    manifest=dict(asset=name,asset_id=asset_id,variant=1,reference=dict(file=f'{pkg}-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest(),note='whole Street Props concept sheet; this prop is one item'),blender=bpy.app.version_string,generator=f'build_{pkg.lower()}.py',
        method='procedural original geometry interpreting the Street Props concept sheet',units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in size],
        meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
        materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans and models)',unreal_mesh=f'SM_{name}',slot_m=list(slot),placement=placement,
        outputs=[f'{name}.blend',f'{name}.glb',f'{name}.fbx'])
    (here/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(pkg.upper()+'_BUILT',manifest['bounds_m'],tris)
