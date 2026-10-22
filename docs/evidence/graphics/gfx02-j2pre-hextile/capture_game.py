"""GFX-02 J2-pre -game timed shots at 1920x1080 (no editor, pawn hidden, no ShowFlag overrides).

$env:GFX_TAG='before'|'after'; UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -RenderOffscreen -unattended -nosound -NoSplash
    -ResX=1920 -ResY=1080 "-ExecCmds=py <this file>"
Writes <tag>/<view>.png beside this script. Views: A1-A3 and bookmarks 01/04 from silverbrook-reference-bookmarks.json, 04 again with the sun at 20 deg
(hex-border lighting self-check), two high overviews and four seeded ground-level context views (Random(20261022)). Sun changes are -game only.
"""
import json, math, os, random
from pathlib import Path
import unreal as u

OUT = Path(__file__).resolve().parent / os.environ.get('GFX_TAG', 'after'); OUT.mkdir(exist_ok=True)
BM = Path(__file__).resolve().parents[4] / 'GameClient/ArtSource/Greenlands/silverbrook-reference-bookmarks.json'
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0); pawn = u.GameplayStatics.get_player_character(world, 0)
camera = next(c for c in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor) if c.get_actor_label() == 'ReviewCamera_Aerial')
sun = u.GameplayStatics.get_all_actors_of_class(world, u.DirectionalLight)[0]
sun0 = sun.get_actor_rotation()
bm = {v['name']: v for v in json.loads(BM.read_text(encoding='utf-8'))['views']}
views = [dict(bm[n]) for n in ('A1-south-approach-30m', 'A2-south-approach-100m', 'A3-south-approach-250m', '01-forest-path-town', '04-forest-floor')]
views.append(dict(bm['04-forest-floor'], name='04-forest-floor-sun20', sun_pitch=-20))
views.append(dict(bm['A1-south-approach-30m'], name='A1-sun20', sun_pitch=-20))
views += [dict(name='ctx-overview-south', xy_m=[181, 761], eye_height_m=300, look_xy_m=[0, 300], look_height_m=0, fov_deg=70),
          dict(name='ctx-overview-east', xy_m=[-500, 400], eye_height_m=300, look_xy_m=[100, 100], look_height_m=0, fov_deg=70)]
rng = random.Random(20261022)
for i in range(4):
    x, y = rng.uniform(-600, 600), rng.uniform(-200, 900); a = rng.uniform(0, 2 * math.pi)
    views.append(dict(name=f'ctx-ground-{i + 1}', xy_m=[round(x), round(y)], eye_height_m=1.7, look_xy_m=[round(x + 40 * math.cos(a)), round(y + 40 * math.sin(a))],
                      look_height_m=1.5, fov_deg=70))
for c in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0', 'r.MotionBlurQuality 0', 'r.ScreenPercentage 100', 'r.ExposureOffset 0',
          'r.Streaming.FullyLoadUsedTextures 1'):
    u.SystemLibrary.execute_console_command(world, c)
pawn.set_actor_hidden_in_game(True); pawn.character_movement.disable_movement()
st = dict(frame=0, i=0)


def ground_z(x, y):
    hit = u.SystemLibrary.line_trace_single(world, u.Vector(x * 100, y * 100, 1e5), u.Vector(x * 100, y * 100, -1e5), u.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn], u.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 0


def tick(_d):
    st['frame'] += 1
    if st['frame'] < 90: return
    local = (st['frame'] - 90) % 170; v = views[st['i']]
    if local == 0:
        (x, y), (tx, ty) = v['xy_m'], v['look_xy_m']
        z = ground_z(x, y) + v['eye_height_m'] * 100; tz = ground_z(tx, ty) + v['look_height_m'] * 100
        camera.set_actor_location(u.Vector(x * 100, y * 100, z), False, False)
        camera.set_actor_rotation(u.Rotator(pitch=math.degrees(math.atan2(tz - z, math.hypot(tx - x, ty - y) * 100)), yaw=math.degrees(math.atan2(ty - y, tx - x)), roll=0), False)
        camera.camera_component.set_field_of_view(v['fov_deg']); pc.set_view_target_with_blend(camera, 0)
        sun.set_actor_rotation(u.Rotator(pitch=v['sun_pitch'], yaw=sun0.yaw, roll=0) if 'sun_pitch' in v else sun0, False)
    if local == 145:
        u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + str(OUT / (v['name'] + '.png')).replace('\\', '/'))
        u.log('J2PRE_CAPTURE ' + v['name'])
    if local == 169:
        st['i'] += 1
        if st['i'] == len(views):
            u.unregister_slate_post_tick_callback(st['h']); u.SystemLibrary.execute_console_command(world, 'quit')


st['h'] = u.register_slate_post_tick_callback(tick)
