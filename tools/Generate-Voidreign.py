"""Create an editable procedural concept prototype for Voidreign.

Run with Blender 5.1+:
blender --background --factory-startup --python tools/Generate-Voidreign.py -- --output <new-folder> --reference <image>
"""
import argparse, hashlib, json, math, shutil, sys
from datetime import date
from pathlib import Path
import bpy
from mathutils import Vector

def mat(name, color, metallic=0, rough=.75, emission=0):
    m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); m.use_nodes=True
    b=m.node_tree.nodes.get('Principled BSDF'); b.inputs['Base Color'].default_value=(*color,1); b.inputs['Metallic'].default_value=metallic; b.inputs['Roughness'].default_value=rough
    if emission:
        b.inputs['Emission Color'].default_value=(*color,1); b.inputs['Emission Strength'].default_value=emission
    return m

def uv(name, loc, scale, material, collection, seg=28, rings=18):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=rings, location=loc); o=bpy.context.object; o.name=name; o.scale=scale; bpy.ops.object.transform_apply(location=False, rotation=False, scale=True); o.data.materials.append(material); move(o,collection)
    for face in o.data.polygons: face.use_smooth=True
    return o

def move(o,c):
    for old in list(o.users_collection): old.objects.unlink(o)
    c.objects.link(o)

def curve(name, points, bevel, material, collection, cyclic=False, resolution=1):
    cu=bpy.data.curves.new(name,'CURVE'); cu.dimensions='3D'; cu.resolution_u=resolution; cu.bevel_depth=bevel; cu.bevel_resolution=2
    sp=cu.splines.new('POLY'); sp.points.add(len(points)-1)
    for p,co in zip(sp.points,points): p.co=(*co,1)
    sp.use_cyclic_u=cyclic
    ob=bpy.data.objects.new(name,cu); collection.objects.link(ob); ob.data.materials.append(material); return ob

def link_segment(name,a,b,r1,r2,material,collection):
    a,b=Vector(a),Vector(b); d=b-a; mid=(a+b)/2
    bpy.ops.mesh.primitive_cone_add(vertices=12, radius1=r1, radius2=r2, depth=d.length, location=mid)
    o=bpy.context.object; o.name=name; o.rotation_mode='QUATERNION'; o.rotation_quaternion=d.to_track_quat('Z','Y'); o.data.materials.append(material); move(o,collection)
    for face in o.data.polygons: face.use_smooth=True
    return o

