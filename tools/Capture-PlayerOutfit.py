"""Player outfit review shots in game: walking from behind, then idle from the front, three-quarter and side (idle, so the frames are still).

UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -windowed -ResX=1920 -ResY=1080 -ForceRes
    -RenderOffscreen -unattended -nosound "-ExecCmds=py tools/Capture-PlayerOutfit.py"
Writes outfit_<view>.png (3840x2160, for close crops) under Saved/Screenshots/; pair them with the concept for docs/visual-review.md.
"""
import unreal

PLAN = [(10.0, 'walk', 14.0), (14.0, 'shot', 'walk_back'), (16.0, 'view', 0.0), (18.0, 'shot', 'front'),
        (18.5, 'view', 40.0), (20.5, 'shot', 'three_quarter'), (21.0, 'view', 90.0), (23.0, 'shot', 'side'), (25.0, 'quit', 0)]
state = {'t': 0.0, 'done': set()}


def tick(dt):
    w = state.get('world') or next((x for x in unreal.ObjectIterator(unreal.World)
                                    if unreal.GameplayStatics.get_player_controller(x, 0)
                                    and unreal.GameplayStatics.get_player_controller(x, 0).get_controlled_pawn()), None)
    if not w:
        return
    state['world'] = w
    state['t'] += dt
    t = state['t']
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    for i, (start, kind, arg) in enumerate(PLAN):
        if kind == 'walk' and start <= t < arg:
            yaw = unreal.Rotator(roll=0, pitch=0, yaw=pc.get_control_rotation().yaw)
            pawn.add_movement_input(yaw.get_forward_vector(), 1.0, True)
        if t < start or i in state['done'] or kind == 'walk':
            continue
        state['done'].add(i)
        if kind == 'view':  # control rotation turns both camera and character: unhook the character and face it arg degrees off the lens
            pawn.set_editor_property('use_controller_rotation_yaw', False)
            cam_yaw = pc.player_camera_manager.get_camera_rotation().yaw
            pawn.set_actor_rotation(unreal.Rotator(roll=0, pitch=0, yaw=cam_yaw + 180.0 - arg), False)
        elif kind == 'shot':
            unreal.SystemLibrary.execute_console_command(w, f'HighResShot 2 filename=outfit_{arg}')
            unreal.log(f'OUTFIT_SHOT {arg} t={t:.1f}')
        elif kind == 'quit':
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
