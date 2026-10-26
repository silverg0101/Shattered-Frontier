"""Held-tool meshes for the starter iron axe and pickaxe (hotbar slots 5 and 6, shown in the right hand).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_handtools.py
Writes SM_Tool_IronAxe.fbx and SM_Tool_IronPickaxe.fbx beside this file; tools/Import-HandTools.py imports them.
Convention for both: the grip (where the right hand closes) at the origin, the haft along +Z with the head up,
the blade / pick point toward +X. The axe is the CC0 Poly Haven wooden_axe (Silverbrook/Models), whose UVs match
MI_SB_PH_wooden_axe; the pickaxe is modelled here with two material slots (Haft, Iron).
"""
import math
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
AXE_SOURCE = HERE.parent / 'Silverbrook/Models/wooden_axe/wooden_axe.gltf'
GRIP_FROM_BUTT = 0.14  # m from the haft's butt end to the hand's centre


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def export(obj, name):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    obj.name = name
    bpy.ops.export_scene.fbx(filepath=str(HERE / f'{name}.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', add_leaf_bones=False,
                             bake_anim=False, axis_forward='-Y', axis_up='Z')
    dims = obj.dimensions
    print(f'HAND_TOOL {name} {dims.x:.3f} x {dims.y:.3f} x {dims.z:.3f} m')


def build_axe():
    reset()
    bpy.ops.import_scene.gltf(filepath=str(AXE_SOURCE))
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    axe = bpy.context.view_layer.objects.active
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    pts = [v.co.copy() for v in axe.data.vertices]
    lo = Vector([min(p[i] for p in pts) for i in range(3)]); hi = Vector([max(p[i] for p in pts) for i in range(3)])
    size = hi - lo
    long_i = max(range(3), key=lambda i: size[i])
    blade_i = max((i for i in range(3) if i != long_i), key=lambda i: size[i])
    mid_long = (lo[long_i] + hi[long_i]) / 2
    # The head is the end of the long axis with the widest spread across the blade axis.
    near_hi = [p[blade_i] for p in pts if p[long_i] > mid_long]; near_lo = [p[blade_i] for p in pts if p[long_i] <= mid_long]
    head_hi = (max(near_hi) - min(near_hi)) > (max(near_lo) - min(near_lo))
    # Haft centre line across the blade axis: the median of the handle-end half.
    handle = sorted(near_lo if head_hi else near_hi)
    haft_c = handle[len(handle) // 2]
    head = [p[blade_i] for p in pts if (p[long_i] > mid_long) == head_hi]
    blade_up = (max(head) - haft_c) > (haft_c - min(head))
    up = Vector([0, 0, 0]); up[long_i] = 1 if head_hi else -1
    fwd = Vector([0, 0, 0]); fwd[blade_i] = 1 if blade_up else -1
    side = up.cross(fwd)
    basis = Matrix((fwd, side, up))  # rows: new x, y, z in old coordinates
    butt = lo[long_i] if head_hi else hi[long_i]
    grip = Vector([0, 0, 0]); grip[long_i] = butt + (GRIP_FROM_BUTT if head_hi else -GRIP_FROM_BUTT); grip[blade_i] = haft_c
    third = 3 - long_i - blade_i
    grip[third] = (lo[third] + hi[third]) / 2
    axe.data.transform(Matrix.Translation(-grip))
    axe.data.transform(basis.to_4x4())
    axe.data.update()
    for slot in axe.material_slots:
        slot.material.name = 'wooden_axe'
    export(axe, 'SM_Tool_IronAxe')


def material(name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    return m


def build_pickaxe():
    reset()
    haft_len, haft_r, butt = 0.80, 0.017, -GRIP_FROM_BUTT
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=haft_r, depth=haft_len, location=(0, 0, butt + haft_len / 2))
    haft = bpy.context.active_object
    haft.data.materials.append(material('Haft'))
    head_z = butt + haft_len - 0.06
    parts = [haft]
    # Collar round the haft top.
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, head_z))
    collar = bpy.context.active_object; collar.scale = (0.055, 0.042, 0.075)
    parts.append(collar)
    # Pick (long, toward +X) and chisel (short, toward -X), each a tapered, slightly drooping spike.
    for direction, length, tip in ((1, 0.27, 0.004), (-1, 0.20, 0.012)):
        bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=0.022, radius2=tip, depth=length,
                                        location=(direction * length / 2, 0, head_z))
        spike = bpy.context.active_object
        spike.rotation_euler = (0, direction * math.radians(90), 0)
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
        spike.rotation_euler = (0, direction * math.radians(12), 0)  # droop toward the haft
        spike.location.z -= 0.025
        parts.append(spike)
    iron = material('Iron')
    for p in parts[1:]:
        p.data.materials.clear(); p.data.materials.append(iron)
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts:
        p.select_set(True)
    bpy.context.view_layer.objects.active = haft
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.join()
    pick = bpy.context.active_object
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.cube_project(cube_size=0.3); bpy.ops.object.mode_set(mode='OBJECT')
    export(pick, 'SM_Tool_IronPickaxe')


build_axe()
build_pickaxe()
