"""Build GFX-02 J0 grass card atlases, three FBX LODs per card and a review render.

Run with Blender 5.1: blender -b --factory-startup -P build_grass_cards.py
The FBX files have separate _LOD0/_LOD1/_LOD2 suffixes for Unreal's manual LOD import.
"""
import json
import math
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

HERE = Path(__file__).resolve().parent
OUT = HERE / 'GrassCards'
OUT.mkdir(exist_ok=True)
SOURCES = HERE / 'Sources'
ATLAS_W, ATLAS_H, TILE = 2048, 1024, 512


def flat_material(source, normal=False):
    material = source.copy()
    material.name = source.name + ('_NormalPass' if normal else '_DiffusePass')
    material.use_nodes = True
    tree = material.node_tree
    output = next(n for n in tree.nodes if n.type == 'OUTPUT_MATERIAL')
    principled = next((n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    color_link = principled.inputs['Base Color'].links[0].from_socket if principled and principled.inputs['Base Color'].is_linked else None
    alpha_link = principled.inputs['Alpha'].links[0].from_socket if principled and principled.inputs['Alpha'].is_linked else None
    alpha_value = principled.inputs['Alpha'].default_value if principled else 1.0
    emission = tree.nodes.new('ShaderNodeEmission')
    if normal:
        geometry = tree.nodes.new('ShaderNodeNewGeometry')
        remap = tree.nodes.new('ShaderNodeVectorMath')
        remap.operation = 'MULTIPLY_ADD'
        remap.inputs[1].default_value = (0.5, 0.5, 0.5)
        remap.inputs[2].default_value = (0.5, 0.5, 0.5)
        tree.links.new(geometry.outputs['Normal'], remap.inputs[0])
        tree.links.new(remap.outputs['Vector'], emission.inputs['Color'])
    elif color_link:
        tree.links.new(color_link, emission.inputs['Color'])
    elif principled:
        emission.inputs['Color'].default_value = principled.inputs['Base Color'].default_value
    transparent = tree.nodes.new('ShaderNodeBsdfTransparent')
    mix = tree.nodes.new('ShaderNodeMixShader')
    tree.links.new(transparent.outputs[0], mix.inputs[1])
    tree.links.new(emission.outputs[0], mix.inputs[2])
    if alpha_link:
        tree.links.new(alpha_link, mix.inputs[0])
    else:
        mix.inputs[0].default_value = alpha_value
    tree.links.new(mix.outputs[0], output.inputs['Surface'])
    material.surface_render_method = 'DITHERED'
    material.use_transparency_overlap = False
    return material


def load_clumps():
    selected = []
    for asset in ('grass_medium_01', 'grass_medium_02'):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=str(SOURCES / asset / (asset + '.gltf')))
        objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
        for obj in objects:
            obj.data.calc_loop_triangles()
        if asset == 'grass_medium_01':
            chosen = sorted((o for o in objects if 300 <= len(o.data.loop_triangles) <= 1600),
                            key=lambda o: len(o.data.loop_triangles))[:4]
            assert len(chosen) == 4, 'Expected four curated grass_medium_01 clumps'
        else:
            chosen = sorted(objects, key=lambda o: o.dimensions.x * o.dimensions.y * o.dimensions.z,
                            reverse=True)[:2]
            assert len(chosen) == 2, 'Expected two grass_medium_02 clumps'
        # Render now: a factory reset on the next asset removes these objects.
        for obj in chosen:
            yield asset, obj


def render_tile(obj, normal, path):
    scene = bpy.context.scene
    materials = list(obj.data.materials)
    for slot, original in enumerate(materials):
        obj.data.materials[slot] = flat_material(original, normal) if original else None
    for other in scene.objects:
        if other.type == 'MESH':
            other.hide_render = other != obj
    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = scene.render.resolution_y = TILE
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.render.image_settings.color_depth = '8'
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'Medium High Contrast'
    scene.view_settings.exposure = 0
    scene.view_settings.gamma = 1
    bounds = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    lo = Vector(tuple(min(v[i] for v in bounds) for i in range(3)))
    hi = Vector(tuple(max(v[i] for v in bounds) for i in range(3)))
    center = (lo + hi) * 0.5
    camera_data = bpy.data.cameras.new('CardCamera')
    camera = bpy.data.objects.new('CardCamera', camera_data)
    scene.collection.objects.link(camera)
    camera.location = center + Vector((0, -max(2.0, obj.dimensions.y * 4), 0))
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = max(hi.x - lo.x, hi.z - lo.z) * 1.08
    scene.camera = camera
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera, do_unlink=True)
    for slot, original in enumerate(materials):
        obj.data.materials[slot] = original
    return [float(v) for v in hi - lo]


