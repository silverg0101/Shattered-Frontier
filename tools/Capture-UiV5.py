"""One -game session that stages the HUD screens reachable by console (gameplay, build, inventory with the cursor on bag cell 1, market, trade)
and screenshots each with "Shot showui". Usage (-game client, 1920x1080):
    UnrealEditor.exe <uproject> <map> -game -windowed -ResX=1920 -ResY=1080 -ForceRes "-ExecCmds=py tools/Capture-UiV5.py <out folder>"
Writes <out folder>/<name>-live.png. Gathering, station, defeated and loading need their server scenarios (tools/Capture-UiScene.ps1)."""
import sys, ctypes
from ctypes import wintypes
from pathlib import Path
import unreal as u

OUT = Path(sys.argv[1]); OUT.mkdir(parents=True, exist_ok=True)
SHOTS = Path(u.Paths.screen_shot_dir())
# (seconds after the pawn exists, action): 'cmd:<console>' | 'cursor:<x>,<y>' | 'shot:<name>'
STEPS = [(6, 'shot:gameplay-hud'), (7, 'cmd:SFUi build'), (10, 'shot:build-mode'), (11, 'cmd:SFUi build'),
         (13, 'cmd:SFUi inventory'), (14, 'cursor:140,624'), (16, 'shot:inventory'), (17, 'cmd:SFUi inventory'),
         (19, 'cmd:SFUi market'), (23, 'shot:market-board'), (24, 'cmd:SFUi market'),
         (26, 'cmd:SFUi trade'), (28, 'cmd:SFUi tradelock'), (30, 'shot:trade'), (32, 'quit')]
state = dict(world=None, t=0.0, i=0, pending=[])


def set_cursor(x, y):
    """Move the real OS cursor to client pixel (x, y) of the game window (Slate reads GetCursorPos for hover)."""
    hwnd = ctypes.windll.user32.GetForegroundWindow(); pt = wintypes.POINT(int(x), int(y))
    ctypes.windll.user32.ClientToScreen(hwnd, ctypes.byref(pt)); ctypes.windll.user32.SetCursorPos(pt.x, pt.y)


def collect():
    """Rename finished engine screenshots to the queued names."""
    for path in sorted(SHOTS.rglob('*.png'), key=lambda p: p.stat().st_mtime):
        if not state['pending']:
            return
        try:
            path.replace(OUT / (state['pending'][0] + '-live.png')); state['pending'].pop(0)
        except OSError:
            return


def on_tick(delta):
    try:
        world = state['world'] or next((w for w in u.ObjectIterator(u.World) if u.GameplayStatics.get_player_controller(w, 0)
                                        and u.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn()), None)
        if not world:
            return
        state['world'] = world; collect(); state['t'] += delta
        while state['i'] < len(STEPS) and state['t'] >= STEPS[state['i']][0]:
            action = STEPS[state['i']][1]; state['i'] += 1
            if action.startswith('cmd:'):
                u.SystemLibrary.execute_console_command(world, action[4:])
            elif action.startswith('cursor:'):
                set_cursor(*action[7:].split(','))
            elif action.startswith('shot:'):
                state['pending'].append(action[5:]); u.SystemLibrary.execute_console_command(world, 'Shot showui')
            elif action == 'quit' and not state['pending']:
                u.unregister_slate_post_tick_callback(state['handle']); u.SystemLibrary.execute_console_command(world, 'quit')
            elif action == 'quit':
                state['i'] -= 1; break
    except Exception as error:
        u.log_error('UI_CAPTURE error %r' % (error,)); u.unregister_slate_post_tick_callback(state['handle'])


state['handle'] = u.register_slate_post_tick_callback(on_tick)
