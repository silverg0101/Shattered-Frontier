"""Build VEG-GreenlandsShrubBroadleaf-01 with Blender 5.1.1.

Run from this directory:
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_shrub.py

Uses the same Poly Haven leaf pieces and bark material inputs as SaplingBroadleaf.
Each bush is a low, many-stem clump with an enclosing volume of leaf clusters.
"""
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import nature_kit as K
import rock_family
import veg_kit as G

G.BARK_TILE = 1.89
VARIANTS = [
    dict(seed=31, height=1.78, width=2.16, depth=1.98, stems=11, cluster_size=.235),
    dict(seed=47, height=1.43, width=1.76, depth=1.84, stems=9, cluster_size=.22),
    dict(seed=73, height=1.08, width=1.34, depth=1.44, stems=8, cluster_size=.19),
]

leaf_spec = dict(name='MI_ShrubBroadleaf_Leaf',
    base='Sources/tree_small_02/textures/tree_small_02_leaves_diff_2k.jpg',
    normal='Sources/tree_small_02/textures/tree_small_02_leaves_nor_gl_2k.jpg',
    arm='Sources/tree_small_02/textures/tree_small_02_leaves_arm_2k.jpg',
    alpha='Sources/tree_small_02/textures/tree_small_02_leaves_alpha_2k.png',
    tint=[1.35, 1.6, .7], sat=1.05, two_sided=True, alpha_mode='MASKED')
h = 'Sources/japanese_hackberry_bark/japanese_hackberry_bark'
bark_spec = dict(name='MI_ShrubBroadleaf_Bark', base=h+'_Diffuse.jpg',
    normal=h+'_nor_gl.jpg', rough=h+'_Rough.jpg', tint=[2.5, 2.5, 2.4], sat=.1,
    blend=dict(base=h+'_Diffuse.jpg', normal=h+'_nor_gl.jpg', rough=h+'_Rough.jpg',
               tint=[.12, .09, .06], sat=.6, amount=1.0, contrast=.2))


def make_wood(vc, rng):
    """Radial basal stems with spreading, downward-filling secondary shoots."""
    tubes = []
    for i in range(vc['stems']):
        a = 2*math.pi*(i+.25*rng.random())/vc['stems']
        length = rng.uniform(.66, .96)*vc['height']
        spread = rng.uniform(.30, .47)*min(vc['width'], vc['depth'])
        t = np.linspace(0, 1, 17)
        p = np.c_[.08*math.cos(a)+spread*math.cos(a)*t**1.5,
                  .08*math.sin(a)+spread*math.sin(a)*t**1.5,
                  .015+length*t]
        p[:, 0] += .025*np.sin(t*8+i)
        p[:, 1] += .025*np.cos(t*7+i)
        r = .014*(1-t)**.8+.003
        tubes.append((p, r, 6))
        for j in range(7):
            u = rng.uniform(.12, .86)
            origin = p[int(u*16)]
            phi = a+rng.uniform(-1.7, 1.7)
            reach = rng.uniform(.14, .34)*vc['width']
            zrise = rng.uniform(-.14, .22)*vc['height']
            q = np.array([origin,
                          origin+[reach*.43*math.cos(phi), reach*.43*math.sin(phi), zrise*.55],
                          origin+[reach*math.cos(phi), reach*math.sin(phi), zrise]], dtype=float)
            q[:, 2] = np.maximum(q[:, 2], .06)
            tubes.append((q, np.array([.006, .004, .0015]), 4))
    return tubes


def make_foliage(vc, lib, rng):
    """Dense outer and inner leaf pieces; no central crown or exposed trunk."""
    vertices, triangles, uvs = [], [], []
    offset = tri_count = 0
    while tri_count < 48000:
        z = rng.beta(1.35, 1.25)*vc['height']
        envelope = .26+.74*max(0, math.sin(math.pi*z/vc['height']))**.48
        angle = rng.uniform(0, 2*math.pi)
        radial = math.sqrt(rng.uniform(.13, 1.0))
        x = math.cos(angle)*radial*envelope*vc['width']*.43
        y = math.sin(angle)*radial*envelope*vc['depth']*.43
        lv, lt, uv = lib[rng.integers(len(lib))]
        span = np.ptp(lv, axis=0).max()
        size = vc['cluster_size']*rng.uniform(.72, 1.18)
        p = (lv @ G.rot(rng).T)*(size/max(span, 1e-6)) + [x, y, z]
        vertices.append(p); triangles.append(lt+offset); uvs.append(uv)
        offset += len(p); tri_count += len(lt)
    return (np.vstack(vertices).astype(np.float32), np.vstack(triangles),
            np.concatenate(uvs).astype(np.float32))


