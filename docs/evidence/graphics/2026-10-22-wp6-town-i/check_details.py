"""WP6e: every Townhouse1/3/6 bay has its sill planter, and no street-group prop overlaps a house-detail prop (2D bounds)."""
import unreal as u

actors = [a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if isinstance(a, u.StaticMeshActor)]
per = {'SM_SilverbrookTownhouse1': 1, 'SM_SilverbrookTownhouse3': 1, 'SM_SilverbrookTownhouse6': 2}
want = sum(per.get(a.static_mesh_component.static_mesh.get_name(), 0) for a in actors
           if str(a.get_folder_path()).startswith('Districts/') and a.static_mesh_component.static_mesh)
have = sum(1 for a in actors if a.get_actor_label().startswith('SillPlanter_'))
DETAIL = ('FlowerTub', 'StoopTub', 'GardenFence', 'GardenFirewood', 'WashingLine', 'SillPlanter')
def box(a):
    c, e = a.get_actor_bounds(False)
    return c.x - e.x, c.y - e.y, c.x + e.x, c.y + e.y
details = [(a.get_actor_label(), box(a)) for a in actors if a.get_actor_label().split('_')[0] in DETAIL]
groups = [(a.get_actor_label(), box(a)) for a in actors if a.get_actor_label().startswith('StreetGroup_')]
clash = [(g, d) for g, gb in groups for d, db in details
         if gb[0] < db[2] and db[0] < gb[2] and gb[1] < db[3] and db[1] < gb[3]]
print('SILL_PLANTERS', have, 'expected', want, 'GROUP_DETAIL_OVERLAPS', len(clash), clash[:5])
assert have == want and not clash
