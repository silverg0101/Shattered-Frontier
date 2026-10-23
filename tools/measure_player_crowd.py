"""Timeline for the player MetaHuman frame-cost gate, run inside a -game client by tools/Measure-PlayerCrowd.ps1.

Each phase settles, then SFFrameSample logs CROWD_SAMPLE (frame/game/render/GPU ms); HighResShot keeps a picture of the phase.
Phases: no crowd, 30 bodies with strand hair, 30 with cards (r.HairStrands.Strands 0), 60 with cards, 60 with strands.
"""
import unreal

SAMPLE = 8  # seconds per sample
STEPS = [  # (seconds after start, console command)
    (8, 'r.VSync 0'), (8, 't.MaxFPS 0'), (8, 'r.HairStrands.Strands 1'),
    (14, f'SFFrameSample {SAMPLE} base'),
    (24, 'SFCrowdTest 30'), (36, f'SFFrameSample {SAMPLE} c30_strands'), (45, 'HighResShot 1920x1080'),
    (47, 'r.HairStrands.Strands 0'), (51, f'SFFrameSample {SAMPLE} c30_cards'),
    (61, 'SFCrowdTest 60'), (75, f'SFFrameSample {SAMPLE} c60_cards'), (84, 'HighResShot 1920x1080'),
    (86, 'r.HairStrands.Strands 1'), (90, f'SFFrameSample {SAMPLE} c60_strands'),
    (101, 'QUIT'),
]
state = {'t': 0.0, 'i': 0}


def tick(delta):
    # The game world with a possessed player (as tools/Capture-UiV5.py finds it); the clock starts once it exists.
    world = state.get('world') or next((w for w in unreal.ObjectIterator(unreal.World)
        if unreal.GameplayStatics.get_player_controller(w, 0)
        and unreal.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn()), None)
    if not world:
        return
    state['world'] = world
    state['t'] += delta
    while state['i'] < len(STEPS) and state['t'] >= STEPS[state['i']][0]:
        command = STEPS[state['i']][1]
        state['i'] += 1
        unreal.log(f'CROWD_STEP t={state["t"]:.1f} {command}')
        if command == 'QUIT':
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
            return
        unreal.SystemLibrary.execute_console_command(world, command)


handle = unreal.register_slate_post_tick_callback(tick)
