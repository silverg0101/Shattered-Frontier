"""Capture one live UI state in a 1920x1080 -game client.

Usage: -ExecCmds="py <this file> <state> <output directory>"
The server scene is selected by Capture-UiScene.ps1; this driver uses dev SFUi
commands to operate the controller and Shot showui for the final frame.
The original one-argument invocation still captures the inventory/market/trade/build sequence.
"""
import sys
import ctypes
from ctypes import wintypes
import time
from pathlib import Path
import unreal as u

name = sys.argv[1] if len(sys.argv) > 2 else 'all'
out = Path(sys.argv[2] if len(sys.argv) > 2 else sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
shots = Path(u.Paths.screen_shot_dir())
started = time.time()
plans = {
    'all': [(6, 'shot:gameplay-hud'), (7, 'cmd:SFUi build'), (10, 'shot:build-mode'),
            (11, 'cmd:SFUi build'), (13, 'cmd:SFUi inventory'), (14, 'cursor:140,624'),
            (16, 'shot:inventory'), (17, 'cmd:SFUi inventory'),
            (19, 'cmd:SFUi market'), (23, 'shot:market-board'), (24, 'cmd:SFUi market'),
            (26, 'cmd:SFUi trade'), (28, 'cmd:SFUi tradelock'), (30, 'shot:trade')],
    'gameplay-hud': [(4, 'cmd:SFUi target'), (5, 'shot')],
    'gathering': [(1.5, 'cmd:SFUi skills'), (2.0, 'cmd:SFUi aim'), (3, 'shot')],
    'crafting-station': [(4, 'cmd:SFUi station'), (6, 'shot')],
    'inventory': [(6, 'cmd:SFUi inventory'), (8, 'cursor:180,312'), (9, 'shot')],
    'defeated': [],
}
steps = plans[name] + [(35 if name == 'defeated' else 32 if name == 'all' else 12, 'quit')]
state = {'world': None, 'time': 0.0, 'index': 0, 'pending': False,
         'pending_name': None, 'defeat_seen': None, 'defeat_shot': False}


def set_cursor(x, y):
    hwnd = ctypes.windll.user32.GetForegroundWindow()
    point = wintypes.POINT(int(x), int(y))
    ctypes.windll.user32.ClientToScreen(hwnd, ctypes.byref(point))
    ctypes.windll.user32.SetCursorPos(point.x, point.y)


def shoot(world):
    u.SystemLibrary.execute_console_command(world, 'SFUi rects')  # exact widget rects of this frame -> Saved/UiRects.json
    u.SystemLibrary.execute_console_command(world, 'Shot showui')


def collect():
    if not state['pending']:
        return
    candidates = [p for p in shots.rglob('*.png') if p.stat().st_mtime >= started]
    if candidates:
        newest = max(candidates, key=lambda p: p.stat().st_mtime)
        try:
            newest.replace(out / (state['pending_name'] + '-live.png'))
            state['pending'] = False
            try:
                import shutil; shutil.copyfile(str(Path(u.Paths.project_saved_dir()) / 'UiRects.json'), str(out / (state['pending_name'] + '-live.json')))
            except OSError:
                pass
            u.log('UI_CAPTURE_SAVED ' + str(out / (state['pending_name'] + '-live.png')))
        except OSError:
            pass


def tick(delta):
    try:
        world = state['world'] or next((w for w in u.ObjectIterator(u.World)
            if u.GameplayStatics.get_player_controller(w, 0)
            and u.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn()), None)
        if not world:
            return
        state['world'] = world
        state['time'] += delta
        collect()
        if name == 'defeated' and state['defeat_seen'] is None and state['time'] >= 3:
            pawn = u.GameplayStatics.get_player_controller(world, 0).get_controlled_pawn()
            if pawn.get_pilot_health_for_capture() == 0:
                state['defeat_seen'] = state['time']
                u.log('UI_CAPTURE_DEFEATED_HEALTH_ZERO')
        if name == 'defeated' and state['defeat_seen'] is not None and not state['defeat_shot'] and state['time'] >= state['defeat_seen'] + 0.35:
            state['pending'] = True
            state['pending_name'] = name
            state['defeat_shot'] = True
            shoot(world)
        while state['index'] < len(steps) and state['time'] >= steps[state['index']][0]:
            action = steps[state['index']][1]
            state['index'] += 1
            if action.startswith('cmd:'):
                u.SystemLibrary.execute_console_command(world, action[4:])
            elif action.startswith('cursor:'):
                set_cursor(*action[7:].split(','))
            elif action == 'shot' or action.startswith('shot:'):
                state['pending'] = True
                state['pending_name'] = action[5:] if action.startswith('shot:') else name
                shoot(world)
            elif action == 'quit':
                if state['pending']:
                    state['index'] -= 1
                    break
                u.unregister_slate_post_tick_callback(state['handle'])
                u.SystemLibrary.execute_console_command(world, 'quit')
    except Exception as error:
        u.log_error('UI_CAPTURE_ERROR %r' % (error,))
        u.unregister_slate_post_tick_callback(state['handle'])


state['handle'] = u.register_slate_post_tick_callback(tick)
