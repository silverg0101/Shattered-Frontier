"""Capture the WP8 reference bookmarks from the open editor viewport (HighResShot), for quick before/after checks.

python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/capture_bookmarks_editor.py <out_dir_name> <view_name> set|shot
Writes docs/evidence/graphics/<out_dir_name>/<view>.png. 
"""
import json, math, sys, time
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parent
args = sys.argv[1:]
out = ROOT.parents[2] / 'docs/evidence/graphics' / args[0]
out.mkdir(parents=True, exist_ok=True)
views = [v for v in json.loads((ROOT / 'silverbrook-reference-bookmarks.json').read_text(encoding='utf-8'))['views'] if v['name'] == args[1]]
step = args[2]  # the editor only ticks between runs: `set` the camera, wait ~8 s outside, then `shot`
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()


def ground_z(x, y):
    hit = u.SystemLibrary.line_trace_single(world, u.Vector(x, y, 3e5), u.Vector(x, y, -3e5), u.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                            u.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 0.0


for v in views:
    (x, y), (lx, ly) = [[m * 100 for m in v[k]] for k in ('xy_m', 'look_xy_m')]
    eye = u.Vector(x, y, ground_z(x, y) + v['eye_height_m'] * 100)
    tgt = u.Vector(lx, ly, ground_z(lx, ly) + v['look_height_m'] * 100)
    rot = u.MathLibrary.find_look_at_rotation(eye, tgt)
    if step == 'set':
        u.get_editor_subsystem(u.LevelEditorSubsystem).editor_set_game_view(True)  # no editor gizmos or labels
        u.EditorLevelLibrary.set_level_viewport_camera_info(eye, rot)
        u.SystemLibrary.execute_console_command(world, f"FOV {v['fov_deg']}")
    else:
        u.SystemLibrary.execute_console_command(world, f"HighResShot 1920x1080 filename={(out / (v['name'] + '.png')).as_posix()}")
    print(step.upper(), v['name'], eye)
