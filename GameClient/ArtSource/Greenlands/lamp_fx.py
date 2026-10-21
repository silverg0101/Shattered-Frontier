"""Silverbrook lamp dressing. In the open map: import lamp_fx; importlib.reload(lamp_fx); lamp_fx.apply().

Uses committed lamp-glass sockets derived from Silverbrook GLBs. Owns only FX_StreetLamp_* actors
in the map and /Game/Frontier/VFX/StreetLamp assets. Rerun replaces owned actors; source meshes
and other sessions' map scripts are untouched. Editor authoring only; no runtime Python tick.
"""
from pathlib import Path
import hashlib
import importlib
import json
import math
import sys

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parent / 'VFX' / 'StreetLamp'
ZONE = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
DEST = '/Game/Frontier/VFX/StreetLamp'
PREFIX = 'FX_StreetLamp_'


def load_config():
    raw = json.loads((HERE / 'lamp-fx.json').read_text(encoding='utf-8'))
    cfg = {}
    for key, spec in raw.items():
        if key.startswith('_'):
            continue
        value = spec['value']
        assert value is not None and spec['default'] is not None and spec['fallback'] is not None, key
        kind = spec['type']
        if kind == 'enum':
            assert isinstance(value, str) and value in spec['values'], key
        else:
            vals = value if kind == 'linear_rgb' else [value]
            assert len(vals) == 3 if kind == 'linear_rgb' else True, key
            for v in vals:
                assert type(v) in ((int,) if kind == 'integer' else (int, float)), key
                assert math.isfinite(v) and spec['min'] <= v <= spec['max'], key
        cfg[key] = value
    assert cfg['lit'] == 'always', 'time-of-day mode needs the GFX WP6 runtime signal; keep always until integrated'
    return cfg


def load_sockets():
    path = SOURCE / 'lamp-sockets.json'
    data = json.loads(path.read_text(encoding='utf-8'))
    assert data and all(k.startswith('SM_') and points for k, points in data.items())
    return data


def _stable_fraction(text):
    return int.from_bytes(hashlib.sha256(text.encode('utf-8')).digest()[:8], 'little') / 2**64


def _mesh_sockets(actor, sockets, u):
    found = []
    for component in actor.get_components_by_class(u.StaticMeshComponent):
        mesh = component.get_editor_property('static_mesh')
        if mesh and mesh.get_name() in sockets:
            # Component local transform covers per-building mesh offsets; static mesh actors are the common case.
            for xyz in sockets[mesh.get_name()]:
                local = u.Vector(*(v * 100 for v in xyz))
                found.append(component.get_world_transform().transform_position(local))
    return found


def _component(actor, name, mesh, material, u):
    component = u.new_object(u.HierarchicalInstancedStaticMeshComponent, outer=actor, name=name)
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    component.set_editor_property('cast_shadow', False)
    component.set_editor_property('instance_start_cull_distance', 0)
    component.set_editor_property('instance_end_cull_distance', 2500 if name == 'Fireflies' else 1800)
    actor.add_instance_component(component)
    component.register_component()
    return component


def _instance(component, place, rotation, scale, u):
    transform = u.Transform(location=place, rotation=rotation, scale=scale)
    component.add_instance_world_space(transform)


def _point_light(subsystem, position, cfg, material, label, u):
    actor = subsystem.spawn_actor_from_class(u.PointLight, position)
    actor.set_actor_label(label)
    actor.set_folder_path('VFX/StreetLamp')
    light = actor.light_component
    light.set_editor_property('mobility', u.ComponentMobility.MOVABLE)
    light.set_editor_property('intensity_units', u.LightUnits.LUMENS)
    light.set_editor_property('intensity', cfg['light_intensity'])
    light.set_editor_property('attenuation_radius', cfg['light_radius'] * 100)
    light.set_editor_property('light_color', u.Color(255, 171, 87, 255))
    light.set_editor_property('cast_shadows', False)
    light.set_editor_property('light_function_material', material)
    light.set_editor_property('max_draw_distance', 2500.0)
    return actor


