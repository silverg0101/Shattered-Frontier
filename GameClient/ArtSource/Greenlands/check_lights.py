"""Light gate (owner: "magical light sources from nowhere"). Independent of lamp-sockets.json and of lamp_fx's socket code:
reads the lantern glass straight from the meshes.

Every PointLight must be attached to a visible lamp/building actor, sit within 20 cm of a lantern-sized piece around that actor's *Glow
material pieces near it (one lantern's panes) (lamp_fx puts wall-panel lights 30 cm in front of the pane), and not be inside any other actor's mesh
(3 cm sphere overlap). Wall-mounted lights (owner not a StreetLamp_) must cast shadows so they cannot light through walls.
Run in the open Silverbrook editor: py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/check_lights.py
Prints LIGHT_CHECK lights N on_glass N orphans N in_wall N unshadowed_wall N shadowed N; any orphan, in-wall light or
unshadowed wall light fails.
"""
import unreal as u

actors = list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
_boxes = {}


_dm = {}


def glow_boxes(mesh, near):   # mesh-space lantern boxes: street lamps' *Glow-slot pieces; on a building the lantern-sized
    # (8-35 cm on every axis) connected pieces of any material within 40 cm of `near` (duplicated from lamp_fx on purpose,
    # WP6i r8 fix 2; a local box, because splitting every building slot runs the editor out of memory)
    name = mesh.get_name()
    if name not in _dm:
        dm = u.DynamicMesh()
        _dm[name], _ = u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(mesh, dm, u.GeometryScriptCopyMeshFromAssetOptions(), u.GeometryScriptMeshReadLOD())
    dm, pool = _dm[name], u.DynamicMeshPool()
    if name.startswith('SM_SilverbrookStreetLamp'):
        if name not in _boxes:
            glow = [i for i, s in enumerate(mesh.static_materials) if 'Glow' in str(s.material_slot_name)]
            parts = u.GeometryScript_MeshDecomposition.split_mesh_by_material_i_ds(dm, mesh_pool=pool)
            meshes = next(x for x in parts if isinstance(x, u.Array) and len(x) and isinstance(x[0], u.DynamicMesh))
            ids = next(x for x in parts if isinstance(x, u.Array) and len(x) and isinstance(x[0], int))
            out = []
            for piece, mid in zip(meshes, ids):
                if mid in glow:
                    comps = u.GeometryScript_MeshDecomposition.split_mesh_by_components(piece, mesh_pool=pool)
                    out += [u.GeometryScript_MeshQueries.get_mesh_bounding_box(c) for c in next(x for x in comps if isinstance(x, u.Array))]
            _boxes[name] = out
        pool.free_all_meshes()
        return _boxes[name]
    sel = u.GeometryScript_MeshSelection.select_mesh_elements_in_box(
        dm, u.Box(u.Vector(near.x - 40, near.y - 40, near.z - 40), u.Vector(near.x + 40, near.y + 40, near.z + 40)),
        u.GeometryScriptMeshSelectionType.TRIANGLES, False, 3)[1]
    sub = u.DynamicMesh(); u.GeometryScript_MeshDecomposition.copy_mesh_selection_to_mesh(dm, sub, sel)
    comps = u.GeometryScript_MeshDecomposition.split_mesh_by_components(sub, mesh_pool=pool)
    out = [bb for bb in (u.GeometryScript_MeshQueries.get_mesh_bounding_box(c) for c in next(x for x in comps if isinstance(x, u.Array)))
           if all(8 <= getattr(bb.max, a) - getattr(bb.min, a) <= 35 for a in 'xyz')]
    pool.free_all_meshes()
    return out


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
            boxes = glow_boxes(mesh, local)
            g = min(gap(local, b) for b in boxes) if boxes else 1e9
            best = min(best, g)
    if best > 20:   # a light 14 cm outside a pane face is 6.5 cm from the box; 20 leaves room for the cage variant (WP6i r8)
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
