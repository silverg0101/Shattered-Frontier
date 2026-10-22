"""Light gate (owner: "magical light sources from nowhere"). Independent of lamp-sockets.json and of lamp_fx's socket code:
reads the lantern glass straight from the meshes.

Every PointLight must be attached to a visible lamp/building actor, sit within 35 cm of the box around that actor's *Glow
material pieces near it (one lantern's panes) (lamp_fx puts wall-panel lights 30 cm in front of the pane), and not be inside any other actor's mesh
(3 cm sphere overlap). Wall-mounted lights (owner not a StreetLamp_) must cast shadows so they cannot light through walls.
Run in the open Silverbrook editor: py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/check_lights.py
Prints LIGHT_CHECK lights N on_glass N orphans N in_wall N unshadowed_wall N shadowed N; any orphan, in-wall light or
unshadowed wall light fails.
"""
import unreal as u

actors = list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
_boxes = {}


def glow_boxes(mesh):   # mesh-space boxes of the connected pieces of the mesh's *Glow material slots
    if mesh.get_name() not in _boxes:
        glow = [i for i, s in enumerate(mesh.static_materials) if 'Glow' in str(s.material_slot_name)]
        dm = u.DynamicMesh()
        dm, _ = u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(mesh, dm, u.GeometryScriptCopyMeshFromAssetOptions(), u.GeometryScriptMeshReadLOD())
        parts = u.GeometryScript_MeshDecomposition.split_mesh_by_material_i_ds(dm, mesh_pool=u.DynamicMeshPool())
        meshes = next(x for x in parts if isinstance(x, u.Array) and len(x) and isinstance(x[0], u.DynamicMesh))
        ids = next(x for x in parts if isinstance(x, u.Array) and len(x) and isinstance(x[0], int))
        out = []
        for piece, mid in zip(meshes, ids):
            if mid in glow:
                comps = u.GeometryScript_MeshDecomposition.split_mesh_by_components(piece, mesh_pool=u.DynamicMeshPool())
                out += [u.GeometryScript_MeshQueries.get_mesh_bounding_box(c) for c in next(x for x in comps if isinstance(x, u.Array))]
        _boxes[mesh.get_name()] = out
    return _boxes[mesh.get_name()]


def gap(p, b):
    return sum(max(getattr(b.min, a) - v, 0, v - getattr(b.max, a)) ** 2 for a, v in zip('xyz', (p.x, p.y, p.z))) ** .5


lights = [a for a in actors if isinstance(a, u.PointLight)]
orphans, in_wall, unshadowed, ok, shadowed = [], [], [], 0, 0
for light in lights:
    p = light.get_actor_location(); label = light.get_actor_label()
    owner = light.get_attach_parent_actor()
    shadows = light.light_component.get_editor_property('cast_shadows'); shadowed += bool(shadows)
    if not owner or owner.is_hidden_ed() or owner.get_editor_property('hidden'):
        orphans.append((label, 'no visible owner')); continue
    best = 1e9
    for comp in owner.get_components_by_class(u.StaticMeshComponent):
        mesh = comp.get_editor_property('static_mesh')
        if mesh:
            local = comp.get_world_transform().inverse_transform_location(p)
            near = [b for b in glow_boxes(mesh) if gap(local, b) <= 60]   # the panes of one lantern; a light may hang inside them
            if near:
                lo = u.Vector(*(min(getattr(b.min, a) for b in near) for a in 'xyz')); hi = u.Vector(*(max(getattr(b.max, a) for b in near) for a in 'xyz'))
                g = gap(local, u.Box(lo, hi))
                if max(getattr(hi, a) - getattr(lo, a) for a in 'xyz') < 60 and g > 3:   # a lantern (< 60 cm): light at the glass (WP6i r6)
                    g = 1e9
                best = min(best, g)
    if best > 35:   # 30 cm in front of a wall pane (WP6h review 3b) plus 5 cm
        orphans.append((label, round(best, 1))); continue
    hits = [h for h in (u.SystemLibrary.sphere_overlap_actors(light, p, 3., [u.ObjectTypeQuery.ECC_WORLD_STATIC], u.StaticMeshActor, [owner]) or [])
            if not h.is_hidden_ed()]
    if hits:
        in_wall.append((label, [h.get_actor_label() for h in hits][:2])); continue
    if not shadows and not owner.get_actor_label().startswith('StreetLamp_'):
        unshadowed.append(label); continue
    ok += 1
print('LIGHT_CHECK lights', len(lights), 'on_glass', ok, 'orphans', len(orphans), 'in_wall', len(in_wall), 'unshadowed_wall', len(unshadowed),
      'shadowed', shadowed, orphans[:5], in_wall[:5], unshadowed[:3])
assert not orphans and not in_wall and not unshadowed, (orphans[:10], in_wall[:10], unshadowed[:5])
