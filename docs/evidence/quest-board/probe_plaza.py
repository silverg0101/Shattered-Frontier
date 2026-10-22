"""Lists placed actors (static meshes, lamps, dressing) whose bounds come within 4 m of candidate Quest Board spots on the north
edge of Market Square, so the layout row picks a clear spot. Run in -game Silverbrook with -ExecCmds="py <this file>"."""
import unreal as u

world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
CANDIDATES = [(x, y) for y in (-46, -42, -38, -20, -14) for x in (-22, -16, -10, -5, 5, 10, 16, 22)]
actors = [(a.get_actor_label(),) + tuple(a.get_actor_bounds(False)) for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
          if a.static_mesh_component.static_mesh and 'Terrain' not in a.static_mesh_component.static_mesh.get_name()]
for x, y in CANDIDATES:
    near = []
    for label, c, e in actors:
        dx = max(abs(c.x - x*100) - e.x, 0); dy = max(abs(c.y - y*100) - e.y, 0)
        if (dx*dx + dy*dy) ** 0.5 < 350 and max(e.x, e.y) < 4000:
            near.append('%s@%.0f,%.0f' % (label, c.x/100, c.y/100))
    u.log('QUEST_PROBE %d,%d near=%d %s' % (x, y, len(near), ' '.join(near[:6])))
u.SystemLibrary.execute_console_command(world, 'quit')
