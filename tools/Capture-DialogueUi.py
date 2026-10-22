"""In-engine capture of the v5 NPC dialogue board. Launch a -game client (see docs/blueprints/ui-hud.md section 4):
    UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -windowed -ResX=1920 -ResY=1080 -ForceRes
        -nosound -nop4 -UserDir=<dir> "-ExecCmds=py tools/Capture-DialogueUi.py <out folder> ["x y yaw" metres/deg]"
Once the pawn exists it stands the Millwarden ahead (SFSpawnDialogueNpc opens his board), waits for the framing camera to settle,
shoots 'Shot showui' as dialogue-open, picks response 1 ('Tell me more') and shoots dialogue-more, accepts the offer and shoots
dialogue-accepted, then quits. The engine writes PNGs under Saved/Screenshots; they are moved to <out>/<name>.png, each with
<name>.json, the exact widget rects of that frame ('SFUi rects'), which tools/ui_pixel_diff.py reads for live geometry."""
import ctypes, shutil, sys
from ctypes import wintypes
from pathlib import Path
import unreal as u

OUT = Path(sys.argv[1]); OUT.mkdir(parents=True, exist_ok=True)
SHOTS = Path(u.Paths.screen_shot_dir())
# (seconds after the pawn appeared, console command, shot name or None)
STAGE = sys.argv[2] if len(sys.argv) > 2 else '-138 300 0'  # Silverbrook river trail west of the mill, facing east (ui-hud.md section 4)
PLAN = [(2.0, 'SFStageAt ' + STAGE, None), (3.0, 'cursor:960,1060', None), (4.0, 'SFSpawnDialogueNpc', None), (11.0, 'Shot showui', 'dialogue-open'), (12.0, 'SFDialogueChoose 0', None),
        (14.0, 'Shot showui', 'dialogue-more'), (15.0, 'SFDialogueChoose 0', None), (17.0, 'Shot showui', 'dialogue-accepted'), (19.0, 'quit', None)]
state = dict(world=None, elapsed=0.0, step=0, pending=[], handle=None)


def set_cursor(x, y):
    """Parks the OS cursor at client pixel (x, y), off the board, so hover does not pick a row (Slate reads GetCursorPos)."""
    hwnd = ctypes.windll.user32.GetForegroundWindow(); pt = wintypes.POINT(int(x), int(y))
    ctypes.windll.user32.ClientToScreen(hwnd, ctypes.byref(pt)); ctypes.windll.user32.SetCursorPos(pt.x, pt.y)


def collect():
    for path in sorted(SHOTS.rglob('*.png'), key=lambda p: p.stat().st_mtime):
        if not state['pending']:
            return
        try:
            path.replace(OUT / (state['pending'][0] + '.png'))
        except OSError:
            return
        shutil.copyfile(str(Path(u.Paths.project_saved_dir()) / 'UiRects.json'), str(OUT / (state['pending'].pop(0) + '.json')))


def on_tick(delta):
    try:
        world = state['world'] or next((w for w in u.ObjectIterator(u.World) if u.GameplayStatics.get_player_controller(w, 0)
                                        and u.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn()), None)
        if not world:
            return
        state['world'] = world
        state['elapsed'] += delta
        collect()
        while state['step'] < len(PLAN) and state['elapsed'] >= PLAN[state['step']][0]:
            _, command, name = PLAN[state['step']]
            state['step'] += 1
            if name:
                state['pending'].append(name)
                u.SystemLibrary.execute_console_command(world, 'SFUi rects')  # this frame's widget rects -> Saved/UiRects.json
            if command.startswith('cursor:'):
                set_cursor(*command[7:].split(','))
            else:
                u.SystemLibrary.execute_console_command(world, command)
            u.log('DIALOGUE_CAPTURE ' + command)
    except Exception as error:  # never leave a -game session running on a script error
        u.log_error('DIALOGUE_CAPTURE error %r' % (error,))
        u.unregister_slate_post_tick_callback(state['handle'])


state['handle'] = u.register_slate_post_tick_callback(on_tick)
