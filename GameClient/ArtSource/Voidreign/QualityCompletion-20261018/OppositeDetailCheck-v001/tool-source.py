"""Fresh CPU source/candidate comparison for a color-only animated mesh revision."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
for key in ['source', 'candidate', 'object', 'mask', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, candidate, mask_path, out = [Path(v).resolve() for v in [a.source, a.candidate, a.mask, a.output]]
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())

def array_hash(values):
    return hashlib.sha256(np.asarray(values).tobytes()).hexdigest()

def image_pixels(image):
    values = np.empty(len(image.pixels), dtype=np.float32)
    image.pixels.foreach_get(values)
    return values.reshape(-1, 4)

def capture(path, capture_surfaces=True):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    obj = bpy.data.objects[a.object]
    mesh = obj.data
    mesh.calc_loop_triangles()
    groups = {g.index: g.name for g in obj.vertex_groups}
    weights = [[(groups[g.group], float(g.weight)) for g in vertex.groups] for vertex in mesh.vertices]
    hashes = dict(vertices=array_hash(np.array([v.co[:] for v in mesh.vertices], dtype=np.float32)),
                  faces=array_hash(np.array([tuple(t.vertices) for t in mesh.loop_triangles], dtype=np.int32)),
                  uv=array_hash(np.array([v.uv[:] for v in mesh.uv_layers.active.data], dtype=np.float32)),
                  polygon_material=array_hash(np.array([f.material_index for f in mesh.polygons], dtype=np.int32)),
                  weights=hashlib.sha256(json.dumps(weights).encode()).hexdigest())
    assert len(mesh.materials) == 1
    bsdf = next(node for node in mesh.materials[0].node_tree.nodes if node.type == 'BSDF_PRINCIPLED')
    channel_images = {}
    for channel in ['Metallic', 'Roughness', 'Normal']:
        pending = [link.from_node for link in bsdf.inputs[channel].links]
        seen, images = set(), []
        while pending:
            node = pending.pop()
            if node in seen:
                continue
            seen.add(node)
            if node.type == 'TEX_IMAGE':
                images.append(dict(size=list(node.image.size), pixels=array_hash(image_pixels(node.image))))
            pending += [link.from_node for socket in node.inputs for link in socket.links]
        channel_images[channel] = images
    color_node = bsdf.inputs['Base Color'].links[0].from_node
    assert color_node.type == 'TEX_IMAGE'
    base_pixels = image_pixels(color_node.image)
    arm = next(modifier.object for modifier in obj.modifiers if modifier.type == 'ARMATURE')
    assert arm.data.pose_position == 'POSE'
    scene = bpy.context.scene
    matrices, surfaces = [], {}
    sample_frames = sorted(set([1, 2, 31, 60, 61, 91, 120, 121, 151, 180, 181, 211, 240, 241, 271, 285, 300, 315, 330]))
    names = sorted(arm.pose.bones.keys())
    for frame in range(1, 331):
        scene.frame_set(frame)
        matrices.append(np.array([arm.matrix_world@arm.pose.bones[name].matrix for name in names]))
        if capture_surfaces and frame in sample_frames:
            evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
            points = np.empty(len(mesh.vertices)*3, dtype=np.float32)
            evaluated.data.vertices.foreach_get('co', points)
            matrix = np.array(obj.matrix_world)
            surfaces[frame] = points.reshape(-1, 3)@matrix[:3, :3].T+matrix[:3, 3]
    return dict(hashes=hashes, channels=channel_images, base=base_pixels, names=names,
                matrices=np.array(matrices), surfaces=surfaces)

before = capture(source)
after = capture(candidate)
mask_image = bpy.data.images.load(str(mask_path), check_existing=False)
mask_image.colorspace_settings.name = 'Non-Color'
mask = image_pixels(mask_image)[:, 0]
# PNG mask quantization can round tiny nonzero bake values to zero. A
# conservative one-pixel 8-neighborhood excludes that boundary from the
# unchanged-area assertion; interior zero-mask pixels are still checked.
size = tuple(mask_image.size)
inside = mask.reshape(size[1], size[0]) > 0
dilated = inside.copy()
for y in [-1, 0, 1]:
    for x in [-1, 0, 1]:
        rolled = np.roll(np.roll(inside, y, axis=0), x, axis=1)
        dilated |= rolled
outside = ~dilated.ravel()
color_delta = np.max(np.abs(after['base'][:, :3]-before['base'][:, :3]), axis=1)
outside_delta = float(color_delta[outside].max())
matrix_delta = float(np.max(np.abs(before['matrices']-after['matrices'])))
surface_delta = max(float(np.linalg.norm(before['surfaces'][frame]-after['surfaces'][frame], axis=1).max()) for frame in before['surfaces'])
checks = [dict(id='geometry_uv_weights_exact', passed=before['hashes'] == after['hashes']),
          dict(id='non_color_images_exact', passed=before['channels'] == after['channels']),
          dict(id='bone_names_exact', passed=before['names'] == after['names']),
          dict(id='bone_transforms_330frames', passed=matrix_delta <= 1e-7, maximum_delta=matrix_delta),
          dict(id='evaluated_surfaces_19frames', passed=surface_delta <= 1e-7, maximum_delta_m=surface_delta),
          dict(id='unpainted_atlas_preservation', passed=outside_delta <= 1/255+1e-6, maximum_channel_delta=outside_delta,
               tolerance=1/255+1e-6, tested_pixels=int(outside.sum()), boundary_exclusion_pixels=1),
          dict(id='nonempty_color_refinement', passed=bool((color_delta > 1/255).any()), changed_pixels=int((color_delta > 1/255).sum()))]
report = dict(status='passed-color-only-invariants; visual/material acceptance open' if all(c['passed'] for c in checks) else 'needs-review',
              source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(), candidate_sha256=hashlib.sha256(candidate.read_bytes()).hexdigest(),
              mask_sha256=hashlib.sha256(mask_path.read_bytes()).hexdigest(), cpu_only=True, checks=checks,
              limitations=['Does not accept generated detail registration, PBR realism or inherited motion/collision defects.'])
(out/'validation.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
if not all(check['passed'] for check in checks):
    raise SystemExit(2)
