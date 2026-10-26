"""CPU-only source/GLB/FBX checks for the 330-frame Voidreign hair pilot.

Checks all integer-frame bone transforms and full mesh deformation at profile frames.
This does not accept hair coverage, collision, gait, or Unreal runtime behavior.
"""
import argparse
import json
import sys
from pathlib import Path
from dataclasses import dataclass

import bpy
import numpy as np
from mathutils.kdtree import KDTree

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BlenderRigImport import import_fbx, sha256

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--folder', required=True)
parser.add_argument('--output', required=True)
parser.add_argument('--name', default='Voidreign-HairPrototype')
parser.add_argument('--profile', default=str(Path(__file__).resolve().parents[1] /
    'docs/art-templates/model-profiles/voidreign-rig-import-20261018.json'))
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
@dataclass(frozen=True)
class CheckSettings:
    frame_start: int
    frame_end: int
    triangles: int
    surface_sample_frames: list
    rest_surface_tolerance_m: float
    coincident_motion_tolerance_m: float
    bone_head_tolerance_m: float
    deformed_surface_tolerance_m: float
    linear_transform_tolerance: float

profile_path = Path(args.profile).resolve()
profile = json.loads(profile_path.read_text())
assert profile['schema_version'] == 1
cfg = CheckSettings(**profile['settings'])
for key in ['frame_start', 'frame_end', 'triangles']:
    assert type(getattr(cfg, key)) is int and getattr(cfg, key) > 0
assert cfg.frame_start < cfg.frame_end <= 10000
assert cfg.surface_sample_frames == sorted(set(cfg.surface_sample_frames))
assert all(type(f) is int and cfg.frame_start <= f <= cfg.frame_end for f in cfg.surface_sample_frames)
assert cfg.frame_start in cfg.surface_sample_frames and cfg.frame_end in cfg.surface_sample_frames
for key in ['rest_surface_tolerance_m', 'coincident_motion_tolerance_m',
            'bone_head_tolerance_m', 'deformed_surface_tolerance_m', 'linear_transform_tolerance']:
    assert type(getattr(cfg, key)) is float and 1e-8 <= getattr(cfg, key) <= .01
folder = Path(args.folder).resolve()
output = Path(args.output).resolve()
assert not (output / 'validation.json').exists(), 'Use a new evidence folder'
output.mkdir(parents=True, exist_ok=True)
name = args.name
contract_path = output / 'rig-import-contract.json'
contract = json.loads(contract_path.read_text())
assert sha256(folder / (name + '.blend')) == contract['source_sha256']
samples = cfg.surface_sample_frames


def mesh_arm():
    arm = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH'
              and any(m.type == 'ARMATURE' and m.object == arm for m in o.modifiers)]
    assert len(meshes) == 1
    return meshes[0], arm


def world_coords(obj):
    ev = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    points = np.empty(len(ev.data.vertices) * 3, dtype=np.float32)
    ev.data.vertices.foreach_get('co', points)
    matrix = np.array(ev.matrix_world, dtype=np.float64)
    return points.reshape(-1, 3) @ matrix[:3, :3].T + matrix[:3, 3]


def matrices(arm):
    return {b.name: np.array(arm.matrix_world @ b.matrix, dtype=np.float64)
            for b in arm.pose.bones}


bpy.ops.wm.open_mainfile(filepath=str(folder / (name + '.blend')))
mesh, arm = mesh_arm()
scene = bpy.context.scene
scene.frame_set(cfg.frame_start)
arm.data.pose_position = 'REST'
bpy.context.view_layer.update()
rest = world_coords(mesh)
arm.data.pose_position = 'POSE'
expected, deformed = {}, {}
for frame in range(cfg.frame_start, cfg.frame_end + 1):
    scene.frame_set(frame)
    expected[frame] = matrices(arm)
    if frame in samples:
        deformed[frame] = world_coords(mesh)
tree = KDTree(len(rest))
for index, point in enumerate(rest):
    tree.insert(point, index)
tree.balance()
# Coincident source vertices are interchangeable only if they deform together.
_, first, inverse = np.unique(rest, axis=0, return_index=True, return_inverse=True)
coincident_motion_error = max(float(np.linalg.norm(points - points[first][inverse], axis=1).max())
                              for points in deformed.values())
assert coincident_motion_error < cfg.coincident_motion_tolerance_m, coincident_motion_error
report = dict(status='pending', cpu_only=True, blender=bpy.app.version_string,
              bone_frames=[cfg.frame_start, cfg.frame_end], bone_sample_count=cfg.frame_end-cfg.frame_start+1,
              profile_path=str(profile_path), profile_sha256=sha256(profile_path),
              mesh_sample_frames=samples, source_sha256=contract['source_sha256'],
              contract_sha256=sha256(contract_path),
              source_coincident_motion_error_m=coincident_motion_error, imports={})
