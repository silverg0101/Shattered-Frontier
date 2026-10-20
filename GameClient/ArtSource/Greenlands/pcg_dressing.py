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


def _meshes(names):
    """Static mesh assets under /Game/Frontier by asset name."""
    ar = u.AssetRegistryHelpers.get_asset_registry()
    flt = u.ARFilter(class_paths=[u.TopLevelAssetPath('/Script/Engine', 'StaticMesh')], package_paths=['/Game/Frontier'], recursive_paths=True)
    found = {str(a.asset_name): a for a in ar.get_assets(flt) if str(a.asset_name) in names}
    missing = sorted(set(names) - set(found))
    assert not missing, f'look-profile meshes not found under /Game/Frontier: {missing}'
    return {n: a.get_asset() for n, a in found.items()}


def build(result, layers, zone, label_suffix=''):
    """result: dress_zone.dress() output; layers: {role: layer name}. Returns (volume actor, expected instance count)."""
    district = result['district'].replace(' ', '')
    name = f'PCG_{zone}_{district}'
    path = f'{GRAPH_DIR}/{name}'
    if u.EditorAssetLibrary.does_asset_exist(path):
        assert u.EditorAssetLibrary.delete_asset(path), f'Could not delete {path}'
    graph = u.AssetToolsHelpers.get_asset_tools().create_asset(name, GRAPH_DIR, u.PCGGraph, u.PCGGraphFactory())
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
            items.setdefault((t['mesh'], layers[role]), []).append(t)
    meshes = _meshes({m for m, _ in items})
    expected = 0
    for (mesh_name, layer), group in sorted(items.items()):
        mesh = meshes[mesh_name]
        b = mesh.get_bounding_box(); height = (b.max.z - b.min.z) / 100
        points = []
        for n, t in enumerate(group):
            p = u.PCGPoint()
            z = t['z'] - t['sink'] * height * t['scale']
            p.set_editor_property('transform', u.Transform(location=u.Vector(t['x'] * 100, t['y'] * 100, z * 100),
                                                         rotation=u.Rotator(roll=0, pitch=0, yaw=t['yaw']),
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
        if layer in NO_COLLISION_LAYERS:
            body = desc.get_editor_property('body_instance')
            body.set_editor_property('collision_profile_name', 'NoCollision')
            desc.set_editor_property('body_instance', body)
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
    vol.set_actor_label(f'PCG_{district}'); vol.set_folder_path('Dressing')
    vol.tags = [u.Name('PCG_Dressing'), u.Name(f'expected:{expected}'), u.Name(f"version:{result['version']}")]
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
