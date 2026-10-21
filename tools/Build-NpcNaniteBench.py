"""Build the skinned-Nanite vs LOD benchmark maps (Editor Python, headless; both maps are new, so the open editor holds nothing).

    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended -nosplash
        -script="<project>/tools/Build-NpcNaniteBench.py --name BeastfolkWarrior --version V2"

Writes /Game/Frontier/Tests/NpcBench_LOD (SK_<name>, engine LODs) and NpcBench_Nanite (SK_<name>_Nanite): a floor, sun,
sky light and 100 looping walk-clip copies in a 10x10 grid, plus PlayerStarts tagged Near (grid edge) and Far (~70 m back).
Run a map with ?game=/Script/Engine.GameModeBase?Portal=Near|Far (see docs/evidence/npc-nanite-bench.md).
"""
import argparse, random, shlex, sys
import unreal

GRID, SPACING = 10, 150.0


def args():
    p = argparse.ArgumentParser(); p.add_argument('--name', default='BeastfolkWarrior'); p.add_argument('--version', default='V2')
    argv = sys.argv[1:]
    if not argv:
        toks = shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)
        i = next(i for i, t in enumerate(toks) if 'Build-NpcNaniteBench.py' in t)
        argv = [t.strip('"') for t in toks[i + 1:] if not t.startswith('-') or t.startswith('--')]
    return p.parse_known_args(argv)[0]


def build(map_path, mesh, anim):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(map_path): unreal.EditorAssetLibrary.delete_asset(map_path)
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
    floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    floor.set_actor_scale3d(unreal.Vector(400, 400, 1)); floor.set_actor_label('Floor')
    actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(roll=0, pitch=-45, yaw=30)).set_actor_label('Sun')
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label('SkyAtmosphere')
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 800)); sky.set_actor_label('SkyLight')
    sky.light_component.set_editor_property('real_time_capture', True)
    rng = random.Random(7)
    half = (GRID - 1) * SPACING / 2
    for i in range(GRID * GRID):
        x, y = (i // GRID) * SPACING - half, (i % GRID) * SPACING - half
        a = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(x, y, 0), unreal.Rotator(roll=0, pitch=0, yaw=rng.uniform(0, 360)))
        a.set_actor_label(f'Npc_{i:03d}')
        c = a.skeletal_mesh_component
        c.set_skinned_asset_and_update(mesh)
        c.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        c.set_animation(anim)
        c.set_editor_property('animation_data', unreal.SingleAnimationPlayData(anim_to_play=anim, saved_looping=True, saved_playing=True,
                                                                              saved_position=rng.uniform(0, 1), saved_play_rate=1.0))
    # Mesh origin is at the feet (~1.85 m tall). Near: 6 m from the grid edge; Far: 70 m back. Both face +X, the grid.
    for tag, dist in (('Near', 600.0), ('Far', 7000.0)):
        ps = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-half - dist, 0, 120), unreal.Rotator(roll=0, pitch=0, yaw=0))
        ps.set_actor_label('PlayerStart_' + tag); ps.set_editor_property('player_start_tag', tag)
    if not unreal.EditorLoadingAndSavingUtils.save_map(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), map_path):
        raise RuntimeError('Could not save ' + map_path)
    unreal.log('NPC_BENCH_MAP ' + map_path)


def main():
    a = args()
    root = f'/Game/Frontier/Characters/NPC/{a.name}/{a.version}'
    anim = unreal.load_asset(f'{root}/Animations/A_{a.name}_ThirdPersonWalk')
    for suffix, sk in (('LOD', f'SK_{a.name}'), ('Nanite', f'SK_{a.name}_Nanite')):
        mesh = unreal.load_asset(f'{root}/Mesh/{sk}')
        if not mesh or not anim: raise RuntimeError('Missing mesh or walk clip under ' + root)
        build(f'/Game/Frontier/Tests/NpcBench_{suffix}', mesh, anim)


main()
