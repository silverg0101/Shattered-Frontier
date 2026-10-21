"""GFX-01 WP2: sky, sun, clouds, height fog, post-process volume and the colour-grade LUT for Silverbrook.

Standalone (open editor):  python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/lighting.py [--map <path>]
From build_silverbrook.py: `import lighting; importlib.reload(lighting); lighting.apply()` (configures the open level, no save).
Values come from silverbrook-lighting.json ({value, min, max, unit} each; out-of-bounds values abort). Idempotent: reuses existing
sun / sky / fog actors, owns the actors labelled below. Writes /Game/Frontier/Core/PostProcess/ only; the 32^3 LUT PNG is generated
here with the standard library (neutral cube + a slight split-tone grade) and committed beside this script.
"""
import json, struct, sys, zlib
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parent
ZONE = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
CLOUD_MATERIAL = '/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst'
LABEL_CLOUD, LABEL_PP = 'Lighting_VolumetricCloud', 'Lighting_PostProcess'


def load_config():
    """Flatten silverbrook-lighting.json to {'sun.pitch_deg': value}, rejecting values outside their bounds."""
    raw = json.loads((ROOT / 'silverbrook-lighting.json').read_text(encoding='utf-8'))
    cfg = {'lut': raw['lut']}
    for group, entries in raw.items():
        if group.startswith('_') or group == 'lut':
            continue
        for key, e in entries.items():
            vals = e['value'] if isinstance(e['value'], list) else [e['value']]
            assert all(e['min'] <= v <= e['max'] for v in vals), f"{group}.{key}={e['value']} outside [{e['min']}, {e['max']}] {e['unit']}"
            cfg[f'{group}.{key}'] = e['value']
    assert cfg['post.exposure_min_ev100'] < cfg['post.exposure_max_ev100'], 'exposure range is empty'
    return cfg


def _png(path, w, h, rgb):
    raw = b''.join(b'\x00' + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def write_lut_png(cfg, path):
    """32 tiles of 32x32 across (1024x32): R along x within a tile, G down y, B across tiles - the layout UE reads."""
    n, sh, hi, lift = cfg['lut']['size'], cfg['lut_grade.shadow_tint_rgb'], cfg['lut_grade.highlight_tint_rgb'], cfg['lut_grade.toe_lift']
    px = bytearray()
    for g in range(n):
        for b in range(n):
            for r in range(n):
                c = (r / (n - 1), g / (n - 1), b / (n - 1))
                t = (0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]) ** 1.5  # 0 in shadow .. 1 in highlight
                px += bytes(round(255 * min(1.0, max(0.0, (v * (sh[k] + (hi[k] - sh[k]) * t)) + lift * (1 - v)))) for k, v in enumerate(c))
    _png(path, n * n, n, px)