def apply(world=None, save=False):
    """Dress all recognized lamp-glass sockets in the open Silverbrook level.

    The builder hook calls apply() after spawning all pending lamp/building actors.
    """
    import unreal as u
    world = world or u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert world and ZONE in world.get_path_name(), f'Open {ZONE} first; got {world}'
    cfg = load_config()
    sockets = load_sockets()
    subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
    actors = list(subsystem.get_all_level_actors())
    targets = []
    unmatched = []
    for actor in actors:
        label = actor.get_actor_label()
        if label.startswith(PREFIX):
            continue
        positions = _mesh_sockets(actor, sockets, u)
        if positions:
            targets.extend((label, i, p) for i, p in enumerate(positions))
        elif label.startswith('StreetLamp_') or 'Lantern' in label:
            unmatched.append(label)
    assert targets, 'No lamp glass found: run the Silverbrook builder/import first'
    assert not unmatched, f'Lamp actors lack authored sockets: {unmatched[:12]}'
    sys.path.insert(0, str(SOURCE))
    import build_materials
    importlib.reload(build_materials)
    mats = build_materials.build(cfg)
    plane = u.load_asset('/Engine/BasicShapes/Plane')
    assert plane, 'Engine Plane mesh unavailable'
    # Build all assets and discover all targets before replacing prior owned actors.
    for actor in actors:
        if actor.get_actor_label().startswith(PREFIX):
            subsystem.destroy_actor(actor)
    host = subsystem.spawn_actor_from_class(u.Actor, u.Vector(0, 0, 0))
    host.set_actor_label(PREFIX + 'Cards')
    host.set_folder_path('VFX/StreetLamp')
    if not host.get_editor_property('root_component'):
        root = u.new_object(u.SceneComponent, outer=host, name='LampFxRoot')
        host.add_instance_component(root)
        host.set_root_component(root)
        root.register_component()
    cards = {key: _component(host, key, plane, mats['M_Lamp' + key], u)
             for key in ('Flame', 'Halo', 'Smoke', 'Ember', 'Firefly') if key != 'Firefly'}
    cards['Fireflies'] = _component(host, 'Fireflies', plane, mats['M_LampFirefly'], u)
    counts = {'sockets': 0, 'lights': 0, 'fireflies': 0}
    for label, index, pos in targets:
        key = f'{label}:{index}'
        yaw = _stable_fraction(key + ':yaw') * 180
        vertical = u.Rotator(pitch=90, yaw=yaw, roll=0)
        high = u.Vector(pos.x, pos.y, pos.z + 20)
        _instance(cards['Flame'], pos, vertical, u.Vector(.12, .24, 1), u)
        _instance(cards['Halo'], pos, vertical, u.Vector(.48, .48, 1), u)
        _instance(cards['Halo'], pos, u.Rotator(pitch=90, yaw=yaw + 90, roll=0), u.Vector(.48, .48, 1), u)
        _instance(cards['Smoke'], high, vertical, u.Vector(.22, .55, 1), u)
        _instance(cards['Ember'], high, vertical, u.Vector(.035, .035, 1), u)
        _point_light(subsystem, pos, cfg, mats['M_LampLightFunction'],
                     PREFIX + f'Light_{label}_{index}', u)
        counts['sockets'] += 1; counts['lights'] += 1
        if _stable_fraction(key + ':fireflies') < cfg['firefly_chance']:
            count = 1 + int(_stable_fraction(key + ':count') * cfg['firefly_count'])
            for i in range(count):
                angle = _stable_fraction(key + f':a{i}') * 2 * math.pi
                radius = 40 + 70 * _stable_fraction(key + f':r{i}')
                fly = u.Vector(pos.x + radius * math.cos(angle), pos.y + radius * math.sin(angle),
                               pos.z - 40 + 70 * _stable_fraction(key + f':z{i}'))
                _instance(cards['Fireflies'], fly, vertical, u.Vector(.055, .055, 1), u)
                counts['fireflies'] += 1
    if save:
        assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    print('LAMP_FX', json.dumps(counts, sort_keys=True))
    return counts


if __name__ == '__main__':
    import unreal as u
    level = u.get_editor_subsystem(u.LevelEditorSubsystem)
    if ZONE not in u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name():
        level.load_level(ZONE)
    apply(save='--save' in sys.argv)
