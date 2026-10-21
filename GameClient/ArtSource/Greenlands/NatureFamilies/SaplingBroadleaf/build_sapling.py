"""Build three leafy single-trunk saplings with Blender 5.1.1 -b -P build_sapling.py."""
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
    dict(seed=5, height=2.76, width=1.56, lean=(.11,.03), branches=11),
    dict(seed=8, height=2.19, width=1.32, lean=(-.09,.07), branches=10),
    dict(seed=12, height=1.61, width=1.02, lean=(.05,-.08), branches=9),
]
leaf_path = 'Sources/tree_small_02/textures/tree_small_02_leaves_'
leaf_spec = dict(name='MI_SaplingBroadleaf_Leaf', base=leaf_path+'diff_2k.jpg',
    normal=leaf_path+'nor_gl_2k.jpg', arm=leaf_path+'arm_2k.jpg',
    alpha=leaf_path+'alpha_2k.png', tint=[1.19,1.39,.79], sat=.91,
    two_sided=True, alpha_mode='MASKED',
    blend=dict(base=leaf_path+'diff_2k.jpg', normal=leaf_path+'nor_gl_2k.jpg',
               arm=leaf_path+'arm_2k.jpg', tint=[.52,.66,.41], sat=.70,
               amount=.67, contrast=.43))
bark_path = 'Sources/japanese_hackberry_bark/japanese_hackberry_bark'
bark_spec = dict(name='MI_SaplingBroadleaf_Bark', base=bark_path+'_Diffuse.jpg',
    normal=bark_path+'_nor_gl.jpg', rough=bark_path+'_Rough.jpg',
    tint=[.63,.56,.44], sat=.70,
    blend=dict(base=bark_path+'_Diffuse.jpg', normal=bark_path+'_nor_gl.jpg',
               rough=bark_path+'_Rough.jpg', tint=[.17,.12,.08], sat=.72,
               amount=1.0, contrast=.2))

def make_wood(vc, rng):
    """A slender central stem and short ascending limbs ending inside foliage."""
    height, width = vc['height'], vc['width']
    t = np.linspace(0, 1, 27)
    stem = np.c_[vc['lean'][0]*t**1.7 + .019*np.sin(t*5)*t,
                 vc['lean'][1]*t**1.7 + .016*np.sin(t*4)*t, height*t*.96]
    tubes = [(stem, .024*(1-t)**.85+.0035, 8)]
    anchors = []
    for j in range(vc['branches']):
        f = .33 + .58*(j+.45)/vc['branches']
        idx = min(25, int(f*26))
        start = stem[idx]
        angle = 2*math.pi*(j*.618034 + .10*rng.random())
        fullness = max(.32, math.sin(math.pi*(f-.19)/.88))
        reach = width*(.28+.12*rng.random())*fullness
        rise = height*(.14+.075*rng.random())
        u = np.linspace(0,1,10)
        limb = start[None,:] + np.c_[reach*np.cos(angle)*u**1.1,
            reach*np.sin(angle)*u**1.1, rise*u + .017*height*np.sin(math.pi*u)]
        tubes.append((limb, .0085*(1-u)**.8+.0016, 5))
        for k in range(3,10):
            anchors.extend([limb[k]] * (2 if k < 7 else 4))
    anchors.extend([stem[k] for k in range(19,27) for _ in range(4)])
    return tubes, np.asarray(anchors)

def make_foliage(vc, lib, rng, anchors):
    """Overlapping leaf pieces form attached clusters with a shaded interior."""
    vertices, tris, uvs, shades = [], [], [], []
    offset = triangle_count = 0
    height = vc['height']
    while triangle_count < 51000:
        anchor = anchors[rng.integers(len(anchors))]
        pos = anchor + rng.normal(size=3)*[.08,.08,.05]
        pos[2] = np.clip(pos[2], .38*height, .985*height)
        lv, lt, luv = lib[rng.integers(len(lib))]
        size = rng.uniform(.075,.110)*height
        piece = (lv @ G.rot(rng).T) * (size/max(np.ptp(lv,axis=0).max(),1e-6)) + pos
        vertices.append(piece); tris.append(lt+offset); uvs.append(luv)
        radial = np.linalg.norm(pos[:2]-np.asarray(vc['lean'])*pos[2]/height)
        interior = np.clip(.68-radial/(vc['width']*.58)+rng.uniform(-.12,.12),0,1)
        shades.append(np.full(len(piece), interior, np.float32))
        offset += len(piece); triangle_count += len(lt)
    return (np.vstack(vertices).astype(np.float32), np.vstack(tris),
            np.concatenate(uvs).astype(np.float32), np.concatenate(shades))