K.reset()
K.render_setup((2000, 1000), 40)
K.add_ground()
lib = G.leaf_library()
mats = [K.build_material(bark_spec), K.build_material(leaf_spec)]
objects, rows = [], []
for index, vc in enumerate(VARIANTS):
    rng = np.random.default_rng(vc['seed'])
    wood_v, wood_t, wood_uv, wood_r = [], [], [], []
    offset = 0
    for j, (p, radius, sides) in enumerate(make_wood(vc, rng)):
        v, t, uv, red = G.tube(p, radius, sides, mark_phase=vc['seed']+j*.17)
        wood_v.append(v); wood_t.append(t+offset); wood_uv.append(uv); wood_r.append(red)
        offset += len(v)
    wv, wt = np.vstack(wood_v), np.vstack(wood_t)
    fv, ft, fuv = make_foliage(vc, lib, rng)
    verts = np.vstack((wv, fv))
    tris = np.vstack((wt, ft+len(wv)))
    uv = np.concatenate((*wood_uv, fuv))
    materials = np.r_[np.zeros(len(wt)), np.ones(len(ft))].astype(np.int32)
    colour = np.zeros((len(verts), 4), np.float32)
    colour[:len(wv), 0] = np.concatenate(wood_r)
    colour[:, 3] = 1
    # Bring the leaf silhouette inside the declared footprint and height.
    lo, hi = verts.min(axis=0), verts.max(axis=0)
    center = (lo+hi)*.5
    verts[:, 0] = (verts[:, 0]-center[0])*vc['width']/(hi[0]-lo[0])
    verts[:, 1] = (verts[:, 1]-center[1])*vc['depth']/(hi[1]-lo[1])
    verts[:, 2] = (verts[:, 2]-lo[2])*vc['height']/(hi[2]-lo[2])
    name = f'SM_ShrubBroadleaf_{index:02d}'
    obj = K.make(name, verts, tris, uv, materials, mats, vcol=colour)
    # One low hull covers the woody heart; foliage is intentionally non-colliding.
    hull = K.frustum(f'UCX_{name}_00', 0, 0, .22, .34, vc['height']*.45)
    obj['ucx'] = [hull.name]
    objects.append(obj)
    rows.append(dict(mesh=name, seed=vc['seed'], stems=vc['stems'],
                     wood_triangles=int(len(wt)), leaf_triangles=int(len(ft)),
                     leaf_cluster_size_m=vc['cluster_size'],
                     materials=[bark_spec['name'], leaf_spec['name']]))

cfg = dict(family='ShrubBroadleaf', dir=str(HERE), prefix='SM_ShrubBroadleaf',
    asset_id='VEG-GreenlandsShrubBroadleaf-01', role='shrub_broadleaf', budget=60000,
    scale_note='At scale 1: rounded multi-stem shrubs 1.08-1.78 m tall, 1.34-2.16 m wide; foliage reaches base.',
    gap=.65, res=(2000, 1000), ortho=2.6, nanite=True, human_x=-.9, human_y=0,
    extra_sources=['tree_small_02', 'japanese_hackberry_bark'])
rock_family.finish(cfg, objects, rows, [bark_spec, leaf_spec],
                   ['tree_small_02', 'japanese_hackberry_bark'])
manifest_path = HERE/'asset-manifest.json'
manifest = json.loads(manifest_path.read_text())
manifest['generator'] = 'build_shrub.py'
assert all(n <= 60000 for n in manifest['triangles'])
assert all(1.0 <= row['size_m'][2] <= 1.8 and 1.2 <= row['size_m'][0] <= 2.2
           for row in manifest['variants'])
manifest_path.write_text(json.dumps(manifest, indent=2)+'\n')
