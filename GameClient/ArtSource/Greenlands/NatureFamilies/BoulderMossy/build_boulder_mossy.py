"""Build three weathered, partly buried granite boulders with CC0 Poly Haven scans."""
import json
import sys
from pathlib import Path

import bpy
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import nature_kit as K
import rock_family as R

MOSS = dict(base='Sources/mossy_rock/mossy_rock_Diffuse.jpg',
            normal='Sources/mossy_rock/mossy_rock_nor_gl.jpg',
            rough='Sources/mossy_rock/mossy_rock_Rough.jpg',
            uv_scale=[3, 3], tint=[.38, .53, .19], sat=.9,
            amount=1, contrast=.32)
VARIANTS = [
    dict(source='namaqualand_boulder_04', scale=1, span=1.9, yaw=32,
         seed=17, tint=[.64,.66,.63]),
    dict(source='namaqualand_boulder_05', scale=1, span=2.55, yaw=78, seed=29, tint=[.69,.70,.67]),
    dict(source='namaqualand_boulder_04', scale=1, span=2.2, yaw=143, mirror=True,
         seed=53, tint=[.70,.71,.68]),
]

K.reset()
K.render_setup((1900, 950), 48)
K.add_ground(colour=(.18,.19,.14))
objects, rows, specs = [], [], []
for i, vc in enumerate(VARIANTS):
    loaded = K.load_gltf(vc['source'])
    scan = max(loaded.values(), key=lambda o: len(o.data.polygons))
    for other in loaded.values():
        if other is not scan:
            bpy.data.objects.remove(other, do_unlink=True)
    v, t, uv, mi = K.arrays(scan)
    bpy.data.objects.remove(scan, do_unlink=True)
    v, t, uv, mi = R.transform(v, t, uv, mi, vc['scale'], vc.get('mirror',False), vc['yaw'])
    v *= vc['span']/max(np.ptp(v[:,0]),np.ptp(v[:,1]))
    # Break scan silhouettes and straight upper ridges without erasing fractures.
    z0,z1=v[:,2].min(),v[:,2].max()
    upper=np.clip((v[:,2]-z0)/max(z1-z0,.001),0,1)**1.4
    v[:,2] += upper*(.10*np.sin(v[:,0]*3.2+vc['seed'])
                     +.07*np.sin(v[:,1]*4.4+v[:,0]*1.8))
    v[:,0] += .045*np.sin(v[:,1]*4.1+v[:,2]*2.3+vc['seed'])
    # Centre the footprint; put the lowest 8-12 cm under the render ground.
    v[:,0] -= (v[:,0].min()+v[:,0].max())/2
    v[:,1] -= (v[:,1].min()+v[:,1].max())/2
    burial = min(.18, .11*(v[:,2].max()-v[:,2].min()))
    v[:,2] -= v[:,2].min()+burial
    spec = R.scan_spec(f'MI_BoulderMossy_{i:02d}', vc['source'], vc['tint'], .32)
    spec['blend'] = MOSS
    name = f'SM_BoulderMossy_{i:02d}'
    obj = K.make(name,v,t,uv,np.zeros(len(t),np.int32),[K.build_material(spec)])
    K.decimate(obj,38000)
    moss = K.moss_mask(obj,seed=vc['seed'],low=.42,high=.82,breakup=.78)
    a=obj.data.color_attributes['Col'];col=np.empty(len(obj.data.vertices)*4,np.float32)
    a.data.foreach_get('color',col.ravel());col=col.reshape(-1,4)
    xyz=K.verts(obj)
    patches=.5+.5*np.sin(xyz[:,0]*4.7+vc['seed'])*np.sin(xyz[:,1]*5.3+xyz[:,2]*3.7)
    col[:,0]*=np.clip((patches-.38)/.3,0,1)**2
    a.data.foreach_set('color',col.ravel())
    moss=float(col[:,0].mean())
    objects.append(obj); specs.append(spec)
    rows.append(dict(mesh=name,source=vc['source'],scale=vc['scale'],yaw=vc['yaw'],
                     mirror=vc.get('mirror',False),burial_m=round(float(burial),3),
                     moss_mean=round(moss,3),material=spec['name']))

cfg = dict(family='BoulderMossy',dir=str(HERE),prefix='SM_BoulderMossy',
           asset_id='ROCK-GreenlandsBoulderMossy-01',role='boulder_mossy',
           budget=40000,nanite=True,gap=1.0,res=(1900,950),look_z=.72,
           scale_note='Granite boulders 1.2-3 m across; origin at ground contact, buried base in mesh.',
           extra_sources=['mossy_rock'])
R.finish(cfg,objects,rows,specs,[v['source'] for v in VARIANTS])

# A separate close view from 3 m, at 1.6 m eye height.
sc = bpy.context.scene
sc.render.resolution_x,sc.render.resolution_y = 1500,1150
ox = objects[0].location.x
K.camera((ox+1,-2.83,1.6),(ox,0,.55),lens=28)
for o in bpy.data.objects:
    if o.name.startswith('RENDER_ONLY_scale_1.8m'): o.hide_render=True
K.render(HERE/'BoulderMossy-Detail-3m.png')
for o in bpy.data.objects:
    if o.name.startswith('RENDER_ONLY_scale_1.8m'): o.hide_render=False
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'BoulderMossy.blend'))
p=HERE/'asset-manifest.json'; m=json.loads(p.read_text())
m['generator']='build_boulder_mossy.py'
m['renders']=['BoulderMossy-Variants.png','BoulderMossy-Variants-Top.png','BoulderMossy-Detail-3m.png']
m['units']='metres, Z up, front -Y, origin at ground contact; buried base below z=0'
m['concept_reference']='docs/Game-Design/concept-art/UI/v5/gameplay-hud.png'
for r in m['variants']: r['origin']='ground contact, footprint centre; base below z=0'
assert len(m['variants'])==3 and max(m['triangles'])<=40000
assert all(1.2<=max(r['size_m'][:2])<=3 for r in m['variants'])
p.write_text(json.dumps(m,indent=2)+'\n')
