"""In-engine timed captures for UI evidence: a viewport screenshot with the UI every <interval> s for <seconds> s, then quit.

Launch on a -game client (scenario flags decide what happens on screen):
    UnrealEditor.exe <uproject> 127.0.0.1:<port> -game -windowed -ResX=1920 -ResY=1080 -ForceRes
        "-ExecCmds=py tools/Capture-TimedShots.py <out folder> [seconds=90] [interval=1]"
Shots start once the local player controller has a pawn. HighResShot renders the scene without UMG, so this uses the
engine's "Shot showui" (viewport resolution, UI included); the engine writes each PNG under the client's
Saved/Screenshots folder, and this script moves it to <out folder>/shot-NNN.png on the next tick.
"""
import sys
from pathlib import Path
import unreal as u

OUT = Path(sys.argv[1]); OUT.mkdir(parents=True, exist_ok=True)
SECONDS = float(sys.argv[2]) if len(sys.argv) > 2 else 90.0
INTERVAL = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
state = dict(world=None, elapsed=0.0, next=0.0, count=0, moved=0)
SHOTS = Path(u.Paths.screen_shot_dir())


def collect():
    """Move finished engine screenshots into OUT in creation order."""
    for path in sorted(SHOTS.rglob('*.png'), key=lambda p: p.stat().st_mtime):
        try:
            path.replace(OUT / ('shot-%03d.png' % state['moved'])); state['moved'] += 1
        except OSError:
            return  # still being written


def on_tick(delta):
    try:
        world = state['world'] or next((w for w in u.ObjectIterator(u.World) if u.GameplayStatics.get_player_controller(w, 0)
                                        and u.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn()), None)
        if not world:
            return
        state['world'] = world
        collect()
        state['elapsed'] += delta
        if state['elapsed'] >= state['next']:
            u.SystemLibrary.execute_console_command(world, 'Shot showui')
            u.log('UI_CAPTURE %d' % state['count'])
            state['count'] += 1; state['next'] += INTERVAL
        if state['elapsed'] > SECONDS:
            u.unregister_slate_post_tick_callback(state['handle'])
            u.SystemLibrary.execute_console_command(world, 'quit')
    except Exception as error:  # never leave a -game session running on a script error
        u.log_error('UI_CAPTURE error %r' % (error,))
        u.unregister_slate_post_tick_callback(state['handle'])


state['handle'] = u.register_slate_post_tick_callback(on_tick)
