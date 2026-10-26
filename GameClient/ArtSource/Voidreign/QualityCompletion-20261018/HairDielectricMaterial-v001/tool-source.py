"""Prototype a declared semantic face material; verify it in a fresh process."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(8*1024*1024),b''):h.update(block)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True);p.add_argument('--verify-build')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and not out.exists()
assert cfg['metallic']==0 and 1<=len(cfg['semantic_labels'])<=8
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
assert cfg['source'] in cfg['input_sha256'] and cfg['selection'] in cfg['input_sha256']
with np.load(cfg['selection']) as ar:labels=ar['labels']
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));obj=bpy.data.objects[cfg['object']]
faces=np.array([p.vertices[:] for p in obj.data.polygons]);assert faces.shape[1]==3 and len(labels)==len(obj.data.vertices)
assert len(obj.data.materials)==1 and obj.data.materials[0].name==cfg['original_material']
mask=np.isin(labels,cfg['semantic_labels']);whole=mask[faces].all(1);mixed=mask[faces].any(1)&~whole
assert whole.any()
selected=np.flatnonzero(whole)
source_indices=np.array([p.material_index for p in obj.data.polygons]);assert (source_indices==0).all()
if a.verify_build:
    build=Path(a.verify_build);report=json.loads((build/'material-change.json').read_text())
    assert report['profile_sha256']==sha(profile) and report['source_sha256']==cfg['input_sha256'][cfg['source']]
    candidate=build/report['candidate'];assert sha(candidate)==report['candidate_sha256']
    with np.load(build/'face-selection.npz') as ar:assert np.array_equal(selected,ar['selected_faces'])
    bpy.ops.wm.open_mainfile(filepath=str(candidate.resolve()));obj=bpy.data.objects[cfg['object']]
    indices=np.array([p.material_index for p in obj.data.polygons]);expected=source_indices.copy();expected[selected]=1
    assert np.array_equal(indices,expected) and len(obj.data.materials)==2
    assert obj.data.materials[0].name==cfg['original_material'] and obj.data.materials[1].name==cfg['new_material']
    mat=obj.data.materials[1];shader=mat.node_tree.nodes[cfg['shader_node']]
    assert not shader.inputs['Metallic'].is_linked and shader.inputs['Metallic'].default_value==cfg['metallic']
    assert shader.inputs['Base Color'].is_linked and shader.inputs['Roughness'].is_linked
    out.mkdir(parents=True);(out/'validation.json').write_text(json.dumps(dict(status='passed-exact-semantic-material-assignment-only',cpu_only=True,source_sha256=cfg['input_sha256'][cfg['source']],candidate_sha256=sha(candidate),profile_sha256=sha(profile),tool_sha256=sha(__file__),selected_faces=len(selected),mixed_boundary_faces_retained=int(mixed.sum()),checks=dict(exact_face_assignment=True,original_slot_retained=True,declared_metallic_zero=True,base_color_and_roughness_links_retained=True),limitations=['Semantic labels remain proposals; visual coverage approval is separate.','Use independent rest/weight/bind/image/animation retention checks; this checks assignment only.']),indent=2))
else:
    old=obj.data.materials[0];mat=old.copy();mat.name=cfg['new_material'];assert mat.name==cfg['new_material']
    shader=mat.node_tree.nodes[cfg['shader_node']];socket=shader.inputs['Metallic']
    for link in list(socket.links):mat.node_tree.links.remove(link)
    socket.default_value=cfg['metallic'];mat.metallic=cfg['metallic'];obj.data.materials.append(mat)
    for i in selected:obj.data.polygons[int(i)].material_index=1
    out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
    np.savez_compressed(out/'face-selection.npz',selected_faces=selected,mixed_faces=np.flatnonzero(mixed))
    candidate=out/'Mount-MaterialPrototype.blend';bpy.ops.wm.save_as_mainfile(filepath=str(candidate),check_existing=False)
    (out/'material-change.json').write_text(json.dumps(dict(schema_version=1,status='semantic-material-prototype-needs-review',cpu_only=True,source=cfg['source'],source_sha256=cfg['input_sha256'][cfg['source']],candidate=candidate.name,candidate_sha256=sha(candidate),profile_sha256=sha(profile),tool_sha256=sha(__file__),selected_faces=len(selected),mixed_boundary_faces_retained=int(mixed.sum()),change='Copy original material to selected whole faces, disconnect Metallic only and set0; retain albedo/roughness/image links. Original material and all mixed/unselected faces retained.',limitations=['Semantic selection is a proposal, not full hair coverage approval.','No geometry, UV, weights, rig, gait, roughness, normal or emission edits.','Source inherits unresolved pose/collision defects; not a promoted full-model candidate.']),indent=2))
assert sha(cfg['source'])==cfg['input_sha256'][cfg['source']]
print(json.dumps(dict(status='verified-assignment-only' if a.verify_build else 'material-prototype-needs-review',selected_faces=len(selected),mixed_faces=int(mixed.sum()))),flush=True)
