"""Engine delivery FBX from a mount clip library: root bone with per-clip root motion, FK-only rig, clean mesh.

Run: blender -b --factory-startup --python tools/Export-MountDelivery.py -- --profile <json> --output <new folder>
Strips diagnostic objects, constraints, modifiers and inert vertex groups; optional dielectric override
(Metallic input forced to a constant) for materials whose metallic map is known to be invalid.
Exports one FBX (mesh + armature + every clip as a take) and verifies it by reimport.
"""
import argparse, hashlib, json, math, sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Quaternion, Vector


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda: f.read(8 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


p = argparse.ArgumentParser()
p.add_argument('--profile', required=True); p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text())
assert cfg['schema_version'] == 1 and not out.exists()
for f, h in cfg['input_sha256'].items():
    assert sha(f) == h, f
lib = json.loads(Path(cfg['clips_report']).read_text())
assert lib['library_sha256'] == cfg['input_sha256'][cfg['library']]
clips = [c for c in lib['clips'] if c['passed'] or c['id'] in cfg.get('include_failed', [])]

bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['library']).resolve()))
scene = bpy.context.scene; vl = bpy.context.view_layer
obj, rig = bpy.data.objects[cfg['object']], bpy.data.objects[lib['rig_object']]
for o in (obj, rig):
    mw = o.matrix_world.copy(); o.parent = None; o.matrix_world = mw
for o in list(bpy.data.objects):
    if o not in (obj, rig): bpy.data.objects.remove(o)
for pb in rig.pose.bones:
    for c in list(pb.constraints): pb.constraints.remove(c)
for m in list(obj.modifiers):
    if not (m.type == 'ARMATURE' and m.object == rig): obj.modifiers.remove(m)
assert len(obj.modifiers) == 1
source_triangles = len(obj.data.polygons)
if cfg.get('target_triangles') and source_triangles > cfg['target_triangles']:
    # Game LOD0 budget: collapse decimation (interpolates UVs and vertex weights), applied ahead of the armature.
    dec = obj.modifiers.new('LOD0 budget', 'DECIMATE'); dec.decimate_type = 'COLLAPSE'; dec.use_collapse_triangulate = True
    dec.ratio = cfg['target_triangles'] / source_triangles
    bpy.context.view_layer.objects.active = obj; bpy.ops.object.modifier_move_to_index(modifier=dec.name, index=0)
    bpy.ops.object.modifier_apply(modifier=dec.name)
    import bmesh  # collapse can leave zero-area and duplicate triangles that FBX readers drop; remove them so counts round-trip exactly
    bm = bmesh.new(); bm.from_mesh(obj.data); bmesh.ops.dissolve_degenerate(bm, dist=1e-7, edges=bm.edges[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:]); seen = set(); dupes = []
    for f in bm.faces:  # duplicate triangles (same three vertices) are merged by FBX readers
        k = frozenset(v.index for v in f.verts); dupes.append(f) if k in seen else seen.add(k)
    bmesh.ops.delete(bm, geom=dupes, context='FACES_ONLY'); bm.to_mesh(obj.data); bm.free(); obj.data.update()
bone_names = {b.name for b in rig.data.bones}
removed_groups = [vg.name for vg in obj.vertex_groups if vg.name not in bone_names]
for n in removed_groups: obj.vertex_groups.remove(obj.vertex_groups[n])
assert all(abs(v) < 1e-6 for v in rig.matrix_world.to_euler()), 'rig object must be unrotated'
# 'Armature' keeps Unreal from adding an object-named root bone and keeps take names short.
rig.name = 'Armature'; rig.data.name = 'Armature'; obj.name = cfg['asset_name']

# Ground root bone under the rig origin; pelvis re-parented without connection.
vl.objects.active = rig; rig.select_set(True); bpy.ops.object.mode_set(mode='EDIT')
eb = rig.data.edit_bones; root = eb.new(cfg['root_bone'])
head = rig.matrix_world.inverted() @ Vector((0, 0, 0)); root.head = head; root.tail = head + Vector((0, cfg['root_bone_length_m'], 0)); root.roll = 0
root.use_deform = False
for b in eb:
    if b.parent is None and b != root: b.parent = root; b.use_connect = False
for extra in cfg.get('extra_bones', []):  # attachment bones (e.g. rider seat): non-deforming, axis-aligned to the mount
    e = eb.new(extra['name']); h = rig.matrix_world.inverted() @ Vector(extra['head_world_m'])
    e.head = h; e.tail = h + Vector((0, extra.get('length_m', 0.15), 0)); e.roll = 0; e.use_deform = False; e.parent = eb[extra['parent']]
bpy.ops.object.mode_set(mode='OBJECT')
R = rig.matrix_world.to_3x3().inverted()

