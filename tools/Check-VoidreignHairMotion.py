"""Runtime hair check on the isolated test map: hair physics off for 8 s, then on for 8 s, same ability demo.

    UnrealEditor.exe <project>.uproject /Game/Frontier/Creatures/Voidreign/V1/Test/VoidreignTest -game -RenderOffscreen
        -windowed -ResX=1280 -ResY=720 -unattended "-ExecCmds=py <project>/tools/Check-VoidreignHairMotion.py"
    (Git Bash: prefix MSYS2_ARG_CONV_EXCL="*" or the /Game path is rewritten.)

Per frame: the largest change of any SEC bone's direction (deg, component space). Writes
Artifacts/Art/Voidreign/hair-motion.json with p99/max per 2 s window and per half, then quits.
Map: tools/Create-VoidreignTestMap.py. Physics: tools/Create-VoidreignHairPhysics.py.
"""
import json, math, time
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_dir()).resolve().parent / 'Artifacts' / 'Art' / 'Voidreign' / 'hair-motion.json'
S = dict(actor=None, start=None, prev=None, rows=[], sec=None, on=False, handle=None)


def tick(dt):
    if S['actor'] is None:
        for a in unreal.ObjectIterator(unreal.Character):
            if 'Voidreign' in a.get_class().get_name() and a.get_world():
                S['actor'] = a; S['start'] = time.time(); a.mesh.toggle_disable_post_process_blueprint()  # start with it off
                S['sec'] = [str(n) for n in a.mesh.get_all_socket_names() if str(n).startswith('SEC_')]
        return
    a = S['actor']; m = a.mesh; t = time.time() - S['start']
    if t > 8 and not S['on']: m.toggle_disable_post_process_blueprint(); S['on'] = True
    dirs = {b: m.get_socket_transform(b, unreal.RelativeTransformSpace.RTS_COMPONENT).rotation.rotate_vector(unreal.Vector(0, 1, 0)) for b in S['sec']}
    if S['prev']:
        step, bone = max((math.degrees(math.acos(max(-1., min(1., S['prev'][b].dot(dirs[b]))))), b) for b in S['sec'])
        S['rows'].append(dict(t=round(t, 3), step=round(step, 2), bone=bone, on=S['on'], action=a.is_performing_action()))
    S['prev'] = dirs
    if t > 16:
        unreal.unregister_slate_post_tick_callback(S['handle'])
        q = lambda rows: (lambda v: dict(frames=len(v), p99=v[int(len(v) * .99) - 1], max=v[-1]) if v else {})(sorted(r['step'] for r in rows))
        rep = dict(off=q([r for r in S['rows'] if not r['on']]), on=q([r for r in S['rows'] if r['on'] and r['t'] > 8.05]),
                   windows={f'{w}-{w + 2}s': q([r for r in S['rows'] if w <= r['t'] < w + 2]) for w in range(0, 16, 2)},
                   spikes_over_40=[r for r in S['rows'] if r['on'] and r['t'] > 8.05 and r['step'] > 40])
        OUT.parent.mkdir(parents=True, exist_ok=True); OUT.write_text(json.dumps(rep, indent=1))
        unreal.log('HAIR_MOTION ' + json.dumps(dict(off=rep['off'], on=rep['on'], spikes=len(rep['spikes_over_40']))))
        unreal.SystemLibrary.quit_game(a.get_world(), None, unreal.QuitPreference.QUIT, False)


S['handle'] = unreal.register_slate_post_tick_callback(tick)
