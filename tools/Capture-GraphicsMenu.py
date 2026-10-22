"""In-engine capture of the Settings > Graphics menu and the scene at three presets. Launch a -game client on the review map:
    UnrealEditor.exe <uproject> /Game/Frontier/Maps/Dev/GraphicsSettings/GraphicsSettings_Review -game -windowed -ResX=1920 -ResY=1080 -ForceRes
        -nosound -nop4 -UserDir=<temp dir> "-ExecCmds=py tools/Capture-GraphicsMenu.py <out folder> ["x y yaw"]"
Once the pawn exists it opens the menu through the dev SFGfx command, selects each preset and shoots 'Shot showui' (menu-<preset>), then
the Custom label, the keep-or-revert prompt, and finally closes the menu and shoots the bare scene at Low, High and Ultra (scene-<preset>).
The High menu and the keep prompt are also written as settings-graphics-live.png / settings-keep-revert-live.png for tools/ui_pixel_diff.py,
each menu shot with its SFUi rects widget dump (<name>.json) so the diff measures exact live geometry.
The optional third argument (metres/degrees, SFStageAt) stands the pawn there before the scene shots; each scene shot also writes
scene-<preset>-cvars.txt with the scalability and Nanite values in effect, so the preset step is evidenced beyond the picture.
The engine writes PNGs under Saved/Screenshots; they are moved into <out>."""
import shutil
import sys
from pathlib import Path
import unreal as u

OUT = Path(sys.argv[1]); OUT.mkdir(parents=True, exist_ok=True)
SHOTS = Path(u.Paths.screen_shot_dir())
# (seconds after the pawn appeared, console command, shot name or None)
PLAN = [(2.0, 'SFGfx apply High', None), (4.0, 'SFGfx menu High', None), (6.0, 'Shot showui', 'menu-high')]
t = 7.0
for preset in ('Low', 'Medium', 'Epic', 'Ultra'):
    PLAN += [(t, 'SFGfx menu ' + preset, None), (t + 1.2, 'Shot showui', 'menu-' + preset.lower())]
    t += 2.4
PLAN += [(t, 'SFGfx custom', None), (t + 1.2, 'Shot showui', 'menu-custom'), (t + 2.4, 'SFGfx keep', None), (t + 3.6, 'Shot showui', 'menu-keep'),
         (t + 4.4, 'SFGfx close', None), (t + 4.6, 'SFGfx close', None)]
t += 6.0
if len(sys.argv) > 2:
    PLAN += [(t, 'SFStageAt ' + sys.argv[2], None)]
    t += 3.0
for preset in ('Low', 'High', 'Ultra'):
    PLAN += [(t, 'SFGfx apply ' + preset, None), (t + 6.0, 'cvars:scene-' + preset.lower(), None), (t + 6.0, 'Shot', 'scene-' + preset.lower())]
    t += 8.0
PLAN += [(t, 'quit', None)]
state = dict(world=None, elapsed=0.0, step=0, pending=[], handle=None)


def collect():
    for path in sorted(SHOTS.rglob('*.png'), key=lambda p: p.stat().st_mtime):
        if not state['pending']:
            return
        name = state['pending'].pop(0)
        try:
            path.replace(OUT / (name + '.png'))
        except OSError:
            state['pending'].insert(0, name)
            return
        rects = Path(u.Paths.project_saved_dir()) / 'UiRects.json'  # written by SFUi rects just before the shot
        if name.startswith('menu-') and rects.exists():
            shutil.copyfile(rects, OUT / (name + '.json'))
        for shot, live in (('menu-high', 'settings-graphics-live'), ('menu-keep', 'settings-keep-revert-live')):
            if name == shot:
                shutil.copyfile(OUT / (name + '.png'), OUT / (live + '.png'))
                if (OUT / (name + '.json')).exists():
                    shutil.copyfile(OUT / (name + '.json'), OUT / (live + '.json'))


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
            if command.startswith('cvars:'):
                names = ['sg.ViewDistanceQuality', 'sg.ShadowQuality', 'sg.GlobalIlluminationQuality', 'sg.ReflectionQuality', 'sg.TextureQuality',
                         'sg.EffectsQuality', 'sg.FoliageQuality', 'sg.PostProcessQuality', 'sg.AntiAliasingQuality', 'r.Nanite.Tessellation',
                         'r.DefaultFeature.MotionBlur', 'r.Shadow.Virtual.Enable', 'grass.DensityScale']
                lines = ['%s=%s' % (n, u.SystemLibrary.get_console_variable_float_value(n)) for n in names]
                lines.append('r.ScreenPercentage=%s' % u.SystemLibrary.get_console_variable_float_value('r.ScreenPercentage'))
                (OUT / (command[6:] + '-cvars.txt')).write_text(chr(10).join(lines) + chr(10))
                continue
            if command == 'Shot showui':  # exact widget rects of this frame for tools/ui_pixel_diff.py
                u.SystemLibrary.execute_console_command(world, 'SFUi rects')
            u.SystemLibrary.execute_console_command(world, command)
            u.log('GFX_CAPTURE ' + command)
    except Exception as error:  # never leave a -game session running on a script error
        u.log_error('GFX_CAPTURE error %r' % (error,))
        u.unregister_slate_post_tick_callback(state['handle'])


state['handle'] = u.register_slate_post_tick_callback(on_tick)
