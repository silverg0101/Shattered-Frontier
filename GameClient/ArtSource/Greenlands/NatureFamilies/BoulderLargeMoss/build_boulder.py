"""VEG-style build for ROCK-GreenlandsBoulderLargeMoss-01: four granite boulders with moss on the top faces.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_boulder.py
Sources (CC0 Poly Haven scans, see ../Sources/provenance.json): boulder_01, namaqualand_boulder_06/03/04, re-graded to grey granite; moss texture from mossy_rock.
"""
import sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import rock_family

MOSS=dict(base='Sources/mossy_rock/mossy_rock_Diffuse.jpg',normal='Sources/mossy_rock/mossy_rock_nor_gl.jpg',rough='Sources/mossy_rock/mossy_rock_Rough.jpg',
          uv_scale=[4,4],tint=[.3,.5,.13],sat=1.1,amount=1.0,contrast=.4)
cfg=dict(family='BoulderLargeMoss',dir=str(HERE),prefix='SM_BoulderLargeMoss',asset_id='ROCK-GreenlandsBoulderLargeMoss-01',role='boulder_large_moss',budget=40000,
    scale_note='look profile place.scale is [1.0, 1.6]: meshes are 1.6-2.6 m long at 1.0, so placed boulders span 1.6-4.2 m (spec 1.5-4 m); sink 0.25',
    moss=MOSS,moss_mask=dict(low=.55,high=.9,breakup=.65),gap=1.2,
    variants=[
        dict(source='boulder_01',scale=1.25,yaw=20,tint=[.66,.68,.7],sat=.25),
        dict(source='namaqualand_boulder_06',scale=2.0,yaw=-15,tint=[.6,.62,.65],sat=.2),
        dict(source='namaqualand_boulder_03',scale=.8,yaw=70,tint=[.62,.64,.67],sat=.22),
        dict(source='namaqualand_boulder_04',scale=.85,yaw=130,mirror=True,tint=[.6,.62,.66],sat=.2),
    ])
rock_family.build(cfg)
