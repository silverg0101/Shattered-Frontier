"""Editable Greenlands harvest oak study. Run with Blender 5.2 background Python.

blender --background --factory-startup --python tools/Create-HarvestOak.py -- --output <NEW folder>
Deterministic project-authored geometry/maps; no downloaded inputs; never overwrites.
This is a hero-resource art prototype, not acceptance of production foliage or collision.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import shutil
import sys

import bpy
from mathutils import Vector
from mathutils.kdtree import KDTree

SEED = 29183
TAU = math.tau


def portable_map(folder, name, size, sample, non_color=False):
    image = bpy.data.images.new(name, width=size, height=size, alpha=False)
    image.colorspace_settings.name = "Non-Color" if non_color else "sRGB"
    pixels = [0.0] * (size * size * 4)
    for y in range(size):
        for x in range(size):
            color = sample(x / size, y / size)
            offset = (y * size + x) * 4
            pixels[offset:offset + 4] = [*color, 1.0]
    image.pixels.foreach_set(pixels)
    image.filepath_raw = str(folder / (name + ".png"))
    image.file_format = "PNG"
    image.save()
    return image


def bark_height(u, v):
    bend = .038 * math.sin(TAU * v * 2) + .012 * math.sin(TAU * (v * 11 + u * 2))
    x = u + bend
    wide = abs(math.sin(TAU * (x * 17 + .08 * math.sin(TAU * v * 7))))
    narrow = abs(math.sin(TAU * (x * 51 + .15 * math.sin(TAU * v * 15))))
    grain = math.sin(TAU * (u * 139 + v * 9)) * math.sin(TAU * (v * 137 + u * 3))
    plates = .5 + .5 * math.sin(TAU * (v * 21 + .22 * math.sin(TAU * u * 13)))
    return max(0, min(1, .10 + .69 * wide ** .28 + .10 * narrow + .035 * grain - .08 * plates * (1 - wide)))


def create_materials(folder, bark_source=None):
    folder.mkdir(parents=True)
    if bark_source:
        target = folder / "T_Oak_Bark_BaseColor.png"
        shutil.copyfile(bark_source, target)
        bark = bpy.data.images.load(str(target))
        bark.name = "T_Oak_Bark_BaseColor"
        source_pixels = list(bark.pixels)
        width, height = bark.size
        def source_height(u, v):
            x, y = int((u % 1) * width) % width, int((v % 1) * height) % height
            i = (y * width + x) * 4
            return .2126 * source_pixels[i] + .7152 * source_pixels[i + 1] + .0722 * source_pixels[i + 2]
        height_field = source_height
    else:
        bark = portable_map(folder, "T_Oak_Bark_BaseColor", 1024,
            lambda u, v: tuple(a + b * bark_height(u, v) for a, b in ((.055, .205), (.044, .168), (.029, .120))))
        height_field = bark_height
    rough = portable_map(folder, "T_Oak_Bark_Roughness", 512,
        lambda u, v: (max(.65, .94 - .16 * height_field(u, v)),) * 3, True)
    def bark_normal(u, v):
        d = 1 / 1024
        dx = (height_field((u + d) % 1, v) - height_field((u - d) % 1, v)) * (5 if bark_source else 1.8)
        dy = (height_field(u, (v + d) % 1) - height_field(u, (v - d) % 1)) * (5 if bark_source else 1.8)
        n = Vector((-dx, -dy, 1)).normalized()
        return tuple(n[i] * .5 + .5 for i in range(3))
    normal = portable_map(folder, "T_Oak_Bark_Normal", 1024, bark_normal, True)
    def wood_color(u, v):
        x, y = (u - .5) * 2, (v - .5) * 2
        angle = math.atan2(y, x)
        radius = math.sqrt(x * x + y * y) * (1 + .035 * math.sin(angle * 5))
        ring = math.sin(TAU * (radius * 21 + .06 * math.sin(angle * 7)))
        ray = abs(math.sin(angle * 53 + radius * 7)) ** 20
        dark = .07 * max(0, ring) ** 8 + .022 * ray
        heart = .045 * max(0, 1 - radius * 1.5)
        return (.63 - dark - heart, .405 - dark * .85 - heart, .19 - dark * .5)
    wood = portable_map(folder, "T_Oak_Endgrain_BaseColor", 512, wood_color)
    wood_r = portable_map(folder, "T_Oak_Endgrain_Roughness", 256, lambda u, v: (.76,) * 3, True)
    def longgrain_color(u, v):
        fiber = .025 * math.sin(TAU * (u * 63 + .22 * math.sin(v * TAU * 2)))
        growth = .05 * max(0, math.sin(TAU * (u * 11 + .06 * math.sin(v * TAU)))) ** 7
        return (.59+fiber-growth,.37+fiber*.8-growth*.8,.17+fiber*.45-growth*.5)
    longgrain = portable_map(folder, "T_Oak_Longgrain_BaseColor", 512, longgrain_color)
    def leaf_color(u, v):
        vein = math.exp(-abs(u - .5) * 160)
        side = abs(math.sin((v * 7 - abs(u - .5) * 4) * math.pi)) ** 35
        shade = .05 * math.sin(v * math.pi) - .02 * side
        return (.045 + shade * .50 + vein * .055, .16 + shade + vein * .065, .018 + shade * .25)
    leaf = portable_map(folder, "T_Oak_Leaf_BaseColor", 256, leaf_color)
    def make(name, base, roughness=.8, normal_image=None, rough_image=None):
        material = bpy.data.materials.new(name)
        material.use_nodes = True
        nodes, links = material.node_tree.nodes, material.node_tree.links
        shader = nodes.get("Principled BSDF")
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = base
        links.new(tex.outputs["Color"], shader.inputs["Base Color"])
        shader.inputs["Roughness"].default_value = roughness
        if rough_image:
            tex_r = nodes.new("ShaderNodeTexImage")
            tex_r.image = rough_image
            links.new(tex_r.outputs["Color"], shader.inputs["Roughness"])
        if normal_image:
            tex_n = nodes.new("ShaderNodeTexImage")
            tex_n.image = normal_image
            normal_node = nodes.new("ShaderNodeNormalMap")
            normal_node.inputs["Strength"].default_value = .30
            links.new(tex_n.outputs["Color"], normal_node.inputs["Color"])
            links.new(normal_node.outputs["Normal"], shader.inputs["Normal"])
        if "Leaf" in name:
            shader.inputs["Subsurface Weight"].default_value = .04
            material.use_backface_culling = False
        material.diffuse_color = (.22, .16, .09, 1) if "Bark" in name else ((.13, .27, .035, 1) if "Leaf" in name else (.63, .41, .20, 1))
        return material
    return [make("M_HarvestOak_Bark", bark, normal_image=normal, rough_image=rough),
            make("M_HarvestOak_CutWood", wood, rough_image=wood_r),
            make("M_HarvestOak_Leaf", leaf), make("M_HarvestOak_Longgrain", longgrain)]


class Geometry:
    def __init__(self):
        self.vertices, self.faces, self.uvs, self.materials = [], [], [], []
        self.axes = []

    def face(self, ids, uvs, material=0):
        self.faces.append(tuple(ids))
        self.uvs.append(uvs)
        self.materials.append(material)

    def tube(self, points, radii, sides=12, seed=0, cut_depth=0, stump=False, cap_wood=False):
        start = len(self.vertices)
        self.axes.extend((Vector(a), Vector(b)) for a, b in zip(points, points[1:]))
        distances = [float(points[0][2])]
        for a, b in zip(points, points[1:]):
            distances.append(distances[-1] + (Vector(b) - Vector(a)).length)
        for j, (p, radius) in enumerate(zip(points, radii)):
            p = Vector(p)
            direction = Vector(points[min(j + 1, len(points) - 1)]) - Vector(points[max(j - 1, 0)])
            direction.normalize()
            normal = Vector((0, 1, 0)).cross(direction).normalized()
            if normal.length < .1:
                normal = Vector((1, 0, 0)).cross(direction).normalized()
            other = direction.cross(normal).normalized()
            for i in range(sides):
                angle = TAU * i / sides
                relief = 1 + .035 * math.sin(angle * 7 + seed) + .023 * math.sin(angle * 13 + j * .7)
                r = radius * relief
                if seed == SEED and p.z < .5:
                    roots = [(.12, .47, .30), (1.35, .21, .42), (2.8, .38, .24), (3.83, .29, .36), (5.45, .49, .25)]
                    flare = sum(amp * math.exp(-(math.atan2(math.sin(angle - a), math.cos(angle - a)) / spread) ** 2) for a, amp, spread in roots)
                    r *= 1 + flare * max(0, 1 - p.z / .7) ** 1.5
                # The cut faces -Y: permanent V-shaped committed extraction, never a tint overlay.
                front = max(0, -math.sin(angle)) ** 2
                band = max(0, 1 - abs(p.z - .85) / .23)
                if cut_depth:
                    r *= 1 - cut_depth * front * band
                v = p + (normal * math.cos(angle) + other * math.sin(angle)) * r
                if stump and j == len(points) - 1:
                    v.z += .022 * math.sin(angle * 5 + .3) + .012 * math.sin(angle * 11)
                self.vertices.append(tuple(v))
        for j in range(len(points) - 1):
            for i in range(sides):
                a, b = start + j * sides + i, start + j * sides + (i + 1) % sides
                ids = (a, b, b + sides, a + sides)
                # UV bark repeats by branch length rather than squeezing every limb into a square.
                u0, u1 = i / sides, (i + 1) / sides
                v0, v1 = distances[j] / 1.8, distances[j + 1] / 1.8
                cut = cut_depth and max(0, -math.sin(TAU * (i + .5) / sides)) > .22 and abs((points[j][2] + points[j + 1][2]) / 2 - .85) < .24
                mat = 1 if cut else 0
                if cut:
                    uv = tuple((.5 + self.vertices[k][0] / .70, .5 + self.vertices[k][1] / .70) for k in ids)
                else:
                    uv = ((u0, v0), (u1, v0), (u1, v1), (u0, v1))
                self.face(ids, uv, mat)
        # End caps remain separate polygons with radial endgrain UVs.
        for j, reverse in ((0, True), (len(points) - 1, False)):
            ids = [start + j * sides + i for i in range(sides)]
            if reverse:
                ids.reverse()
            self.face(ids, tuple((.5 + .48 * math.cos(TAU * (i % sides) / sides), .5 + .48 * math.sin(TAU * (i % sides) / sides)) for i in (range(sides - 1, -1, -1) if reverse else range(sides))), 1 if cap_wood else 0)

    def leaf(self, position, axis, size, rng):
        position, axis = Vector(position), Vector(axis).normalized()
        width = axis.cross(Vector((0, 0, 1)))
        if width.length < .1:
            width = Vector((1, 0, 0))
        width.normalize()
        width = (width + Vector((0, 0, rng.uniform(-.65, .65)))).normalized()
        normal = width.cross(axis).normalized()
        # Solid modeled lobes: opaque leaves, no alpha cards or blob canopy shells.
        outline = [(-.08, 0), (-.32, .13), (-.20, .22), (-.48, .34), (-.29, .43), (-.43, .58),
                   (-.19, .68), (-.24, .78), (0, 1), (.24, .78), (.19, .68), (.43, .58),
                   (.29, .43), (.48, .34), (.20, .22), (.32, .13), (.08, 0)]
        start = len(self.vertices)
        for x, y in outline:
            p = position + axis * size * y + width * size * x + normal * size * (.045 * math.sin(y * math.pi) - .075 * abs(x))
            self.vertices.append(tuple(p))
        self.vertices.append(tuple(position + axis * size * .48 + normal * size * .035))
        center = start + len(outline)
        for i in range(len(outline)):
            n = (i + 1) % len(outline)
            self.face((center, start + i, start + n), ((.5, .48), (.5 + outline[i][0], outline[i][1]), (.5 + outline[n][0], outline[n][1])), 2)

    def make_object(self, name, materials):
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(self.vertices, [], self.faces)
        mesh.update()
        layer = mesh.uv_layers.new(name="UVMap")
        for polygon, uv, material in zip(mesh.polygons, self.uvs, self.materials):
            polygon.material_index = material
            polygon.use_smooth = material in (0, 2)
            for loop, coord in zip(polygon.loop_indices, uv):
                layer.data[loop].uv = coord
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.collection.objects.link(obj)
        for material in materials:
            mesh.materials.append(material)
        return obj


def curved_path(start, end, tangent, lift, count=9):
    start, end, tangent = Vector(start), Vector(end), Vector(tangent)
    first = start + tangent
    second = end - (end - start).normalized() * .35 + Vector((0, 0, -lift))
    result = []
    for i in range(count):
        t = i / (count - 1)
        result.append((1-t)**3 * start + 3*(1-t)**2*t*first + 3*(1-t)*t*t*second + t**3*end)
    return result


def build_crown(g, leaves, twigs):
    rng = random.Random(SEED)
    g.tube([(.05,.025,1.75),(.10,.04,2.5),(.18,.03,3.3),(.07,.12,4.15),(.24,.13,4.95),(.39,.02,5.8),(.47,-.12,6.6)],
           [.27,.225,.18,.13,.078,.04,.006], 20, SEED)
    # An irregular decurrent canopy, with broad curved limbs and some open sky between groups.
    specs = [(2.15,-.22,2.8,4.3),(2.65,1.13,2.6,4.9),(3.05,2.94,2.8,5.3),
             (3.5,4.33,2.4,5.6),(3.9,5.56,2.25,5.9),(4.35,2.03,2.1,6.35),
             (4.9,.46,1.6,6.75),(5.35,3.65,1.35,6.9)]
    for i,(z, angle, length, tip_z) in enumerate(specs):
        base = Vector((.07 + .03*math.sin(z),.05,z))
        direction = Vector((math.cos(angle), math.sin(angle), 0))
        tip = base + direction * length
        tip.z = tip_z
        points = curved_path(base, tip, direction * length * .52 + Vector((0,0,rng.uniform(.1,.5))), .35)
        radii = [(.155 - i*.010)*(1-j/(len(points)-1))**.8 + .009 for j in range(len(points))]
        g.tube(points, radii, 12, i)
        for j in range(4):
            at = 3 + j
            branchbase = points[at]
            a = angle + [-.93,.63,-.48,.75][j] + rng.uniform(-.23,.23)
            reach = rng.uniform(.6,1.25)
            branchend = branchbase + Vector((math.cos(a)*reach,math.sin(a)*reach,rng.uniform(.25,.9)))
            path = curved_path(branchbase, branchend, Vector((math.cos(a)*.30,math.sin(a)*.30,.12)), .15,7)
            g.tube(path,[.047*(1-k/6)**.8+.004 for k in range(7)],9,i*7+j)
            # Fine twigs and alternate leaf pairs are authored along the shoot, not as leaf-stars.
            for k in range(13):
                along = .24 + .75*k/12
                branchpoint = path[min(5,int(along*6))].lerp(path[min(6,int(along*6)+1)],(along*6)%1)
                sign = -1 if k%2 else 1
                a2 = a + sign*rng.uniform(.45,1.3)
                twig_length = rng.uniform(.38,.82)
                twig_tip = branchpoint + Vector((math.cos(a2)*twig_length,math.sin(a2)*twig_length,rng.uniform(-.05,.35)))
                twig_path = curved_path(branchpoint,twig_tip,Vector((math.cos(a2)*.15,math.sin(a2)*.15,.035)),.03,4)
                twigs.tube(twig_path,[.008,.006,.0035,.0015],5,i*93+j*13+k)
                for pair in range(rng.randint(8,12)):
                    t = .08+.88*pair/11
                    p = twig_path[min(2,int(t*3))].lerp(twig_path[min(3,int(t*3)+1)],(t*3)%1)
                    for side in (-1,1):
                        yaw = a2+side*rng.uniform(.50,1.25)
                        axis = Vector((math.cos(yaw), math.sin(yaw), rng.uniform(-.40,.30))).normalized()
                        leaves.leaf(p+axis*.012,axis,rng.uniform(.105,.18),rng)


def fuse_woody_forks(obj, axes):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    modifier = obj.modifiers.new("Organic fork union", "REMESH")
    modifier.mode = "VOXEL"
    modifier.voxel_size = .022
    modifier.use_smooth_shade = True
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    smooth = obj.modifiers.new("Grown fork smoothing", "SMOOTH")
    smooth.factor, smooth.iterations = .65, 4
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    decimate = obj.modifiers.new("Retain limb silhouette", "DECIMATE")
    decimate.ratio = .45
    bpy.ops.object.modifier_apply(modifier=decimate.name)
    # Cylinder UVs follow the nearest authored limb axis, including the newly fused fork surfaces.
    tree = KDTree(len(axes)*3)
    for i,(a,b) in enumerate(axes):
        for j,t in enumerate((.15,.5,.85)):
            tree.insert(a.lerp(b,t), i*3+j)
    tree.balance()
    while obj.data.uv_layers:
        obj.data.uv_layers.remove(obj.data.uv_layers[0])
    uv = obj.data.uv_layers.new(name="UVMap")
    for poly in obj.data.polygons:
        center = sum((obj.data.vertices[v].co for v in poly.vertices), Vector()) / len(poly.vertices)
        _, index, _ = tree.find(center)
        a,b = axes[index//3]
        axis = (b-a).normalized()
        x = Vector((0,1,0)).cross(axis).normalized()
        if x.length < .1: x=Vector((1,0,0)).cross(axis).normalized()
        y = axis.cross(x).normalized()
        coords=[]
        for v in poly.vertices:
            point = obj.data.vertices[v].co-a
            coords.append((math.atan2(point.dot(y),point.dot(x))/TAU+.5,(a.z+point.dot(axis))/1.2))
        if max(u for u,v in coords)-min(u for u,v in coords)>.5:
            coords=[(u+1 if u<.5 else u,v) for u,v in coords]
        for loop,coord in zip(poly.loop_indices,coords): uv.data[loop].uv=coord
        poly.material_index=0
        poly.use_smooth=True


def trunk(g, cut=0, stump=False, felled=False):
    if stump:
        points = [(0, 0, 0), (.012, .01, .18), (.023, -.006, .48), (.025, -.012, .86)]
        radii = [.55, .43, .345, .32]
    elif felled:
        points = [(.025, -.012, .86), (.035, -.02, 1.15), (.05, .025, 1.75)]
        radii = [.32, .31, .27]
    else:
        points = [(0, 0, 0), (.012, .01, .18), (.023, -.006, .48), (.025, -.012, .62), (.025, -.012, .85),
                  (.032, -.018, 1.08), (.035, -.02, 1.15), (.05, .025, 1.75)]
        radii = [.55, .43, .345, .325, .32, .315, .31, .27]
    g.tube(points, radii, 48, SEED, cut_depth=cut, stump=stump, cap_wood=stump or felled)

def aim(obj, at):
    obj.rotation_euler = (Vector(at) - obj.location).to_track_quat("-Z", "Y").to_euler()


def make_fragments(materials):
    results=[]
    for i,(name,length,radius,flatten) in enumerate([("Splinter01",.22,.023,.6),("Splinter02",.14,.015,.8),("BarkChip01",.11,.035,.15)]):
        geometry=Geometry()
        geometry.tube([(-.009,0,-length*.51),(0,.003,-length*.22),(.006,-.004,length*.24),(.012,.002,length*.49)],
                      [.001,radius,radius*.65,.002],7,83+i)
        geometry.vertices=[(x,y*flatten,z) for x,y,z in geometry.vertices]
        middle=[(min(v[a] for v in geometry.vertices)+max(v[a] for v in geometry.vertices))*.5 for a in range(3)]
        geometry.vertices=[tuple(v[a]-middle[a] for a in range(3)) for v in geometry.vertices]
        geometry.materials=[0 if i==2 and index%3==0 else 3 for index in range(len(geometry.faces))]
        results.append(geometry.make_object("SM_HarvestOak_"+name,materials))
    return results


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bark-basecolor", type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise RuntimeError("Choose a new empty output directory; existing artwork is never overwritten")
    output.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1
    materials = create_materials(output / "Textures", args.bark_basecolor)
    crown, leaves, twigs = Geometry(), Geometry(), Geometry()
    build_crown(crown, leaves, twigs)
    base = crown.make_object("Oak_Crown_Source", materials)
    fuse_woody_forks(base, crown.axes)
    for geometry in (twigs, leaves):
        part=geometry.make_object("Oak_Crown_Detail", materials)
        bpy.ops.object.select_all(action="DESELECT")
        part.select_set(True)
        base.select_set(True)
        bpy.context.view_layer.objects.active=base
        bpy.ops.object.join()
    objects = []
    for name, depth, stump, felled in [("Intact", 0, False, False), ("Cut01", .38, False, False),
                                      ("Cut02", .75, False, False), ("Stump", 0, True, False),
                                      ("Felled", 0, False, True)]:
        g = Geometry()
        trunk(g, depth, stump, felled)
        obj = g.make_object("SM_HarvestOak_" + name, materials)
        if not stump:
            copy = base.copy()
            copy.data = base.data.copy()
            bpy.context.collection.objects.link(copy)
            bpy.ops.object.select_all(action="DESELECT")
            copy.select_set(True)
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.join()
        if felled:
            # Local ground is the cut-plane pivot; exported geometry stays upright for runtime rotation.
            for vertex in obj.data.vertices:
                vertex.co.z -= .85
        objects.append(obj)
    # Runtime cutting affects this closed low-poly local trunk, never the leaf canopy.
    # One continuous flared base avoids intersecting root shells in the boolean input.
    g = Geometry()
    g.tube([(0, 0, 0), (.012, .01, .18), (.023, -.006, .48), (.025, -.012, .85),
            (.035, -.02, 1.15), (.05, .025, 1.75)], [.55, .43, .345, .32, .31, .27], 48, SEED, cap_wood=True)
    objects.append(g.make_object("SM_HarvestOak_Trunk", materials))
    upper = base.copy()
    upper.data = base.data.copy()
    upper.name = "SM_HarvestOak_Crown"
    bpy.context.collection.objects.link(upper)
    objects.append(upper)
    bpy.data.objects.remove(base, do_unlink=True)
    objects.extend(make_fragments(materials))
    export_dir = output / "Exports"
    export_dir.mkdir()
    stats = []
    for obj in objects:
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        obj.data.calc_loop_triangles()
        stats.append({"name": obj.name, "vertices": len(obj.data.vertices), "triangles": len(obj.data.loop_triangles),
                      "dimensions_m": [round(v, 4) for v in obj.dimensions],
                      "pivot_m": [0, 0, .85] if obj.name.endswith("Felled") else [0, 0, 0]})
        bpy.ops.export_scene.fbx(filepath=str(export_dir / (obj.name + ".fbx")), use_selection=True,
            object_types={"MESH"}, apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z",
            bake_anim=False, add_leaf_bones=False, path_mode="RELATIVE", use_mesh_modifiers=True)
        bpy.ops.export_scene.gltf(filepath=str(export_dir / (obj.name + ".glb")), export_format="GLB", use_selection=True,
            export_yup=False, export_apply=True, export_texcoords=True, export_normals=True)
        obj.hide_render = not obj.name.endswith("Intact")
        obj.hide_set(not obj.name.endswith("Intact"))
    # Neutral preview setup: no preview ground/light/camera in exported files.
    bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -.035))
    ground = bpy.context.object
    ground.name = "PREVIEW_Ground"
    ground_mat = bpy.data.materials.new("PREVIEW_Ground_Material")
    ground_mat.diffuse_color = (.13, .145, .105, 1)
    ground.data.materials.append(ground_mat)
    world = bpy.data.worlds.new("NeutralSky")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (.32, .40, .53, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = .42
    for name, position, energy, size in [("Key", (1, -5, 10), 1600, 7), ("Fill", (-6, -1, 6), 1100, 8), ("Rim", (4, 5, 9), 1800, 6)]:
        bpy.ops.object.light_add(type="AREA", location=position)
        lamp = bpy.context.object
        lamp.name = "PREVIEW_" + name
        lamp.data.energy, lamp.data.shape, lamp.data.size = energy, "DISK", size
        aim(lamp, (0, 0, 3.5))
    bpy.ops.object.camera_add(location=(10, -14, 8))
    camera = bpy.context.object
    camera.name = "PREVIEW_Camera"
    scene.camera = camera
    camera.data.type, camera.data.ortho_scale = "ORTHO", 9
    aim(camera, (0, 0, 3.5))
    scene.render.engine = "CYCLES"
    scene.render.threads_mode = "FIXED"
    scene.render.threads = 4
    scene.cycles.samples = 32
    scene.cycles.use_denoising = True
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1100, 1100, 100
    scene.view_settings.view_transform = "AgX"
    bpy.ops.file.pack_all()
    for image in bpy.data.images:
        if image.filepath and image.source == "FILE":
            image.filepath = bpy.path.relpath(image.filepath, start=str(output))
    bpy.ops.wm.save_as_mainfile(filepath=str(output / "HarvestOak.blend"))
    for filename, position, look, scale, target in [
        ("Oak-Beauty.png", (10, -14, 8), (0, 0, 3.5), 9, "Intact"),
        ("Oak-Front.png", (0, -16, 5), (0, 0, 3.4), 8.5, "Intact"),
        ("Oak-Side.png", (16, 0, 5), (0, 0, 3.4), 8.5, "Intact"),
        ("Oak-Rear.png", (0, 16, 5), (0, 0, 3.4), 8.5, "Intact"),
        ("Oak-Forks-Close.png", (1.8, -3.5, 3.7), (0, 0, 2.8), 2.8, "Intact"),
        ("Oak-Cut02-Close.png", (1.8, -3, 1.8), (0, 0, .85), 2.0, "Cut02"),
        ("Oak-Stump-Close.png", (1.8, -3, 2), (0, 0, .45), 2.0, "Stump"),
        ("Oak-Splinter-Close.png", (.35, -.6, .35), (0, 0, 0), .40, "Splinter01")]:
        for obj in objects:
            obj.hide_render = not obj.name.endswith("_" + target)
        ground.hide_render = target == "Splinter01"
        camera.location = position
        camera.data.ortho_scale = scale
        aim(camera, look)
        scene.render.filepath = str(output / filename)
        bpy.ops.render.render(write_still=True)
    manifest = {"generator": "tools/Create-HarvestOak.py", "method": "Deterministic authored procedural mesh and portable raster materials",
                "bark_source": {"path": str(args.bark_basecolor), "sha256": hashlib.sha256(args.bark_basecolor.read_bytes()).hexdigest()} if args.bark_basecolor else None,
                "seed": SEED, "blender": bpy.app.version_string, "units": "metres", "up": "+Z", "cut_faces": "-Y",
                "cut_height_m": .85, "leaf_count":len(leaves.vertices)//18, "objects": stats, "limitations": ["No LODs, collision, wind, tool animation, audio or Unreal acceptance", "Major forks voxel-fused; fine twig junctions remain intersecting surfaces", "Opaque modeled leaves need in-engine foliage/shadow profiling", "Cut-stage interiors and controlled felling need rendered play-distance review"],
                "files": {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest() for p in output.rglob("*") if p.is_file()}}
    (output / "asset-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("HARVEST_OAK_COMPLETE " + json.dumps(stats))


if __name__ == "__main__":
    main()
