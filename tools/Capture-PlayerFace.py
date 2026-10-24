"""Close head-and-shoulders shots of every player face in game (faces.json ids), front view, for skin/hair review.

UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -windowed -ResX=1920 -ResY=1080 -ForceRes
    -RenderOffscreen -unattended -nosound "-ExecCmds=py tools/Capture-PlayerFace.py"
One launch walks every face: SFFace <id>, 15 s for the outfit to compile, a spawned camera 120 cm in front of the head (FOV 35, ~45 cm
tall frame), HighResShot 2 -> face_<id>.png under Saved/Screenshots/. Env SF_FACES=<id,id> limits it; SF_DROP, SF_ORBIT, SF_TAG, SF_DIST frame collar and three-quarter shots. Compose with tools/Make-FaceSheet.py.
"""
import json
import math
import os
from pathlib import Path

import unreal

IDS = [i for i in (os.environ.get('SF_FACES') or 'Godric,Aelfwyn,Edwin,Hilda,Orlando,Wulfric,Eadgyth,Osric,Ebba').split(',') if i]
SUN_YAW = float(os.environ.get('SF_SUN_YAW', 20))  # degrees off the sun direction, for some side light
DIST, FOV, STEP = float(os.environ.get('SF_DIST', 120)), float(os.environ.get('SF_FOV', 35)), 22.0
DROP, ORBIT, TAG = float(os.environ.get('SF_DROP', 0)), math.radians(float(os.environ.get('SF_ORBIT', 0))), os.environ.get('SF_TAG', 'face')  # DROP: aim cm below the head bone (collar shots); ORBIT: camera degrees around the head (three-quarter); TAG: file prefix
MALE = {f['id']: f['gender'] == 'male' for f in json.load(open(Path(__file__).resolve().parents[1] / 'GameClient/ArtSource/Characters/Player/faces.json'))['faces']}
state = {'t': 0.0, 'done': set(), 'cam': None}


def head(pawn, face):  # head bone + a per-gender drop: the bone sits about 18 cm higher on the men's bodies than the women's
    for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
        if c.does_socket_exist('head'):
            return c.get_socket_location('head') + unreal.Vector(0, 0, 4 if MALE.get(face) else -14)
    return pawn.get_actor_location() + unreal.Vector(0, 0, 72)


def tick(dt):
    w = state.get('world') or next((x for x in unreal.ObjectIterator(unreal.World)
                                    if unreal.GameplayStatics.get_player_controller(x, 0)
                                    and unreal.GameplayStatics.get_player_controller(x, 0).get_controlled_pawn()), None)
    if not w:
        return
    state['world'] = w
    state['t'] += dt
    t = state['t'] - 3.0
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    n = int(t // STEP) if t >= 0 else -1
    if n >= len(IDS):
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
        return
    if n < 0:
        return
    phase = t - n * STEP
    face = IDS[n]
    if phase >= 0 and (n, 'face') not in state['done']:
        state['done'].add((n, 'face'))
        unreal.SystemLibrary.execute_console_command(w, f'SFFace {face}', pc)
    if phase >= 15.0:  # keep the camera on the head until the shot (the pawn is idle, but the head moves with the new body)
        pawn = pc.get_controlled_pawn()
        pawn.set_editor_property('use_controller_rotation_yaw', False)
        sun = state.setdefault('sun', unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DirectionalLight))  # face the sun so the face is lit
        if sun:
            d = sun[0].get_actor_forward_vector()
            pawn.set_actor_rotation(unreal.Rotator(roll=0, pitch=0, yaw=math.degrees(math.atan2(-d.y, -d.x)) + SUN_YAW), False)
        h = head(pawn, face) - unreal.Vector(0, 0, DROP)
        f0 = pawn.get_actor_forward_vector()
        fwd = unreal.Vector(f0.x * math.cos(ORBIT) - f0.y * math.sin(ORBIT), f0.x * math.sin(ORBIT) + f0.y * math.cos(ORBIT), 0)
        pos = h + fwd * DIST
        if state['cam'] is None:  # the pawn's own follow camera, made world-absolute so the boom no longer moves it (no spawn needed; the pawn stays the view target)
            state['cam'] = pawn.get_component_by_class(unreal.CameraComponent)
            state['cam'].set_editor_property('field_of_view', FOV)
            state['cam'].set_absolute(True, True, True)  # stop the boom driving it
        state['cam'].set_world_location_and_rotation(pos, unreal.MathLibrary.find_look_at_rotation(pos, h), False, False)
        if phase >= 18.0 and (n, 'shot') not in state['done']:
            state['done'].add((n, 'shot'))
            unreal.SystemLibrary.execute_console_command(w, f'HighResShot 2 filename={TAG}_{face}')
            unreal.log(f'FACE_SHOT {face} t={t:.1f}')


handle = unreal.register_slate_post_tick_callback(tick)
