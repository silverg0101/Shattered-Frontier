"""Foundation gate, independent of the builder's footprint code (owner: houses floated except for a front strip).

Per house, every 50 cm along each side of the house's own frame, a horizontal ray at 60 cm above the mesh's lowest point
finds the ground-floor wall face (rays that reach > 150 cm inside the bounds went through an opening and are skipped;
samples past the two perpendicular main walls graze corner trims and are skipped; faces > 20 cm out of the side's main wall
are bays/porches, counted as overhang points and not tested). At each wall point the wall foot is found by stepping that ray
down (2 cm, then 0.5 cm) while it still hits the wall or solid mesh standing out of it; then a ray straight down, 2 cm
outside the face, must hit the house's footing (plinth or the stone base under its own doorstep) before the terrain (no
open air under the wall), with the footing top at most 2 cm under the wall foot (a foot set into the stone is fine), and
the footing bottom >= 100 cm under the terrain there.
Prints FOUNDATION_CHECK houses N points P failures F and the worst point per failing house; writes foundation-check.json.
"""
import json, math
from pathlib import Path
import unreal as u

actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
by = {a.get_actor_label(): a for a in actors}
terrain = next(a for a in actors if isinstance(a, u.StaticMeshActor) and a.static_mesh_component.static_mesh
               and a.static_mesh_component.static_mesh.get_name() == 'SM_Silverbrook_Terrain')


def down(comp, x, y, z0, z1):
    h = comp.line_trace_component(u.Vector(x, y, z0), u.Vector(x, y, z1), True, False)
    return h[0].z if h else None


