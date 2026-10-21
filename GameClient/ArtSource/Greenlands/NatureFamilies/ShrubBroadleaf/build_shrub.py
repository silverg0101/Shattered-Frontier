"""Build three irregular broadleaf shrubs. Run with Blender 5.1.1 -b -P build_shrub.py."""
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
# Centre and radii are fractions of width, depth and height. Separate lobes
# leave uneven edges and openings; each variant has a different arrangement.
VARIANTS = [
    dict(seed=31, height=1.70, width=2.52, depth=2.02, lobes=[
        (-.34,-.05,.58,.20,.24,.23),(-.23,.13,.80,.21,.22,.20),
        (-.04,-.13,.72,.22,.23,.23),(.11,.11,.88,.19,.19,.18),
        (.30,-.08,.65,.22,.23,.25),(.39,.12,.50,.15,.20,.19),
        (-.06,.22,.54,.20,.17,.20)]),
    dict(seed=47, height=1.42, width=2.14, depth=1.78, lobes=[
        (-.35,.10,.56,.18,.23,.24),(-.17,-.12,.84,.22,.25,.19),
        (.04,.15,.69,.22,.22,.22),(.25,-.12,.78,.20,.21,.20),
        (.36,.16,.49,.17,.18,.22),(-.03,-.24,.49,.17,.17,.17)]),
    dict(seed=73, height=1.10, width=1.74, depth=1.50, lobes=[
        (-.37,-.05,.48,.16,.19,.22),(-.24,.17,.69,.18,.20,.20),
        (-.08,-.17,.83,.20,.21,.18),(.14,.09,.74,.20,.20,.21),
        (.36,-.05,.54,.19,.23,.23),(.17,-.23,.52,.17,.17,.19),
        (-.02,.25,.51,.16,.17,.18),(.32,.20,.41,.15,.16,.16)]),
]

leaf_spec = dict(name='MI_ShrubBroadleaf_Leaf',
    base='Sources/tree_small_02/textures/tree_small_02_leaves_diff_2k.jpg',
    normal='Sources/tree_small_02/textures/tree_small_02_leaves_nor_gl_2k.jpg',
    arm='Sources/tree_small_02/textures/tree_small_02_leaves_arm_2k.jpg',
    alpha='Sources/tree_small_02/textures/tree_small_02_leaves_alpha_2k.png',
    tint=[1.22,1.38,.80], sat=.91, two_sided=True, alpha_mode='MASKED',
    blend=dict(base='Sources/tree_small_02/textures/tree_small_02_leaves_diff_2k.jpg',
               normal='Sources/tree_small_02/textures/tree_small_02_leaves_nor_gl_2k.jpg',
               arm='Sources/tree_small_02/textures/tree_small_02_leaves_arm_2k.jpg',
               tint=[.56,.68,.43], sat=.68, amount=1.0, contrast=.48))
h = 'Sources/japanese_hackberry_bark/japanese_hackberry_bark'
bark_spec = dict(name='MI_ShrubBroadleaf_Bark', base=h+'_Diffuse.jpg',
    normal=h+'_nor_gl.jpg', rough=h+'_Rough.jpg', tint=[.55,.49,.41], sat=.65,
    blend=dict(base=h+'_Diffuse.jpg', normal=h+'_nor_gl.jpg', rough=h+'_Rough.jpg',
               tint=[.16,.12,.08], sat=.7, amount=1.0, contrast=.2))


def centres(vc):
    return [(np.array([x*vc['width'],y*vc['depth'],z*vc['height']]),
             np.array([rx*vc['width'],ry*vc['depth'],rz*vc['height']]))
            for x,y,z,rx,ry,rz in vc['lobes']]


def make_wood(vc,rng):
    """Curved tapered bark tubes terminate inside their own foliage lobe."""
    tubes = []
    for i,(center,radius) in enumerate(centres(vc)):
        angle = math.atan2(center[1],center[0])
        base = np.array([rng.uniform(.025,.075)*math.cos(angle),
                         rng.uniform(.025,.075)*math.sin(angle),.014])
        end = center - np.array([0,0,radius[2]*.27])
        t = np.linspace(0,1,22)
        p = base[None,:]*(1-t[:,None]**1.35)+end[None,:]*t[:,None]**1.35
        p[:,2] += .12*vc['height']*np.sin(math.pi*t)
        p[:,0] += .035*np.sin(2*math.pi*t+i)*t*(1-t)
        p[:,1] += .027*np.cos(2*math.pi*t+i)*t*(1-t)
        tubes.append((p,.017*(1-t)**.9+.0025,7))
        for j in range(2):
            origin = p[int((.43+j*.20)*21)]
            target = center+rng.uniform(-.33,.33,3)*radius
            target[2] = max(target[2],origin[2]+.035)
            q = np.array([origin,.5*(origin+target)+[0,0,.045],target])
            tubes.append((q,np.array([.006,.004,.0018]),5))
    return tubes