# Materials: constant metallic where the authored map is invalid.
overridden = []
if cfg.get('dielectric_override') is not None:
    for mat in {s.material for s in obj.material_slots if s.material}:
        bsdf = next((n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None) if mat.use_nodes else None
        if bsdf is None: continue
        sock = bsdf.inputs['Metallic']
        for link in list(sock.links): mat.node_tree.links.remove(link)
        sock.default_value = cfg['dielectric_override']; overridden.append(mat.name)

# Root motion onto the root bone for each kept clip.
rig.animation_data_create(); takes = []
for c in clips:
    action = bpy.data.actions[c['id']]; slot = action.slots[0]
    bag = action.layers[0].strips[0].channelbag(slot, ensure=True)
    n = c['frames']; curve = {r['frame']: r for r in c['root_curve']}
    for path, size in [('location', 3), ('rotation_quaternion', 4)]:
        for i in range(size):
            fc = bag.fcurves.new(f'pose.bones["{cfg["root_bone"]}"].{path}', index=i, group_name=cfg['root_bone'])
            vals = []
            for fr in range(1, n + 1):
                r = curve[fr]; v = (R @ Vector(r['root_m'])) if path == 'location' else Quaternion((0, 0, 1), math.radians(r['yaw_deg']))
                vals += [fr, v[i]]
            fc.keyframe_points.add(n); fc.keyframe_points.foreach_set('co', vals)
            for kp in fc.keyframe_points: kp.interpolation = 'LINEAR'
            fc.update()
    takes.append(c['id'])
for act in list(bpy.data.actions):
    if act.name not in takes: bpy.data.actions.remove(act)

out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes()); (out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
rig.animation_data.action = bpy.data.actions[takes[0]]; rig.animation_data.action_slot = bpy.data.actions[takes[0]].slots[0]
scene.frame_start, scene.frame_end = 1, max(c['frames'] for c in clips)
src_blend = out / (cfg['asset_name'] + '.blend'); bpy.ops.wm.save_as_mainfile(filepath=str(src_blend), relative_remap=True)

# Reference samples: bone world heads at the middle frame of each take.
def sample(o, act_name, fr):
    o.animation_data.action = bpy.data.actions[act_name]
    if hasattr(o.animation_data, 'action_slot') and bpy.data.actions[act_name].slots: o.animation_data.action_slot = bpy.data.actions[act_name].slots[0]
    scene.frame_set(fr); ev = o.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return {b.name: np.array(ev.matrix_world @ b.head) for b in ev.pose.bones}
ref = {c['id']: sample(rig, c['id'], max(1, c['frames'] // 2)) for c in clips}
counts = dict(vertices=len(obj.data.vertices), polygons=len(obj.data.polygons), bones=len(rig.data.bones), materials=len(obj.material_slots))

fbx = out / (cfg['asset_name'] + '.fbx')
sys.path.insert(0, str(Path(__file__).resolve().parent)); from BlenderUnrealUnits import to_centimetres
to_centimetres([obj, rig], [bpy.data.actions[t] for t in takes])  # FBX natively in cm: no root-bone scale in Unreal
bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); rig.select_set(True); vl.objects.active = rig
bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False,
                         bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                         bake_anim_simplify_factor=0.0, path_mode='COPY', embed_textures=False, mesh_smooth_type='FACE', use_armature_deform_only=False)

# Reimport into an empty scene and compare.
bpy.ops.wm.read_factory_settings(use_empty=True); scene = bpy.context.scene
bpy.ops.import_scene.fbx(filepath=str(fbx), use_anim=True, automatic_bone_orientation=False)
mesh = next(o for o in scene.objects if o.type == 'MESH'); arm = next(o for o in scene.objects if o.type == 'ARMATURE')
got = dict(vertices=len(mesh.data.vertices), polygons=len(mesh.data.polygons), bones=len(arm.data.bones), materials=len(mesh.material_slots))
imported = {act.name: act for act in bpy.data.actions}
take_of = {t: next((n for n in imported if n.endswith(t) or n == t or ('|' + t) in n), None) for t in takes}
err = {}
for t, name in take_of.items():
    if name is None: err[t] = None; continue
    c = next(c for c in clips if c['id'] == t); s = sample(arm, name, max(1, c['frames'] // 2))
    err[t] = float(max(np.linalg.norm(s[b] - ref[t][b]) for b in ref[t] if b in s))
checks = dict(counts=got['polygons'] == counts['polygons'] and got['bones'] == counts['bones'],
              all_takes=all(v is not None for v in take_of.values()),
              pose_match=all(v is not None and v <= cfg['reimport_tolerance_m'] for v in err.values()))
report = dict(schema_version=1, status='delivery-export-verified' if all(checks.values()) else 'delivery-export-needs-review', cpu_only=True,
              input_sha256=cfg['input_sha256'], tool_sha256=sha(__file__), profile_sha256=sha(profile), fbx_sha256=sha(fbx), source_blend_sha256=sha(src_blend),
              source_counts=counts, source_triangles_before_budget=source_triangles, reimport_counts=got, takes=takes, excluded_failed_clips=[c['id'] for c in lib['clips'] if c['id'] not in takes],
              take_names_in_fbx=take_of, reimport_mid_frame_bone_error_m=err, checks=checks, removed_vertex_groups=len(removed_groups),
              dielectric_override_materials=overridden, root_bone=cfg['root_bone'], forward_axis='Blender -Y (exporter default axes)',
              limitations=['Reimport is Blender FBX round trip, not Unreal import acceptance.', 'Single high-poly LOD0; engine LODs not authored.',
                           'Dielectric override replaces an invalid uniform metallic map; no per-class PBR masks.'])
(out / 'delivery-report.json').write_text(json.dumps(report, indent=1))
print('DELIVERY', json.dumps(dict(status=report['status'], checks=checks, counts=got, takes=len(takes), err=err)), flush=True)
