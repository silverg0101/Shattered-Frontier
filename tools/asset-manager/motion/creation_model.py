"""Blender: validate a generated native rig, optionally size it, export its clean rest model."""
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


def main():
    args = sys.argv[sys.argv.index('--') + 1:]
    source, out, report = args[:3]
    if Path(source).suffix.lower() == '.blend':
        bpy.ops.wm.open_mainfile(filepath=str(Path(source).resolve()))
    else:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=str(Path(source).resolve()))
    arms = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE']
    if len(arms) != 1 or len(arms[0].data.bones) < 3:
        raise ValueError('native creature needs one armature with at least three bones')
    arm = arms[0]
    arm.animation_data_clear()
    all_meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    meshes = [o for o in all_meshes if any(m.type == 'ARMATURE' and m.object == arm for m in o.modifiers)]
    ignored = [o.name for o in all_meshes if o not in meshes]
    vertices = unweighted = 0
    for o in meshes:
        names = {g.index for g in o.vertex_groups if g.name in arm.data.bones}
        for v in o.data.vertices:
            vertices += 1
            if sum(g.weight for g in v.groups if g.group in names) < .99:
                unweighted += 1
    if not vertices or unweighted:
        raise ValueError(f'{unweighted}/{vertices} vertices have missing or incomplete skin weights')
    yaw_degrees = 0.0
    if '--align-forward' in args:
        named = {b.name.lower():b for b in arm.data.bones}
        if 'hips' not in named or 'head' not in named:
            raise ValueError('forward alignment needs semantic Hips and Head bones')
        hips = arm.matrix_world @ named['hips'].head_local
        head = arm.matrix_world @ named['head'].head_local
        delta = head - hips
        if math.hypot(delta.x,delta.y) < .02 * max((o.dimensions.z for o in meshes),default=0):
            raise ValueError('forward alignment needs a measurable hips-to-head heading')
        yaw = math.atan2(delta.x,-delta.y)
        arm.matrix_world = Matrix.Rotation(-yaw,4,'Z') @ arm.matrix_world
        yaw_degrees = math.degrees(-yaw)
    if '--size-cm' in args:
        pts = [o.matrix_world @ Vector(p) for o in meshes for p in o.bound_box]
        length = max(max(p[k] for p in pts) - min(p[k] for p in pts) for k in (0, 1))
        scale = float(args[args.index('--size-cm') + 1]) / 100 / max(length, 1e-6)
        for o in bpy.context.scene.objects:
            if o.parent is None:
                o.scale *= scale
                o.location *= scale
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action='DESELECT')
    for o in [arm, *meshes]:
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.gltf(filepath=str(Path(out).resolve()), export_format='GLB', export_animations=False, use_selection=True)
    Path(report).write_text(json.dumps({'bones': len(arm.data.bones), 'vertices': vertices, 'unweighted': unweighted,
                                      'ignored_unskinned': ignored, 'yaw_correction_degrees':yaw_degrees}))


if __name__ == '__main__':
    main()
