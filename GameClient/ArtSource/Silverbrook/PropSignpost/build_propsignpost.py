"""Silverbrook Prop Signpost: oak post on a stone footing with three carved, plain direction arms (no text).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propsignpost.py
Designed to match Silverbrook; no owner concept. Arms are blank pointer boards with a routed border.
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
from prop_common_wns import revolve,finalize
import bpy
SLOT=(1.1,0.4,2.8)   # slot_m: measured bounds rounded up to 0.1 m (finalize asserts the fit)
scene=kit.reset(5103);asset=bpy.data.collections.new('SilverbrookPropSignpost');scene.collection.children.link(asset)
M=dict(
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    carve=kit.solid('SB_PropSignpost_Carving',(.07,.045,.028),.85))
BANDS=kit.bands('weathered_brown_planks');band=lambda:kit.rng.choice(BANDS)
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
G=dict(stone=Geo('SM_SBPropSignpost_Stone',asset),timber=Geo('SM_SBPropSignpost_Timber',asset),detail=Geo('SM_SBPropSignpost_Details',asset))
P2,TOP=.06,2.58
for z0,z1,s,rot in ((0,.13,.36,0),(.13,.24,.27,.4)):G['stone'].box((0,0,(z0+z1)/2),(s,s,z1-z0),M['stone'],.02,Matrix.Rotation(rot,3,'Z'),rect=kit.rng.choice(STONES))
G['timber'].box((0,0,.24+(TOP-.24)/2),(2*P2,2*P2,TOP-.24),M['timber'],.01,grain=True,band=band())
for z in (.34,1.05,TOP-.10):G['detail'].box((0,0,z),(2*P2+.02,2*P2+.02,.06),M['iron'],.005)
revolve(G['timber'],(0,0,TOP),(0,0,1),[(.085,0),(.075,.04),(.045,.10),(0,.20)],M['timber'],4,math.pi/4)   # tall pointed finial
# ---- three plain arrow arms: yaw (deg from +X, anticlockwise from above), height, length ----------
for yaw,z,L in ((-10,2.38,.56),(12,2.08,.50),(172,1.82,.54)):
    R=Matrix.Rotation(math.radians(yaw),3,'Z');AH=.15;T=.04;pt=.12
    at=lambda u,y,v:Vector((0,0,z))+R@Vector((u,y,v))
    outline=[(-.03,-AH/2),(L-pt,-AH/2),(L,0),(L-pt,AH/2),(-.03,AH/2)]
    G['timber'].prism([at(u,0,v) for u,v in outline],R@Vector((0,1,0)),T,M['timber'])
    for sgn in (-1,1):
        y=sgn*(T/2)
        for v in (-AH/2+.02,AH/2-.02):G['detail'].box(at(L/2-.02,y,v),(L-.16,.004,.009),M['carve'],0,R)          # routed edge lines
        revolve(G['detail'],at(.09,y,0),R@Vector((0,sgn,0)),[(.016,-.003),(.016,.003),(.009,.007)],M['iron'],8)    # bolt head by the post
        revolve(G['detail'],at(L-.17,y,0),R@Vector((0,sgn,0)),[(.022,-.003),(.022,.003),(.013,.006)],M['carve'],10)   # carved boss
for g in G.values():g.object()
finalize(HERE,'PropSignpost','PROP-SilverbrookSignpost-01',scene,asset,SLOT,
    dict(footprint_m=[SLOT[0],SLOT[1]],front='-Y',ground_contact=True,near=['road','square','crossroads','gatehouse','market'],clearance_m=1.2))