def import_lut(cfg):
    png = ROOT / cfg['lut']['source_png']
    if not png.exists():
        write_lut_png(cfg, png)
    dest = cfg['lut']['asset']
    task = u.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = str(png), dest.rsplit('/', 1)[0], dest.rsplit('/', 1)[1]
    task.automated, task.save, task.replace_existing = True, True, True
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = u.load_asset(dest)
    assert tex, f'LUT import failed: {dest}'
    tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_COLOR_LOOKUP_TABLE)
    tex.set_editor_property('mip_gen_settings', u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('address_x', u.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', u.TextureAddress.TA_CLAMP)
    u.EditorAssetLibrary.save_loaded_asset(tex)
    return tex


def _actors(cls):
    return [a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if isinstance(a, cls)]


def _one(cls, label=None, at=(0, 0, 0)):
    """The level's actor of `cls` (labelled `label` when given), spawned if missing."""
    found = [a for a in _actors(cls) if label is None or a.get_actor_label() == label]
    if found:
        return found[0]
    a = u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(cls, u.Vector(*at))
    if label:
        a.set_actor_label(label)
    a.set_folder_path('Lighting')
    return a


def _override(ps, **fields):
    for k, v in fields.items():
        ps.set_editor_property('override_' + k, True)
        ps.set_editor_property(k, v)


def apply(save=False):
    cfg = load_config()
    lut = import_lut(cfg)
    sun = _one(u.DirectionalLight, at=(0, 0, 50000))
    lc = sun.light_component
    for k, v in dict(atmosphere_sun_light=True, intensity=cfg['sun.intensity_lux'], use_temperature=True, temperature=cfg['sun.temperature_k'],
                     light_source_angle=cfg['sun.source_angle_deg']).items():
        lc.set_editor_property(k, v)
    sun.set_actor_rotation(u.Rotator(roll=0, pitch=cfg['sun.pitch_deg'], yaw=cfg['sun.yaw_deg']), False)

    sk = _one(u.SkyLight, at=(0, 0, 1000)).light_component
    sk.set_editor_property('real_time_capture', True)
    sk.set_editor_property('intensity', cfg['sun.sky_light_intensity'])

    atm = _one(u.SkyAtmosphere).get_component_by_class(u.SkyAtmosphereComponent)
    atm.set_editor_property('aerial_pespective_view_distance_scale', cfg['aerial_perspective.distance_scale'])
    atm.set_editor_property('aerial_perspective_start_depth', cfg['aerial_perspective.start_depth_m'] / 1000.0)  # km

    fog = _one(u.ExponentialHeightFog).component
    for k, v in dict(fog_density=cfg['fog.density'], fog_height_falloff=cfg['fog.height_falloff'], start_distance=cfg['fog.start_distance_m'] * 100,
                     fog_max_opacity=cfg['fog.max_opacity'], enable_volumetric_fog=bool(cfg['fog.volumetric'])).items():
        fog.set_editor_property(k, v)

    cloud = _one(u.VolumetricCloud, LABEL_CLOUD).get_component_by_class(u.VolumetricCloudComponent)
    cloud.set_editor_property('layer_bottom_altitude', cfg['clouds.layer_bottom_km'])
    cloud.set_editor_property('layer_height', cfg['clouds.layer_height_km'])
    cloud.set_editor_property('tracing_max_distance', cfg['clouds.tracing_max_distance_km'])
    mat = u.load_asset(CLOUD_MATERIAL)
    assert mat, f'Engine cloud material missing: {CLOUD_MATERIAL}'
    cloud.set_editor_property('material', mat)  # the engine's own instance gives soft cumulus; overriding its parent parameters removed the clouds

    pp = _one(u.PostProcessVolume, LABEL_PP)
    pp.set_editor_property('unbound', True)
    pp.set_editor_property('priority', 0.0)
    ps = pp.get_editor_property('settings')
    s = lambda v: u.Vector4(v, v, v, 1)
    _override(ps, auto_exposure_method=u.AutoExposureMethod.AEM_HISTOGRAM, auto_exposure_min_brightness=cfg['post.exposure_min_ev100'],
              auto_exposure_max_brightness=cfg['post.exposure_max_ev100'], auto_exposure_bias=cfg['post.exposure_bias_ev'],
              lumen_final_gather_quality=cfg['post.lumen_final_gather_quality'], lumen_scene_lighting_quality=cfg['post.lumen_scene_lighting_quality'],
              color_grading_lut=lut, color_grading_intensity=cfg['post.lut_intensity'],
              color_saturation=s(cfg['post.grade_saturation']), color_contrast=s(cfg['post.grade_contrast']))
    pp.set_editor_property('settings', ps)

    ws = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_world_settings()
    ws.set_editor_property('force_no_precomputed_lighting', True)  # the importers set no lightmap UVs; Lumen lights everything
    if save:
        assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level(), 'Could not save the level'
    u.log('LIGHTING_APPLIED ' + json.dumps({k: v for k, v in cfg.items() if k != 'lut'}))


if __name__ == '__main__':
    argv = sys.argv[1:]
    zone = argv[argv.index('--map') + 1] if '--map' in argv else ZONE
    levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
    if levels.get_current_level().get_outermost().get_name() != zone:
        levels.load_level(zone)
    apply(save=True)
