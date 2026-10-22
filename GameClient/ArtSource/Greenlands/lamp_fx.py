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
import re
import sys

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parent / 'VFX' / 'StreetLamp'
ZONE = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
DEST = '/Game/Frontier/VFX/StreetLamp'
PREFIX = 'FX_StreetLamp_'
LANTERN_MATS = '/Game/Frontier/Environment/Greenlands/Silverbrook/Materials'


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
    return cfg


def load_sockets():
    path = SOURCE / 'lamp-sockets.json'
    data = json.loads(path.read_text(encoding='utf-8'))
    assert data and all(k.startswith('SM_') and points for k, points in data.items())
    return data


def _stable_fraction(text):
    return int.from_bytes(hashlib.sha256(text.encode('utf-8')).digest()[:8], 'little') / 2**64


_GLASS = {}


def _lantern(slot):   # lantern and street-lamp glass only: a window's light is its emissive (WP6i review r4 fix 5)
    nm = str(slot.material_slot_name) + ' ' + (slot.material_interface.get_name() if slot.material_interface else '')
    return 'Glow' in nm and not re.search(r'LitGlass|Window', nm, re.I)


def glass_sockets(mesh, u):
    """Mesh-space cm centre of every lantern-glass chamber, read from the imported mesh itself: the connected pieces of its
    *Glow material slots, merged when closer than 60 cm. (The GLB-derived lamp-sockets.json points of building meshes
    were 2-20 m off their imported glass, so their lights shone on bare walls; street lamps matched to 0 cm.) A flat glow
    panel (< 5 cm thick, >= 60 cm across) gets its light 30 cm in front of it, outward from the mesh bounds centre; a lantern
    pane (every axis < 60 cm) keeps cards and light at its centre (WP6i r6: the old offset put flames beside the glass);
    on a building, a glow volume >= 30 cm in every axis (a lit room or doorway seen through glass) or one centred under
    1 m (a cellar grille) gets no lamp."""
    name = mesh.get_name()
    if name not in _GLASS:
        box = mesh.get_bounding_box(); centre = [(box.min.x + box.max.x) / 2, (box.min.y + box.max.y) / 2]
        glow = [i for i, slot in enumerate(mesh.static_materials) if _lantern(slot)]
        dm = u.DynamicMesh()
        dm, _ = u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
            mesh, dm, u.GeometryScriptCopyMeshFromAssetOptions(), u.GeometryScriptMeshReadLOD())
        parts = u.GeometryScript_MeshDecomposition.split_mesh_by_material_i_ds(dm, mesh_pool=u.DynamicMeshPool())
        meshes = next(x for x in parts if isinstance(x, u.Array) and len(x) and isinstance(x[0], u.DynamicMesh))
        ids = next(x for x in parts if isinstance(x, u.Array) and len(x) and isinstance(x[0], int))
        boxes = []
        for piece, mid in zip(meshes, ids):
            if mid not in glow:
                continue
            comps = u.GeometryScript_MeshDecomposition.split_mesh_by_components(piece, mesh_pool=u.DynamicMeshPool())
            for comp in next(x for x in comps if isinstance(x, u.Array)):
                bb = u.GeometryScript_MeshQueries.get_mesh_bounding_box(comp)
                boxes.append([bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z])
        merged = []
        for b in boxes:   # union boxes whose gap is under 60 cm into one chamber
            for m in merged:
                if all(max(m[i], b[i]) - min(m[i + 3], b[i + 3]) < 60 for i in range(3)):
                    m[:] = [min(m[i], b[i]) for i in range(3)] + [max(m[i + 3], b[i + 3]) for i in range(3)]
                    break
            else:
                merged.append(list(b))
        out = []
        for m in merged:
            c = [(m[i] + m[i + 3]) / 2 for i in range(3)]
            size = [m[i + 3] - m[i] for i in range(3)]
            u.log(f'LAMP_SOCKET {name} centre {[round(v) for v in c]} size {[round(v) for v in size]}')
            if not name.startswith('SM_SilverbrookStreetLamp') and (min(size) >= 30 or c[2] < 100 or size[2] < 5):
                # a glowing room or doorway volume behind glass, a cellar grille at the ground, or a horizontal glow strip
                # (SM_SilverbrookTownhouse1's first-floor sill: centre z 417, box 204 x 19 x 2 cm; WP6i r5 fix 3): not a lantern
                continue
            thin = min(range(2), key=lambda i: size[i])
            if size[thin] < 5 and max(size) >= 60:   # a flush glow panel (window-sized): light 30 cm in front of it, outward from the mesh bounds centre
                c[thin] += 30 if c[thin] >= centre[thin] else -30
            out.append(c)   # a lantern pane (every axis under 60 cm) keeps its cards and light at the glass centre (WP6i r6 fix 1)
        _GLASS[name] = out
    return _GLASS[name]


