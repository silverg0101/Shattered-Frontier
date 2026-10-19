"""Deterministic editable Greenlands ore-boulder prototype; Blender 5.2.

blender --background --factory-startup --python tools/Create-HarvestRock.py -- --output <new folder>
No Unreal import, gameplay mutation or external asset downloads.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import sys
import shutil

import bpy
import bmesh
from mathutils import Vector
from mathutils.noise import noise_vector

BASE_COLOR = None


def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


def cut(obj, point, normal, keep_positive=False):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    result = bmesh.ops.bisect_plane(bm, geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
        dist=0.00001, plane_co=point, plane_no=normal,
        clear_inner=keep_positive, clear_outer=not keep_positive)
    edges = [e for e in result['geom_cut'] if isinstance(e, bmesh.types.BMEdge) and e.is_boundary]
    if edges:
        filled = bmesh.ops.holes_fill(bm, edges=edges, sides=0)
        for face in filled['faces']:
            face.material_index = 1
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def rock_material(name, interior=False):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    bs = nt.nodes.new('ShaderNodeBsdfPrincipled')
    nt.links.new(bs.outputs['BSDF'], out.inputs['Surface'])
    coords = nt.nodes.new('ShaderNodeTexCoord')
    fine = nt.nodes.new('ShaderNodeTexNoise')
    fine.inputs['Scale'].default_value = 48
    fine.inputs['Detail'].default_value = 4
    nt.links.new(coords.outputs['Object'], fine.inputs['Vector'])
    coarse = nt.nodes.new('ShaderNodeTexNoise')
    coarse.inputs['Scale'].default_value = 3.7
    coarse.inputs['Detail'].default_value = 3
    nt.links.new(coords.outputs['Object'], coarse.inputs['Vector'])
    ramp = nt.nodes.new('ShaderNodeValToRGB')
    ramp.color_ramp.elements[0].position = 0.24
    ramp.color_ramp.elements[0].color = (0.012, 0.018, 0.019, 1) if not interior else (0.036, 0.044, 0.046, 1)
    ramp.color_ramp.elements[1].position = 0.79
    ramp.color_ramp.elements[1].color = (0.10, 0.12, 0.112, 1) if not interior else (0.17, 0.18, 0.17, 1)
    nt.links.new(coarse.outputs['Fac'], ramp.inputs[0])
    veins = nt.nodes.new('ShaderNodeTexVoronoi')
    veins.feature = 'DISTANCE_TO_EDGE'
    veins.inputs['Scale'].default_value = 3.3
    nt.links.new(coords.outputs['Object'], veins.inputs['Vector'])
    threshold = nt.nodes.new('ShaderNodeMath')
    threshold.operation = 'GREATER_THAN'
    threshold.inputs[1].default_value = 0.70
    nt.links.new(coarse.outputs['Fac'], threshold.inputs[0])
    mix = nt.nodes.new('ShaderNodeMixRGB')
    nt.links.new(threshold.outputs[0], mix.inputs[0])
    nt.links.new(ramp.outputs['Color'], mix.inputs[1])
    mix.inputs[2].default_value = (0.105, 0.039, 0.012, 1)
    nt.links.new(mix.outputs[0], bs.inputs['Base Color'])
    bump = nt.nodes.new('ShaderNodeBump')
    bump.inputs['Strength'].default_value = 0.42
    bump.inputs['Distance'].default_value = 0.025 if not interior else 0.012
    nt.links.new(fine.outputs['Fac'], bump.inputs['Height'])
    nt.links.new(bump.outputs['Normal'], bs.inputs['Normal'])
    rough = nt.nodes.new('ShaderNodeMapRange')
    rough.inputs['To Min'].default_value = 0.66
    rough.inputs['To Max'].default_value = 0.94
    nt.links.new(fine.outputs['Fac'], rough.inputs['Value'])
    nt.links.new(rough.outputs['Result'], bs.inputs['Roughness'])
    bs.inputs['Metallic'].default_value = 0.0
    if BASE_COLOR:
        image = nt.nodes.new('ShaderNodeTexImage')
        image.image = bpy.data.images.load(str(BASE_COLOR), check_existing=True)
        image.projection = 'BOX'
        image.projection_blend = 0.3
        nt.links.new(coords.outputs['Object'], image.inputs['Vector'])
        nt.links.new(image.outputs['Color'], bs.inputs['Base Color'])
    return mat


def aim(obj, point):
    obj.rotation_euler = (Vector(point)-obj.location).to_track_quat('-Z', 'Y').to_euler()


def main():
    global BASE_COLOR
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True)
    parser.add_argument('--base-color', type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    root = Path(args.output).resolve()
    if root.exists():
        raise RuntimeError('Refusing to overwrite an existing art packet')
    root.mkdir(parents=True)
    (root/'Textures').mkdir()
    (root/'Exports').mkdir()
    if args.base_color:
        BASE_COLOR = root/'Textures'/'T_HarvestRock_InteriorBaseColor.png'
        shutil.copy2(args.base_color.resolve(), BASE_COLOR)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    random.seed(19019)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 1.0
    outer = rock_material('M_RockWeathered_Authoring')
    inner = rock_material('M_RockFracture_Authoring', True)
    outer.use_fake_user = True
    inner.use_fake_user = True
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=1)
    base = bpy.context.object
    base.name = 'SM_HarvestRock_Intact'
    for vert in base.data.vertices:
        p = vert.co.copy()
        n = noise_vector(p*2.1, noise_basis='PERLIN_ORIGINAL')
        factor = 1 + 0.17*n.x + 0.06*math.sin(p.z*9+p.x*5)
        vert.co = (p.x*0.90*factor, p.y*0.74*factor, (p.z*0.69+0.64)*factor)
    base.data.materials.append(outer)
    base.data.materials.append(inner)
    cut(base, (0, 0, 0), (0, 0, -1))
    # Only a hidden ground closure belongs on the intact resource. Visible joint
    # planes looked pre-extracted in V1; angularity comes from the uneven shell.
    stages = [base]
    cuts = [((0.0, -0.48, 0.69), (0.35, -1.0, 0.40)),
            ((0.37, 0.0, 0.70), (1.0, 0.1, 0.45)),
            ((0, 0, 0.32), (0.12, -0.18, 1.0))]
    for index, name in enumerate(('SM_HarvestRock_Mined01', 'SM_HarvestRock_Mined02', 'SM_HarvestRock_Remnant')):
        obj = stages[-1].copy()
        obj.data = stages[-1].data.copy()
        bpy.context.collection.objects.link(obj)
        obj.name = name
        cut(obj, *cuts[index])
        stages.append(obj)
    chips = []
    for i in range(5):
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1)
        obj = bpy.context.object
        obj.name = f'SM_HarvestRock_Chip{i+1:02d}'
        for vert in obj.data.vertices:
            vert.co.x *= 0.13+0.04*i
            vert.co.y *= 0.09+0.018*i
            vert.co.z *= 0.09+0.012*i
            vert.co *= random.uniform(0.8, 1.2)
        obj.data.materials.append(inner)
        obj.data.materials.append(outer)
        for face in obj.data.polygons:
            face.material_index = 1 if random.random()<0.3 else 0
        chips.append(obj)
    assets = stages+chips
    select(assets)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.012)
    bpy.ops.object.mode_set(mode='OBJECT')
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    # Colour/roughness/tangent-normal passes have no light transport; one sample
    # avoids spending path-tracing samples on these deterministic texture data.
    scene.cycles.samples = 1
    scene.cycles.bake_type = 'DIFFUSE'
    scene.render.bake.use_pass_direct = False
    scene.render.bake.use_pass_indirect = False
    scene.render.bake.use_pass_color = True
    scene.render.bake.margin = 12
    baked = {}
    for kind, bake_type in (('BaseColor', 'DIFFUSE'), ('Roughness', 'ROUGHNESS'), ('Normal', 'NORMAL')):
        img = bpy.data.images.new('T_HarvestRock_'+kind, width=2048, height=2048, alpha=False)
        if kind != 'BaseColor':
            img.colorspace_settings.name = 'Non-Color'
        for mat in (outer, inner):
            node = mat.node_tree.nodes.new('ShaderNodeTexImage')
            node.image = img
            mat.node_tree.nodes.active = node
        select(assets)
        bpy.ops.object.bake(type=bake_type)
        img.filepath_raw = str(root/'Textures'/f'T_HarvestRock_{kind}.png')
        img.file_format = 'PNG'
        img.save()
        baked[kind] = img
    baked_mat = bpy.data.materials.new('M_HarvestRock_Exterior')
    baked_mat.use_nodes = True
    nt = baked_mat.node_tree
    bs = nt.nodes.get('Principled BSDF')
    for kind, socket in (('BaseColor', 'Base Color'), ('Roughness', 'Roughness')):
        n = nt.nodes.new('ShaderNodeTexImage')
        n.image = baked[kind]
        nt.links.new(n.outputs['Color'], bs.inputs[socket])
    normal = nt.nodes.new('ShaderNodeNormalMap')
    n = nt.nodes.new('ShaderNodeTexImage')
    n.image = baked['Normal']
    nt.links.new(n.outputs['Color'], normal.inputs['Color'])
    nt.links.new(normal.outputs['Normal'], bs.inputs['Normal'])
    baked_inner = baked_mat.copy()
    baked_inner.name = 'M_HarvestRock_Fracture'
    if BASE_COLOR:
        nt = baked_inner.node_tree
        bs = nt.nodes.get('Principled BSDF')
        for socket in ('Base Color','Roughness','Normal'):
            for link in list(bs.inputs[socket].links):
                nt.links.remove(link)
        tex = nt.nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(str(BASE_COLOR), check_existing=True)
        tex.projection = 'BOX'
        tex.projection_blend = 0.3
        coords = nt.nodes.new('ShaderNodeTexCoord')
        nt.links.new(coords.outputs['Object'], tex.inputs['Vector'])
        nt.links.new(tex.outputs['Color'], bs.inputs['Base Color'])
        bs.inputs['Roughness'].default_value = 0.88
    stats = []
    for obj in assets:
        indices = [face.material_index for face in obj.data.polygons]
        obj.data.materials.clear()
        obj.data.materials.append(baked_mat)
        obj.data.materials.append(baked_inner)
        for face, material_index in zip(obj.data.polygons, indices):
            face.material_index = material_index
            face.use_smooth = material_index == 0
        obj.data.calc_loop_triangles()
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        nonmanifold = sum(not edge.is_manifold for edge in bm.edges)
        bm.free()
        stats.append({'name':obj.name, 'vertices':len(obj.data.vertices), 'triangles':len(obj.data.loop_triangles),
            'dimensions_metres':list(obj.dimensions), 'nonmanifold_edges':nonmanifold})
        select([obj])
        bpy.ops.export_scene.fbx(filepath=str(root/'Exports'/(obj.name+'.fbx')), use_selection=True,
            object_types={'MESH'}, axis_forward='-Y', axis_up='Z', apply_unit_scale=True,
            bake_anim=False, path_mode='RELATIVE', add_leaf_bones=False)
    select(assets)
    bpy.ops.export_scene.gltf(filepath=str(root/'Exports'/'HarvestRock.glb'), use_selection=True, export_format='GLB')
    # Review layout: source object origins remain at ground; preview offsets are instances.
    for obj in assets:
        obj.hide_render = True
        obj.hide_set(True)
    obj = bpy.data.objects.new('Review_'+base.name, base.data)
    bpy.context.collection.objects.link(obj)
    for i, source in enumerate(chips):
        obj = bpy.data.objects.new('Review_'+source.name, source.data)
        bpy.context.collection.objects.link(obj)
        obj.location = (i*0.30-0.6, -1.05, source.dimensions.z/2)
    bpy.ops.mesh.primitive_plane_add(size=200)
    ground = bpy.context.object
    ground.name = 'ReviewGround'
    gm = bpy.data.materials.new('ReviewGroundMaterial')
    gm.use_nodes = True
    gm.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value = (0.12,0.135,0.12,1)
    gm.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value = 0.9
    ground.data.materials.append(gm)
    bpy.ops.object.camera_add(location=(2.8, -4, 2.6))
    camera = bpy.context.object
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = 3.7
    aim(camera, (0, -0.1, 0.50))
    scene.camera = camera
    for location, energy, size in (((-3,-4,7),600,5), ((4,2,5),450,4), ((0,-1,4),180,3)):
        bpy.ops.object.light_add(type='AREA', location=location)
        lamp = bpy.context.object
        lamp.data.energy = energy
        lamp.data.shape = 'DISK'
        lamp.data.size = size
        aim(lamp, (0,0,0.5))
    scene.world.color = (0.08, 0.08, 0.08)
    scene.render.resolution_x = 1400
    scene.render.resolution_y = 1100
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = 'AgX'
    scene.render.filepath = str(root/'HarvestRock_IntactAndChips.png')
    for img in baked.values():
        img.filepath = '//Textures/'+Path(img.filepath).name
    bpy.ops.wm.save_as_mainfile(filepath=str(root/'HarvestRock.blend'))
    manifest = {'generator':'tools/Create-HarvestRock.py', 'blender':bpy.app.version_string,
        'seed':19019, 'method':'Original procedural mesh; sequential closed extraction cuts. Optional supplied generated-image box projection, otherwise procedural colour.',
        'base_color_source_sha256':hashlib.sha256(BASE_COLOR.read_bytes()).hexdigest() if BASE_COLOR else None,
        'coordinates':'metres, Z up; stage origins ground (0,0,0); chips centered',
        'meshes':stats, 'textures':'Shared 2048px UV atlas: BaseColor sRGB, Roughness and OpenGL tangent Normal non-color',
        'limitations':['Procedural concept prototype pending Unreal close-play visual acceptance.',
            'No collision, LODs, animation, audio, VFX or runtime integration.',
            'Extraction planes need art review for convincing pick contact and mineral composition.'],
        'outputs':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in root.rglob('*') if p.is_file()}}
    (root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n', encoding='utf-8')
    print('HARVEST_ROCK_PACKET '+str(root))


if __name__ == '__main__':
    main()
