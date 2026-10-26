"""Read-only closed convex-envelope fit against source-indexed feature motion.

This rejects unsuitable contact representations before a physics bake. It
does not replace the source collider, approve deformation, or spend a bake.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import bmesh
import numpy as np
from mathutils.bvhtree import BVHTree


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--profile', required=True)
parser.add_argument('--output', required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
profile = Path(args.profile).resolve()
cfg = json.loads(profile.read_text(encoding='utf-8-sig'))
out = Path(args.output).resolve()
assert cfg['schema_version'] == 1 and not out.exists()
assert cfg['units'] == 'metres'
assert 0 < cfg['required_free_clearance_m'] <= .1
assert 0 < cfg['numerical_tolerance_m'] <= 1e-5
assert 1 <= cfg['maximum_frames'] <= 500
assert 4 <= cfg['maximum_context_vertices'] <= 20000
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
with np.load(cfg['capture_points']) as ar:
    frames = ar['frames']
    feature_vertices = ar['feature_vertices']
    feature_points = ar['feature_world_points']
    context = ar['context_world_points']
    context_vertices = ar['context_vertices']
    context_faces = ar['context_faces']
with np.load(cfg['region_roots']) as ar:
    roots = ar['root_vertices']
assert len(frames) <= cfg['maximum_frames']
assert context.shape[1] <= cfg['maximum_context_vertices']
free = ~np.isin(feature_vertices, roots)
assert free.any() and (~free).any()
assert np.isfinite(context).all() and np.isfinite(feature_points).all()
def convex_signed(input_points, points):
    bm = bmesh.new()
    for point in input_points:
        bm.verts.new(tuple(point))
    bm.verts.ensure_lookup_table()
    bmesh.ops.convex_hull(bm, input=list(bm.verts), use_existing_faces=False)
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    bm.verts.index_update()
    coordinates = np.array([v.co[:] for v in bm.verts])
    faces = np.array([[v.index for v in face.verts] for face in bm.faces], int)
    # Unused input vertices are deliberately omitted from hull membership.
    hull_vertices = np.unique(faces)
    center = coordinates[hull_vertices].mean(axis=0)
    triangles = coordinates[faces]
    normals = np.cross(triangles[:, 1] - triangles[:, 0],
                       triangles[:, 2] - triangles[:, 0])
    lengths = np.linalg.norm(normals, axis=1)
    assert (lengths > 0).all()
    normals /= lengths[:, None]
    reverse = np.einsum('ij,ij->i', normals, triangles[:, 0] - center) < 0
    faces[reverse] = faces[reverse, ::-1]
    normals[reverse] *= -1
    triangles = coordinates[faces]
    edges = np.sort(np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]],
                                    faces[:, [2, 0]]]), axis=1)
    _, counts = np.unique(edges, axis=0, return_counts=True)
    assert (counts == 2).all(), 'Envelope must be closed/manifold'
    # All convex face halfspaces must contain an interior point.
    assert (np.einsum('ij,ij->i', center - triangles[:, 0], normals) <= 0).all()
    plane_gap = (points @ normals.T - np.einsum('ij,ij->i', triangles[:, 0], normals)[None, :]).max(axis=1)
    inside = plane_gap < -cfg['numerical_tolerance_m']
    bvh = BVHTree.FromPolygons(coordinates.tolist(), faces.tolist(), all_triangles=True)
    unsigned = np.array([bvh.find_nearest(tuple(p))[3] for p in points])
    signed = np.where(inside, -unsigned, unsigned)
    remap = np.full(len(coordinates), -1, int)
    remap[hull_vertices] = np.arange(len(hull_vertices))
    result = coordinates[hull_vertices].copy(), remap[faces].copy(), signed
    bm.free()
    return result


partitions = [('whole-context', np.arange(context.shape[1]))]
if cfg.get('segment_by_primary_weights', False):
    import bpy
    bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['primary_source']).resolve()))
    ob = bpy.data.objects[cfg['source_object']]
    bones = cfg['allowed_primary_bones']
    assert 1 <= len(bones) <= 16 and len(bones) == len(set(bones))
    group_ids = [ob.vertex_groups[name].index for name in bones]
    weights = np.zeros((len(context_vertices), len(bones)))
    foreign = []
    for i, vertex in enumerate(context_vertices):
        other = {}
        for group in ob.data.vertices[int(vertex)].groups:
            if group.group in group_ids:
                weights[i, group_ids.index(group.group)] = group.weight
            elif ob.vertex_groups[group.group].name.startswith('PRI.'):
                other[ob.vertex_groups[group.group].name] = group.weight
        if cfg.get('partition_contract') == 'allowed-bone-must-dominate-each-context-vertex':
            assert max(other.values(), default=0) <= weights[i].max(), dict(source_vertex=int(vertex), foreign=other, allowed=weights[i].tolist())
        else:
            assert max(other.values(), default=0) <= cfg['foreign_primary_weight_tolerance'], dict(source_vertex=int(vertex), foreign=other)
        if other:
            foreign.append(dict(source_vertex=int(vertex), weights=other))
    assert (weights.sum(axis=1) > 0).all()
    source_triangles = np.array([ob.data.polygons[int(i)].vertices[:] for i in context_faces])
    local_triangles = np.searchsorted(context_vertices, source_triangles)
    assert np.array_equal(context_vertices[local_triangles], source_triangles)
    assignments = weights[local_triangles].sum(axis=1).argmax(axis=1)
    partitions = [(name, np.unique(local_triangles[assignments == i])) for i, name in enumerate(bones) if (assignments == i).any()]
    assert all(len(indices) >= 4 for name, indices in partitions)
    # Every context triangle is wholly enclosed by one assigned convex part.
    assert sum(int((assignments == i).sum()) for i in range(len(bones))) == len(source_triangles)

rows = []
baseline = {}
for index, frame in enumerate(frames):
    signed_parts = []
    vertex_count = face_count = 0
    for part_index, (name, selected) in enumerate(partitions):
        hp, hf, signed = convex_signed(context[index, selected], feature_points[index])
        signed_parts.append(signed)
        vertex_count += len(hp)
        face_count += len(hf)
        if index == 0:
            baseline['vertices_' + str(part_index)] = hp
            baseline['triangles_' + str(part_index)] = hf
    # Union membership: any negative per-part sign is inside the union.
    # Min is a conservative per-part clearance diagnostic, not the exact union SDF.
    signed = np.min(signed_parts, axis=0)
    inside = signed < -cfg['numerical_tolerance_m']
    worst = int(np.flatnonzero(free)[np.argmin(signed[free])])
    rows.append(dict(frame=int(frame), hull_vertices=vertex_count,
        hull_triangles=face_count, free_inside_count=int((inside & free).sum()),
        free_below_required_clearance_count=int((free & (signed < cfg['required_free_clearance_m'])).sum()),
        minimum_free_signed_clearance_m=float(signed[free].min()),
        worst_source_vertex=int(feature_vertices[worst]),
        maximum_root_inside_depth_m=float(np.maximum(-signed[~free], 0).max())))
    if index == 0:
        baseline.update(feature_signed_clearance_m=signed, source_vertices=feature_vertices)
status = ('rejected-convex-envelope-interferes-with-source-feature'
          if any(row['free_below_required_clearance_count'] for row in rows)
          else 'sampled-envelope-fit-only-needs-design-review')
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out / 'baseline-envelope.npz', **baseline)
report = dict(schema_version=1, status=status, cpu_only=True,
    profile_sha256=sha(profile), tool_sha256=sha(__file__),
    input_sha256=cfg['input_sha256'], required_free_clearance_m=cfg['required_free_clearance_m'],
    frames=len(frames), free_source_vertices=int(free.sum()), parts=[dict(name=name, context_vertices=len(indices)) for name, indices in partitions], source_secondary_primary_influences=foreign if cfg.get('segment_by_primary_weights') else [], curves=rows,
    limitations=['Per-frame convex envelopes are fit diagnostics, not a temporally coherent runtime collider.',
        'Part-union minimum signed distance is a conservative per-part diagnostic, not exact union depth.',
        'Primary-weight segmentation is a proposed partition, not semantic/anatomical approval.',
        'Source-context envelope can include ornaments/concavities and overfill intended free space.',
        'Sampled source integer frames only; no cloth rebake, full-body/self/swept collision acceptance.',
        'No source model, weight, animation, contact mask or attempt counter changed.'])
(out / 'fit-report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(dict(status=status, frames=len(frames),
    worst=min(rows, key=lambda row: row['minimum_free_signed_clearance_m']))), flush=True)
