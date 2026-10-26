"""Read-only rigid-core/collar weight proposal on an existing linear-skin clip.

Evaluates every edge incident to a changed vertex, including unchanged boundary
vertices. Never writes a Blender scene or changes source vertex groups.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(8*1024*1024), b''):
            h.update(block)
    return h.hexdigest()

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text())
assert cfg['schema_version'] == 1 and cfg['units'] == 'metres' and not out.exists()
assert 1 <= cfg['frame_start'] <= cfg['frame_end'] <= 1000
assert 0 < cfg['baseline_tolerance_m'] <= 1e-5
assert cfg['maximum_influences'] == 4
for f, h in cfg['input_sha256'].items():
    assert sha(f) == h, f
contract = json.loads(Path(cfg['contract']).read_text())
assert contract['status'] == 'author-reviewed-selection-for-weight-proposal-only'
assert sha(contract['selection']) == contract['selection_sha256']
assert sha(contract['rest_selection']) == contract['rest_selection_sha256']
with np.load(contract['rest_selection']) as ar:
    faces, rest = ar['triangles'], ar['vertices'].astype(float)
with np.load(cfg['semantic_selection']) as ar:
    canonical = ar['canonical_node']
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
scene = bpy.context.scene
obj, rig = bpy.data.objects[cfg['object']], bpy.data.objects[cfg['rig_object']]
mods = [m for m in obj.modifiers if m.show_viewport]
assert len(mods) == 1 and mods[0].type == 'ARMATURE' and mods[0].object == rig
assert not mods[0].use_deform_preserve_volume and not mods[0].use_bone_envelopes
assert all(b.bbone_segments == 1 for b in rig.data.bones)
names = [b.name for b in rig.data.bones if b.use_deform]
bone_index = {n: i for i, n in enumerate(names)}
weights = np.zeros((len(obj.data.vertices), len(names)))
for v in obj.data.vertices:
    for g in v.groups:
        n = obj.vertex_groups[g.group].name
        if n in bone_index:
            weights[v.index, bone_index[n]] = g.weight
proposed = weights.copy()
selection = np.load(contract['selection'])
protected = np.zeros(len(weights), bool)
foot_rows = []
method = cfg.get('collar_method', 'linear-geodesic')
assert method in ['linear-geodesic', 'harmonic-dirichlet']
if method == 'harmonic-dirichlet':
    assert 1e-10 <= cfg['solver_tolerance'] <= 1e-6 and 1 <= cfg['solver_iterations'] <= 2000
    first_nodes = np.unique(canonical, return_index=True)[1]
    node_points = rest[first_nodes]
    node_weights = weights[first_nodes]
    assert np.max(abs(weights-node_weights[canonical])) <= 1e-7
    cf = canonical[faces]
    graph_edges = np.unique(np.sort(np.concatenate([cf[:,[0,1]],cf[:,[1,2]],cf[:,[2,0]]]),axis=1),axis=0)
    graph_edges = graph_edges[graph_edges[:,0] != graph_edges[:,1]]
    graph_length = np.linalg.norm(node_points[graph_edges[:,0]]-node_points[graph_edges[:,1]],axis=1)
    assert (graph_length > 0).all()
    conductance = 1/graph_length

def harmonic_collar(core, collar, bone):
    """Dirichlet Laplacian: core fixed rigid, outside fixed original weights."""
    unknown = np.unique(canonical[collar])
    lookup = np.full(len(node_weights), -1, int)
    lookup[unknown] = np.arange(len(unknown))
    fixed = node_weights.copy()
    fixed[np.unique(canonical[core])] = 0
    fixed[np.unique(canonical[core]),bone] = 1
    x0, x1 = graph_edges.T
    touch = (lookup[x0] >= 0) | (lookup[x1] >= 0)
    x0, x1, coeff = x0[touch], x1[touch], conductance[touch]
    # Directed rows with unknown source; neighbors can be fixed or unknown.
    row, neighbor, value = np.concatenate([lookup[x0],lookup[x1]]), np.concatenate([x1,x0]), np.tile(coeff,2)
    active = row >= 0
    row, neighbor, value = row[active], neighbor[active], value[active]
    column = lookup[neighbor]
    diagonal = np.bincount(row,weights=value,minlength=len(unknown))
    assert (diagonal > 0).all()
    rhs = np.zeros((len(unknown),len(names)))
    boundary = column < 0
    np.add.at(rhs,row[boundary],value[boundary,None]*fixed[neighbor[boundary]])
    interior = ~boundary
    def matvec(x):
        result = diagonal[:,None]*x
        np.add.at(result,row[interior],-value[interior,None]*x[column[interior]])
        return result
    x = node_weights[unknown].copy()
    r = rhs-matvec(x)
    z = r/diagonal[:,None]
    direction, rz = z.copy(), np.sum(r*z,axis=0)
    scale = np.maximum(np.linalg.norm(rhs,axis=0),1.)
    for iteration in range(cfg['solver_iterations']):
        if np.max(np.linalg.norm(r,axis=0)/scale) <= cfg['solver_tolerance']:
            break
        product = matvec(direction)
        denom = np.sum(direction*product,axis=0)
        alpha = np.divide(rz,denom,out=np.zeros_like(rz),where=abs(denom)>1e-30)
        x += direction*alpha
        r -= product*alpha
        z = r/diagonal[:,None]
        new_rz = np.sum(r*z,axis=0)
        beta = np.divide(new_rz,rz,out=np.zeros_like(rz),where=abs(rz)>1e-30)
        direction = z+direction*beta
        rz = new_rz
    residual = float(np.max(np.linalg.norm(rhs-matvec(x),axis=0)/scale))
    assert residual <= cfg['solver_tolerance'], residual
    assert x.min() >= -1e-7 and x.max() <= 1+1e-7
    # Only round-off clamping; no influence pruning or support expansion allowed.
    x = np.clip(x,0,1)
    assert ((x > 1e-10).sum(1) <= cfg['maximum_influences']).all(), 'Harmonic field crosses influence domains'
    return x[lookup[canonical[collar]]], dict(iterations=iteration+1,relative_residual=residual)

for foot in contract['feet']:
    core, collar = selection[foot['core_key']], selection[foot['collar_key']]
    blend = selection[foot['collar_blend_key']]
    assert len(core) == foot['expected_core_vertices'] and ((blend >= 0) & (blend <= 1)).all()
    assert not protected[np.union1d(core, collar)].any(), 'Overlapping part domains'
    j = bone_index[foot['rigid_bone']]
    proposed[core] = 0
    proposed[core, j] = 1
    solver = None
    if method == 'linear-geodesic':
        proposed[collar] *= (1-blend[:, None])
        proposed[collar, j] += blend
    else:
        proposed[collar], solver = harmonic_collar(core,collar,j)
    protected[np.union1d(core, collar)] = True
    assert np.all(proposed[core, j] == 1)
    foot_rows.append(dict(id=foot['id'], core_vertices=len(core), collar_vertices=len(collar),solver=solver))
changed = np.flatnonzero(np.max(abs(proposed-weights), axis=1) > 1e-10)
assert len(changed) > 0
assert ((proposed[changed] > 1e-10).sum(1) <= cfg['maximum_influences']).all(), 'Proposal adds a fifth influence'
assert np.max(abs(proposed[changed].sum(1)-1)) < 1e-5
first = np.unique(canonical, return_index=True)[1]
seam_error = float(np.max(abs(proposed-proposed[first][canonical])))
assert seam_error <= 1e-7, seam_error
edges = np.unique(np.sort(np.concatenate([faces[:, [0,1]], faces[:, [1,2]], faces[:, [2,0]]]), axis=1), axis=0)
mask = np.zeros(len(weights), bool)
mask[changed] = True
edges = edges[mask[edges].any(1)]
vertices, inverse = np.unique(edges, return_inverse=True)
local_edges = inverse.reshape((-1, 2))
assert np.min(weights[vertices].sum(1)) > .99999
coords = np.array([obj.data.vertices[int(i)].co[:] for i in vertices])
hom = np.column_stack([coords, np.ones(len(coords))])
scene.frame_set(cfg['frame_start'])
world = hom @ np.array(obj.matrix_world).T
rest_length = np.linalg.norm(world[local_edges[:, 0], :3]-world[local_edges[:, 1], :3], axis=1)
assert (rest_length > 0).all()
maxima = np.zeros((2, len(edges)))
curves, baseline_error = [], 0.
for frame in range(cfg['frame_start'], cfg['frame_end']+1):
    scene.frame_set(frame)
    matrices = np.array([rig.matrix_world @ rig.pose.bones[n].matrix @ rig.data.bones[n].matrix_local.inverted() @ rig.matrix_world.inverted() @ obj.matrix_world for n in names])
    bone_points = np.einsum('bij,vj->vbi', matrices[:, :3], hom)
    normal = np.einsum('vb,vbi->vi', weights[vertices], bone_points)
    counter = np.einsum('vb,vbi->vi', proposed[vertices], bone_points)
    ev = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    actual = np.array([(obj.matrix_world @ ev.data.vertices[int(i)].co)[:] for i in vertices])
    baseline_error = max(baseline_error, float(np.linalg.norm(normal-actual, axis=1).max()))
    assert baseline_error <= cfg['baseline_tolerance_m'], baseline_error
    values = []
    for points in (normal, counter):
        values.append(abs(np.linalg.norm(points[local_edges[:, 0]]-points[local_edges[:, 1]], axis=1)-rest_length))
    maxima = np.maximum(maxima, values)
    curves.append(dict(frame=frame, baseline_maximum_m=float(values[0].max()), proposed_maximum_m=float(values[1].max())))
    if frame % 12 == 1:
        print(json.dumps(dict(frame=frame, edges=len(edges))), flush=True)
delta = maxima[1]-maxima[0]
order = np.argsort(delta)[-20:][::-1]
report = dict(schema_version=1, status='read-only-rigid-skin-preflight-not-a-candidate', cpu_only=True,
    collar_method=method,
    source_sha256=sha(cfg['source']), input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__), profile_sha256=sha(profile),
    changed_vertices=len(changed), incident_edges=len(edges), checked_vertices=len(vertices), feet=foot_rows,
    maximum_baseline_position_error_m=baseline_error, proposed_seam_weight_error=seam_error,
    baseline_maximum_edge_change_m=float(maxima[0].max()), proposed_maximum_edge_change_m=float(maxima[1].max()),
    worsened_edges_above_reporting_floor=int((delta > cfg['delta_reporting_floor_m']).sum()),
    reporting_floor_m=cfg['delta_reporting_floor_m'],
    largest_regressions=[dict(vertices=edges[i].tolist(), baseline_m=float(maxima[0,i]), proposed_m=float(maxima[1,i]), increase_m=float(delta[i])) for i in order], curves=curves,
    limitations=['No model, animation, geometry, bind or vertex groups changed. Proposed weights are arrays only.', 'Reporting floor suppresses floating-point noise, not an acceptance threshold.', 'All incident edges checked; contacts, volume, self-collision and visual quality are separate gates.'])
assert sha(cfg['source']) == cfg['input_sha256'][cfg['source']]
out.mkdir(parents=True)
np.savez_compressed(out/'proposal-arrays.npz', changed_vertices=changed, weights=proposed[changed], bone_names=np.array(names), incident_edges=edges, baseline_edge_maxima=maxima[0], proposed_edge_maxima=maxima[1])
(out/'profile.json').write_bytes(profile.read_bytes())
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'preflight.json').write_text(json.dumps(report, indent=2))
print(json.dumps({k: report[k] for k in ('status','changed_vertices','incident_edges','baseline_maximum_edge_change_m','proposed_maximum_edge_change_m','worsened_edges_above_reporting_floor')}), flush=True)
