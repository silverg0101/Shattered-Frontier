"""Write a district's dressing (from dress_zone.py) into a PCG graph and generate it in the open editor.

Graph /Game/Frontier/PCG/PCG_<Zone>_<District>: per mesh, Create Points (the decided transforms, world space)
-> Difference against every volume tagged PCG_Exclude (the owner's first fine-tuning tool) -> Static Mesh Spawner.
A PCGVolume in the zone map hosts it; generation is on demand only, so output is made in the editor and saved,
never at runtime. PCG generates over the next editor ticks, so the build ends with a second run of this file:
    python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/pcg_dressing.py
which checks every district volume generated the expected instance count and saves the map.
"""
import json
import unreal as u

GRAPH_DIR = '/Game/Frontier/PCG'
EXCLUDE_TAG = 'PCG_Exclude'
NO_COLLISION_LAYERS = ('ground_cover', 'litter', 'understory')
COSMETIC_LAYERS = NO_COLLISION_LAYERS  # client-only: tagged so a dedicated server destroys them in ASFGameMode::StartPlay
COSMETIC_TAG = 'SF_ClientOnlyCosmetic'
CULL_M = {'ground_cover': 50, 'understory': 50, 'litter': 25}  # default end-cull metres per layer; a role's place.cull_m overrides
CULL_BOUNDS = (10, 200)
NO_SHADOW_LAYERS = ('ground_cover', 'litter')
TILT = 0.6  # 0 = upright, 1 = follow the terrain normal
WPO_DISABLE_CM = 3000  # leaf wind (M_GN_Leaf) switches off beyond 30 m on every instance: VSM cost grows with WPO


def _meshes(names):
    """Static mesh assets under /Game/Frontier by asset name."""
    ar = u.AssetRegistryHelpers.get_asset_registry()
    flt = u.ARFilter(class_paths=[u.TopLevelAssetPath('/Script/Engine', 'StaticMesh')], package_paths=['/Game/Frontier'], recursive_paths=True)
    found = {str(a.asset_name): a for a in ar.get_assets(flt) if str(a.asset_name) in names}
    missing = sorted(set(names) - set(found))
    assert not missing, f'look-profile meshes not found under /Game/Frontier: {missing}'
    return {n: a.get_asset() for n, a in found.items()}


def build(result, layers, zone):
    """result: dress_zone.dress() output; layers: {role: layer name}. Trees and rocks go in the district's gameplay
    volume (the server keeps their collision); cosmetic layers go in a second, client-only volume.
    Returns (gameplay volume actor, expected instance count over both)."""
    cosmetic = lambda r: layers[r] in COSMETIC_LAYERS and r not in result.get('collide', ())  # a role with collision (saplings) stays on the server
    split = {c: dict(result, roles={r: v for r, v in result['roles'].items() if cosmetic(r) == c}) for c in (False, True)}
    vol, expected = _build(split[False], layers, zone, False)
    if split[True]['roles']:
        expected += _build(split[True], layers, zone, True)[1]
    return vol, expected


def _terrain(label=None):
    """The final terrain mesh actor, found by its mesh, so placement is projected onto what the player walks on."""
    world = u.EditorLevelLibrary.get_editor_world()
    fallback = None
    for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor):
        m = a.static_mesh_component.static_mesh
        if m and m.get_name() == 'SM_Silverbrook_Terrain':
            return world, a
        if a.get_actor_label() == (label or 'Terrain') and m:
            fallback = a
    if fallback:
        return world, fallback
    raise AssertionError('SM_Silverbrook_Terrain is not in the open map')


def ray(world, terrain, x, y, top, bottom):
    """(impact point, impact normal) where a vertical ray (cm) first meets the terrain mesh, or None; buildings, trunks and earlier dressing are ignored."""
    r = u.SystemLibrary.line_trace_multi(world, u.Vector(x, y, top), u.Vector(x, y, bottom), u.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], u.DrawDebugTrace.NONE, True)
    hits = r[1] if isinstance(r, tuple) else r
    return next(((h.to_tuple()[5], h.to_tuple()[7]) for h in hits or () if h.to_tuple()[9] == terrain), None)


