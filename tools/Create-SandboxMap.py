"""Create one small, empty mechanic test map in the Unreal Editor.

    python tools/Run-InEditor.py tools/Create-SandboxMap.py <Name> [--force]
    UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="tools/Create-SandboxMap.py <Name> [--force]"

Only /Game/Frontier/Maps/Sandbox/SB_<Name> is written. --force replaces that map.
"""

import re
import sys

import unreal as u


def main():
    args = sys.argv[1:]
    force = '--force' in args
    names = [arg for arg in args if arg != '--force']
    if len(names) != 1 or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', names[0]):
        raise ValueError('Usage: Create-SandboxMap.py <Name> [--force]; Name must be letters, digits or underscores')

    path = '/Game/Frontier/Maps/Sandbox/SB_' + names[0]
    assets = u.EditorAssetLibrary
    levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    game_mode = u.load_class(None, '/Script/ShatteredFrontier.SFGameMode')
    cube = u.load_asset('/Engine/BasicShapes/Cube')
    if not game_mode or not cube:
        raise RuntimeError('Required SFGameMode or Engine cube asset is unavailable')
    replacing = assets.does_asset_exist(path)
    if replacing:
        if not force:
            raise RuntimeError('Map already exists; pass --force to replace only ' + path)
        # Build a fresh transient level; NewLevel rejects an existing destination even
        # just after DeleteAsset because its asset registry entry remains until reload.
        u.EditorLoadingAndSavingUtils.new_blank_map(False)
    elif not levels.new_level(path):
        raise RuntimeError('Could not create ' + path)

    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    settings = world.get_world_settings()
    settings.set_editor_property('default_game_mode', game_mode)
    settings.set_editor_property('force_no_precomputed_lighting', True)

    # The Engine cube is 100 cm across: 100 * 100 cm = a 100 m square.
    floor = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(0, 0, -50))
    floor.set_actor_label('Ground')
    floor.static_mesh_component.set_static_mesh(cube)
    floor.static_mesh_component.set_collision_profile_name('BlockAll')
    floor_size = 260 if names[0] == 'RestRegen' else (180 if names[0] in ('Evade', 'AttackerMemory', 'Regeneration') else 100)
    floor.set_actor_scale3d(u.Vector(floor_size, floor_size, 1))
    start_height = 2000 if names[0] == 'FallDamage' else 110
    actors.spawn_actor_from_class(u.PlayerStart, u.Vector(0, 0, start_height)).set_actor_label('PlayerStart')
    if names[0] == 'Unstuck':
        fallback = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(1200, 0, 110))
        fallback.set_actor_label('UnstuckRespawnPoint')
        fallback.static_mesh_component.set_static_mesh(cube)
        fallback.static_mesh_component.set_collision_profile_name('NoCollision')
        fallback.set_actor_scale3d(u.Vector(0.2, 0.2, 0.2))
        fallback.set_editor_property('tags', [u.Name('SF_RespawnPoint')])
    if names[0] == 'FallDamage':
        platform = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(0, 0, 1800))
        platform.set_actor_label('FallPlatform')
        platform.static_mesh_component.set_static_mesh(cube)
        platform.static_mesh_component.set_collision_profile_name('BlockAll')
        platform.set_actor_scale3d(u.Vector(3, 3, 1))
    if names[0] == 'Swimming':
        # Default PhysicsVolume brush is 200 cm cubed. Its upper face is 3 m
        # above the floor so the player crosses the 1.3 m depth threshold.
        water = actors.spawn_actor_from_class(u.PhysicsVolume, u.Vector(2300, 0, 150))
        water.set_actor_label('DeepWater')
        water.set_actor_scale3d(u.Vector(10, 5, 1.5))
        water.set_editor_property('water_volume', True)
    if names[0] == 'RoadSpeed':
        road_class = u.load_class(None, '/Script/ShatteredFrontier.SFRoadSpeedVolume')
        if not road_class:
            raise RuntimeError('SFRoadSpeedVolume is unavailable; build the Editor module first')
        road = actors.spawn_actor_from_class(road_class, u.Vector(2000, 0, 150))
        road.set_actor_label('RoadTravelVolume')
        road.set_actor_scale3d(u.Vector(10, 15, 3))
    if names[0] == 'DeepSnow':
        snow_class = u.load_class(None, '/Script/ShatteredFrontier.SFDeepSnowVolume')
        if not snow_class:
            raise RuntimeError('SFDeepSnowVolume is unavailable; build the Editor module first')
        snow = actors.spawn_actor_from_class(snow_class, u.Vector(2000, 0, 150))
        snow.set_actor_label('DeepSnowTravelVolume')
        snow.set_actor_scale3d(u.Vector(10, 15, 3))
    if names[0] in ('Casting', 'Interrupt'):
        target_class = u.load_class(None, '/Script/ShatteredFrontier.SFCastingTarget')
        if not target_class:
            raise RuntimeError('SFCastingTarget class is unavailable; build the Editor module first')
        actors.spawn_actor_from_class(target_class, u.Vector(350, 0, 100)).set_actor_label('TrainingTarget')
    if names[0] == 'RestAreas':
        rest_class = u.load_class(None, '/Script/ShatteredFrontier.SFRestAreaVolume')
        if not rest_class:
            raise RuntimeError('SFRestAreaVolume is unavailable; build the Editor module first')
        inn = actors.spawn_actor_from_class(rest_class, u.Vector(-1200, 0, 150))
        inn.set_actor_label('InnRestArea')
        inn.set_editor_property('kind', u.SFRestAreaKind.INN)
        # The first authored Housing plot is centred at (430, 215) m. The game mode
        # loads its claim and structures through the same services as Silverbrook.
        plot = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(43000, 21500, -50))
        plot.set_actor_label('HousingClaimPlotGround')
        plot.static_mesh_component.set_static_mesh(cube)
        plot.static_mesh_component.set_collision_profile_name('BlockAll')
        plot.set_actor_scale3d(u.Vector(20, 20, 1))

    sun = actors.spawn_actor_from_class(u.DirectionalLight, u.Vector(0, 0, 1000), u.Rotator(pitch=-45, yaw=25))
    sun.set_actor_label('Sun')
    sun.light_component.set_editor_property('atmosphere_sun_light', True)
    actors.spawn_actor_from_class(u.SkyAtmosphere, u.Vector(0, 0, 0)).set_actor_label('SkyAtmosphere')
    sky = actors.spawn_actor_from_class(u.SkyLight, u.Vector(0, 0, 800))
    sky.set_actor_label('SkyLight')
    sky.root_component.set_mobility(u.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property('real_time_capture', True)
    actors.spawn_actor_from_class(u.ExponentialHeightFog, u.Vector(0, 0, 0)).set_actor_label('HeightFog')

    nav = actors.spawn_actor_from_class(u.NavMeshBoundsVolume, u.Vector(0, 0, 500))
    nav.set_actor_label('NavMeshBounds')
    nav.set_actor_scale3d(u.Vector(50, 50, 6))  # default 200 cm brush -> 100 m square
    if names[0] == 'Companions':
        boar = u.load_class(None, '/Script/ShatteredFrontier.SFPilotBoar')
        rest_area = u.load_class(None, '/Script/ShatteredFrontier.SFRestAreaVolume')
        interaction = u.load_class(None, '/Script/ShatteredFrontier.SFE1InteractionTarget')
        if not boar or not rest_area or not interaction:
            raise RuntimeError('Companion sandbox requires boar, rest area and interaction classes')
        actors.spawn_actor_from_class(interaction, u.Vector(-300, -750, 60)).set_actor_label('NetworkProbe')
        actors.spawn_actor_from_class(rest_area, u.Vector(-1200, 0, 100)).set_actor_label('TrainingRestArea')
        for index, xy in enumerate(((2500, 0), (2800, 350), (2800, -350))):
            creature = actors.spawn_actor_from_class(boar, u.Vector(xy[0], xy[1], 115))
            creature.set_actor_label('SandboxBoar_' + str(index + 1))
            creature.set_editor_property('tags', [u.Name('SF_CompanionSandboxWildlife')])
        for label, x, y in (('TrainingRouteStart', -1400, -400),
                            ('TrainingRouteEnd', -700, -400), ('TrainingHoldStation', -700, 400)):
            marker = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(x, y, 50))
            marker.set_actor_label(label)
            marker.static_mesh_component.set_static_mesh(cube)
            marker.static_mesh_component.set_collision_profile_name('NoCollision')
            marker.set_actor_scale3d(u.Vector(0.5, 0.5, 1))
    if names[0] in ('Evade', 'Threat', 'Taunt', 'Aggro', 'AttackerMemory', 'Regeneration', 'RestRegen', 'Assist', 'Respawn', 'AutoAttack', 'Dodge', 'SpiritRelease', 'ResurrectionAccept'):
        boar = u.load_class(None, '/Script/ShatteredFrontier.SFPilotBoar')
        interaction = u.load_class(None, '/Script/ShatteredFrontier.SFE1InteractionTarget')
        if not boar or not interaction:
            raise RuntimeError('Combat sandbox requires boar and network probe classes')
        actors.spawn_actor_from_class(interaction, u.Vector(-300, -750, 60)).set_actor_label('NetworkProbe')
        creature = actors.spawn_actor_from_class(boar, u.Vector(250 if names[0] in ('AutoAttack', 'Dodge') else 1800, 0, 115),
                                                 u.Rotator(yaw=180) if names[0] in ('Aggro', 'Dodge') else u.Rotator())
        creature.set_actor_label(names[0] + 'Boar')
        creature.set_editor_property('tags', [u.Name('SF_' + names[0] + 'SandboxWildlife')])
        if names[0] == 'SpiritRelease':
            point = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(3500, 0, 115))
            point.set_actor_label('SpiritHealerRespawnPoint')
            point.static_mesh_component.set_static_mesh(cube)
            point.static_mesh_component.set_collision_profile_name('NoCollision')
            point.set_actor_scale3d(u.Vector(0.2, 0.2, 0.2))
            point.set_editor_property('tags', [u.Name('SF_RespawnPoint')])
        if names[0] == 'RestRegen':
            rest_class = u.load_class(None, '/Script/ShatteredFrontier.SFRestAreaVolume')
            if not rest_class:
                raise RuntimeError('Rest-area volume class is unavailable')
            rest = actors.spawn_actor_from_class(rest_class, u.Vector(11000, 0, 110))
            rest.set_actor_label('RestRegenVolume')
        if names[0] == 'Respawn':
            point = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(2600, 0, 115))
            point.set_actor_label('RespawnPoint_1')
            point.static_mesh_component.set_static_mesh(cube)
            point.static_mesh_component.set_collision_profile_name('NoCollision')
            point.set_actor_scale3d(u.Vector(0.2, 0.2, 0.2))
            point.set_editor_property('tags', [u.Name('SF_RespawnPoint')])
        if names[0] == 'Assist':
            companion = actors.spawn_actor_from_class(boar, u.Vector(1600, 250, 115))
            companion.set_actor_label('AssistDefensiveBoar')
            companion.set_editor_property('tags', [u.Name('SF_AssistSandboxWildlife'), u.Name('SF_AssistDefensive')])
        if names[0] == 'Aggro':
            wall = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(900, 0, 150))
            wall.set_actor_label('SightWall')
            wall.static_mesh_component.set_static_mesh(cube)
            wall.static_mesh_component.set_collision_profile_name('BlockAll')
            wall.set_actor_scale3d(u.Vector(1, 8, 3))
    saved = u.EditorLoadingAndSavingUtils.save_map(world, path) if replacing else levels.save_current_level()
    if not saved:
        raise RuntimeError('Could not save ' + path)
    u.log('SANDBOX_MAP_READY ' + path)


main()
