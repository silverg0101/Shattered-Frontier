"""Shared helpers for the sheet-driven Silverbrook props (ShopSign, WashingLine, Ladder, SillPlanter, IvyClump). New file; silverbrook_kit.py and prop_common_* untouched.

image_mat: PNG-texture material (masked in Unreal via M_SB_Card). crop_png: cut a panel from a concept sheet. finish: like prop_common_wns.finalize but the
model is built in final coordinates (origin = the documented mount point, no recentring) and the reference is the package's own concept sheet.
"""
import math,json,hashlib
from pathlib import Path
import bpy,numpy as np
import silverbrook_kit as kit
from silverbrook_kit import Vector,Matrix
from prop_common_street import slot_of

def load_rgba(path):
    i=bpy.data.images.load(str(path),check_existing=True);a=np.empty(len(i.pixels),np.float32);i.pixels.foreach_get(a)
    return a.reshape(i.size[1],i.size[0],4)[::-1].copy()   # row 0 = image top
def save_png(arr,path):
    h,w=arr.shape[:2];i=bpy.data.images.new(Path(path).stem+'_out',w,h,alpha=True);i.pixels.foreach_set(np.ascontiguousarray(arr[::-1],np.float32).ravel())
    i.filepath_raw=str(path);i.file_format='PNG';i.save();bpy.data.images.remove(i)
def resize(arr,size):
    h,w=arr.shape[:2];i=bpy.data.images.new('rs',w,h,alpha=True);i.pixels.foreach_set(np.ascontiguousarray(arr[::-1],np.float32).ravel());i.scale(*size)
    b=np.empty(size[0]*size[1]*4,np.float32);i.pixels.foreach_get(b);bpy.data.images.remove(i);return b.reshape(size[1],size[0],4)[::-1]
def crop_png(sheet,box,out,size):
    """box=(x0,y0,x1,y1) in sheet pixels from the top-left; saved opaque at `size`."""
    x0,y0,x1,y1=box;a=load_rgba(sheet)[y0:y1,x0:x1];a[...,3]=1;save_png(resize(a,size),out)

def image_mat(name,png,rough=.8,variants=None):
    """Principled material with PNG colour+alpha, two-sided. Manifest row {name,image,...}; `variants` {suffix: png name} become extra MIs in Unreal."""
    m=bpy.data.materials.new(name);m.use_nodes=True;m.use_backface_culling=False;nt=m.node_tree;p=nt.nodes.get('Principled BSDF')
    t=nt.nodes.new('ShaderNodeTexImage');t.image=bpy.data.images.load(str(png),check_existing=True);nt.links.new(t.outputs['Color'],p.inputs['Base Color']);nt.links.new(t.outputs['Alpha'],p.inputs['Alpha'])
    p.inputs['Roughness'].default_value=rough;kit.MATS[name]=m;kit.SCALE[name]=1;kit.AGED[name]=0
    kit.ROWS.append(dict(name=name,image=Path(png).name,roughness=rough,**({'variants':variants} if variants else {})));return m
def add_variants(name,variants):
    """Colour variants of a kit.solid row: {suffix: [r,g,b]} -> MI_<name>_<suffix> in Unreal."""
    next(r for r in kit.ROWS if r['name']==name)['variants']=variants

def finish(here,pkg,scene,asset,placement,expect,sheet,pivot,extra=None,ground=True,wall=False):
    name=f'Silverbrook{pkg}';objs=[o for o in asset.objects if o.type=='MESH']
    for o in objs:o.data.transform(o.matrix_world);o.matrix_world=Matrix()
    pts=[Vector(c) for o in objs for c in o.bound_box];lo=[min(p[k] for p in pts) for k in range(3)];hi=[max(p[k] for p in pts) for k in range(3)];size=[h-l for l,h in zip(lo,hi)]
    assert all(a<=s<=b for s,(a,b) in zip(size,expect)),f'size {size} outside {expect}'
    slot=slot_of(asset);placement['footprint_m']=list(slot[:2]);out=here/name
    for o in bpy.data.objects:o.select_set(o in objs)
    bpy.context.view_layer.objects.active=objs[0]
    bpy.ops.export_scene.gltf(filepath=str(out)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
    bpy.ops.export_scene.fbx(filepath=str(out)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
    pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre);big=max(size);ctr=Vector([(l+h)/2 for l,h in zip(lo,hi)])
    def plane(n,pts4,col):
        me=bpy.data.meshes.new(n);me.from_pydata(pts4,[],[(0,1,2,3)]);o=bpy.data.objects.new(n,me);pre.objects.link(o);o.data.materials.append(kit.solid('Preview_'+n,col,.95))
    if ground:plane('Ground',[(-30,-30,-.002),(30,-30,-.002),(30,30,-.002),(-30,30,-.002)],(.11,.12,.07))
    if wall:plane('Wall',[(-30,.003,-30),(30,.003,-30),(30,.003,30),(-30,.003,30)],(.3,.27,.22))
    sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
    world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
    tgt=bpy.data.objects.new('Target',None);tgt.location=ctr;pre.objects.link(tgt)
    def cam(n,off,ortho=None,lens=34):
        c=bpy.data.cameras.new(n);o=bpy.data.objects.new(n,c);o.location=ctr+Vector(off);pre.objects.link(o)
        t=o.constraints.new('TRACK_TO');t.target=tgt;t.track_axis='TRACK_NEGATIVE_Z';t.up_axis='UP_Y'
        if ortho:c.type='ORTHO';c.ortho_scale=ortho
        else:c.lens=lens
    d=big*1.35;cam('Beauty',(-d*.7,-d*.9,size[2]*.25),lens=34);cam('Front',(0,-40,0),big*1.3);cam('Side',(-40,0,0),big*1.3);cam('Rear',(0,40,0),big*1.3)
    scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
    bpy.ops.wm.save_as_mainfile(filepath=str(out)+'.blend',relative_remap=True)
    tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=8000,f'prop budget: {tris} triangles'
    ref=here/'concept'/sheet
    manifest=dict(asset=name,asset_id=pkg,variant=1,reference=dict(file=f'concept/{sheet}',sha256=hashlib.sha256(ref.read_bytes()).hexdigest(),note=f'Codex concept sheet for {pkg}'),blender=bpy.app.version_string,generator=f'build_{pkg.lower()}.py',
        method='procedural original geometry (concept interpretation, not image reconstruction)',units=f'metres, Z up, front -Y, origin {pivot}',bounds_m=[round(v,3) for v in size],
        meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
        materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans); PNG textures cut from the concept sheet',unreal_mesh=f'SM_{name}',slot_m=list(slot),placement=placement,
        outputs=[f'{name}.blend',f'{name}.glb',f'{name}.fbx'],**(extra or {}))
    (here/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(pkg.upper()+'_BUILT',manifest['bounds_m'],tris)
