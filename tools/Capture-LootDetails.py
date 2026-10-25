"""Extra development-display views for the final independent loot review."""
import math
import os
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Artifacts/LootItems/in-engine/round3'
OUT.mkdir(parents=True, exist_ok=True)
world = u.load_asset('/Game/Frontier/Maps/Sandbox/SB_LootModels')
pc = u.GameplayStatics.get_player_controller(world, 0)
camera = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor)
              if a.get_actor_label() == 'LootReviewCamera')
actors = {a.get_actor_label(): a for a in
          u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)}
assert pc and camera
pawn = u.GameplayStatics.get_player_character(world, 0)
if pawn:
    pawn.set_actor_hidden_in_game(True)
    pawn.character_movement.disable_movement()
for cmd in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0',
            'r.MotionBlurQuality 0', 'r.ScreenPercentage 100'):
    u.SystemLibrary.execute_console_command(world, cmd)

# type: name, stem, horizontal focus fraction, distance multiplier, camera height,
# target height, FOV, view mode. The 10 cm cube stays visible in general close-ups.
shots = [
    ('captains_longsword_guard', 'captains_longsword', -.55, .62, 30, 5, 42, 'lit'),
    ('captains_longsword_low', 'captains_longsword', 0, 1.45, 8, 3, 45, 'lit'),
    ('captains_longsword_unlit', 'captains_longsword', 0, 1.55, 75, 3, 50, 'unlit'),
    ('wolf_pelt_end', 'wolf_pelt', -.30, 1.00, 36, 8, 40, 'lit'),
    ('wolf_pelt_unlit', 'wolf_pelt', 0, 1.35, 70, 8, 50, 'unlit'),
    ('archers_gloves_unlit', 'archers_gloves', 0, 1.35, 70, 4, 50, 'unlit'),
    ('cudgel_unlit', 'cudgel', 0, 1.35, 70, 4, 50, 'unlit'),
    ('fangtooth_dagger_close', 'fangtooth_dagger', 0, 1.12, 49, 4, 45, 'lit'),
    ('brigand_blade_close', 'brigand_blade', 0, 1.12, 49, 4, 45, 'lit'),
    ('wolfhide_bracers_close', 'wolfhide_bracers', 0, 1.80, 49, 4, 45, 'lit'),
]
requested = os.environ.get('SF_LOOT_DETAIL_ONLY')
if requested:
    names = set(requested.split(','))
    shots = [shot for shot in shots if shot[0] in names]
    assert len(shots) == len(names), requested
state = {'frame': 0, 'index': 0}


def tick(_delta):
    state['frame'] += 1
    if state['frame'] < 60:
        return
    local = (state['frame'] - 60) % 140
    name, stem, fraction, distance_mult, height, target_height, fov, mode = shots[state['index']]
    actor = actors['Loot_' + stem]
    cube = actors['Scale10cm_' + stem]
    group = [actor, cube]
    if stem == 'wolfhide_bracers':
        group.append(actors['Loot_wolfhide_bracers_mirrored'])
    if local == 0:
        for label, prop in actors.items():
            if label.startswith(('Loot_', 'Scale10cm_')):
                prop.set_actor_hidden_in_game(prop not in group)
        center, extent = actor.get_actor_bounds(False)
        min_y = min(a.get_actor_bounds(False)[0].y - a.get_actor_bounds(False)[1].y for a in group)
        max_y = max(a.get_actor_bounds(False)[0].y + a.get_actor_bounds(False)[1].y for a in group)
        target = u.Vector(center.x + fraction * extent.x, center.y, target_height)
        width = extent.x * 2
        if stem == 'wolfhide_bracers':
            min_x = min(a.get_actor_bounds(False)[0].x - a.get_actor_bounds(False)[1].x for a in group)
            max_x = max(a.get_actor_bounds(False)[0].x + a.get_actor_bounds(False)[1].x for a in group)
            target.x = (min_x + max_x) / 2
            width = max_x - min_x
        distance = max(25.0, width / (2 * math.tan(math.radians(fov / 2)))) * distance_mult
        eye = u.Vector(target.x + distance * .06, min_y - distance, height)
        if name.endswith('_low'):
            eye = u.Vector(center.x, min_y - distance, height)
            target = u.Vector(center.x, center.y, target_height)
        direction = target - eye
        yaw = math.degrees(math.atan2(direction.y, direction.x))
        pitch = math.degrees(math.atan2(direction.z, math.hypot(direction.x, direction.y)))
        camera.set_actor_location(eye, False, False)
        camera.set_actor_rotation(u.Rotator(pitch=pitch, yaw=yaw), False)
        camera.camera_component.set_field_of_view(fov)
        pc.set_view_target_with_blend(camera, 0)
        u.SystemLibrary.execute_console_command(world, 'viewmode ' + mode)
        u.log('LOOT_DETAIL_VIEW ' + name + ' eye=' + str(eye))
    if local == 110:
        path = str(OUT / (name + '.png')).replace('\\', '/')
        u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + path)
        u.log('LOOT_DETAIL_CAPTURE ' + path)
    if local == 139:
        state['index'] += 1
        if state['index'] == len(shots):
            u.unregister_slate_post_tick_callback(state['handle'])
            u.SystemLibrary.execute_console_command(world, 'quit')


state['handle'] = u.register_slate_post_tick_callback(tick)