for variant in ['glb', 'fbx-default', 'fbx-contract']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = contract['fps']
    scene.render.fps_base = contract['fps_base']
    changes = {}
    if variant == 'glb':
        path = folder / (name + '.glb')
        bpy.ops.import_scene.gltf(filepath=str(path))
    else:
        path = folder / (name + '.fbx')
        if variant == 'fbx-contract':
            _, changes = import_fbx(path, contract_path)
        else:
            bpy.ops.import_scene.fbx(filepath=str(path), ignore_leaf_bones=False,
                                     force_connect_children=False, anim_offset=0)
    mesh, arm = mesh_arm()
    assert set(arm.data.bones.keys()) == set(contract['bones'])
    mesh.data.calc_loop_triangles()
    assert len(mesh.data.loop_triangles) == cfg.triangles
    assert mesh.data.uv_layers.active
    linked_images = set()
    def collect_images(node, visited):
        if node in visited:
            return
        visited.add(node)
        if node.type == 'TEX_IMAGE' and node.image:
            linked_images.add(node.image)
        for socket in node.inputs:
            for link in socket.links:
                collect_images(link.from_node, visited)
    for material in mesh.data.materials:
        if material and material.use_nodes:
            for node in material.node_tree.nodes:
                if node.type == 'BSDF_PRINCIPLED':
                    collect_images(node, set())
    image_sizes = [list(image.size) for image in linked_images]
    assert image_sizes and all(min(size) >= 4096 for size in image_sizes), image_sizes
    scene.frame_set(cfg.frame_start)
    arm.data.pose_position = 'REST'
    bpy.context.view_layer.update()
    imported_rest = world_coords(mesh)
    matches = [tree.find(point) for point in imported_rest]
    mapping = np.array([hit[1] for hit in matches])
    rest_error = max(hit[2] for hit in matches)
    assert rest_error < cfg.rest_surface_tolerance_m, rest_error
    imported_tree = KDTree(len(imported_rest))
    for index, point in enumerate(imported_rest):
        imported_tree.insert(point, index)
    imported_tree.balance()
    reverse_rest_error = max(imported_tree.find(point)[2] for point in rest)
    assert reverse_rest_error < cfg.rest_surface_tolerance_m, reverse_rest_error
    # UV/normal splits can reorder/duplicate vertices; coincident source vertices
    # in this spatially weighted rig have the same motion. Check the whole surface.
    arm.data.pose_position = 'POSE'
    bone_error = matrix_error = linear_error = vertex_error = 0.
    worst_frame = None
    for frame in range(cfg.frame_start, cfg.frame_end + 1):
        scene.frame_set(frame)
        actual = matrices(arm)
        current = max(np.linalg.norm(actual[k][:3, 3] - expected[frame][k][:3, 3])
                      for k in expected[frame])
        if current > bone_error:
            bone_error, worst_frame = float(current), frame
        matrix_error = max(matrix_error, max(float(np.max(np.abs(actual[k] - expected[frame][k])))
                                             for k in expected[frame]))
        linear_error = max(linear_error, max(float(np.max(np.abs(
            actual[k][:3, :3] - expected[frame][k][:3, :3]))) for k in expected[frame]))
        if frame in samples:
            vertex_error = max(vertex_error, float(np.linalg.norm(
                world_coords(mesh) - deformed[frame][mapping], axis=1).max()))
    passed = (bone_error < cfg.bone_head_tolerance_m
              and vertex_error < cfg.deformed_surface_tolerance_m
              and linear_error < cfg.linear_transform_tolerance)
    report['imports'][variant] = dict(status='passed' if passed else 'failed',
        exchange_sha256=sha256(path), restored_connectivity=changes,
        vertices=len(mesh.data.vertices), triangles=len(mesh.data.loop_triangles),
        connected_texture_sizes=image_sizes,
        rest_surface_error_m=rest_error, reverse_rest_surface_error_m=reverse_rest_error,
        max_bone_head_error_m=bone_error,
        worst_bone_frame=worst_frame, max_matrix_element_error=matrix_error,
        max_linear_transform_element_error=linear_error, linear_transform_tolerance=cfg.linear_transform_tolerance,
        max_deformed_surface_error_m=vertex_error,
        bone_tolerance_m=cfg.bone_head_tolerance_m,
        deformed_surface_tolerance_m=cfg.deformed_surface_tolerance_m)
    if variant == 'fbx-contract':
        scene.frame_set(125)
        scene.frame_start, scene.frame_end = cfg.frame_start, cfg.frame_end
        bpy.ops.wm.save_as_mainfile(filepath=str(output / 'Voidreign-FBX-Reimport.blend'))
    print(json.dumps({variant: report['imports'][variant]}), flush=True)
if all(report['imports'][v]['status'] == 'passed' for v in ['glb', 'fbx-default']):
    report['status'] = 'passed-default-imports'
elif all(report['imports'][v]['status'] == 'passed' for v in ['glb', 'fbx-contract']):
    report['status'] = 'passed-with-required-fbx-import-contract'
else:
    report['status'] = 'failed'
report['scope'] = 'Exchange roundtrip only; no coverage, collision, gait or engine acceptance'
(output / 'validation.json').write_text(json.dumps(report, indent=2))
if report['status'] == 'failed':
    raise SystemExit(2)