def _mesh_sockets(actor, sockets, u):
    found = []
    for component in actor.get_components_by_class(u.StaticMeshComponent):
        mesh = component.get_editor_property('static_mesh')
        if mesh and mesh.get_name() in sockets:
            # Component local transform covers per-building mesh offsets; static mesh actors are the common case.
            for xyz in glass_sockets(mesh, u):
                found.append(u.MathLibrary.transform_location(component.get_world_transform(), u.Vector(*xyz)))
    return found


def _component(actor, name, mesh, material, u):
    subobjects = u.get_engine_subsystem(u.SubobjectDataSubsystem)
    actor_handle = subobjects.k2_gather_subobject_data_for_instance(actor)[0]
    handle, failure = subobjects.add_new_subobject(u.AddNewSubobjectParams(
        parent_handle=actor_handle, new_class=u.HierarchicalInstancedStaticMeshComponent))
    assert u.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(handle), failure
    component = u.SubobjectDataBlueprintFunctionLibrary.get_associated_object(
        u.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    component.set_editor_property('cast_shadow', False)
    component.set_editor_property('instance_start_cull_distance', 0)
    component.set_editor_property('instance_end_cull_distance', 2500 if name == 'Fireflies' else 1800)
    return component


def _instance(component, place, rotation, scale, u):
    transform = u.Transform(location=place, rotation=rotation, scale=scale)
    component.add_instance_world_space(transform)


def _point_light(subsystem, position, cfg, material, label, owner, u):
    actor = subsystem.spawn_actor_from_class(u.PointLight, position)
    actor.set_actor_label(label)
    actor.set_folder_path('VFX/StreetLamp')
    light = actor.light_component
    light.set_editor_property('mobility', u.ComponentMobility.MOVABLE)
    light.set_editor_property('intensity_units', u.LightUnits.LUMENS)
    wall = not owner.get_actor_label().startswith('StreetLamp_')
    # WP6h review 3b / WP6i fixes: a wall lantern at full output clipped the stone beside it; 0.17 x (27 lm, a candle), 6 cm source.
    light.set_editor_property('intensity', cfg['light_intensity'] * (.17 if wall else 1.))
    if wall:
        light.set_editor_property('source_radius', 6.)
    light.set_editor_property('attenuation_radius', cfg['light_radius'] * 100)
    # UE Python positional Color arguments map to BGRA; name channels to avoid a cyan lamp.
    light.set_editor_property('light_color', u.Color(r=255, g=150, b=65, a=255))
    # Wall-mounted lanterns cast shadows so their light cannot pass through the wall they hang on (owner: light pools on
    # bare walls); free-standing street lamps stay shadowless (budget).
    light.set_editor_property('cast_shadows', wall)
    light.set_editor_property('light_function_material', material)
    light.set_editor_property('max_draw_distance', 2500.0)
    # Follows its lantern: dress_footings raises houses after placement.
    actor.attach_to_actor(owner, '', u.AttachmentRule.KEEP_WORLD, u.AttachmentRule.KEEP_WORLD, u.AttachmentRule.KEEP_WORLD, False)
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
            targets.extend((label, i, p, actor) for i, p in enumerate(positions))
        elif label.startswith('StreetLamp_') or 'Lantern' in label:   # building meshes may have no lantern glass at all
            unmatched.append(label)
    assert targets, 'No lamp glass found: run the Silverbrook builder/import first'
    assert not unmatched, f'Lamp actors lack authored sockets: {unmatched[:12]}'
    sys.path.insert(0, str(SOURCE))
    import build_materials
    importlib.reload(build_materials)
    mats = build_materials.build(cfg)
    # FBX glass was gated by the sun and the shared .9 metallic iron reflected the blue sky.
    # Restrict the correction to the four street-lamp mesh families.
    mel = u.MaterialEditingLibrary
    iron = u.load_asset('/Game/Frontier/VFX/StreetLamp/MI_LampIron')
    if not iron:
        source_iron = u.load_asset('/Game/Frontier/Environment/Greenlands/Silverbrook/Materials/MI_SB_ForgedIron')
        iron = u.AssetToolsHelpers.get_asset_tools().duplicate_asset('MI_LampIron', DEST, source_iron)
    assert iron
    mel.set_material_instance_scalar_parameter_value(iron, 'Metallic', .25)
    mel.set_material_instance_scalar_parameter_value(iron, 'Roughness', .78)
    assert u.EditorAssetLibrary.save_loaded_asset(iron)
    for actor in actors:
        if not actor.get_actor_label().startswith('StreetLamp_'):
            continue
        for mesh_component in actor.get_components_by_class(u.StaticMeshComponent):
            mesh = mesh_component.get_editor_property('static_mesh')
            if not mesh or not mesh.get_name().startswith('SM_SilverbrookStreetLamp'):
                continue
            # Nanite rejects translucent glass on a mixed-material mesh.
            mesh_component.set_editor_property('disallow_nanite', True)
            for slot in range(mesh_component.get_num_materials()):
                material = mesh_component.get_material(slot)
                if not material:
                    continue
                name = material.get_name()
                if name == 'MI_SB_ForgedIron' or name == 'MI_LampIron':
                    mesh_component.set_material(slot, iron)
                elif name.endswith('_Glow') or name == 'M_LampGlass':
                    mesh_component.set_material(slot, mats['M_LampGlass'])
    # WP6h review 3a: building lantern panes glow with the flame (they were dark tan while their light washed the wall),
    # and with lit = time-of-day they follow the same sun gate as the windows (M_SB_WindowGlow's Gate).
    lanterns = []
    for path in u.EditorAssetLibrary.list_assets(LANTERN_MATS, recursive=False, include_folder=False):
        mi = u.load_asset(path)
        if isinstance(mi, u.MaterialInstanceConstant) and mi.get_name() == 'MI_SB_LanternGlow':   # the kit lantern pane (gate_windows' ALWAYS_LIT list holds vendor signs, no lanterns)
            mel.set_material_instance_vector_parameter_value(mi, 'Emission', u.LinearColor(*cfg['flame_colour'], 1))
            mel.set_material_instance_scalar_parameter_value(mi, 'EmissionStrength', cfg['glass_emission_strength'])
            mel.set_material_instance_scalar_parameter_value(mi, 'Gate', 1. if cfg['lit'] == 'time-of-day' else 0.)
            mel.set_material_instance_scalar_parameter_value(mi, 'GlowScale', 1.)   # the window scale (gate_windows) is not for the pane
            assert u.EditorAssetLibrary.save_loaded_asset(mi); lanterns.append(mi.get_name())
    assert lanterns, 'no building lantern glow instances under ' + LANTERN_MATS
    print('LANTERN_PANES', lanterns, 'GlowScale', [mel.get_material_instance_scalar_parameter_value(u.load_asset(LANTERN_MATS + '/' + n), 'GlowScale') for n in lanterns])
    plane = u.load_asset('/Engine/BasicShapes/Plane')
    assert plane, 'Engine Plane mesh unavailable'
    # Build all assets and discover all targets before replacing prior owned actors.
    for actor in actors:
        if actor.get_actor_label().startswith(PREFIX):
            subsystem.destroy_actor(actor)
    host = subsystem.spawn_actor_from_class(u.Actor, u.Vector(0, 0, 0))
    host.set_actor_label(PREFIX + 'Cards')
    host.set_folder_path('VFX/StreetLamp')
    cards = {key: _component(host, key, plane, mats['M_Lamp' + key], u)
             for key in ('Flame', 'Halo', 'Smoke')}
    cards['Fireflies'] = _component(host, 'Fireflies', plane, mats['M_LampFirefly'], u)
    counts = {'sockets': 0, 'lights': 0, 'fireflies': 0}
    for label, index, pos, owner in targets:
        key = f'{label}:{index}'
        yaw = _stable_fraction(key + ':yaw') * 180
        vertical = u.Rotator(pitch=90, yaw=yaw, roll=0)
        high = u.Vector(pos.x, pos.y, pos.z + 45)
        _instance(cards['Flame'], pos, vertical, u.Vector(.10, .22, 1), u)
        _instance(cards['Flame'], pos, u.Rotator(pitch=90, yaw=yaw + 90, roll=0), u.Vector(.10, .22, 1), u)
        _instance(cards['Halo'], pos, vertical, u.Vector(.26, .26, 1), u)
        _instance(cards['Halo'], pos, u.Rotator(pitch=90, yaw=yaw + 90, roll=0), u.Vector(.26, .26, 1), u)
        _instance(cards['Smoke'], high, vertical, u.Vector(.16, .25, 1), u)
        _point_light(subsystem, pos, cfg, mats['M_LampLightFunction'],
                     PREFIX + f'Light_{label}_{index}', owner, u)
        counts['sockets'] += 1; counts['lights'] += 1
        if _stable_fraction(key + ':fireflies') < cfg['firefly_chance']:
            count = 1 + int(_stable_fraction(key + ':count') * cfg['firefly_count'])
            for i in range(count):
                angle = _stable_fraction(key + f':a{i}') * 2 * math.pi
                radius = 130 + 130 * _stable_fraction(key + f':r{i}')
                fly = u.Vector(pos.x + radius * math.cos(angle), pos.y + radius * math.sin(angle),
                               pos.z - 40 + 70 * _stable_fraction(key + f':z{i}'))
                _instance(cards['Fireflies'], fly, vertical, u.Vector(.018, .018, 1), u)
                _instance(cards['Fireflies'], fly, u.Rotator(pitch=90, yaw=yaw + 90, roll=0), u.Vector(.018, .018, 1), u)
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
