"""Compare frozen source-bound UV class-mask coverage, without accepting it."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args();profile,out=Path(a.profile).resolve(),Path(a.output).resolve()
cfg=json.loads(profile.read_text(encoding='utf-8'));assert cfg['schema_version']==1 and not out.exists()
assert 2<=len(cfg['mask_reports'])<=4
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
approval=json.loads(Path(cfg['approval_profile']).read_text(encoding='utf-8'))
face_to_item={face:x['id'] for x in approval['items'] for face in x['face_ids']}
with np.load(cfg['uv_topology']) as ar:uv=ar['loop_uv'].astype(float);faces=ar['triangles']
with np.load(cfg['rest_selection']) as ar:
    assert np.array_equal(ar['triangles'],faces)
    rest=ar['vertices']
rows=[];resolutions=set();degenerate=None
for path in cfg['mask_reports']:
    path=Path(path);r=json.loads(path.read_text(encoding='utf-8'));resolution=r['resolution']
    assert resolution not in resolutions;resolutions.add(resolution)
    assert r['input_sha256'][cfg['uv_topology']]==cfg['input_sha256'][cfg['uv_topology']]
    coverage=path.parent/'coverage.npz';assert sha(coverage)==r['outputs']['coverage.npz']
    with np.load(coverage) as ar:
        counts,safe,approved=ar['face_sample_counts'],ar['nonconflicting_face_pixel_counts'],ar['approved_faces']
        missing=ar['approved_faces_without_safe_pixel']
        if degenerate is None:degenerate=ar['uv_degenerate_faces']
        else:assert np.array_equal(degenerate,ar['uv_degenerate_faces'])
        cases=[]
        for face in missing:
            triangle=uv[face];d1,d2=triangle[1]-triangle[0],triangle[2]-triangle[0];world=rest[faces[face]]
            cases.append(dict(source_face=int(face),patch=face_to_item[int(face)],
                uv_area_pixels=float(abs(d1[0]*d2[1]-d1[1]*d2[0])*.5*resolution**2),
                source_rest_surface_area_m2=float(np.linalg.norm(np.cross(world[1]-world[0],world[2]-world[0]))*.5)))
        rows.append(dict(resolution=resolution,approved_faces_without_safe_pixel=len(missing),
            approved_faces_with_conflicted_samples=int((counts[approved]>safe[approved]).sum()),
            overlap_pixels=r['overlap_pixels'],all_overlap_candidate_faces=int((counts>safe).sum()),
            extra_triangle_sample_hits=int(counts.sum()-int((ar['source_face_owner']>=0).sum())),
            class_pixel_counts=r['class_pixel_counts'],missing_cases=cases))
points=rest[faces[degenerate]]
areas=np.linalg.norm(np.cross(points[:,1]-points[:,0],points[:,2]-points[:,0]),axis=1)*.5
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='source-bound-UV-resolution-diagnosis-only',input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(out/'tool-source.py'),comparisons=sorted(rows,key=lambda r:r['resolution']),uv_degenerate_source_faces=degenerate.tolist(),uv_degenerate_faces_have_nonzero_rest_surface_area=bool((areas>0).all()),uv_degenerate_rest_area_m2=areas.tolist(),
    limitations=['Sampled pixel-center coverage, not filtered/mipped coverage or material/engine acceptance.',
        'UV reuse/overlap is not automatically an error or permission to reunwrap/delete geometry.',
        'Exact face-level material responses remain available for wholly approved subpixel faces.',
        'Class IDs0/254/255 are unknown/overlap/unused, never PBR scalar values.'])
(out/'comparison.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(dict(status=report['status'],comparisons=[{k:r[k] for k in ['resolution','approved_faces_without_safe_pixel','approved_faces_with_conflicted_samples','overlap_pixels']} for r in report['comparisons']],uv_degenerate_faces=len(degenerate))))
