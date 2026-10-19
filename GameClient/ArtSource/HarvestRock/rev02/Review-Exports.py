"""Fresh-process source render and focused exchange checks; invoke with saved blend."""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Vector

root = Path(bpy.data.filepath).parent
for image in bpy.data.images:
    if image.name.startswith('Ironstone-BaseColor'):
        image.filepath='//Textures/T_HarvestRock_InteriorBaseColor.png'
assets = [o for o in bpy.data.objects if o.name.startswith('SM_HarvestRock_')]
outer = bpy.data.materials.get('M_HarvestRock_Baked') or bpy.data.materials['M_HarvestRock_Exterior']
outer.name = 'M_HarvestRock_Exterior'
inner = bpy.data.materials.get('M_HarvestRock_Fracture')
if inner is None:
    inner = outer.copy()
    inner.name = 'M_HarvestRock_Fracture'
interior_image=bpy.data.images.load(str(root/'Textures'/'T_HarvestRock_InteriorBaseColor.png'), check_existing=True)
interior_image.filepath='//Textures/T_HarvestRock_InteriorBaseColor.png'
nt=inner.node_tree
bs=nt.nodes.get('Principled BSDF')
for socket in ('Base Color','Roughness','Normal'):
    for link in list(bs.inputs[socket].links):
        nt.links.remove(link)
image_node=nt.nodes.new('ShaderNodeTexImage')
image_node.image=interior_image
image_node.projection='BOX'
image_node.projection_blend=0.3
coords=nt.nodes.new('ShaderNodeTexCoord')
nt.links.new(coords.outputs['Object'],image_node.inputs['Vector'])
nt.links.new(image_node.outputs['Color'],bs.inputs['Base Color'])
bs.inputs['Roughness'].default_value=0.88
stats = []
for obj in assets:
    if 'Chip' not in obj.name:
        minimum = min(v.co.z for v in obj.data.vertices)
        for vertex in obj.data.vertices:
            vertex.co.z -= minimum
    obj.data.materials.clear()
    obj.data.materials.append(outer)
    obj.data.materials.append(inner)
    for polygon in obj.data.polygons:
        polygon.material_index = int(len(polygon.vertices)>4)
        polygon.use_smooth = polygon.material_index == 0
    obj.data.calc_loop_triangles()
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    if obj.name == 'SM_HarvestRock_Intact':
        assert all(all(abs(obj.data.vertices[v].co.z)<0.0001 for v in p.vertices)
            for p in obj.data.polygons if p.material_index==1), 'Intact contains a pre-extracted side face'
    stats.append({'name':obj.name, 'vertices':len(obj.data.vertices), 'triangles':len(obj.data.loop_triangles),
        'dimensions_metres':list(obj.dimensions), 'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),
        'material_slots':[m.name for m in obj.data.materials]})
    bm.free()
    bpy.ops.object.select_all(action='DESELECT')
    obj.hide_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(root/'Exports'/(obj.name+'.fbx')),use_selection=True,
        object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,
        path_mode='RELATIVE',add_leaf_bones=False)
    obj.hide_set(True)
for obj in assets:
    obj.hide_set(False)
    obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(root/'Exports'/'HarvestRock.glb'),use_selection=True,export_format='GLB')
for obj in assets:
    obj.select_set(False)
    obj.hide_set(True)
scene = bpy.context.scene
scene.world.color = (0.08,0.08,0.08)
for name, energy in (('Area',600),('Area.001',450),('Area.002',180)):
    light=bpy.data.objects.get(name)
    if light:
        light.data.energy=energy
ground=bpy.data.objects['ReviewGround'].data.materials[0]
ground.use_nodes=True
ground.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(0.12,0.135,0.12,1)
ground.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=0.9
scene.cycles.samples = 8
scene.render.resolution_x = 1400
scene.render.resolution_y = 1100
bpy.ops.wm.save_as_mainfile(filepath=str(root/'HarvestRock.blend'))
bpy.ops.render.render(write_still=True)
for obj in bpy.data.objects:
    if obj.name.startswith('Review_SM_'):
        obj.hide_render = True
source = bpy.data.objects['SM_HarvestRock_Intact']
source.hide_render = False
camera = scene.camera
camera.location = (1.7,-2.4,1.65)
camera.data.ortho_scale = 2.35
camera.rotation_euler = (Vector((0,0,0.60))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.filepath = str(root/'HarvestRock_Close.png')
scene.render.resolution_x = 1200
scene.render.resolution_y = 1200
bpy.ops.render.render(write_still=True)
report = {'source_meshes':stats, 'fbx':[], 'glb':None}
for path in sorted((root/'Exports').glob('*.fbx')):
    for existing in list(bpy.data.objects):
        bpy.data.objects.remove(existing, do_unlink=True)
    bpy.ops.import_scene.fbx(filepath=str(path))
    imported = [o for o in bpy.context.scene.objects if o.type=='MESH']
    report['fbx'].append({'file':path.name,'mesh_count':len(imported),
        'dimensions_metres':[list(o.dimensions) for o in imported],
        'material_slots':[[m.name for m in o.data.materials] for o in imported]})
for existing in list(bpy.data.objects):
    bpy.data.objects.remove(existing, do_unlink=True)
bpy.ops.import_scene.gltf(filepath=str(root/'Exports'/'HarvestRock.glb'))
report['glb'] = {'mesh_count':len([o for o in bpy.context.scene.objects if o.type=='MESH'])}
(root/'exchange-validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('ROCK_REVIEW_COMPLETE')
