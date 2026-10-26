"""Silverbrook lamp dressing. In the open map: import lamp_fx; importlib.reload(lamp_fx); lamp_fx.apply().

Uses committed lamp-glass sockets derived from Silverbrook GLBs (Y mirrored on import; confirmed against the mesh's lantern pane). Owns only FX_StreetLamp_* actors
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
_FRONT = {}   # mesh name -> Blender-space socket per entry
_OUTWARD = {}   # mesh name -> mesh-space unit outward direction per glass_sockets() entry (buildings only)


def _lantern(slot):   # lantern and street-lamp glass only: a window's light is its emissive (WP6i review r4 fix 5)
    nm = str(slot.material_slot_name) + ' ' + (slot.material_interface.get_name() if slot.material_interface else '')
    return 'Glow' in nm and not re.search(r'LitGlass|Window', nm, re.I)


LANTERN_AXES = (8., 35.)   # a kit lantern pane is a 15 x 15 x 24 cm box (silverbrook_kit.lantern; the cage variant 12 x 12 x 22)
PLATE_DROP = 30.   # cm below the pane centre: 15.75 cm under the 19 cm bottom plate (body - 0.1425 m), so no iron face is lit point-blank (WP6i r11)
PLATE_CLEAR = 12.   # cm outward of the pane centre along the overhang axis: 2.5 cm beyond the plate's edge (WP6i r11)


def glass_sockets(mesh, u, sockets=None):
    """Mesh-space cm light positions. Street lamps: the connected pieces of the *Glow slots merged within 60 cm (translucent
    M_LampGlass; GLB sockets matched to 0 cm). Buildings: the committed lamp-sockets.json point (Blender metres; UE's FBX
    import mirrors Y, so (x, -y, z) x 100): the light goes at the socket; a lantern-sized piece within 40 cm only confirms a lantern is there.
    The dynamic-mesh material IDs of the merged building meshes do not line up with static_materials (WP6i r8: the 'glow'
    ID returned Townhouse2/4's banner and Townhouse1's sill), so the slot scan is not used on buildings. The kit pane is an
    opaque emissive box and the light casts shadows, so the light sits PLATE_DROP cm below the pane centre and PLATE_CLEAR cm
    outward of it, below and outside the bottom plate."""
    name = mesh.get_name()
    if name in _GLASS:
        return _GLASS[name]
    street = name.startswith('SM_SilverbrookStreetLamp')
    glow = [i for i, slot in enumerate(mesh.static_materials) if _lantern(slot)]
    pool = u.DynamicMeshPool()   # one pool per mesh, freed below (WP6i r9: splitting every building slot ran the editor out of memory)
    dm = u.DynamicMesh()
    dm, _ = u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        mesh, dm, u.GeometryScriptCopyMeshFromAssetOptions(), u.GeometryScriptMeshReadLOD())

    def pieces(source):   # bounding boxes [min xyz, max xyz] of the connected pieces of a dynamic mesh
        comps = u.GeometryScript_MeshDecomposition.split_mesh_by_components(source, mesh_pool=pool)
        out_ = []
        for comp in next(x for x in comps if isinstance(x, u.Array)):
            bb = u.GeometryScript_MeshQueries.get_mesh_bounding_box(comp)
            out_.append([bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z])
        return out_

    boxes = []
    if street:
        parts = u.GeometryScript_MeshDecomposition.split_mesh_by_material_i_ds(dm, mesh_pool=pool)
        meshes = next(x for x in parts if isinstance(x, u.Array) and len(x) and isinstance(x[0], u.DynamicMesh))
        ids = next(x for x in parts if isinstance(x, u.Array) and len(x) and isinstance(x[0], int))
        for piece, mid in zip(meshes, ids):
            if mid in glow:
                boxes += pieces(piece)
    out = []
    outward = []
    front = []
    if street:
        merged = []
        for b in boxes:   # union boxes whose gap is under 60 cm into one chamber
            for m in merged:
                if all(max(m[i], b[i]) - min(m[i + 3], b[i + 3]) < 60 for i in range(3)):
                    m[:] = [min(m[i], b[i]) for i in range(3)] + [max(m[i + 3], b[i + 3]) for i in range(3)]
                    break
            else:
                merged.append(list(b))
        out = [[(m[i] + m[i + 3]) / 2 for i in range(3)] for m in merged]
    else:
        box = mesh.get_bounding_box()
        centre = [(box.min.x + box.max.x) / 2, (box.min.y + box.max.y) / 2]
        half = [max((box.max.x - box.min.x) / 2, 1), max((box.max.y - box.min.y) / 2, 1)]
        for p in (sockets or load_sockets())[name]:
            t = [p[0] * 100, -p[1] * 100, p[2] * 100]
            # only the triangles inside a 120 cm box round the socket, split into pieces: lantern-sized ones are the pane
            sel = u.GeometryScript_MeshSelection.select_mesh_elements_in_box(
                dm, u.Box(u.Vector(t[0] - 60, t[1] - 60, t[2] - 60), u.Vector(t[0] + 60, t[1] + 60, t[2] + 60)),
                u.GeometryScriptMeshSelectionType.TRIANGLES, False, 3)[1]
            sub = u.DynamicMesh(); u.GeometryScript_MeshDecomposition.copy_mesh_selection_to_mesh(dm, sub, sel)
            boxes = [b for b in pieces(sub) if all(LANTERN_AXES[0] <= b[i + 3] - b[i] <= LANTERN_AXES[1] for i in range(3))]
            near = sorted(((sum(((b[i] + b[i + 3]) / 2 - t[i]) ** 2 for i in range(3)) ** .5, b) for b in boxes), key=lambda n: n[0])
            d, b = near[0] if near else (1e9, None)
            u.log(f'LAMP_MATCH {name} json {[round(v) for v in t]} piece {b and [round((b[i] + b[i + 3]) / 2) for i in range(3)]} size {b and [round(b[i + 3] - b[i]) for i in range(3)]} dist {round(d)}')
            u.log(f'LAMP_PIECES {name} ' + ' '.join(f'{[round((q[i] + q[i + 3]) / 2 - t[i]) for i in range(3)]}:{[round(q[i + 3] - q[i]) for i in range(3)]}' for q in pieces(sub)))
            if d > 40:   # no kit lantern there (e.g. SM_SilverbrookWeaponsVendor's z 112 socket, nearest piece 208 cm): no light from nowhere
                u.log(f'LAMP_NOMATCH {name} json {[round(v) for v in t]}'); continue
            c = list(t)   # the socket is the pane centre (extract_sockets: the *LanternGlow island centre); WP6i r9 lit the cap by using the matched piece
            # WP6i r9-r11: the pane is an opaque emissive box and the light casts shadows. Level with the cap (r9) the cap's
            # outward slope glowed; outward of the pane (r10 try) the pane shadowed the wall; 1.75 cm under the bottom plate (r10)
            # the plate glowed and shadowed the wall above it. Below and just outside the plate every iron face is >= 17 cm away.
            a = max(range(2), key=lambda i: abs(c[i] - centre[i]) / half[i])   # the horizontal axis the lantern overhangs on
            sign = 1. if c[a] >= centre[a] else -1.
            c[a] += sign * PLATE_CLEAR
            c[2] -= PLATE_DROP
            out.append(c)
            outward.append((sign if a == 0 else 0., sign if a == 1 else 0., 0.))
            front.append(p)   # Blender-space socket, for door_index()
    pool.free_all_meshes()
    _OUTWARD[name] = outward
    _FRONT[name] = front
    _GLASS[name] = out
    return out


def door_index(points):
    """Index of the front-door lantern among a building's sockets (Blender metres, front = -Y): the one on the front face
    (lowest Y, within 0.4 m of it) nearest the building's centre line, where the door sits. None for no sockets."""
    if not points:
        return None
    front_y = min(p[1] for p in points)
    return min((i for i, p in enumerate(points) if p[1] <= front_y + .4), key=lambda i: abs(points[i][0]))


