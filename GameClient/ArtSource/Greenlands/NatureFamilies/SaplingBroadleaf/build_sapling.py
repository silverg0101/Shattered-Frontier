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
    dict(seed=5, height=2.76, width=1.56, lean=(.15,.04), branches=6),
    dict(seed=8, height=2.19, width=1.32, lean=(-.12,.08), branches=5),
    dict(seed=12, height=1.61, width=1.02, lean=(.08,-.10), branches=5),
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
    """Uneven ascending limbs fork into exposed twigs below separate leaf tips."""
    height, width = vc['height'], vc['width']
    t = np.linspace(0, 1, 33)
    stem = np.c_[vc['lean'][0]*t**1.5 + .014*np.sin(t*7)*t,
                 vc['lean'][1]*t**1.5 + .012*np.sin(t*5)*t, height*t*.96]
    tubes = [(stem, .027*(1-t)**.9+.003, 8)]
    anchors = []
    # Small unfurnished shoots interrupt the clear trunk without becoming a shrub.
    for f, angle in ((.19, .28), (.29, 2.78)):
        root = stem[int(f*32)]
        u = np.linspace(0, 1, 6)
        reach = width*(.095 if f < .2 else .13)
        shoot = root + np.c_[reach*np.cos(angle)*u,
                             reach*np.sin(angle)*u, height*.07*u]
        tubes.append((shoot, np.linspace(.006,.0015,len(u)), 5))
    for j in range(vc['branches']):
        f = .36 + .34*j/max(vc['branches']-1,1) + rng.uniform(-.025,.025)
        start = stem[int(f*32)]
        angle = 2*math.pi*(j*.618034) + rng.uniform(-.22,.22)
        fullness = math.sqrt(max(.25,1-(f-.46)*1.6))
        reach = width*rng.uniform(.34,.48)*fullness
        rise = height*rng.uniform(.10,.18)
        u = np.linspace(0,1,11)
        limb = start + np.c_[reach*np.cos(angle)*u**1.12,
            reach*np.sin(angle)*u**1.12, rise*u + .012*height*np.sin(math.pi*u)]
        tubes.append((limb, .012*(1-u)**.75+.002, 6))
        # Two asymmetric forks leave a readable Y beneath each foliage group.
        for side in (-1,1):
            fork_at = 6 if side < 0 else 8
            fork_angle = angle + side*rng.uniform(.43,.78)
            length = reach*rng.uniform(.38,.64)
            v = np.linspace(0,1,7)
            fork = limb[fork_at] + np.c_[length*np.cos(fork_angle)*v,
                length*np.sin(fork_angle)*v, height*rng.uniform(.035,.09)*v]
            tubes.append((fork, np.linspace(.0055,.0015,len(v)), 5))
            anchors.append(fork[-1])
        anchors.append(limb[-1])
    # Sparse leader tips round the top without filling the centre of the crown.
    for k in (28,31,32):
        anchors.append(stem[k] + rng.normal(0,[width*.075,width*.075,height*.012]))
    return tubes, np.asarray(anchors)

def make_foliage(vc, lib, rng, anchors):
    """Loose leaf tufts at branch ends preserve dark interiors and sky gaps."""
    vertices, tris, uvs, shades = [], [], [], []
    offset = triangle_count = 0
    height = vc['height']
    for n,anchor in enumerate(anchors):
        count = rng.integers(36,47) if n < len(anchors)-3 else rng.integers(16,24)
        for _ in range(count):
            # A little droop and an anisotropic tuft make individual lobes.
            pos = anchor + rng.normal(size=3)*[vc['width']*.065,
                                                vc['width']*.065,height*.028]
            pos[2] -= height*.025
            lv, lt, luv = lib[rng.integers(len(lib))]
            size = rng.uniform(.065,.092)*height
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
