"""Fixed-view captures at arbitrary points in a -game run (the same rig as review_silverbrook_in_game.py's GFX-01 capture).

UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -RenderOffscreen -unattended -nosound -NoSplash
    -ResX=1920 -ResY=1080 "-ExecCmds=py <this file> <views.json> <out dir>"
views.json: {"views": [{"name", "xy_m", "eye_height_m", "look_xy_m", "look_height_m", "fov_deg"}]}. One PNG per view, then quits.
"""
import json, math, sys
from pathlib import Path
import unreal as u

views = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))['views']
out = Path(sys.argv[2]); out.mkdir(parents=True, exist_ok=True)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world() if False else u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
camera = next(c for c in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor) if c.get_actor_label() == 'ReviewCamera_Aerial')
pawn, pc = u.GameplayStatics.get_player_character(world, 0), u.GameplayStatics.get_player_controller(world, 0)
for command in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0', 'r.MotionBlurQuality 0', 'r.ScreenPercentage 100', 'r.ExposureOffset 0', 'ShowFlag.TextRender 0'):
    u.SystemLibrary.execute_console_command(world, command)
if pawn:
    pawn.character_movement.disable_movement()
MPC = u.load_asset('/Game/Frontier/Materials/MPC_Interaction')  # WP7's C++ writes this in the real game; the probe stands in for it
st = dict(frame=0, index=0)


def ground_z(x, y):
    hit = u.SystemLibrary.line_trace_single(world, u.Vector(x * 100, y * 100, 1e5), u.Vector(x * 100, y * 100, -1e5), u.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn] if pawn else [], u.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 0


def tick(_d):
    st['frame'] += 1
    if pawn and MPC:
        p = pawn.get_actor_location()
        u.MaterialLibrary.set_vector_parameter_value(world, MPC, 'PlayerPosition', u.LinearColor(p.x, p.y, p.z, 0))
    if st['frame'] < 90:
        return
    local = (st['frame'] - 90) % 150
    v = views[st['index']]
    if local == 0:
        x, y = v['xy_m']; tx, ty = v['look_xy_m']
        z = ground_z(x, y) + v['eye_height_m'] * 100
        tz = ground_z(tx, ty) + v['look_height_m'] * 100
        camera.set_actor_location(u.Vector(x * 100, y * 100, z), False, False)
        camera.set_actor_rotation(u.Rotator(pitch=math.degrees(math.atan2(tz - z, math.hypot(tx - x, ty - y) * 100)), yaw=math.degrees(math.atan2(ty - y, tx - x)), roll=0), False)
        camera.camera_component.set_field_of_view(v.get('fov_deg', 70))
        pc.set_view_target_with_blend(camera, 0)
        if pawn:
            pawn.set_actor_hidden_in_game(not v.get('show_pawn', False))
            if 'player_xy_m' in v:  # park the pawn here, standing in the shrub's push radius
                px, py = v['player_xy_m']
                pawn.set_actor_location(u.Vector(px * 100, py * 100, ground_z(px, py) + 100), False, True)
    if local == 125:
        u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + str(out / (v['name'] + '.png')).replace(chr(92), '/'))
        u.log('PROBE_CAPTURE ' + v['name'])
    if local == 149:
        st['index'] += 1
        if st['index'] == len(views):
            u.unregister_slate_post_tick_callback(st['h'])
            u.SystemLibrary.execute_console_command(world, 'quit')


st['h'] = u.register_slate_post_tick_callback(tick)