def _snap(group):
    """(z metres, up normal) of the terrain under each placement, by a ray from 15 m above to 15 m below its decided height."""
    world, terrain = _terrain()
    out = []
    for t in group:
        hit = ray(world, terrain, t['x'] * 100, t['y'] * 100, (t['z'] + 15) * 100, (t['z'] - 15) * 100)
        out.append((hit[0].z / 100, hit[1]) if hit else (t['z'], u.Vector(0, 0, 1)))
    return out


def _cull_cm(result, layers, role):
    metres = result.get('cull', {}).get(role, CULL_M.get(layers[role]))
    return 0 if metres is None else min(max(metres, CULL_BOUNDS[0]), CULL_BOUNDS[1]) * 100


def _build(result, layers, zone, cosmetic):
    district = result['district'].replace(' ', '')
    name = f'PCG_{zone}_{district}' + ('_Cosmetic' if cosmetic else '')
    label = f'PCG_{district}' + ('_Cosmetic' if cosmetic else '')
    path = f'{GRAPH_DIR}/{name}'
    if u.EditorAssetLibrary.does_asset_exist(path):  # reuse and empty the graph: deleting a saved .uasset fails with Windows error 32 while the editor or another process holds it
        graph = u.load_asset(path)
        for node in list(graph.get_editor_property('nodes')):
            graph.remove_node(node)
    else:
        graph = u.AssetToolsHelpers.get_asset_tools().create_asset(name, GRAPH_DIR, u.PCGGraph, u.PCGGraphFactory())
    assert graph, f'Could not create {path}'
    excl_node, excl = graph.add_node_of_type(u.PCGGetVolumeSettings)
    sel = excl.get_editor_property('actor_selector')
    sel.set_editor_property('actor_filter', u.PCGActorFilter.ALL_WORLD_ACTORS)
    sel.set_editor_property('actor_selection', u.PCGActorSelection.BY_TAG)
    sel.set_editor_property('actor_selection_tag', EXCLUDE_TAG)
    sel.set_editor_property('select_multiple', True)
    excl.set_editor_property('actor_selector', sel)

    items = {}
    for role, placed in result['roles'].items():
        for t in placed:
            items.setdefault((t['mesh'], layers[role], _cull_cm(result, layers, role)), []).append(t)
    meshes = _meshes({m for m, _, _ in items})
    expected = 0
    for (mesh_name, layer, cull), group in sorted(items.items()):
        mesh = meshes[mesh_name]
        b = mesh.get_bounding_box(); height = (b.max.z - b.min.z) / 100
        points = []
        ground = _snap(group)
        for n, (t, (gz, normal)) in enumerate(zip(group, ground)):
            p = u.PCGPoint()
            z = gz - t['sink'] * height * t['scale'] - t['sink_cm'] / 100
            tilt = u.Vector(normal.x * TILT, normal.y * TILT, 1 - TILT + normal.z * TILT) if layer in NO_COLLISION_LAYERS else u.Vector(0, 0, 1)  # small cover leans with the ground
            rot = u.MathLibrary.compose_rotators(u.Rotator(roll=0, pitch=0, yaw=t['yaw']), u.MathLibrary.make_rot_from_z(tilt))
            p.set_editor_property('transform', u.Transform(location=u.Vector(t['x'] * 100, t['y'] * 100, z * 100),
                                                         rotation=rot,
                                                         scale=u.Vector(t['scale'], t['scale'], t['scale'])))
            p.set_editor_property('density', 1.0)
            p.set_editor_property('seed', n + 1)
            points.append(p)
        expected += len(points)
        cp_node, cp = graph.add_node_of_type(u.PCGCreatePointsSettings)
        cp.set_editor_property('coordinate_space', u.PCGCoordinateSpace.WORLD)
        cp.set_editor_property('cull_points_outside_volume', False)
        cp.set_editor_property('points_to_create', points)
        df_node, _ = graph.add_node_of_type(u.PCGDifferenceSettings)
        sp_node, sp = graph.add_node_of_type(u.PCGStaticMeshSpawnerSettings)
        sp.set_editor_property('mesh_selector_type', u.PCGMeshSelectorWeighted)
        entry = u.PCGMeshSelectorWeightedEntry()
        desc = entry.get_editor_property('descriptor')
        desc.set_editor_property('static_mesh', mesh)
        if cosmetic:
            body = desc.get_editor_property('body_instance')
            body.set_editor_property('collision_profile_name', 'NoCollision')
            desc.set_editor_property('body_instance', body)
        if cull:
            desc.set_editor_property('instance_end_cull_distance', int(cull))
            desc.set_editor_property('instance_start_cull_distance', int(cull * 0.8))  # dither out over the last fifth
        desc.set_editor_property('world_position_offset_disable_distance', WPO_DISABLE_CM)
        if layer in NO_SHADOW_LAYERS:
            desc.set_editor_property('cast_shadow', False)  # tiny cover: no VSM cost, and no shadow left behind when its instance is culled
        entry.set_editor_property('descriptor', desc)
        entry.set_editor_property('weight', 1)
        sp.get_editor_property('mesh_selector_parameters').set_editor_property('mesh_entries', [entry])
        graph.add_edge(cp_node, 'Out', df_node, 'Source')
        graph.add_edge(excl_node, 'Out', df_node, 'Differences')
        graph.add_edge(df_node, 'Out', sp_node, 'In')
    edges = len(graph.get_all_edges()) if hasattr(graph, 'get_all_edges') else None
    assert edges is None or edges >= 3 * len(items), f'graph edges missing: {edges} for {len(items)} meshes'
    u.EditorAssetLibrary.save_loaded_asset(graph)

    xs = [t['x'] for g in items.values() for t in g]; ys = [t['y'] for g in items.values() for t in g]
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    vol = actors.spawn_actor_from_class(u.PCGVolume, u.Vector((min(xs) + max(xs)) * 50, (min(ys) + max(ys)) * 50, 0))
    vol.set_actor_scale3d(u.Vector((max(xs) - min(xs) + 20) / 2, (max(ys) - min(ys) + 20) / 2, 50))  # brush is 2 m; 100 m tall
    vol.set_actor_label(label); vol.set_folder_path('Dressing')
    vol.tags = [u.Name('PCG_Dressing'), *([u.Name(COSMETIC_TAG)] if cosmetic else []), u.Name(f'expected:{expected}'), u.Name(f"version:{result['version']}")]
    comp = vol.get_component_by_class(u.PCGComponent)
    comp.set_graph(graph)
    comp.set_editor_property('generation_trigger', u.PCGComponentGenerationTrigger.GENERATE_ON_DEMAND)
    comp.generate(True)
    return vol, expected


