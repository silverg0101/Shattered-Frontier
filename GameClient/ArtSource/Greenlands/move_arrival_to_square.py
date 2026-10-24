"""Move the arrival group (player start, waystone, label) in the open Silverbrook map to the layout's arrival point.

Runs inside the editor (python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/move_arrival_to_square.py) so the
open map is edited in place; build_silverbrook.py places the same group from silverbrook-town-layout.json on a rebuild.
Each actor keeps its offset from the player start and its height above the ground (traced at the new spot).
"""
import json, math
from pathlib import Path
from validate_layout import SILVERBROOK_LAYOUT
import unreal

L = json.loads(SILVERBROOK_LAYOUT.read_text(encoding='utf-8'))
ax, ay = L['arrival']['at']; fx, fy = L['arrival']['face']
yaw = math.degrees(math.atan2(fy - ay, fx - ax))
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
group = [a for a in actors.get_all_level_actors() if a.get_actor_label().startswith('Arrival')]
start = next(a for a in group if a.get_actor_label() == 'Arrival_PlayerStart')


def ground(x, y, ignore):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 200000), unreal.Vector(x, y, -200000),
                                                 unreal.TraceTypeQuery.ECC_VISIBILITY, False, ignore, unreal.DrawDebugTrace.NONE, True)
    if not hit: raise RuntimeError(f'No ground under {x}, {y}')
    return hit.to_tuple()[4].z  # impact point


old = start.get_actor_location(); delta = unreal.Vector(ax * 100 - old.x, ay * 100 - old.y, 0)
moved = {}
for a in group:
    p = a.get_actor_location()
    above = p.z - ground(p.x, p.y, group)
    nx, ny = p.x + delta.x, p.y + delta.y
    a.set_actor_location(unreal.Vector(nx, ny, ground(nx, ny, group) + above), False, False)
    if a == start: a.set_actor_rotation(unreal.Rotator(roll=0, pitch=0, yaw=yaw), False)
    moved[a.get_actor_label()] = [round(v) for v in (nx, ny, a.get_actor_location().z)]
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log('ARRIVAL_MOVED ' + json.dumps(dict(at=[ax, ay], yaw=round(yaw), actors=moved)))
