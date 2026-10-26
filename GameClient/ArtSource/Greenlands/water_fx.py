"""Silverbrook water look and bridge ends: the underwater post-process volume of each water box, and an idempotent patch for the open map.

build_silverbrook._water() calls underwater_volume() for every swim volume it places, so a rebuild keeps the haze. patch_open_map() brings the
already-built Greenlands_Silverbrook map to the same state without a rebuild (flowing material on the strips, still on basins and the sea,
one PostProcessVolume per swim volume):

    UnrealEditor-Cmd.exe <project> -run=pythonscript -script="GameClient/ArtSource/Greenlands/water_fx.py"   (or Run-InEditor.py)
"""
import unreal as u

NATURE_MATS = '/Game/Frontier/Environment/Greenlands/Nature/Materials'
ZONE = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'


def underwater_volume(actors, label, loc, scale, rot):
    """A bounded post-process volume on a water box (the spawned brush is a 2 m cube, so scale is half-extents in metres)."""
    pp = actors.spawn_actor_from_class(u.PostProcessVolume, loc, rot)
    pp.set_actor_scale3d(scale)
    pp.set_editor_property('unbound', False); pp.set_editor_property('priority', 5.0); pp.set_editor_property('blend_radius', 0.0)
    st = pp.get_editor_property('settings')  # built-in grading: a green-blue cast, less colour, darker, soft edges (no custom post-process material)
    for name, value in (('color_gain', u.Vector4(0.45, 0.8, 0.9, 1.0)), ('color_saturation', u.Vector4(0.75, 0.9, 1.0, 1.0)), ('color_gamma', u.Vector4(1.0, 1.0, 1.05, 1.0)),
                        ('vignette_intensity', 0.45), ('bloom_intensity', 0.4)):
        st.set_editor_property('override_' + name, True); st.set_editor_property(name, value)
    pp.set_editor_property('settings', st)
    pp.set_actor_label(f'{label}_Underwater'); pp.set_folder_path('Water/Underwater')
    return pp


def wet_fn(actors):
    """(x, y, z) cm -> True when the point is inside a water swim volume (under a river, brook, basin or sea surface). Dressing must not stand there."""
    import math
    boxes = []
    for a in actors.get_all_level_actors():
        if isinstance(a, u.PhysicsVolume) and a.get_editor_property('water_volume'):
            o, e = a.get_actor_bounds(False); s = a.get_actor_scale3d(); yaw = math.radians(a.get_actor_rotation().yaw)
            boxes.append((o.x, o.y, o.z + e.z, 100 * s.x, 100 * s.y, math.cos(yaw), math.sin(yaw)))
    def wet(x, y, z):
        for ox, oy, top, hx, hy, c, sn in boxes:
            dx, dy = x - ox, y - oy
            if z <= top + 5 and abs(dx * c + dy * sn) <= hx and abs(-dx * sn + dy * c) <= hy: return True
        return False
    return wet


EARTH_SLOT = 'SB_RiverAbutment_Grass'  # the abutment's flat earth fill between its wing walls (bright, untextured-looking grass at the deck end)


def approach_paving():
    """The river bridge's own paving: the fill at each deck end continues the deck, as the cobbled approach road does."""
    mi = u.load_asset('/Game/Frontier/Environment/Greenlands/Silverbrook/Materials/MI_SB_RiverBridge_Paving')
    assert mi, 'import the RiverBridge package first'
    return mi


def abutment_earth(actor, material):
    """Put the approach-road material on an abutment's earth fill. Returns True if the actor had that slot."""
    mesh = actor.static_mesh_component.static_mesh
    for i, slot in enumerate(mesh.static_materials if mesh else []):
        if str(slot.material_slot_name) == EARTH_SLOT:
            actor.static_mesh_component.set_material(i, material); return True
    return False


def patch_open_map():
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    river, pond = (u.load_asset(f'{NATURE_MATS}/{n}') for n in ('MI_GN_River', 'MI_GN_Pond'))
    assert river and pond, 'run NatureFamilies/import_ground.py first'
    swapped = made = 0
    for a in actors.get_all_level_actors():  # rebuild the volumes so a settings change reaches the saved map
        if isinstance(a, u.PostProcessVolume) and str(a.get_folder_path()) == 'Water/Underwater': actors.destroy_actor(a)
    have = set()
    for a in actors.get_all_level_actors():
        if isinstance(a, u.StaticMeshActor) and str(a.get_folder_path()) == 'Water':
            a.static_mesh_component.set_material(0, river if '_' in a.get_actor_label() else pond); swapped += 1  # strips are Name_ii_kk; basins and the sea have no suffix
        elif isinstance(a, u.PhysicsVolume) and str(a.get_folder_path()) == 'Water/Swim' and a.get_actor_label().replace('_Swim', '_Underwater') not in have:
            underwater_volume(actors, a.get_actor_label()[:-5], a.get_actor_location(), a.get_actor_scale3d(), a.get_actor_rotation()); made += 1
    wet = wet_fn(actors); drowned = [a for a in actors.get_all_level_actors() if isinstance(a, u.StaticMeshActor) and str(a.get_folder_path()) == 'Silverbrook/Dressing'
                                     and wet(a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z)]
    for a in drowned: actors.destroy_actor(a)  # props the street dressing left standing in the river bed
    ends = sum(abutment_earth(a, approach_paving()) for a in actors.get_all_level_actors() if isinstance(a, u.StaticMeshActor) and a.get_actor_label().startswith('Abutment_'))
    u.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    print('WATER_FX_PATCHED planes', swapped, 'underwater_volumes', made, 'abutment_earth', ends, 'props_removed_from_water', len(drowned))


if __name__ == '__main__':
    u.EditorLoadingAndSavingUtils.load_map(ZONE)
    patch_open_map()
