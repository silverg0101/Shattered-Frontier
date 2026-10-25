"""Capture each loot mesh in the review sandbox at 1920x1080 in -game mode."""
import json
import math
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Artifacts/LootItems/in-engine'
OUT.mkdir(parents=True, exist_ok=True)
world = u.load_asset('/Game/Frontier/Maps/Sandbox/SB_LootModels')
pc = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_character(world, 0)
camera = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor)
              if a.get_actor_label() == 'LootReviewCamera')
actors = {a.get_actor_label(): a for a in
          u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)}
rows = json.loads((ROOT / 'Artifacts/LootItems/import-evidence.json').read_text())
assert len(rows) == 8 and pc and camera
for row in rows:
    stem = row['item_id'][5:]
    assert 'Loot_' + stem in actors and 'Scale10cm_' + stem in actors, stem
    ref = actors['Scale10cm_' + stem]
    assert abs(ref.get_actor_scale3d().x - .1) < .001, stem
assert 'Loot_wolfhide_bracers_mirrored' in actors
assert actors['Loot_wolfhide_bracers_mirrored'].get_actor_scale3d().y < 0
for cmd in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0',
            'r.MotionBlurQuality 0', 'r.ScreenPercentage 100'):
    u.SystemLibrary.execute_console_command(world, cmd)
if pawn:
    pawn.set_actor_hidden_in_game(True)
    pawn.character_movement.disable_movement()

state = {'frame': 0, 'index': 0}


def tick(_delta):
    state['frame'] += 1
    if state['frame'] < 60:
        return
    local = (state['frame'] - 60) % 140
    row = rows[state['index']]
    stem = row['item_id'][5:]
    actor = actors['Loot_' + stem]
    ref = actors['Scale10cm_' + stem]
    target = float(row['target_cm']['length'])
    center_x = (actor.get_actor_location().x + ref.get_actor_location().x) / 2
    # The camera's 40-degree horizontal FOV is only about 23 degrees vertically
    # at 16:9. Leave headroom for an upright 95 cm sword and the paired bracer.
    distance = max(110.0, target * 3.5)
    eye = u.Vector(center_x + distance * .12,
                   actor.get_actor_location().y - distance,
                   max(35.0, target * .8))
    look = u.Vector(center_x, actor.get_actor_location().y, max(5.0, target * .43))
    if local == 0:
        for label, prop in actors.items():
            if label.startswith(('Loot_', 'Scale10cm_')):
                prop.set_actor_hidden_in_game(label not in (
                    'Loot_' + stem, 'Scale10cm_' + stem,
                    'Loot_wolfhide_bracers_mirrored' if stem == 'wolfhide_bracers' else ''))
        direction = look - eye
        yaw = math.degrees(math.atan2(direction.y, direction.x))
        pitch = math.degrees(math.atan2(direction.z, math.hypot(direction.x, direction.y)))
        camera.set_actor_location(eye, False, False)
        camera.set_actor_rotation(u.Rotator(pitch=pitch, yaw=yaw), False)
        camera.camera_component.set_field_of_view(40.0)
        pc.set_view_target_with_blend(camera, 0)
        u.log('LOOT_VIEW ' + stem + ' model=' + row['mesh'] + ' 10cm_cube=' + ref.get_actor_label())
    if local == 110:
        path = str(OUT / (stem + '.png')).replace('\\', '/')
        u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + path)
        u.log('LOOT_CAPTURE ' + path)
    if local == 139:
        state['index'] += 1
        if state['index'] == len(rows):
            u.unregister_slate_post_tick_callback(state['handle'])
            u.SystemLibrary.execute_console_command(world, 'quit')


state['handle'] = u.register_slate_post_tick_callback(tick)