def broad_leaf_piece(piece):
    points = piece[0]-piece[0].mean(axis=0)
    singular = np.linalg.svd(points,compute_uv=False)
    return singular[0]/max(singular[1],1e-6) < 5

K.reset()
K.render_setup((2000,1100),40)
K.add_ground()
library = G.leaf_library()
# Harvested source components include narrow stem fragments. Keep broad pieces
# so the leaf mass has no straight floating splinters at its edge.
library = [piece for piece in library if broad_leaf_piece(piece)]
assert library
materials = [K.build_material(bark_spec),K.build_material(leaf_spec)]
objects, rows = [], []
for i,vc in enumerate(VARIANTS):
    rng = np.random.default_rng(vc['seed'])
    wood_v,wood_t,wood_uv,wood_red = [],[],[],[]
    offset = 0
    tubes,anchors = make_wood(vc,rng)
    for j,(path,radius,sides) in enumerate(tubes):
        v,t,uv,red = G.tube(path,radius,sides,mark_phase=vc['seed']+j*.31)
        wood_v.append(v); wood_t.append(t+offset); wood_uv.append(uv); wood_red.append(red)
        offset += len(v)
    wv,wt = np.vstack(wood_v),np.vstack(wood_t)
    fv,ft,fuv,shade = make_foliage(vc,library,rng,anchors)
    vertices = np.vstack((wv,fv)); triangles = np.vstack((wt,ft+len(wv)))
    uv = np.concatenate((*wood_uv,fuv))
    material_ids = np.r_[np.zeros(len(wt)),np.ones(len(ft))].astype(np.int32)
    colour = np.zeros((len(vertices),4),np.float32)
    colour[:len(wv),0] = np.concatenate(wood_red)
    colour[len(wv):,0] = shade
    colour[:,3] = 1
    low,high = vertices[:,2].min(),vertices[:,2].max()
    vertices[:,2] = (vertices[:,2]-low)*vc['height']/(high-low)
    name = f'SM_SaplingBroadleaf_{i:02d}'
    obj = K.make(name,vertices,triangles,uv,material_ids,materials,vcol=colour)
    hull = K.frustum(f'UCX_{name}_00',0,0,.027,.014,vc['height']*.72)
    obj['ucx'] = [hull.name]
    objects.append(obj)
    rows.append(dict(mesh=name,seed=vc['seed'],stems=1,branches=vc['branches'],
        wood_triangles=int(len(wt)),leaf_triangles=int(len(ft)),
        materials=[bark_spec['name'],leaf_spec['name']]))

cfg = dict(family='SaplingBroadleaf',dir=str(HERE),prefix='SM_SaplingBroadleaf',
    asset_id='VEG-GreenlandsSaplingBroadleaf-01',role='sapling_broadleaf',budget=60000,
    scale_note='At scale 1: leafy single-trunk saplings 1.61-2.76 m tall; no ground.',
    gap=.90,res=(2000,1100),ortho=3.7,nanite=True,human_x=-1.15,human_y=0,
    extra_sources=['tree_small_02','japanese_hackberry_bark'])
rock_family.finish(cfg,objects,rows,[bark_spec,leaf_spec],
                   ['tree_small_02','japanese_hackberry_bark'])

# Eye-level lineup with the 1.8 m marker, plus one detail from a 3 m eye distance.
widths = [np.ptp(K.verts(o)[:,0]) for o in objects]
total = sum(widths)+cfg['gap']*2
cx = (total-1.6)/2
K.camera((cx+1.2,-11.5,1.58),(cx,0,1.30),lens=40)
K.render(HERE/'SaplingBroadleaf-Variants.png')
for marker in bpy.data.objects:
    if marker.name.startswith('RENDER_ONLY_scale_1.8m'):
        marker.hide_render = True
bpy.context.scene.render.resolution_x = 1500
bpy.context.scene.render.resolution_y = 1200
ox = objects[0].location.x
K.camera((ox+.85,-2.88,1.58),(ox,0,1.42),lens=30)
K.render(HERE/'SaplingBroadleaf-Detail-3m.png')
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'SaplingBroadleaf.blend'))
manifest_path = HERE/'asset-manifest.json'
manifest = json.loads(manifest_path.read_text())
manifest['generator'] = 'build_sapling.py'
manifest['renders'] = ['SaplingBroadleaf-Variants.png','SaplingBroadleaf-Detail-3m.png']
assert all(n <= 60000 for n in manifest['triangles'])
assert all(1.6 <= row['size_m'][2] <= 2.8 for row in manifest['variants'])
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