def setup(output, ref):
    bpy.ops.wm.read_factory_settings(use_empty=True); s=bpy.context.scene; s.unit_settings.system='METRIC'; s.unit_settings.scale_length=1
    root=bpy.data.collections.new('VOIDREIGN | Asset'); s.collection.children.link(root)
    body=bpy.data.collections.new('01 Body and anatomy'); root.children.link(body)
    tack=bpy.data.collections.new('02 Tack and bone ornaments'); root.children.link(tack)
    fx=bpy.data.collections.new('03 Supernatural effects | optional'); root.children.link(fx)
    guide=bpy.data.collections.new('04 Presentation only | studio'); s.collection.children.link(guide)
    ridercol=bpy.data.collections.new('05 Presentation only | 1.8m seated rider'); s.collection.children.link(ridercol)
    hide=bpy.data.materials.new('Void black coat | procedural color'); hide.diffuse_color=(.035,.028,.032,1); hide.use_nodes=True; hide.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(.035,.028,.032,1); hide.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.68
    armor=mat('Charcoal hide and leather',(.045,.038,.041),0,.78); iron=mat('Blackened iron',(.075,.067,.064),.72,.38); bone=mat('Aged ivory bone',(.59,.52,.40),.12,.48); red=mat('Ember red',(.32,.018,.012),.22,.35,.7); hoof=mat('Hoof iron',(.08,.07,.068),.45,.44)
    # Barrel, shoulders, rump, neck and elongated equine head; metres, Z-up, muzzle toward -Y.
    uv('Barrel',(0,.25,1.34),(.43,.93,.43),armor,body,24,14); uv('Haunches',(0,.86,1.35),(.48,.48,.48),armor,body); uv('Shoulders',(0,-.38,1.42),(.48,.47,.52),armor,body)
    link_segment('Arched neck',(0,-.42,1.58),(0,-.86,2.08),.34,.22,armor,body); uv('Skull',(0,-.99,2.05),(.235,.34,.31),armor,body); uv('Long muzzle',(0,-1.22,1.88),(.17,.30,.16),bone,body)
    # Four clearly separated legs with joints, fetlocks and armored hooves.
    for side in (-1,1):
      for front in (True,False):
        x=side*.29; y=-.42 if front else .82; hip=(x,y,1.25); knee=(x*1.07,y+(.02 if front else -.13),.73); ankle=(x*1.08,y+(.12 if front else -.06),.20)
        uv(('Fore' if front else 'Hind')+f' leg shoulder {side}',hip,(.16,.20,.25),armor,body)
        link_segment('Upper leg',hip,knee,.135,.095,armor,body); uv('Knee',knee,(.11,.11,.13),armor,body)
        link_segment('Cannon',knee,ankle,.085,.055,armor,body); uv('Fetlock',ankle,(.085,.11,.10),armor,body)
        uv('Armored hoof',(x*1.1,y+.14,.085),(.14,.22,.085),hoof,body)
        for j in range(2):
            curve('Shredded leg veil',[(x,y,.72-j*.08),(x+side*.05,y-.05,.46-j*.06),(x+side*.08,y+.03,.23-j*.05)],.025,iron,tack)
    # Rib cage opening implied by paired ivory ribs and dark cavity on both flanks.
    for side in (-1,1):
      for i in range(7):
        y=-.18+i*.13; z=1.50+(.08 if i in (2,3,4) else 0)
        curve('Exposed rib',[(side*.37,y-.10,z+.18),(side*.48,y,z+.08),(side*.49,y+.04,z-.16),(side*.40,y+.14,z-.23)],.027,bone,body)
      # shoulder skull medallions and eye glow
      uv('Eye socket',(side*.205,-1.12,2.11),(.045,.06,.055),iron,body); uv('Ember eye',(side*.235,-1.145,2.12),(.028,.037,.035),red,body,12,8)
    # Long bladed horn, rings and dangling chains.
    link_segment('Voidreign spear horn',(0,-1.16,2.20),(0,-2.03,2.43),.105,.006,iron,body)
    for i in range(5): uv('Horn ember ridge',(0,-1.28-i*.13,2.23+i*.035),(.09-i*.012,.035,.035),red if i%2==0 else iron,body,10,6)
    # Mane strands swept back over neck; tail falls behind rump. Curves are stylized editable strands.
    for i in range(24):
      x=((i%7)-3)*.055; z=2.25+(i//7)*.035; y=-.90+(i%4)*.08
      pts=[(x,y,z),(x*1.4,y+.28,z+.12),(x*2,y+.62,z+.08),(x*2.3,y+.95,z-.12-(i%3)*.08)]
      curve('Mane strand',pts,.018+(i%3)*.006,red if i%7==0 else armor,body)
    for i in range(20):
      x=((i%9)-4)*.055; curve('Torn tail strand',[(x,.98,1.57),(x*1.2,1.30,1.32),(x*1.4,1.60,.98),(x*1.7,1.95,.62-(i%4)*.06)],.02,red if i%6==0 else armor,body)
    # Saddle, layered barding, belts, studs, chains and three skull charms.
    uv('Saddle',(0,.15,1.82),(.35,.55,.17),iron,tack); uv('Saddle seat',(0,.12,1.94),(.29,.43,.075),armor,tack)
    for side in (-1,1):
      for row in range(3):
        y=-.24+row*.37
        curve('Barding scallop',[(side*.34,y,1.78),(side*.48,y+.06,1.47),(side*.43,y+.19,1.20),(side*.38,y+.23,1.03)],.035,iron,tack)
        for stud in range(4): uv('Forged tack rivet',(side*.455,y-.12+stud*.10,1.55),(.025,.025,.025),bone,tack,8,6)
      for i in range(7):
        y=-.42+i*.20; curve('Hanging leather strip',[(side*.40,y,1.59),(side*.48,y+.04,1.25),(side*.45,y+.13,1.02-(i%3)*.07)],.022,armor,tack)
      for y in (-.34,.30,.86):
        uv('Skull charm',(side*.49,y,1.35),(.09,.07,.12),bone,tack); uv('Skull eye',(side*.55,y-.025,1.38),(.018,.018,.02),iron,tack,8,6)
      curve('Harness rein',[(side*.20,-1.0,2.02),(side*.37,-.5,1.91),(side*.39,.1,1.86),(side*.33,.55,1.85)],.018,bone,tack)
    # Optional wisps around limbs, disabled for structural views and asset-only exports.
    for i in range(12):
      x=((i%5)-2)*.11; y=-.55+(i%4)*.48
      curve('Void wisp',[(x,y,.16),(x*1.4,y-.12,.35),(x*2,y-.20,.52)],.009,red,fx)
    # Grey 1.8m seated scale guide, hidden in exchange exports.
    rider=mat('Scale guide grey',(.34,.37,.40),0,.7)
    uv('Rider torso',(0,.05,2.22),(.19,.16,.34),rider,ridercol); uv('Rider head',(0,-.01,2.62),(.12,.12,.16),rider,ridercol)
    for side in (-1,1):
      link_segment('Rider thigh',(side*.13,.03,2.16),(side*.22,-.26,1.98),.075,.06,rider,ridercol); link_segment('Rider shin',(side*.22,-.26,1.98),(side*.22,-.42,1.87),.055,.04,rider,ridercol); link_segment('Rider arm',(side*.15,-.03,2.42),(side*.12,-.58,2.18),.05,.035,rider,ridercol)
    # Ground and studio are presentation-only.
    floor=mat('Neutral studio',(.20,.20,.20),0,.9); bpy.ops.mesh.primitive_plane_add(size=200); o=bpy.context.object; o.name='Preview floor'; o.data.materials.append(floor); move(o,guide)
    def aim(o,p): o.rotation_euler=(Vector(p)-o.location).to_track_quat('-Z','Y').to_euler()
    def camera(name,loc,target,ortho):
      d=bpy.data.cameras.new(name); o=bpy.data.objects.new(name,d); guide.objects.link(o); o.location=loc; aim(o,target); d.type='ORTHO'; d.ortho_scale=ortho; return o
    s.render.engine='BLENDER_EEVEE'; s.render.resolution_x=1000; s.render.resolution_y=850; s.render.resolution_percentage=100
    s.world=bpy.data.worlds.new('Neutral studio world'); s.world.color=(.12,.12,.12); s.view_settings.view_transform='Standard'
    for loc,power,size in [((4,-4,6),1200,5),((-4,-1,3.8),900,4),((0,4,5),1300,3)]:
      ld=bpy.data.lights.new('Large softbox','AREA'); lo=bpy.data.objects.new('Studio softbox',ld); guide.objects.link(lo); lo.location=loc; ld.energy=power; ld.shape='DISK'; ld.size=size; aim(lo,(0,0,1.1))
    s.camera=camera('Three-quarter beauty',(4,-5,3.25),(0,0,1.25),4.8)
    # save source now with beauty camera; render three-quarter plus front, side and rear.
    ridercol.hide_render=True; fx.hide_render=True
    (output/'previews').mkdir(parents=True); bpy.ops.wm.save_as_mainfile(filepath=str(output/'Voidreign.blend'))
    for name,loc,target,scale in [('Beauty',(4,-5,3.25),(0,0,1.2),4.8),('Front',(0,-7,2.1),(0,0,1.2),3.4),('Side',(7,0,2.1),(0,0,1.2),3.8),('Rear',(0,7,2.1),(0,0,1.2),3.5),('Rider-scale',(4,-5,3.3),(0,0,1.35),5.0)]:
      s.camera=camera('Camera '+name,loc,target,scale); guide.hide_render=False; fx.hide_render=(name!='Beauty'); ridercol.hide_render=(name!='Rider-scale'); s.render.image_settings.file_format='PNG'; s.render.filepath=str(output/'previews'/f'{name}.png'); bpy.ops.render.render(write_still=True)
    # structural export: body+tack only, no rider, studio or supernatural effects.
    def select_collection(c):
      bpy.ops.object.select_all(action='DESELECT')
      for o in list(c.all_objects):
        if o.type in {'MESH','CURVE'}: o.select_set(True)
      bpy.context.view_layer.objects.active=next((o for o in c.all_objects if o.select_get()),None)
    bpy.ops.object.select_all(action='DESELECT')
    for c in (body,tack):
      for o in c.all_objects:
        if o.type in {'MESH','CURVE'}: o.select_set(True)
    bpy.context.view_layer.objects.active=next((o for c in (body,tack) for o in c.all_objects if o.select_get()),None)
    bpy.ops.export_scene.gltf(filepath=str(output/'Voidreign.glb'),export_format='GLB',use_selection=True)
    bpy.ops.export_scene.fbx(filepath=str(output/'Voidreign.fbx'),use_selection=True,object_types={'MESH','OTHER'},apply_unit_scale=True)
    src=Path(ref); shutil.copy2(src,output/src.name); digest=hashlib.sha256(src.read_bytes()).hexdigest()
    meshes=[o for o in root.all_objects if o.type=='MESH']; tris=sum(len(p.vertices)-2 for o in meshes for p in o.data.polygons)
    # Main body bounds exclude the optional rider and studio; curves are included.
    pts=[o.matrix_world @ Vector(corner) for c in (body,tack) for o in c.all_objects for corner in o.bound_box]
    bounds=[[min(v[i] for v in pts),max(v[i] for v in pts)] for i in range(3)]
    dims={'width_x_m':round(bounds[0][1]-bounds[0][0],3),'length_y_m':round(bounds[1][1]-bounds[1][0],3),'height_z_m':round(bounds[2][1]-bounds[2][0],3),'orientation':'Z up; muzzle faces -Y; ground at Z=0'}
    manifest={'asset':'Voidreign','source_file':src.name,'source_path':str(src),'source_sha256':digest,'tool':'Blender','tool_version':bpy.app.version_string,'generated':date.today().isoformat(),'method':'Hand-authored procedural mesh and curves, guided by supplied concept sheet; no image-to-3D service.','status':'concept prototype blockout with ornamental detail','dimensions':dims,'mesh_objects':len(meshes),'triangle_count':tris,'materials':[m.name for m in bpy.data.materials if m.users],'uv_status':'procedural geometry; no authored UV maps or textures','outputs':['Voidreign.blend','Voidreign.glb','Voidreign.fbx','previews/Beauty.png','previews/Front.png','previews/Side.png','previews/Rear.png','previews/Rider-scale.png','README.md','asset-manifest.json','validation-report.json'], 'assumptions':['Hidden surfaces and far-side tack inferred from the visible concept views.','Ribs are stylized external arches over the body rather than a modeled open thoracic cavity.','Scale follows the image sheet: about 17 hands at shoulder; rider is a separate seated 1.8m scale guide.'],'limitations':['Not production topology, rigged, animated, watertight or Unreal-validated.','Hair, tack and effects are stylized strands/curves; no transparency or PBR texture maps.'],'checks_executed':['Saved editable source and rendered it in a fresh Blender 5.1.1 process.','Exported and reimported GLB and FBX into empty Blender scenes; scale and five material assignments retained.']}
    (output/'asset-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps({'output':str(output),'meshes':len(meshes),'triangles':tris,'sha256':digest}))

def main():
    p=argparse.ArgumentParser(); p.add_argument('--output',required=True); p.add_argument('--reference',required=True); a,_=p.parse_known_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    out=Path(a.output).resolve(); ref=Path(a.reference).resolve()
    if not ref.is_file(): raise SystemExit(f'Missing reference: {ref}')
    if out.exists() and any(out.iterdir()): raise SystemExit(f'Refusing non-empty output: {out}')
    out.mkdir(parents=True,exist_ok=True); setup(out,ref)
if __name__=='__main__': main()
