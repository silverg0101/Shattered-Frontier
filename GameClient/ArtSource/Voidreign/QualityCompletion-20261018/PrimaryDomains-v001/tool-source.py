"""Source-indexed primary ownership proposals from authored seeds and mesh paths.

Disconnected islands stay unresolved; spatial proximity is not attachment proof.
"""
import argparse
import hashlib
import heapq
import json
from pathlib import Path
import numpy as np

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text())
assert cfg['schema_version'] == 1 and cfg['units'] == 'metres' and not out.exists()
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
for key in ['semantic_selection', 'landmark_profile']:
    assert cfg[key] in cfg['input_sha256']
landmark_cfg = json.loads(Path(cfg['landmark_profile']).read_text())
landmarks = {r['id']: np.asarray(r['position_m']) for r in landmark_cfg['landmarks']}
with np.load(cfg['semantic_selection']) as archive:
    canonical = archive['canonical_node']
    points = archive['unique_world_vertices'].astype(np.float64)
    edges = archive['edges']
    labels = archive['labels']
count = len(points)
seed_owner = np.zeros(count, np.int16)
body = np.zeros(count, bool)
np.logical_or.at(body, canonical, labels == 0)
body &= points[:, 2] >= cfg['body_seed_minimum_z_m']
seed_owner[body] = cfg['body_owner_id']
seed_rows = []
for leg in cfg['legs']:
    mask = np.zeros(count, bool)
    for segment in leg['seed_segments']:
        first, second = (landmarks[k] for k in segment['landmarks'])
        delta = second - first
        assert delta @ delta > 1e-8 and 0 < segment['radius_m'] < .5
        t = np.clip((points-first) @ delta / (delta @ delta), 0, 1)
        mask |= np.linalg.norm(points-first-t[:, None]*delta, axis=1) <= segment['radius_m']
    mask &= (points[:, 2] <= cfg['leg_seed_maximum_z_m']) & (seed_owner == 0)
    allowed = np.zeros(count, bool)
    np.logical_or.at(allowed, canonical, labels == 0)
    mask &= allowed
    assert mask.sum() >= cfg['minimum_seed_vertices']
    seed_owner[mask] = leg['owner_id']
    seed_rows.append(dict(id=leg['id'], owner_id=leg['owner_id'], canonical_seeds=int(mask.sum())))
adj = [[] for _ in range(count)]
for (i, j), length in zip(edges, np.linalg.norm(points[edges[:, 0]]-points[edges[:, 1]], axis=1)):
    if i == j:
        continue
    adj[int(i)].append((int(j), float(length)))
    adj[int(j)].append((int(i), float(length)))
distance = np.full(count, np.inf)
owners = seed_owner.copy()
queue = []
for index in np.flatnonzero(seed_owner):
    distance[index] = 0
    heapq.heappush(queue, (0., int(index)))
while queue:
    d, i = heapq.heappop(queue)
    if d != distance[i]:
        continue
    for j, length in adj[i]:
        proposed = d + length
        if proposed < distance[j] and seed_owner[j] == 0:
            distance[j], owners[j] = proposed, owners[i]
            heapq.heappush(queue, (proposed, j))
source_owners = owners[canonical]
# Existing semantic Mane/Tail are separately owned and must not become leg skin.
source_owners[labels != 0] = 0
selections = {row['id']: np.flatnonzero(source_owners == row['owner_id']).astype(np.int32) for row in cfg['legs']}
selections['body-seeded'] = np.flatnonzero(source_owners == cfg['body_owner_id']).astype(np.int32)
selections['unresolved-nonhair'] = np.flatnonzero((source_owners == 0) & (labels == 0)).astype(np.int32)
out.mkdir(parents=True)
np.savez_compressed(out / 'primary-domains.npz', owner_per_vertex=source_owners, canonical_owner=owners,
    geodesic_seed_distance_m=distance, **selections)
(out / 'profile.json').write_bytes(profile.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
report = dict(schema_version=1, status='primary-domain-proposal-needs-review', cpu_only=True,
    input_sha256=cfg['input_sha256'], profile_sha256=sha(profile), tool_sha256=sha(__file__),
    seeds=seed_rows, source_vertices=len(canonical), canonical_vertices=count,
    domain_vertices={k:len(v) for k,v in selections.items()},
    limitations=['Geodesic paths improve ownership proposals but fused false bridges can still misassign tack.',
     'Disconnected nonhair islands stay unresolved, never nearest-bound or deleted automatically.',
     'Lower-leg seeds do not define joint centers, final upper-leg blend weights or articulated acceptance.'])
(out / 'domain-report.json').write_text(json.dumps(report, indent=2))
for path, expected in cfg['input_sha256'].items():
    assert sha(path) == expected, path
print(json.dumps(report['domain_vertices']))
