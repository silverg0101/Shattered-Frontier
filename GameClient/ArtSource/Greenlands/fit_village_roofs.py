"""Inspect the supplied roof profile and author a solid matching lining."""
import bpy,math,json
from mathutils import Vector
from pathlib import Path
ROOT=Path(__file__).resolve().parent
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(ROOT/'PreparedArrival/SM_House08_Roof.fbx'))
objs=[o for o in bpy.context.scene.objects if o.type=='MESH']
corners=[o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
lo=Vector([min(c[i] for c in corners) for i in range(3)])
hi=Vector([max(c[i] for c in corners) for i in range(3)])
center=(lo+hi)/2;radius=(hi-lo).length
print('ROOF_BOUNDS',list(lo),list(hi),flush=True)
material=bpy.data.materials.new('Roof inspection');material.diffuse_color=(.35,.25,.15,1)
for obj in objs:obj.data.materials.clear();obj.data.materials.append(material)
bpy.ops.object.camera_add(location=center+Vector((1.2,-1.8,.9))*radius)
cam=bpy.context.object;cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=radius
bpy.context.scene.camera=cam;bpy.context.scene.render.engine='CYCLES';bpy.context.scene.cycles.samples=12
bpy.context.scene.world=bpy.data.worlds.new('Inspection sky');bpy.context.scene.world.color=(.5,.5,.5)
bpy.ops.object.light_add(type='SUN');bpy.context.object.rotation_euler=(.4,-.5,-.4);bpy.context.object.data.energy=3
bpy.context.scene.render.resolution_x=1000;bpy.context.scene.render.resolution_y=700;bpy.context.scene.render.resolution_percentage=100
bpy.context.scene.render.filepath=str(ROOT/'Village-Roof-Fit.png');bpy.ops.render.render(write_still=True)
# An independent solid timber lining repairs the supplied aged thatch's holes.
# Its hip profile sits just inside the measured roof; no source mesh is embedded.
bpy.ops.wm.read_factory_settings(use_empty=True)
vertices=[(-4.05,-3.25,-1.52),(4.05,-3.25,-1.52),(4.05,3.25,-1.52),(-4.05,3.25,-1.52),(-.8,0,1.25),(.8,0,1.25)]
faces=[(0,1,5,4),(1,2,5),(2,3,4,5),(3,0,4)]
mesh=bpy.data.meshes.new('Solid hip-roof timber lining');mesh.from_pydata(vertices,[],faces);mesh.update()
obj=bpy.data.objects.new('SM_VillageRoofLining',mesh);bpy.context.collection.objects.link(obj)
uv=mesh.uv_layers.new()
for poly in mesh.polygons:
    for loop_index in poly.loop_indices:
        v=mesh.vertices[mesh.loops[loop_index].vertex_index].co;uv.data[loop_index].uv=(v.x/3,(v.y+v.z)/3)
obj.select_set(True);bpy.context.view_layer.objects.active=obj
bpy.ops.export_scene.fbx(filepath=str(ROOT/'PreparedArrival/SM_VillageRoofLining.fbx'),use_selection=True,axis_forward='-Y',axis_up='Z')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'PreparedArrival/VillageRoofLining.blend'))
