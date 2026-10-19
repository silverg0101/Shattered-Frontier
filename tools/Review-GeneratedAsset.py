"""Import a generated GLB into Blender, normalize scale and create review views."""
import argparse,json,math,sys
from pathlib import Path
import bpy
from mathutils import Vector

p=argparse.ArgumentParser()
p.add_argument('--input',required=True);p.add_argument('--output',required=True)
p.add_argument('--reference',required=True);p.add_argument('--yaw',type=float,default=0)
p.add_argument('--asset-name',default='Voidreign',help='Asset label and output basename')
p.add_argument('--pitch',type=float,default=0)
p.add_argument('--roll',type=float,default=0)
p.add_argument('--height',type=float,help='Optional reviewed target height in metres; omitted keeps native normalized scale')
p.add_argument('--geometry-only',action='store_true',help='Preserve untextured geometry; no UV/texture claim')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
if not a.asset_name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for c in a.asset_name):
    raise ValueError('--asset-name must contain only letters, numbers, underscore or hyphen')
# Keep the historical Voidreign benchmark default while leaving other assets unscaled.
target_height=a.height if a.height is not None else (2.6 if a.asset_name=='Voidreign' else None)
out=Path(a.output).resolve()
if out.exists() and any(out.iterdir()): raise RuntimeError('Choose an empty review folder')
out.mkdir(parents=True,exist_ok=True);(out/'previews').mkdir();(out/'textures').mkdir()
bpy.ops.wm.read_factory_settings(use_empty=True)
s=bpy.context.scene;s.unit_settings.system='METRIC';s.unit_settings.scale_length=1
bpy.ops.import_scene.gltf(filepath=str(Path(a.input).resolve()))
objects=list(s.objects)
asset=bpy.data.collections.new(f'{a.asset_name} | Generated asset');s.collection.children.link(asset)
for o in objects:
    for c in list(o.users_collection):c.objects.unlink(o)
    asset.objects.link(o)
root=bpy.data.objects.new('Art scale and orientation',None);asset.objects.link(root)
for o in objects:
    if o.parent is None:o.parent=root
root.rotation_euler=(math.radians(a.pitch),math.radians(a.roll),math.radians(a.yaw));bpy.context.view_layer.update()
meshes=[o for o in objects if o.type=='MESH']
def bounds():
    vertices=[o.matrix_world@Vector(c) for o in meshes for c in o.bound_box]
    return [Vector([min(v[i] for v in vertices) for i in range(3)]),Vector([max(v[i] for v in vertices) for i in range(3)])]
lo,hi=bounds()
if target_height is not None:root.scale*=target_height/(hi.z-lo.z)
bpy.context.view_layer.update()
lo,hi=bounds();root.location=Vector((-(lo.x+hi.x)/2,-(lo.y+hi.y)/2,-lo.z));bpy.context.view_layer.update()
if a.geometry_only:
    neutral=bpy.data.materials.new('Geometry review grey');neutral.use_nodes=True
    neutral.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(.18,.19,.20,1)
    neutral.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.8
    for o in meshes:o.data.materials.clear();o.data.materials.append(neutral)
for i,im in enumerate(bpy.data.images):
    if im.has_data:
        im.filepath_raw=str(out/'textures'/f'{a.asset_name}-{i}.png');im.file_format='PNG';im.save()
bpy.ops.object.select_all(action='DESELECT')
for o in asset.objects:o.select_set(True)
bpy.context.view_layer.objects.active=meshes[0]
bpy.ops.export_scene.gltf(filepath=str(out/f'{a.asset_name}.glb'),export_format='GLB',use_selection=True)
bpy.ops.export_scene.fbx(filepath=str(out/f'{a.asset_name}.fbx'),use_selection=True,object_types={'MESH','EMPTY'},path_mode='COPY',embed_textures=True)
studio=bpy.data.collections.new('Presentation | excluded from exports');s.collection.children.link(studio)
def move(o):
    for c in list(o.users_collection):c.objects.unlink(o)
    studio.objects.link(o)
