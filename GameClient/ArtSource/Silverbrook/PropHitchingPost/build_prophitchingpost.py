"""Silverbrook street prop: HITCHING POST. Two square oak posts, a mortised rail, two hanging iron tie rings and a dressed stone mounting block.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_prophitchingpost.py
Owner concept: docs/Game-Design/concept-art/Silverbrook/Street Props.png (2.5 m x 1.1 m). Rail runs along X, block stands in front (-Y).
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
from prop_common_street import torus,finish
import bpy
scene=kit.reset(5201);asset=bpy.data.collections.new('SilverbrookPropHitchingPost');scene.collection.children.link(asset)
M=dict(stone=kit.scan('SB_Ashlar','stone_wall_04'),timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9))
BANDS=kit.bands('weathered_brown_planks');band=lambda:kit.rng.choice(BANDS)
G=dict(timber=Geo('SM_SBHitchingPost_Timber',asset),stone=Geo('SM_SBHitchingPost_Stone',asset),iron=Geo('SM_SBHitchingPost_Iron',asset))
oak=lambda c,s:G['timber'].box(c,s,M['timber'],.012,grain=True,band=band())
PX,PS,H=1.115,.15,1.1       # post centre x, post section, height: 2*(1.09+.125)=2.5 m
for s in (-1,1):
    oak((s*PX,0,H/2),(PS,PS,H))
    G['stone'].box((s*PX,0,.04),(PS+.1,PS+.1,.08),M['stone'],.012,rot=Matrix.Rotation(.07*s,3,'Z'),w=(.3,0))  # footing stone
    G['iron'].box((s*PX,0,.86),(PS+.016,PS+.016,.04),M['iron'],.004)          # strap at the rail mortise
RZ=.86;oak((0,0,RZ),(2*PX-PS+.02,.09,.12))                                    # rail, tenoned into both posts
for s in (-1,1):                                                              # tie rings: staple in the rail underside, ring hanging from it
    x=s*.5;G['iron'].box((x,-.052,RZ-.075),(.03,.014,.05),M['iron'],.002)
    G['iron'].box((x,0,RZ-.065),(.03,.12,.014),M['iron'],.002)                # staple plate wrapping the rail
    torus(G['iron'],(x,-.052,RZ-.09-.085),(1,0,0),(0,0,1),.085,.0125,M['iron'])
# ---- dressed grey mounting block: two stepped courses, SB_Ashlar world UVs at the scan's real scale (no stretch), slight damp darkening --------
def blk(c,sz,rot=0):G['stone'].box(c,sz,M['stone'],.012,rot=Matrix.Rotation(rot,3,'Z'),w=(.3,0))
BY=-.40
blk((0,BY,.11),(.6,.5,.22),.03)               # lower course
blk((.06,BY+.01,.285),(.42,.38,.13),-.05)     # upper step
for g in G.values():g.object()
finish(HERE,'PropHitchingPost',scene,asset,dict(footprint_m=None,front='-Y',ground_contact=True,near=['stables','farmhouse','square','blacksmith','market','road'],clearance_m=1.6),[(2.3,2.7),(.4,1.0),(1.0,1.2)])
