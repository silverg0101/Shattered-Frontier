"""QST-02 review captures in a 1920x1080 Silverbrook -game client (standalone: it is its own server, so the quest log,
the layout-placed board and Oswin exist). World views are HighResShots with the pawn hidden (market = the WP6 bookmark,
quest-board = the board from the square); UI views use Shot showui with the board and journal opened by `SFUi`.

Launch (editor closed; see README.md):
  UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -windowed -ResX=1920 -ResY=1080 -ForceRes
    -nosound -QuestAutoAccept=quest/mills_supply,quest/silverbrook_introductions "-ExecCmds=py <this file> <out dir>"
"""
import math
import sys
import time
from pathlib import Path
import unreal as u

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent
OUT.mkdir(parents=True, exist_ok=True)
SHOTS = Path(u.Paths.screen_shot_dir())
STARTED = time.time()
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_character(world, 0)
camera = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor) if a.get_actor_label() == 'ReviewCamera_Aerial')
terrain = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
               if a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name() == 'SM_Silverbrook_Terrain')
WORLD_VIEWS = [
    ('market', (30, -62, 3.0), (0, -30, 1.2), 70),
    ('quest-board', (-7.6, -26.6, 1.7), (-10, -20, 1.3), 60),
]
# (seconds after the world views, action, argument)
UI_STEPS = [
    (1.0, 'cmd', 'SFStageAt -8.3 -21.7 135'),
    (5.0, 'shot', 'quest-hud'),
    (6.0, 'cmd', 'SFUi board'),
    (8.0, 'shot', 'quest-board-panel'),
    (9.0, 'cmd', 'SFUi journal'),
    (11.0, 'shot', 'quest-journal'),
    (13.0, 'quit', ''),
]
props = [(a.get_actor_label(),) + tuple(a.get_actor_bounds(False)) for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
         if str(a.get_folder_path()).startswith('Silverbrook/Dressing') or a.get_actor_label().startswith('StreetLamp')]


def ground(x, y):
    hit = terrain.static_mesh_component.line_trace_component(u.Vector(x*100, y*100, 100000), u.Vector(x*100, y*100, -100000), True, False)
    assert hit, (x, y)
    return hit[0].z


for cmd in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0', 'r.MotionBlurQuality 0', 'r.ScreenPercentage 100'):
    u.SystemLibrary.execute_console_command(world, cmd)
state = {'frame': 0, 'index': 0, 'ui_start': None, 'ui_index': 0, 'pending': None}


def collect():
    if not state['pending']:
        return
    found = [p for p in SHOTS.rglob('*.png') if p.stat().st_mtime >= STARTED]
    if found:
        max(found, key=lambda p: p.stat().st_mtime).replace(OUT / (state['pending'] + '-live.png'))
        u.log('QUEST_CAPTURE ' + state['pending'])
        state['pending'] = None


def tick(_delta):
    state['frame'] += 1
    if state['frame'] < 90:  # the subsystem places the board on BeginPlay; give streaming a moment
        return
    if state['index'] < len(WORLD_VIEWS):
        if state['frame'] == 90:
            pawn.set_actor_hidden_in_game(True)
            boards = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.Actor) if a.get_class().get_name() == 'SFQuestBoard']
            assert boards, 'no quest board placed'
            u.log('QUEST_BOARD_FOUND %s' % boards[0].get_actor_location())
        local = (state['frame'] - 90) % 140
        name, eye, look, fov = WORLD_VIEWS[state['index']]
        if local == 0:
            x, y, h = eye; tx, ty, th = look
            z = ground(x, y) + h*100; tz = ground(tx, ty) + th*100
            d = u.Vector(tx*100 - x*100, ty*100 - y*100, tz - z); d = d / d.length(); e = u.Vector(x*100, y*100, z)
            block = [(l, c) for l, c, ext in props if 0 < (c - e).dot(d) < 1000 and (c - e).cross(d).length() < 150 + ext.length()]
            if block:  # a blocked frame is a bug, not a result: no shot, the run fails
                u.log_error('QUEST_SIGHTLINE_BLOCKED %s %s' % (name, block[:3])); u.SystemLibrary.execute_console_command(world, 'quit'); return
            u.log(f'QUEST_SIGHTLINE_CLEAR {name} props {len(props)}')
            camera.set_actor_location(e, False, False)
            camera.set_actor_rotation(u.Rotator(pitch=math.degrees(math.atan2(tz-z, math.hypot(tx-x, ty-y)*100)), yaw=math.degrees(math.atan2(ty-y, tx-x))), False)
            camera.camera_component.set_field_of_view(fov)
            pc.set_view_target_with_blend(camera, 0)
        if local == 115:
            u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + str(OUT / (name + '-live.png')).replace('\\', '/'))
            u.log('QUEST_CAPTURE ' + name)
        if local == 139:
            state['index'] += 1
        return
    now = time.time()
    if state['ui_start'] is None:
        pawn.set_actor_hidden_in_game(False)
        pc.set_view_target_with_blend(pawn, 0)
        state['ui_start'] = now
    collect()
    if state['ui_index'] >= len(UI_STEPS):
        return
    at, kind, arg = UI_STEPS[state['ui_index']]
    if now - state['ui_start'] < at:
        return
    state['ui_index'] += 1
    if kind == 'cmd':
        u.SystemLibrary.execute_console_command(world, arg)
    elif kind == 'shot':
        u.SystemLibrary.execute_console_command(world, 'Shot showui')
        state['pending'] = arg
    else:
        u.unregister_slate_post_tick_callback(state['handle'])
        u.SystemLibrary.execute_console_command(world, 'quit')


state['handle'] = u.register_slate_post_tick_callback(tick)
