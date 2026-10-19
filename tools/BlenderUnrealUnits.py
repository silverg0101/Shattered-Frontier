"""Convert a metre Blender scene to centimetres before FBX export to Unreal.

Blender otherwise writes its metre->centimetre factor as a node scale that Unreal folds into the root
bone (root scale 100, child translations in metres). After this, export with the default
apply_unit_scale=True / FBX_SCALE_NONE: the FBX is natively centimetres with unit scale on no node.
"""
import bpy
from mathutils import Matrix


def to_centimetres(objects, actions):
    scene = bpy.context.scene
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:
        o.matrix_world = Matrix.Scale(100.0, 4) @ o.matrix_world; o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    for act in actions:  # pose translations live in bone space, which the scale just stretched
        for layer in act.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for fc in bag.fcurves:
                        if fc.data_path.endswith('.location'):
                            for kp in fc.keyframe_points:
                                kp.co.y *= 100.0; kp.handle_left.y *= 100.0; kp.handle_right.y *= 100.0
    scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = 0.01