def material(name,color):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True;m.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(*color,1);return m
bpy.ops.mesh.primitive_plane_add(size=200);o=bpy.context.object;o.name='Neutral floor';o.data.materials.append(material('Studio grey',(.17,.18,.20)));move(o)
s.world=bpy.data.worlds.new('Neutral world');s.world.use_nodes=True;s.world.node_tree.nodes.get('Background').inputs[0].default_value=(.3,.3,.3,1);s.world.node_tree.nodes.get('Background').inputs[1].default_value=.45
def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
for loc,power,size in [((4,-4,6),1200,5),((-4,-1,4),1100,4),((0,4,5),1200,3)]:
    ld=bpy.data.lights.new('Softbox','AREA');o=bpy.data.objects.new('Softbox',ld);studio.objects.link(o);o.location=loc;ld.energy=power;ld.shape='DISK';ld.size=size;aim(o,(0,0,1.2))
ref=bpy.data.images.load(str(Path(a.reference).resolve()));ref.pack()
lo,hi=bounds();center=(lo+hi)*.5;extent=max(hi.x-lo.x,hi.y-lo.y,hi.z-lo.z)
o=bpy.data.objects.new('REFERENCE | clean input (viewport)',None);studio.objects.link(o);o.empty_display_type='IMAGE';o.data=ref;o.empty_display_size=max(2,extent*1.5);o.location=(center.x,center.y-extent*2,center.z);o.rotation_euler=(math.pi/2,0,math.pi/2);o.empty_image_depth='FRONT';o.hide_render=True
s.render.engine='BLENDER_EEVEE';s.render.resolution_x=1100;s.render.resolution_y=900;s.render.resolution_percentage=100
s.view_settings.view_transform='AgX'
s.view_settings.look='AgX - Medium High Contrast';s.view_settings.exposure=1
lo,hi=bounds();size=max(hi.x-lo.x,hi.y-lo.y,hi.z-lo.z)*1.35
if a.asset_name=='Voidreign':
    camera_specs=[('Beauty',(6,-4,3.4),(0,0,1.3)),('Front',(0,-7,2),(0,0,1.3)),('Side',(7,0,2),(0,0,1.3)),('Rear',(0,7,2),(0,0,1.3))]
else:
    center=(lo+hi)*.5;extent=max(hi.x-lo.x,hi.y-lo.y,hi.z-lo.z)
    camera_specs=[('Beauty',(center.x+2.3*extent,center.y-2.3*extent,center.z+1.0*extent),center),('Front',(center.x,center.y-3.0*extent,center.z),center),('Side',(center.x+3.0*extent,center.y,center.z),center),('Rear',(center.x,center.y+3.0*extent,center.z),center)]
for name,loc,target in camera_specs:
    d=bpy.data.cameras.new(name);cam=bpy.data.objects.new(name,d);studio.objects.link(cam);cam.location=loc;aim(cam,target);d.type='ORTHO';d.ortho_scale=size;s.camera=cam
    s.render.filepath=str(out/'previews'/f'{name}.png');bpy.ops.render.render(write_still=True)
s.camera=bpy.data.objects['Beauty']
clay=material('Review clay',(.18,.19,.20));s.view_layers[0].material_override=clay
s.view_settings.exposure=-.5
s.render.filepath=str(out/'previews'/'Clay.png');bpy.ops.render.render(write_still=True)
s.view_layers[0].material_override=None
s.view_settings.exposure=1
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            space=area.spaces.active
            space.shading.type='MATERIAL'
            space.region_3d.view_rotation=bpy.data.objects['Side'].rotation_euler.to_quaternion()
            space.region_3d.view_location=center
            space.region_3d.view_distance=max(3,extent*3)
            space.region_3d.view_perspective='ORTHO'
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(out/f'{a.asset_name}.blend'))
report=dict(asset_name=a.asset_name,mesh_objects=len(meshes),triangles=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in meshes),dimensions=list(hi-lo),dimension_unit='metres' if target_height is not None else 'native normalized model units',target_height_m=target_height,scale_status='explicit target height' if target_height is not None else 'unresolved; normalized output preserved',yaw_degrees=a.yaw,materials=[m.name for o in meshes for m in o.data.materials if m],uv_layers=[len(o.data.uv_layers) for o in meshes])
report['geometry_only']=a.geometry_only
report['pitch_degrees']=a.pitch;report['roll_degrees']=a.roll
(out/'asset-review.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
