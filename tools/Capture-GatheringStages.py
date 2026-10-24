"""GAT-01 growth stages in game: one gathering node shown at young, growing and mature, composed side by side.

In game (Silverbrook, -game):
UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -windowed -ResX=1920 -ResY=1080 -ForceRes
    -RenderOffscreen -unattended -nosound "-ExecCmds=py tools/Capture-GatheringStages.py"
Writes gat01_<stage>.png (HighResShot) under Saved/Screenshots/.
Then, outside Unreal: py -3 tools/Capture-GatheringStages.py <shots dir> docs/evidence/gat01-stages/stages.png   (needs Pillow)
Env SF_NODE_KIND=Iron|Oak|Stone picks the node kind (default Iron: the Silverbrook iron veins carry the growth profile).
"""
import os
import sys

STAGES = ['young', 'growing', 'mature']

try:
    import unreal
except ImportError:
    unreal = None

if unreal:
    KIND = os.environ.get('SF_NODE_KIND', 'Iron')
    # (start second, action, arg). Move the pawn once the world has settled, then preview each stage and shoot it.
    PLAN = [(12.0, 'place', 0)]
    for i, name in enumerate(STAGES):
        PLAN += [(15.0 + i * 5.0, 'stage', name), (18.0 + i * 5.0, 'shot', name)]
    PLAN += [(15.0 + len(STAGES) * 5.0, 'quit', 0)]
    state = {'t': 0.0, 'done': set()}

    def tick(dt):
        w = state.get('world') or next((x for x in unreal.ObjectIterator(unreal.World)
                                        if unreal.GameplayStatics.get_player_controller(x, 0)
                                        and unreal.GameplayStatics.get_player_controller(x, 0).get_controlled_pawn()), None)
        if not w:
            return
        state['world'] = w
        state['t'] += dt
        pc = unreal.GameplayStatics.get_player_controller(w, 0)
        pawn = pc.get_controlled_pawn()
        for i, (start, kind, arg) in enumerate(PLAN):
            if state['t'] < start or i in state['done']:
                continue
            state['done'].add(i)
            if kind == 'place':
                nodes = [n for n in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.SFGatheringPilotNode)
                         if str(n.get_editor_property('kind')).endswith(KIND.upper()) or KIND.lower() in str(n.get_editor_property('kind')).lower()]
                here = pawn.get_actor_location()
                node = min(nodes, key=lambda n: (n.get_actor_location() - here).length())
                state['node'] = node
                at = node.get_actor_location()
                # stand 4 m south of the node, looking north at it, character hidden so it does not block the view
                pawn.set_actor_location(at + unreal.Vector(0, -400, 80), False, True)
                pc.set_control_rotation(unreal.Rotator(roll=0, pitch=-8, yaw=90))
                pawn.set_actor_hidden_in_game(True)
                unreal.log(f'GROWTH_SHOT_NODE {node.get_name()} {at}')
            elif kind == 'stage':
                state['node'].dev_preview_growth_stage(arg)
            elif kind == 'shot':
                unreal.SystemLibrary.execute_console_command(w, f'HighResShot 2 filename=gat01_{arg}')
                unreal.log(f'GROWTH_SHOT {arg}')
            elif kind == 'quit':
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.SystemLibrary.quit_editor()

    handle = unreal.register_slate_post_tick_callback(tick)
else:
    from PIL import Image, ImageDraw
    src, dst = sys.argv[1], sys.argv[2]
    shots = [Image.open(os.path.join(src, f'gat01_{s}.png')).convert('RGB') for s in STAGES]
    w, h = shots[0].size
    sheet = Image.new('RGB', (w * len(shots) // 2, h // 2 + 40), (20, 20, 20))
    for i, (s, im) in enumerate(zip(STAGES, shots)):
        sheet.paste(im.resize((w // 2, h // 2)), (i * w // 2, 40))
        ImageDraw.Draw(sheet).text((i * w // 2 + 12, 12), s, fill=(255, 255, 255))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    sheet.save(dst)
    print('wrote', dst)
