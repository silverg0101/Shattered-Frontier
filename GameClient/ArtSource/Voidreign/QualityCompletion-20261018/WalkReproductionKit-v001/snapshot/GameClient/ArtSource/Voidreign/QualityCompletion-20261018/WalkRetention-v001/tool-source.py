"""Compare unchanged indexed mesh/appearance data and report rig rebind changes.

Fresh-process diagnostic only. Identical vertex/face order is required; this is
not a topology correspondence tool or an appearance/animation acceptance test.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path
import bpy
import numpy as np


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def signature(value):
    if isinstance(value, np.ndarray):
        return dict(shape=list(value.shape), dtype=str(value.dtype),
                    sha256=hashlib.sha256(value.tobytes()).hexdigest())
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def plain(value):
    if isinstance(value, (str, float, int, bool)) or value is None:
        return value
    try:
        return list(value)
    except TypeError:
        return str(value)


def capture(path, name):
    bpy.ops.wm.open_mainfile(filepath=str(path.resolve()))
    bpy.context.scene.frame_set(1)
    obj = bpy.data.objects[name]
    mesh = obj.data
    assert not mesh.shape_keys, 'Shape-key retention needs a separate contract'
    assert all(len(f.vertices) == 3 for f in mesh.polygons)
    state = dict(
        positions=signature(np.array([v.co[:] for v in mesh.vertices], np.float32)),
        faces=signature(np.array([f.vertices[:] for f in mesh.polygons], np.int32)),
        edges=signature(np.array([e.vertices[:] for e in mesh.edges], np.int32)),
        loop_vertices=signature(np.array([v.vertex_index for v in mesh.loops], np.int32)),
        uv={l.name: signature(np.array([v.uv[:] for v in l.data], np.float32)) for l in mesh.uv_layers},
        colors={l.name: dict(domain=l.domain, data_type=l.data_type,
            data=signature(np.array([v.color[:] for v in l.data], np.float32))) for l in mesh.color_attributes},
        face_materials=signature(np.array([f.material_index for f in mesh.polygons], np.int32)),
        smooth_flags=signature(np.array([f.use_smooth for f in mesh.polygons], np.bool_)),
        custom_normals=mesh.has_custom_normals,
        corner_normals=signature(np.array([v.vector[:] for v in mesh.corner_normals], np.float32)),
        active_uv=mesh.uv_layers.active.name if mesh.uv_layers.active else None)
    materials = []
    images = {}
    for slot in obj.material_slots:
        mat = slot.material
        if mat is None:
            materials.append(None)
            continue
        info = dict(name=mat.name, use_nodes=mat.use_nodes,
                    diffuse_color=list(mat.diffuse_color),
                    use_backface_culling=mat.use_backface_culling,
                    metallic=mat.metallic, roughness=mat.roughness)
        if mat.node_tree:
            info['nodes'] = []
            for node in sorted(mat.node_tree.nodes, key=lambda n: n.name):
                n = dict(name=node.name, type=node.bl_idname,
                         inputs={s.identifier: plain(s.default_value) for s in node.inputs if hasattr(s, 'default_value')})
                for key in ['operation', 'blend_type', 'interpolation', 'extension', 'projection', 'attribute_name', 'layer_name', 'uv_map']:
                    if hasattr(node, key):
                        n[key] = plain(getattr(node, key))
                if getattr(node, 'image', None):
                    im = node.image
                    n['image'] = im.name
                    if im.name not in images:
                        values = np.empty(len(im.pixels), np.float32)
                        im.pixels.foreach_get(values)
                        images[im.name] = dict(size=list(im.size), source=im.source,
                            colorspace=im.colorspace_settings.name, alpha_mode=im.alpha_mode,
                            pixels=signature(values))
                info['nodes'].append(n)
            info['links'] = sorted((l.from_node.name, l.from_socket.identifier,
                                    l.to_node.name, l.to_socket.identifier) for l in mat.node_tree.links)
        materials.append(info)
    state['material_signature'] = signature(materials)
    state['images'] = images
    names = {g.index: g.name for g in obj.vertex_groups}
    weights = [dict(sorted((names[g.group], g.weight) for g in v.groups)) for v in mesh.vertices]
    armatures = []
    for mod in obj.modifiers:
        if mod.type == 'ARMATURE':
            arm = mod.object
            armatures.append(dict(name=arm.name, matrix_world=[list(r) for r in arm.matrix_world],
                bones={b.name: dict(parent=b.parent.name if b.parent else None,
                    matrix_local=[list(r) for r in b.matrix_local]) for b in arm.data.bones}))
    return state, weights, armatures


p = argparse.ArgumentParser(description=__doc__)
for k in ['source', 'candidate', 'object', 'output']:
    p.add_argument('--' + k, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
source, candidate, out = map(lambda v: Path(v).resolve(), [a.source, a.candidate, a.output])
assert not out.exists()
source_hash, candidate_hash = sha(source), sha(candidate)
before, old_weights, old_bind = capture(source, a.object)
after, new_weights, new_bind = capture(candidate, a.object)
assert len(old_weights) == len(new_weights)
checks = {k: before[k] == after[k] for k in before}
changed = []
max_delta = 0.0
for i, (old, new) in enumerate(zip(old_weights, new_weights)):
    if old != new:
        changed.append(i)
        max_delta = max(max_delta, max(abs(old.get(k, 0) - new.get(k, 0)) for k in old.keys() | new.keys()))
assert sha(source) == source_hash and sha(candidate) == candidate_hash
out.mkdir(parents=True, exist_ok=False)
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out / 'changed-weight-vertices.npz', indices=np.array(changed, np.int32))
report = dict(status='passed-indexed-rest-retention-only' if all(checks.values()) else 'failed-rest-retention',
    cpu_only=True, source=str(source), source_sha256=source_hash,
    candidate=str(candidate), candidate_sha256=candidate_hash,
    tool_sha256=sha(__file__), object=a.object, checks=checks,
    before=before, after=after, changed_weight_vertex_count=len(changed),
    maximum_named_weight_delta=max_delta, bind_data_exact=old_bind == new_bind,
    source_bind_signature=signature(old_bind), candidate_bind_signature=signature(new_bind),
    limitations=['Indexed rest data only; no posed-surface, semantic or delivery acceptance.',
        'Weights and binds are reported, not assumed unchanged or automatically accepted.',
        'Material signature covers listed common node properties, inputs and links; not arbitrary nested groups, drivers or all shader properties.'])
(out / 'validation.json').write_text(json.dumps(report, indent=2))
print(json.dumps({k: report[k] for k in ['status', 'checks', 'changed_weight_vertex_count', 'maximum_named_weight_delta', 'bind_data_exact']}), flush=True)
