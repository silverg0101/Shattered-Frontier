"""Capture each loot mesh in the review sandbox at 1920x1080 in -game mode."""
import json
import math
import os
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Artifacts/LootItems/in-engine/round2'
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
only = os.environ.get('SF_LOOT_ONLY')
if only:
    rows = [row for row in rows if row['item_id'][5:] == only]
    assert len(rows) == 1, only
for row in rows:
    stem = row['item_id'][5:]
    assert 'Loot_' + stem in actors and 'Scale10cm_' + stem in actors, stem
    ref = actors['Scale10cm_' + stem]
    assert abs(ref.get_actor_scale3d().x - .1) < .001, stem
assert 'Loot_wolfhide_bracers_mirrored' in actors
mirrored = actors['Loot_wolfhide_bracers_mirrored']
assert mirrored.get_actor_scale3d().y < 0
assert mirrored.static_mesh_component.static_mesh.get_name() == 'SM_WolfhideBracers'
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
    group = [actor, ref]
    if stem == 'wolfhide_bracers':
        group.append(mirrored)
    bounds = [prop.get_actor_bounds(False) for prop in group]
    lo_x = min(center.x - extent.x for center, extent in bounds)
    hi_x = max(center.x + extent.x for center, extent in bounds)
    lo_y = min(center.y - extent.y for center, extent in bounds)
    hi_y = max(center.y + extent.y for center, extent in bounds)
    lo_z = min(center.z - extent.z for center, extent in bounds)
    hi_z = max(center.z + extent.z for center, extent in bounds)
    center_x = (lo_x + hi_x) / 2
    center_y = (lo_y + hi_y) / 2
    # At 50° horizontal FOV, fit the item plus 10 cm cube with 20% margin.
    distance = max(22.0, (hi_x - lo_x) / (2 * math.tan(math.radians(25)) * .8))
    eye = u.Vector(center_x + distance * .12, center_y - distance,
                   max(20.0, hi_z + distance * .6))
    look = u.Vector(center_x, center_y, (lo_z + hi_z) / 2)
    fov = 50.0
    if stem == 'wolf_fang':
        # A 5 cm fang beside a 10 cm cube needs a nearly top-down detail view.
        eye = u.Vector(center_x, center_y - 5, 40)
        look = u.Vector(center_x, center_y, 0)
        fov = 30.0
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
        camera.camera_component.set_field_of_view(fov)
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