def card_mesh(name, width, height, slot, level):
    planes = 2 if level == 2 else 3
    columns, rows = (2, 3) if level == 0 else ((2, 1) if level == 1 else (1, 1))
    vertices, faces, uvs, heights = [], [], [], []
    for plane in range(planes):
        angle = math.radians(plane * 60)
        ca, sa = math.cos(angle), math.sin(angle)
        start = len(vertices)
        for j in range(rows + 1):
            t = j / rows
            for i in range(columns + 1):
                x = (i / columns - 0.5) * width
                bend = 0 if level else math.copysign(math.tan(math.radians(12)) * height * t * t, x)
                radial = x + bend
                vertices.append((radial * ca, radial * sa, height * t))
                uvs.append(((slot % 4 + i / columns) / 4, (slot // 4 + t) / 2))
                heights.append(t)
        for j in range(rows):
            for i in range(columns):
                a = start + j * (columns + 1) + i
                b = a + 1
                c = a + columns + 2
                d = a + columns + 1
                faces.append((a, b, c))
                faces.append((a, c, d))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    uv = mesh.uv_layers.new(name='UVMap')
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            uv.data[loop_index].uv = uvs[mesh.loops[loop_index].vertex_index]
    colors = mesh.color_attributes.new(name='HeightWeight', type='FLOAT_COLOR', domain='POINT')
    for i, t in enumerate(heights):
        colors.data[i].color = (t, 0, 0, 1)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj, len(faces)


def save_atlas(paths, name):
    pixels = np.zeros((ATLAS_H, ATLAS_W, 4), dtype=np.float32)
    for slot, path in enumerate(paths):
        image = bpy.data.images.load(str(path), check_existing=False)
        assert tuple(image.size) == (TILE, TILE)
        tile = np.empty(TILE * TILE * 4, dtype=np.float32)
        image.pixels.foreach_get(tile)
        y, x = divmod(slot, 4)
        pixels[y*TILE:(y+1)*TILE, x*TILE:(x+1)*TILE] = tile.reshape(TILE, TILE, 4)
        bpy.data.images.remove(image)
    atlas = bpy.data.images.new(name, width=ATLAS_W, height=ATLAS_H, alpha=True)
    atlas.pixels.foreach_set(pixels.ravel())
    atlas.filepath_raw = str(OUT / (name + '.png'))
    atlas.file_format = 'PNG'
    atlas.save()
    return atlas


def export_cards(records):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for record in records:
        name = record['mesh']
        width = max(record['dimensions_metres'][0], record['dimensions_metres'][1])
        height = record['dimensions_metres'][2]
        record['lod_triangles'] = []
        for lod in range(3):
            obj, tris = card_mesh(name + '_LOD' + str(lod), width, height, record['slot'], lod)
            record['lod_triangles'].append(tris)
            bpy.ops.object.select_all(action='DESELECT')
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            bpy.ops.export_scene.fbx(filepath=str(OUT / (name + '_LOD' + str(lod) + '.fbx')),
                                     use_selection=True, object_types={'MESH'}, axis_forward='-Y', axis_up='Z',
                                     colors_type='LINEAR', path_mode='STRIP', add_leaf_bones=False)
            bpy.data.objects.remove(obj, do_unlink=True)
        assert record['lod_triangles'] == [36, 12, 4]


def render_sheet(records, atlas):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    atlas = bpy.data.images.load(str(OUT / 'T_GN_GrassCards_D.png'))
    material = bpy.data.materials.new('GrassCardReview')
    material.use_nodes = True
    material.surface_render_method = 'DITHERED'
    tree = material.node_tree
    tree.nodes.clear()
    texture = tree.nodes.new('ShaderNodeTexImage')
    texture.image = atlas
    emission = tree.nodes.new('ShaderNodeEmission')
    transparent = tree.nodes.new('ShaderNodeBsdfTransparent')
    mix = tree.nodes.new('ShaderNodeMixShader')
    output = tree.nodes.new('ShaderNodeOutputMaterial')
    tree.links.new(texture.outputs['Color'], emission.inputs['Color'])
    tree.links.new(texture.outputs['Alpha'], mix.inputs[0])
    tree.links.new(transparent.outputs[0], mix.inputs[1])
    tree.links.new(emission.outputs[0], mix.inputs[2])
    tree.links.new(mix.outputs[0], output.inputs['Surface'])
    for record in records:
        slot = record['slot']
        width = max(record['dimensions_metres'][:2])
        height = record['dimensions_metres'][2]
        obj, _ = card_mesh(record['mesh'], width, height, slot, 0)
        obj.scale = (4, 4, 4)
        obj.location = ((slot % 3 - 1) * 2.2, 0, (1 - slot // 3) * 1.8)
        obj.data.materials.append(material)
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = 1920
    scene.render.resolution_y = 1080
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = False
    scene.world = bpy.data.worlds.new('ReviewWorld')
    scene.world.color = (0.12, 0.12, 0.12)
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGB'
    camera_data = bpy.data.cameras.new('ReviewCamera')
    camera = bpy.data.objects.new('ReviewCamera', camera_data)
    scene.collection.objects.link(camera)
    camera.location = (0, -12, 1.8)
    camera.rotation_euler = (Vector((0, 0, 1.8)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = 6.5
    scene.camera = camera
    scene.render.filepath = str(OUT / 'six-cards-live.png')
    bpy.ops.render.render(write_still=True)


def main():
    records, diffuse, normals = [], [], []
    for asset, obj in load_clumps():
        slot = len(records)
        obj.data.calc_loop_triangles()
        dims = render_tile(obj, False, OUT / ('tile_%02d_D.png' % slot))
        render_tile(obj, True, OUT / ('tile_%02d_N.png' % slot))
        records.append(dict(slot=slot, mesh='SM_GN_GrassCard_%02d' % slot, source_asset=asset,
                            source_object=obj.name, source_triangles=len(obj.data.loop_triangles),
                            dimensions_metres=dims, screen_sizes=[1.0, 0.35, 0.12],
                            nanite=False, collision=False))
        diffuse.append(OUT / ('tile_%02d_D.png' % slot))
        normals.append(OUT / ('tile_%02d_N.png' % slot))
    assert len(records) == 6
    atlas = save_atlas(diffuse, 'T_GN_GrassCards_D')
    save_atlas(normals, 'T_GN_GrassCards_N')
    export_cards(records)
    render_sheet(records, atlas)
    (OUT / 'manifest.json').write_text(json.dumps(dict(schema='gfx02-grass-cards-v1',
        source='Poly Haven CC0', atlas_px=[ATLAS_W, ATLAS_H], tile_px=[TILE, TILE],
        cards=records), indent=2) + '\n')
    print('GFX02_J0_CARDS', len(records), 'ATLAS', ATLAS_W, ATLAS_H,
          'LODS', [r['lod_triangles'] for r in records], flush=True)


if __name__ == '__main__':
    main()