def make_foliage(vc,lib,rng):
    """Leaf pieces occupy distinct uneven lobes with sparse interiors."""
    V,T,UV,C = [],[],[],[]
    offset = tri_count = 0
    lobes = centres(vc)
    weights = np.array([np.prod(r)**.65 for _,r in lobes])
    weights /= weights.sum()
    while tri_count < 30000:
        k = rng.choice(len(lobes),p=weights)
        center,radius = lobes[k]
        direction = rng.normal(size=3)
        direction /= np.linalg.norm(direction)
        shell = rng.uniform(.40,1.04)**.38
        pos = center+direction*radius*np.array([.77,.78,.86])*shell
        pos[2] -= .18*vc['height']*max(0,abs(pos[0])/(.5*vc['width'])-.55)
        if pos[2] < .19*vc['height']:
            continue
        lv,lt,uv = lib[rng.integers(len(lib))]
        size = rng.uniform(.075,.120)*vc['height']
        p = (lv@G.rot(rng).T)*(size/max(np.ptp(lv,axis=0).max(),1e-6))+pos
        V.append(p); T.append(lt+offset); UV.append(uv)
        interior = np.clip((.95-shell)*.9+(.64-pos[2]/vc['height'])*.35
                           +rng.uniform(-.13,.13),0,1)
        C.append(np.full(len(p),interior,np.float32))
        offset += len(p); tri_count += len(lt)
    return np.vstack(V).astype(np.float32),np.vstack(T),np.concatenate(UV).astype(np.float32),np.concatenate(C)


K.reset()
K.render_setup((2000,1000),40)
K.add_ground()
lib = G.leaf_library()
mats = [K.build_material(bark_spec),K.build_material(leaf_spec)]
objects,rows = [],[]
for index,vc in enumerate(VARIANTS):
    rng = np.random.default_rng(vc['seed'])
    wood_v,wood_t,wood_uv,wood_r = [],[],[],[]
    offset = 0
    tubes = make_wood(vc,rng)
    for j,(p,radius,sides) in enumerate(tubes):
        v,t,uv,red = G.tube(p,radius,sides,mark_phase=vc['seed']+j*.17)
        wood_v.append(v); wood_t.append(t+offset); wood_uv.append(uv); wood_r.append(red)
        offset += len(v)
    wv,wt = np.vstack(wood_v),np.vstack(wood_t)
    fv,ft,fuv,shade = make_foliage(vc,lib,rng)
    verts = np.vstack((wv,fv))
    tris = np.vstack((wt,ft+len(wv)))
    uv = np.concatenate((*wood_uv,fuv))
    materials = np.r_[np.zeros(len(wt)),np.ones(len(ft))].astype(np.int32)
    colour = np.zeros((len(verts),4),np.float32)
    colour[:len(wv),0] = np.concatenate(wood_r)
    colour[len(wv):,0] = shade
    colour[:,3] = 1
    lo,hi = verts.min(axis=0),verts.max(axis=0)
    center = (lo+hi)*.5
    verts[:,0] = (verts[:,0]-center[0])*vc['width']/(hi[0]-lo[0])
    verts[:,1] = (verts[:,1]-center[1])*vc['depth']/(hi[1]-lo[1])
    verts[:,2] = (verts[:,2]-lo[2])*vc['height']/(hi[2]-lo[2])
    name = f'SM_ShrubBroadleaf_{index:02d}'
    obj = K.make(name,verts,tris,uv,materials,mats,vcol=colour)
    hull = K.frustum(f'UCX_{name}_00',0,0,.12,.24,vc['height']*.37)
    obj['ucx'] = [hull.name]
    objects.append(obj)
    rows.append(dict(mesh=name,seed=vc['seed'],lobe_count=len(vc['lobes']),
                     stems=len(tubes),wood_triangles=int(len(wt)),
                     leaf_triangles=int(len(ft)),
                     materials=[bark_spec['name'],leaf_spec['name']]))

cfg = dict(family='ShrubBroadleaf',dir=str(HERE),prefix='SM_ShrubBroadleaf',
    asset_id='VEG-GreenlandsShrubBroadleaf-01',role='shrub_broadleaf',budget=60000,
    scale_note='At scale 1: irregular open shrubs 1.10-1.70 m tall and 1.74-2.52 m wide; tapered bare base.',
    gap=.65,res=(2000,1000),ortho=2.6,nanite=True,human_x=-.9,human_y=0,
    extra_sources=['tree_small_02','japanese_hackberry_bark'])
rock_family.finish(cfg,objects,rows,[bark_spec,leaf_spec],
                   ['tree_small_02','japanese_hackberry_bark'])

# Eye-level three-quarter lineup and one view from 3 m.
total = sum(vc['width'] for vc in VARIANTS)+cfg['gap']*2
cx = (total-1.6)/2
K.camera((cx+1.5,-13.0,1.58),(cx,0,.86),lens=40)
K.render(HERE/'ShrubBroadleaf-Variants.png')
ox = objects[0].location.x
bpy.context.scene.render.resolution_x = 1500
bpy.context.scene.render.resolution_y = 1200
K.camera((ox+1.0,-2.83,1.58),(ox,0,.83),lens=26)
scale_props = [o for o in bpy.data.objects if o.name.startswith('RENDER_ONLY_scale_1.8m')]
for o in scale_props:
    o.hide_render = True
K.render(HERE/'ShrubBroadleaf-Detail-3m.png')
for o in scale_props:
    o.hide_render = False
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'ShrubBroadleaf.blend'))

manifest_path = HERE/'asset-manifest.json'
manifest = json.loads(manifest_path.read_text())
manifest['generator'] = 'build_shrub.py'
manifest['renders'] = ['ShrubBroadleaf-Variants.png','ShrubBroadleaf-Detail-3m.png']
assert all(n <= 60000 for n in manifest['triangles'])
assert all(1.0 <= row['size_m'][2] <= 1.8 and
           1.3 <= max(row['size_m'][:2])/row['size_m'][2] <= 1.8
           for row in manifest['variants'])
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
