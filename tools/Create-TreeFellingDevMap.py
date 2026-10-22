"""Create the tree-felling review maps in the open editor; refuse overwrite.

TreeFelling_Level: flat ground. TreeFelling_Slope: 15 deg ground rising toward -X. Each has one oak gathering node at
(0,-350); the GM-01 dev-map spawn puts the player at (-200,-400), i.e. uphill on the slope, so the oak falls downhill.
Run: py -3 tools/Run-InEditor.py tools/Create-TreeFellingDevMap.py
Review: tools/Test-HarvestFellingReview.ps1 (close this editor's copy of the maps first).
"""
import sys
from pathlib import Path

import unreal as u

ROOT = '/Game/Frontier/Maps/Dev/TreeFelling/'
NODE, SPAWN = u.Vector(0, -350, 0), u.Vector(-200, -400, 0)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
ground = u.load_asset('/Game/Frontier/Environment/Greenlands/Nature/Materials/MI_GN_ForestFloor')
cube = u.load_asset('/Engine/BasicShapes/Cube')
node_class = u.load_class(None, '/Script/ShatteredFrontier.SFGatheringPilotNode')
assert ground and cube and node_class
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'GameClient/ArtSource/Greenlands'))
import lighting


def surface(slab, at):
    hit = slab.static_mesh_component.line_trace_component(u.Vector(at.x, at.y, 10000), u.Vector(at.x, at.y, -10000), True, False)
    assert hit and hit[0], at  # (location, normal, bone, hit result)
    return hit[0].z


for name, pitch in (('TreeFelling_Level', 0.0), ('TreeFelling_Slope', 15.0)):
    path = ROOT + name
    if u.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError(f'Refusing to overwrite {path}')
    assert levels.new_level(path), path
    slab = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(NODE.x, NODE.y, -50), u.Rotator(pitch=pitch))
    slab.set_actor_label('Ground')
    c = slab.static_mesh_component
    c.set_static_mesh(cube); c.set_material(0, ground); c.set_collision_profile_name('BlockAll')
    slab.set_actor_scale3d(u.Vector(400, 400, 1))  # 400 m: the edge stays inside the height fog
    if pitch and surface(slab, SPAWN) < surface(slab, NODE):
        slab.set_actor_rotation(u.Rotator(pitch=-pitch), False)
    # The player spawn must stand on the ground: put the surface under the spawn at z=0.
    slab.set_actor_location(slab.get_actor_location() - u.Vector(0, 0, surface(slab, SPAWN)), False, False)
    base = surface(slab, NODE)
    node = actors.spawn_actor_from_class(node_class, u.Vector(NODE.x, NODE.y, base + 70))
    node.set_actor_label('FellingOak')
    actors.spawn_actor_from_class(u.PlayerStart, u.Vector(SPAWN.x, SPAWN.y, 110), u.Rotator(yaw=0))
    lighting.apply()
    # Review sun from -X, behind the stump: the oak falls toward +X, so the crown's shadow falls away from the stump and butt.
    for sun in actors.get_all_level_actors():
        if isinstance(sun, u.DirectionalLight):
            sun.set_actor_rotation(u.Rotator(pitch=-38, yaw=20), False)
    assert levels.save_current_level(), path
    print('DEV_MAP_READY', path, 'tree_base_z', round(base, 2), 'spawn_ground_z', round(surface(slab, SPAWN), 2))
