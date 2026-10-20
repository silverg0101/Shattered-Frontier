"""Shared finishing + geometry helpers for the PropHayCart / PropHayBales / PropWaterTrough packages (kit untouched)."""
import math,json,hashlib
from pathlib import Path
import bpy
from mathutils import Vector,Matrix
import silverbrook_kit as kit

def oriented(G,pts,mat,want,**kw):
    """poly whose normal faces `want`"""
    pts=[Vector(p) for p in pts]
    if (pts[1]-pts[0]).cross(pts[2]-pts[0]).dot(want)<0:pts.reverse()
    G.poly(pts,mat,**kw)

def cyl_x(G,mat,c,r0,r1,x0,x1,seg=16,**kw):
    """Tube (r0>0) or solid cylinder (r0=0) along X from x0 to x1 about centre (y,z)=c[1:]."""
    cy,cz=c[1],c[2];pt=lambda r,a,x:(x,cy+r*math.cos(a),cz+r*math.sin(a))
    for j in range(seg):
        a,b=math.tau*j/seg,math.tau*(j+1)/seg;m=(a+b)/2;out=Vector((0,math.cos(m),math.sin(m)))
        oriented(G,[pt(r1,a,x0),pt(r1,b,x0),pt(r1,b,x1),pt(r1,a,x1)],mat,out,**kw)
        if r0>0:
            oriented(G,[pt(r0,a,x0),pt(r0,b,x0),pt(r0,b,x1),pt(r0,a,x1)],mat,-out,**kw)
            oriented(G,[pt(r0,a,x0),pt(r0,b,x0),pt(r1,b,x0),pt(r1,a,x0)],mat,(-1,0,0),**kw)
            oriented(G,[pt(r0,a,x1),pt(r0,b,x1),pt(r1,b,x1),pt(r1,a,x1)],mat,(1,0,0),**kw)
        else:
            oriented(G,[(x0,cy,cz),pt(r1,a,x0),pt(r1,b,x0)],mat,(-1,0,0),**kw);oriented(G,[(x1,cy,cz),pt(r1,a,x1),pt(r1,b,x1)],mat,(1,0,0),**kw)

def finish(here,name,G,slot,placement,budget,preview_ground=(.11,.12,.07),beauty=None,ortho=None,zc=None,weld=(),crop=None):
    """Recentre (xy to the middle, z to the ground), assert slot + triangle budget, export blend/fbx/glb, write manifest."""
    scene=bpy.context.scene;objs=[g.object() for g in G.values()]
    import bmesh
    for o in objs:  # weld + smooth-shade the organic meshes (hay, water)
        if o.name in weld:
            bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=bm.verts,dist=1e-5);bm.to_mesh(o.data);bm.free()
            for p in o.data.polygons:p.use_smooth=True
    lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
    for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2])))
    size=[h-l for l,h in zip(lo,hi)]
    assert all(h-l<=s+1e-3 and h-l>s-.1-1e-3 for l,h,s in zip(lo,hi,slot)),f'slot {slot} vs bounds {[round(v,3) for v in size]}'
    out=here/f'Silverbrook{name}'
    for o in bpy.data.objects:o.select_set(o in objs)
    bpy.context.view_layer.objects.active=objs[0]
    bpy.ops.export_scene.gltf(filepath=str(out)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
    bpy.ops.export_scene.fbx(filepath=str(out)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
    pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
    gm=bpy.data.meshes.new('Ground');gm.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',gm);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',preview_ground,.95))
    sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
    world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
    def cam(n,loc,rot,ortho=None,lens=40):
        c=bpy.data.cameras.new(n);o=bpy.data.objects.new(n,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
        if ortho:c.type='ORTHO';c.ortho_scale=ortho
        else:c.lens=lens
    d=max(size);zc=zc or size[2]/2;ortho=ortho or d*1.15
    cam('Beauty',beauty or (-d*1.0,-d*1.35,size[2]*.8+.4),(75,0,-36),lens=36);cam('Front',(0,-30,zc),(90,0,0),ortho);cam('Side',(-30,0,zc),(90,0,-90),ortho);cam('Rear',(0,30,zc),(90,0,180),ortho)
    scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
    bpy.ops.wm.save_as_mainfile(filepath=str(out)+'.blend',relative_remap=True)
    tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=budget,f'triangle budget: {tris}'
    SHEET=here.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Street Props.png';refp=here/f'{name}-reference.png'  # crop of the sheet, saved beside the build
    man=dict(asset=f'Silverbrook{name}',reference=dict(file=refp.name,sha256=hashlib.sha256(refp.read_bytes()).hexdigest(),source='docs/Game-Design/concept-art/Silverbrook/Street Props.png',crop_px=list(crop)),blender=bpy.app.version_string,generator=f'build_{name.lower()}.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
        units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in size],
        meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
        materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh=f'SM_Silverbrook{name}',slot_m=list(slot),placement=placement,
        outputs=[f'Silverbrook{name}.blend',f'Silverbrook{name}.glb',f'Silverbrook{name}.fbx'])
    (here/'asset-manifest.json').write_text(json.dumps(man,indent=2)+'\n');print(name.upper(),'BUILT',man['bounds_m'],tris)

REVIEW='''"""Fresh-process review: render the saved source, then reimport GLB/FBX into empty scenes.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup Silverbrook{n}.blend -P review_{l}.py
"""
import bpy,json
from pathlib import Path
from mathutils import Vector
HERE=Path(__file__).resolve().parent;scene=bpy.context.scene
for name in ['Beauty','Front','Side','Rear']:
    scene.camera=bpy.data.objects[name];scene.render.filepath=str(HERE/f'{n}-{{name}}.png');bpy.ops.render.render(write_still=True)
def summary(col=None):
    objs=[o for o in (col.objects if col else bpy.data.objects) if o.type=='MESH'];pts=[o.matrix_world@Vector(c) for o in objs for c in o.bound_box]
    return dict(meshes=len(objs),triangles=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs),
        size_m=[round(max(p[k] for p in pts)-min(p[k] for p in pts),3) for k in range(3)],
        materials=sorted({{m.name for o in objs for m in o.data.materials if m}}),colour_attributes=sorted({{a.name for o in objs for a in o.data.color_attributes}}),
        images_missing=sorted(i.name for i in bpy.data.images if i.source=='FILE' and not Path(bpy.path.abspath(i.filepath)).exists()),images=len([i for i in bpy.data.images if i.source=='FILE']))
report={{'source':summary(bpy.data.collections['Silverbrook{n}'])}}
for ext,op in [('glb',lambda f:bpy.ops.import_scene.gltf(filepath=f)),('fbx',lambda f:bpy.ops.import_scene.fbx(filepath=f))]:
    bpy.ops.wm.read_factory_settings(use_empty=True);op(str(HERE/f'Silverbrook{n}.{{ext}}'));report[ext]=summary()
(HERE/'exchange-review.json').write_text(json.dumps(report,indent=2)+'\\n');assert not report['fbx']['images_missing'] and report['fbx']['images'],'FBX texture references do not resolve';print('REVIEW',json.dumps({{k:(v['meshes'],v['triangles'],v['size_m']) for k,v in report.items()}}))
'''
def write_review(here,n):(here/f'review_{n.lower()}.py').write_text(REVIEW.format(n=n,l=n.lower()))
