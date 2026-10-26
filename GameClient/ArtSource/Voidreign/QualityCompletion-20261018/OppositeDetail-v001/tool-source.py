"""Bake registered fixed-view color edits onto an exact animated source copy.

Only visible facing surfaces inside authored regions contribute. Geometry, UVs,
weights, animation and non-color material channels are preserved. No inference.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
for key in ['source', 'guides', 'edits', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, guide_path, edit_path, out = [Path(value).resolve() for value in [a.source, a.guides, a.edits, a.output]]
guides = json.loads(guide_path.read_text())
edits = json.loads(edit_path.read_text())
assert guides['schema_version'] == edits['schema_version'] == 1
assert hashlib.sha256(source.read_bytes()).hexdigest() == guides['source_sha256']
assert hashlib.sha256(guide_path.read_bytes()).hexdigest() == edits['guides_sha256']
out.mkdir(parents=True, exist_ok=False)
(out/'textures').mkdir()
for name, path in [('tool-source.py', Path(__file__)), ('edits.json', edit_path), ('projection-guides.json', guide_path)]:
    (out/name).write_bytes(path.read_bytes())
bpy.ops.wm.open_mainfile(filepath=str(source))
scene = bpy.context.scene
scene.frame_set(1)
obj = bpy.data.objects[guides['object']]
assert len(obj.data.materials) == 1, 'Author a multi-material adapter before projecting this asset'
arms = [modifier.object for modifier in obj.modifiers if modifier.type == 'ARMATURE']
for arm in arms:
    arm.data.pose_position = 'REST'
bpy.context.view_layer.update()
mesh = obj.data
mesh.calc_loop_triangles()
vertices = np.array([v.co[:] for v in mesh.vertices], dtype=np.float32)
faces = np.array([tuple(t.vertices) for t in mesh.loop_triangles], dtype=np.int32)
assert hashlib.sha256(vertices.tobytes()+faces.tobytes()).hexdigest() == guides['mesh_hash']
uv_before = np.array([loop.uv[:] for loop in mesh.uv_layers.active.data], dtype=np.float32)
world = [obj.matrix_world@vertex.co for vertex in mesh.vertices]
bvh = BVHTree.FromPolygons(world, [tuple(face.vertices) for face in mesh.polygons], all_triangles=True)
material = mesh.materials[0]
nodes, links = material.node_tree.nodes, material.node_tree.links
bsdf = next(node for node in nodes if node.type == 'BSDF_PRINCIPLED')
assert bsdf.inputs['Base Color'].is_linked
base = bsdf.inputs['Base Color'].links[0].from_socket
assert base.node.type == 'TEX_IMAGE' and tuple(base.node.image.size) == (4096, 4096)
original_image = base.node.image
original_pixels = np.empty(4096*4096*4, dtype=np.float32)
original_image.pixels.foreach_get(original_pixels)
channel_inputs = {name: [(link.from_node.name, link.from_socket.name) for link in bsdf.inputs[name].links]
                  for name in ['Metallic', 'Roughness', 'Normal']}

def smooth(value):
    value = max(0., min(1., value))
    return value*value*(3-2*value)

counts, mask_socket = {}, None
normals = obj.matrix_world.to_3x3().inverted().transposed()
for spec in guides['views']:
    edit = next(item for item in edits['views'] if item['name'] == spec['name'])
    image_path = (edit_path.parent/edit['image']).resolve()
    assert hashlib.sha256(image_path.read_bytes()).hexdigest() == edit['sha256']
    camera = bpy.data.objects.new(spec['name']+' fixed projection', bpy.data.cameras.new(spec['name']+' fixed projection'))
    scene.collection.objects.link(camera)
    camera.matrix_world = Matrix(spec['camera_matrix_world'])
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = spec['scale_m']
    inverse = camera.matrix_world.inverted()
    scale = spec['scale_m']
    toward_camera = camera.matrix_world.to_3x3()@Vector((0, 0, 1))
    bounds = np.array(spec['region_bounds_m'])
    feather = spec['region_feather_m']
    assert bounds.shape == (3, 2) and .01 <= feather <= .5
    weights = []
    for vertex, point in zip(mesh.vertices, world):
        local = inverse@point
        u, v = .5+local.x/scale, .5+local.y/scale
        edge = smooth(min(u, v, 1-u, 1-v)/.08)
        region = 1.
        for axis in range(3):
            region *= smooth(min(point[axis]-bounds[axis, 0], bounds[axis, 1]-point[axis])/feather)
        facing = smooth(((normals@vertex.normal).normalized().dot(toward_camera)-.12)/.45)
        weight = edge*region*facing
        if weight > .001:
            hit, _, _, _ = bvh.ray_cast(point+toward_camera*10, -toward_camera, 20)
            if hit is None or (hit-point).length > .004:
                weight = 0.
        weights.append(weight)
    attribute = mesh.color_attributes.new(name=spec['name']+' projection weight', type='FLOAT_COLOR', domain='POINT')
    rgba = np.column_stack([weights, weights, weights, np.ones(len(weights))]).astype(np.float32)
    attribute.data.foreach_set('color', rgba.ravel())
    counts[spec['name']] = int((np.array(weights) > .01).sum())
    assert counts[spec['name']] > 100, 'Empty projection: inspect frame, camera, region and BVH'
    coords = nodes.new('ShaderNodeTexCoord')
    coords.object = camera
    multiply = nodes.new('ShaderNodeVectorMath')
    multiply.operation = 'MULTIPLY'
    multiply.inputs[1].default_value = (1/scale, 1/scale, 0)
    links.new(coords.outputs['Object'], multiply.inputs[0])
    offset = nodes.new('ShaderNodeVectorMath')
    offset.operation = 'ADD'
    offset.inputs[1].default_value = (.5, .5, 0)
    links.new(multiply.outputs[0], offset.inputs[0])
    texture = nodes.new('ShaderNodeTexImage')
    texture.image = bpy.data.images.load(str(image_path))
    texture.extension = 'CLIP'
    links.new(offset.outputs[0], texture.inputs['Vector'])
    color = nodes.new('ShaderNodeVertexColor')
    color.layer_name = attribute.name
    mix = nodes.new('ShaderNodeMixRGB')
    links.new(color.outputs['Color'], mix.inputs[0])
    links.new(base, mix.inputs[1])
    links.new(texture.outputs['Color'], mix.inputs[2])
    base = mix.outputs[0]
    if mask_socket is None:
        mask_socket = color.outputs['Color']
    else:
        maximum = nodes.new('ShaderNodeMath')
        maximum.operation = 'MAXIMUM'
        links.new(mask_socket, maximum.inputs[0])
        links.new(color.outputs['Color'], maximum.inputs[1])
        mask_socket = maximum.outputs[0]
output = next(node for node in nodes if node.type == 'OUTPUT_MATERIAL' and node.is_active_output)
emission = nodes.new('ShaderNodeEmission')
links.new(base, emission.inputs['Color'])
links.new(emission.outputs[0], output.inputs['Surface'])
target = nodes.new('ShaderNodeTexImage')
image = bpy.data.images.new('Voidreign-Opposite-Detail-BaseColor-4K', width=4096, height=4096, alpha=True)
target.image = image
nodes.active = target
bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 1
scene.cycles.use_denoising = False
if hasattr(scene.cycles, 'denoising_use_gpu'):
    scene.cycles.denoising_use_gpu = False
scene.render.bake.margin = 12
bpy.ops.object.bake(type='EMIT')
baked = np.empty_like(original_pixels)
image.pixels.foreach_get(baked)
mask_image = bpy.data.images.new('Projection UV mask', width=4096, height=4096, alpha=True)
mask_image.colorspace_settings.name = 'Non-Color'
target.image = mask_image
links.new(mask_socket, emission.inputs['Color'])
bpy.ops.object.bake(type='EMIT')
mask_pixels = np.empty_like(original_pixels)
mask_image.pixels.foreach_get(mask_pixels)
changed = mask_pixels.reshape(-1, 4)[:, 0] > 1e-5
final = original_pixels.reshape(-1, 4).copy()
final[changed, :3] = baked.reshape(-1, 4)[changed, :3]
image.pixels.foreach_set(final.ravel())
image.filepath_raw = str(out/'textures/BaseColor-4K.png')
image.file_format = 'PNG'
image.save()
mask_image.filepath_raw = str(out/'textures/ProjectionMask-4K.png')
mask_image.file_format = 'PNG'
mask_image.save()
target.image = image
links.new(target.outputs['Color'], bsdf.inputs['Base Color'])
links.new(bsdf.outputs[0], output.inputs['Surface'])
assert channel_inputs == {name: [(link.from_node.name, link.from_socket.name) for link in bsdf.inputs[name].links] for name in channel_inputs}
assert np.array_equal(vertices, np.array([v.co[:] for v in mesh.vertices], dtype=np.float32))
assert np.array_equal(uv_before, np.array([loop.uv[:] for loop in mesh.uv_layers.active.data], dtype=np.float32))
for arm in arms:
    arm.data.pose_position = 'POSE'
scene.frame_set(125)
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Voidreign-CompleteHair.blend'))
root = bpy.data.objects[edits['assembly_root']]
bpy.ops.object.select_all(action='DESELECT')
for item in [obj, root]+arms:
    item.select_set(True)
bpy.context.view_layer.objects.active = arms[0] if arms else obj
bpy.ops.export_scene.gltf(filepath=str(out/'Voidreign-CompleteHair.glb'), export_format='GLB',
    use_selection=True, export_animations=True, export_animation_mode='SCENE', export_anim_scene_split_object=False)
bpy.ops.export_scene.fbx(filepath=str(out/'Voidreign-CompleteHair.fbx'), use_selection=True,
    object_types={'MESH', 'ARMATURE', 'EMPTY'}, path_mode='COPY', embed_textures=True,
    bake_anim=True, bake_anim_simplify_factor=0., bake_anim_use_nla_strips=False,
    bake_anim_use_all_actions=False, add_leaf_bones=False)
report = dict(status='material-projection-candidate; fresh checks and visual review required',
              source_sha256=guides['source_sha256'], source=str(source), mesh_hash=guides['mesh_hash'],
              guides_sha256=edits['guides_sha256'], edits_sha256=hashlib.sha256(edit_path.read_bytes()).hexdigest(),
              tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              weighted_vertices=counts, atlas_pixels_in_projection_mask=int(changed.sum()),
              atlas_pixels_preserved=int((~changed).sum()), outside_mask_linear_pixel_delta=0.,
              unchanged_geometry_uvs=True, non_color_channel_links_preserved=True, cpu_only=True,
              limitations=['Unlit base color only; no new normal/roughness/metallic detail.',
                           'View registration and interpolated visibility masks require seam/hidden-surface review.',
                           'Inherited rig surface/coverage defects remain; no gait/runtime acceptance.'])
(out/'projection-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
