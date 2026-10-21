"""Locomotion check on the isolated test map: idle 2 s, then walk forward 3 s (Python in a -game run).

    UnrealEditor.exe <project>.uproject /Game/Frontier/Creatures/Voidreign/V1/Test/VoidreignTest -game -RenderOffscreen
        -windowed -ResX=1280 -ResY=720 -unattended "-ExecCmds=py <project>/tools/Check-VoidreignLocomotion.py"

Fails the old blend-space fault (reference pose: static legs, rider standing): hoof height must vary while walking and
the idle rider's foot must hang well below its reference (standing) height. Writes Artifacts/Art/Voidreign/locomotion.json,
takes screenshots (idle, walking) and quits.
"""
import json, time
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_dir()).resolve().parent / 'Artifacts' / 'Art' / 'Voidreign' / 'locomotion.json'
HOOF, FOOT = 'PRI_fore_L_fetlock', 'foot_l'
S = dict(actor=None, start=None, rows=[], shots=[1.5, 2.6, 3.0], handle=None)


def tick(dt):
    if S['actor'] is None:
        for a in unreal.ObjectIterator(unreal.Character):
            if 'Voidreign' in a.get_class().get_name() and a.get_world():
                a.set_editor_property('DemoAbilityInterval', 0.0); a.spawn_default_controller()
                S['actor'] = a; S['start'] = time.time()
        return
    a = S['actor']; t = time.time() - S['start']
    if 2 < t < 5: a.add_movement_input(a.get_actor_forward_vector(), 0.5)
    hoof = a.mesh.get_socket_transform(HOOF, unreal.RelativeTransformSpace.RTS_COMPONENT).translation.z
    rider = a.get_editor_property('RiderMesh')
    foot = rider.get_socket_transform(FOOT, unreal.RelativeTransformSpace.RTS_COMPONENT).translation.z
    S['rows'].append(dict(t=round(t, 3), v=round(a.get_velocity().size2d() if hasattr(a.get_velocity(), 'size2d') else a.get_velocity().length()), hoof=round(hoof, 2), foot=round(foot, 2)))
    if S['shots'] and t > S['shots'][0]:
        unreal.SystemLibrary.execute_console_command(a.get_world(), 'HighResShot 1280x720'); S['shots'].pop(0)
    if t > 5.5:
        unreal.unregister_slate_post_tick_callback(S['handle'])
        ref = rider.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
        ref_foot = unreal.AnimPoseExtensions.get_ref_bone_pose(unreal.AnimPoseExtensions.get_reference_pose(ref), FOOT, unreal.AnimPoseSpaces.WORLD).translation.z
        idle = [r for r in S['rows'] if 0.5 < r['t'] < 2]; walk = [r for r in S['rows'] if 3 < r['t'] < 5]
        rep = dict(walk_speed=max(r['v'] for r in walk), hoof_range_walk_cm=round(max(r['hoof'] for r in walk) - min(r['hoof'] for r in walk), 2),
                   hoof_range_idle_cm=round(max(r['hoof'] for r in idle) - min(r['hoof'] for r in idle), 2),
                   rider_foot_idle_cm=round(sum(r['foot'] for r in idle) / len(idle), 2), rider_foot_reference_cm=round(ref_foot, 2))
        rep['legs_move'] = rep['hoof_range_walk_cm'] > 5 and rep['walk_speed'] > 50
        rep['rider_seated'] = rep['rider_foot_idle_cm'] < rep['rider_foot_reference_cm'] - 20
        OUT.parent.mkdir(parents=True, exist_ok=True); OUT.write_text(json.dumps(rep, indent=1))
        unreal.log('VOIDREIGN_LOCOMOTION ' + json.dumps(rep))
        unreal.SystemLibrary.quit_game(a.get_world(), None, unreal.QuitPreference.QUIT, False)


S['handle'] = unreal.register_slate_post_tick_callback(tick)