def finish():
    """After PCG has generated: every dressing volume holds its expected instances; then save the map."""
    world = u.EditorLevelLibrary.get_editor_world()
    report = {}
    for vol in u.GameplayStatics.get_all_actors_with_tag(world, 'PCG_Dressing'):
        comp = vol.get_component_by_class(u.PCGComponent)
        tags = {str(t).split(':')[0]: str(t).split(':', 1)[1] for t in vol.tags if ':' in str(t)}
        ism = [c for c in vol.get_components_by_class(u.InstancedStaticMeshComponent)]
        made = sum(c.get_instance_count() for c in ism)
        report[vol.get_actor_label()] = dict(generated=comp.get_editor_property('generated'), in_progress=comp.get_editor_property('generation_in_progress'),
                                             instances=made, expected=int(tags['expected']), components=len(ism), version=tags['version'])
    assert report, 'no PCG_Dressing volume in the open map'
    busy = [k for k, r in report.items() if r['in_progress']]
    assert not busy, f'PCG still generating {busy}; run this again in a few seconds'
    wrong = {k: r for k, r in report.items() if r['instances'] != r['expected']}
    assert not wrong, f'dressing instance counts differ: {wrong}'
    assert u.EditorLoadingAndSavingUtils.save_dirty_packages(True, True), 'Could not save the map'
    u.log(f'DRESSING_SAVED {json.dumps(report)}')


if __name__ == '__main__':
    finish()