rows, total = [], 0
for house in actors:
    if not isinstance(house, u.StaticMeshActor) or not str(house.get_folder_path()).startswith('Districts/'):
        continue
    mesh = house.static_mesh_component.static_mesh
    if not mesh or not mesh.get_name().startswith('SM_Silverbrook') or '_Door' in mesh.get_name():
        continue
    label = house.get_actor_label(); plinth = by.get(label + '_Plinth')
    if not plinth:
        rows.append(dict(house=label, points=0, failures=1, worst='missing plinth')); continue
    c = house.static_mesh_component; pc = plinth.static_mesh_component
    parts = [c] + [x.static_mesh_component for x in house.get_attached_actors() if isinstance(x, u.StaticMeshActor)]   # doors belong to the wall
    b = mesh.get_bounding_box(); T = house.get_actor_transform()
    W = lambda q: T.transform_location(u.Vector(*q))
    # Footing = the plinth plus the stone bases under the mesh's own doorsteps (label_StoopBase*); risers (label_Stoop<k>) are steps, not footing.
    footing = [pc] + [a.static_mesh_component for l, a in by.items() if l.startswith(label + '_StoopBase')]
    pts, fails, overhangs = [], [], 0
    def ray(ax, sg, t, z):   # horizontal ray from outside the bounds toward the house's centre line
        face = (b.max if sg > 0 else b.min).x if ax == 0 else (b.max if sg > 0 else b.min).y
        p0 = [0, 0, z]; p1 = [0, 0, z]; p0[ax] = face + sg * 100; p0[1 - ax] = t; p1[1 - ax] = t
        hs = [h for h in (k.line_trace_component(W(p0), W(p1), True, False) for k in parts) if h]
        if not hs: return None
        h = min(hs, key=lambda h: (h[0] - W(p0)).length()); q = T.inverse_transform_location(h[0])
        return (q.x if ax == 0 else q.y), h[0]
    sides = {}
    for ax, sg in ((0, -1), (0, 1), (1, -1), (1, 1)):
        face = (b.max if sg > 0 else b.min).x if ax == 0 else (b.max if sg > 0 else b.min).y
        lo, hi = (b.min.y, b.max.y) if ax == 0 else (b.min.x, b.max.x)
        hits = [(t, h) for t, h in ((lo + 25 + 50 * i, ray(ax, sg, lo + 25 + 50 * i, b.min.z + 60)) for i in range(int((hi - lo - 25) // 50) + 1))
                if h and abs(face - h[0]) <= 150]   # deeper = through an opening
        if not hits: continue
        ds = sorted(h[0] for _, h in hits); groups = [[ds[0]]]
        for d in ds[1:]: (groups[-1].append(d) if d - groups[-1][-1] <= 12 else groups.append([d]))
        big = max(groups, key=len); sides[(ax, sg)] = (sorted(big)[len(big) // 2], hits)   # this side's main wall face
    for (ax, sg), (dominant, hits) in sides.items():
        face = (b.max if sg > 0 else b.min).x if ax == 0 else (b.max if sg > 0 else b.min).y
        # Only the stretch between the two perpendicular main walls: past them a ray grazes corner trims and drainpipes.
        ends = [sides[(1 - ax, e)][0] for e in (-1, 1) if (1 - ax, e) in sides]
        for t, hit in hits:
            if len(ends) == 2 and not (min(ends) + 10 <= t <= max(ends) - 10): continue
            if sg * (hit[0] - dominant) > 20:   # bay, porch or stoop standing > 20 cm out of the main wall: not on the footing
                overhangs += 1; continue
            z = b.min.z + 60; d0 = hit[0]; ref = min(hit[0], dominant, key=lambda d: sg * d)   # the more inward of hit and wall
            ok = lambda q: q is not None and abs(face - q[0]) <= 150 and sg * (q[0] - ref) <= 150
            while z > b.min.z - 20:
                # Follow the wall down: solid mesh standing out of it (base course, ledge, stoop steps) and the wall again
                # below a ledge are followed; a miss, or a ray that runs > 150 cm into the house (an opening), ends it.
                nxt = ray(ax, sg, t, z - 2)
                if not ok(nxt): break
                z -= 2; hit = nxt; d0 = nxt[0]
            while ok(ray(ax, sg, t, z - .5)):   # refine the foot to 0.5 cm
                z -= .5; hit = ray(ax, sg, t, z); d0 = hit[0]
            foot = hit[1].z
            out = [0, 0, 0]; out[ax] = d0 + sg * 2; out[1 - ax] = t   # 2 cm outside the wall face
            o = T.transform_location(u.Vector(out[0], out[1], z))
            tops = [(z_, f_) for z_, f_ in ((down(f_, o.x, o.y, foot + 10, foot - 400), f_) for f_ in footing) if z_ is not None]
            top, hitc = max(tops, key=lambda q: q[0]) if tops else (None, None)
            ground = down(terrain.static_mesh_component, o.x, o.y, foot + 10, foot - 5000)
            r = dict(x=round(o.x), y=round(o.y), foot=round(foot, 1), side=('x', 'y')[ax] + '-+'[sg > 0], local=(round(d0), round(t), round(z - b.min.z, 1)))
            if top is None or (ground is not None and ground > top):
                r['error'] = 'open air under the wall'; r['air_cm'] = None if ground is None else round(foot - ground, 1)
            else:
                bottom = hitc.get_owner().get_actor_location().z - 50 * hitc.get_owner().get_actor_scale3d().z
                r['gap_cm'] = round(foot - top, 2); r['buried_cm'] = None if ground is None else round(ground - bottom, 1)
                if not (-10 <= r['gap_cm'] <= 2): r['error'] = 'plinth top not within 2 cm under the wall foot'   # < 0: foot set into the stone
                elif r['buried_cm'] is None or r['buried_cm'] < 100: r['error'] = 'plinth buried < 100 cm'
            pts.append(r)
            if 'error' in r: fails.append(r)
    total += len(pts)
    worst = fails[0] if fails else max(pts, key=lambda r: r['gap_cm'], default=None)
    rows.append(dict(house=label, mesh=mesh.get_name(), points=len(pts), overhang_points=overhangs, min_buried_cm=min((q['buried_cm'] for q in pts if q.get('buried_cm') is not None), default=None), failures=len(fails) + (0 if pts else 1), worst=worst if pts else 'no wall points', failed=fails))
bad = [r for r in rows if r['failures']]
report = dict(houses=len(rows), points=total, failing_houses=len(bad), failing_points=sum(r['failures'] for r in bad), rows=rows)
Path(__file__).with_name('foundation-check.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
print('FOUNDATION_CHECK houses', len(rows), 'points', total, 'overhang_points_skipped', sum(r.get('overhang_points', 0) for r in rows), 'failing_houses', len(bad), 'failing_points', report['failing_points'],
      'min_points_per_house', min(r['points'] for r in rows))
allp = [w for r in rows if isinstance(r.get('worst'), dict) for w in [r['worst']] if 'gap_cm' in w]
print('FOUNDATION_WORST max_gap_cm', max(w['gap_cm'] for w in allp), 'min_gap_cm', min(w['gap_cm'] for w in allp),
      'min_buried_cm', min(r['min_buried_cm'] for r in rows if r.get('min_buried_cm') is not None))
for r in bad[:40]:
    print('FOUNDATION_FAIL', r['house'], r['failures'], '/', r['points'], r['worst'])
assert not bad, f'{len(bad)} houses fail'
