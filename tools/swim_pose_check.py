"""Standalone Silverbrook check (run by Test-SwimPose.ps1): a pawn placed in Eastwood Pond must swim (server depth gate) and show the swim pose.
Prints SWIM_POSE_CHECK PASS or SWIM_POSE_CHECK FAIL <reason>, then quits."""
import unreal as u

world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pawn = u.GameplayStatics.get_player_character(world, 0)
pond = next((a for a in u.GameplayStatics.get_all_actors_of_class(world, u.PhysicsVolume) if a.get_actor_label() == 'EastwoodPond_Swim'), None)
state = {'f': 0}


def finish(msg):
    u.log('SWIM_POSE_CHECK ' + msg)
    u.unregister_slate_post_tick_callback(state['h']); u.SystemLibrary.execute_console_command(world, 'quit')


def tick(_dt):
    state['f'] += 1
    f = state['f']
    if f == 1:
        if not pond: return finish('FAIL no EastwoodPond_Swim volume in the map')
        o, e = pond.get_actor_bounds(False)
        pawn.set_actor_location(u.Vector(o.x, o.y, o.z + e.z - 40), False, False)
    elif f == 240:  # ~2 s: the server rule has switched the movement mode and the client mesh follows
        swimming = pawn.character_movement.movement_mode == u.MovementMode.MOVE_SWIMMING
        if not swimming: return finish(f'FAIL not swimming in Eastwood Pond (mode {pawn.character_movement.movement_mode})')
        if not pawn.is_swim_pose_active(): return finish('FAIL swimming but the swim pose is not active')
        finish('PASS')


state['h'] = u.register_slate_post_tick_callback(tick)