def _mesh_sockets(actor, sockets, u):
    """[(world position, world outward direction or None, is the front-door lantern)] for every socket of the actor."""
    found = []
    for component in actor.get_components_by_class(u.StaticMeshComponent):
        mesh = component.get_editor_property('static_mesh')
        if mesh and mesh.get_name() in sockets:
            # Component local transform covers per-building mesh offsets; static mesh actors are the common case.
            name = mesh.get_name()
            for i, xyz in enumerate(glass_sockets(mesh, u, sockets)):
                tf = component.get_world_transform()
                out = _OUTWARD.get(name)
                direction = u.MathLibrary.transform_direction(tf, u.Vector(*out[i])) if out else None
                door = bool(out) and i == door_index(_FRONT[name])
                found.append((u.MathLibrary.transform_location(tf, u.Vector(*xyz)), direction, door))
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


def _door_light(subsystem, position, direction, cfg, label, owner, u):
    """The front-door lantern lights its house front. A shadowless spot aimed out of the wall (its cone stays on the street
    side, so nothing leaks into the rooms behind the wall) and tilted down onto the step and the cobbles. Steady, ungated:
    the owner wants shaded fronts lit at any hour (2026-10-26)."""
    aim = u.Vector(direction.x, direction.y, 0)
    aim = aim / aim.length() if aim.length() else u.Vector(0, 1, 0)
    rotation = u.MathLibrary.make_rot_from_x(u.Vector(aim.x, aim.y, -math.tan(math.radians(cfg['door_light_tilt_deg']))))
    actor = subsystem.spawn_actor_from_class(u.SpotLight, position, rotation)
    actor.set_actor_label(label)
    actor.set_folder_path('VFX/StreetLamp')
    light = actor.get_component_by_class(u.SpotLightComponent)
    light.set_editor_property('mobility', u.ComponentMobility.MOVABLE)
    light.set_editor_property('intensity_units', u.LightUnits.CANDELAS)
    light.set_editor_property('intensity', cfg['door_light_intensity'])
    light.set_editor_property('attenuation_radius', cfg['door_light_radius'] * 100)
    light.set_editor_property('source_radius', 6.)
    light.set_editor_property('inner_cone_angle', cfg['door_light_inner_cone_deg'])
    light.set_editor_property('outer_cone_angle', cfg['door_light_outer_cone_deg'])
    light.set_editor_property('light_color', u.Color(r=255, g=150, b=65, a=255))
    light.set_editor_property('cast_shadows', False)
    light.set_editor_property('max_draw_distance', cfg['door_light_draw_m'] * 100)
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
            targets.extend((label, i, p[0], actor, p[1], p[2]) for i, p in enumerate(positions))
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
            mel.set_material_instance_scalar_parameter_value(mi, 'Gate', 1. if cfg['lit'] == 'time-of-day' and cfg['door_lantern_lit'] == 'time-of-day' else 0.)
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
    counts = {'sockets': 0, 'lights': 0, 'fireflies': 0, 'door_lights': 0}
    doors = 0
    for label, index, pos, owner, direction, door in targets:
        key = f'{label}:{index}'
        yaw = _stable_fraction(key + ':yaw') * 180
        vertical = u.Rotator(pitch=90, yaw=yaw, roll=0)
        high = u.Vector(pos.x, pos.y, pos.z + 45)
        if label.startswith('StreetLamp_'):   # a building lantern's opaque emissive pane is the flame (WP6i r8 fix 1)
            _instance(cards['Flame'], pos, vertical, u.Vector(.10, .22, 1), u)
            _instance(cards['Flame'], pos, u.Rotator(pitch=90, yaw=yaw + 90, roll=0), u.Vector(.10, .22, 1), u)
            _instance(cards['Halo'], pos, vertical, u.Vector(.26, .26, 1), u)
            _instance(cards['Halo'], pos, u.Rotator(pitch=90, yaw=yaw + 90, roll=0), u.Vector(.26, .26, 1), u)
            _instance(cards['Smoke'], high, vertical, u.Vector(.16, .25, 1), u)
        if door and direction is not None and doors < cfg['door_light_max_count']:
            _door_light(subsystem, pos, direction, cfg, PREFIX + f'DoorLight_{label}_{index}', owner, u)
            doors += 1
        else:
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
    counts['door_lights'] = doors
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
