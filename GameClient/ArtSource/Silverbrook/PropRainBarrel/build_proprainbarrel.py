"""Silverbrook street prop: RAIN BARREL. Poly Haven wine_barrel_01 (iron-hooped oak) on four flat stones, a short oak post carrying a wooden gutter whose iron pipe pours into it.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_proprainbarrel.py
Owner concept: docs/Game-Design/concept-art/Silverbrook/Street Props.png (0.8 m x 1.4 m). Post and gutter are behind the barrel (+Y), front faces -Y.
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
from prop_common_wns import revolve
from prop_common_street import finish
import bpy
scene=kit.reset(5202);asset=bpy.data.collections.new('SilverbrookPropRainBarrel');scene.collection.children.link(asset)
M=dict(stone=kit.scan('SB_Ashlar','stone_wall_04'),timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),water=kit.solid('SB_PropRainBarrel_Water',(.012,.02,.025),.04))
BANDS=kit.bands('weathered_brown_planks');band=lambda:kit.rng.choice(BANDS)
G=dict(timber=Geo('SM_SBRainBarrel_Timber',asset),stone=Geo('SM_SBRainBarrel_Stones',asset),iron=Geo('SM_SBRainBarrel_Iron',asset),water=Geo('SM_SBRainBarrel_Water',asset))
PATCH=kit.patches('stone_wall_04',(.09,.06),dark=.7,frac=.01,std=.22,bright=1.05)
S,ZB=1.07,.07                                   # barrel scale (0.742 m model -> 0.79 m) and bed-stone height
kit.model(asset,'wine_barrel_01',Matrix.Translation((0,0,ZB))@Matrix.Diagonal((S,S,S,1)),aged=.25,name='SM_SBRainBarrel_PH_Barrel')
for k in range(4):                              # flat stones under the staves, rim stones keep the barrel off the mud
    a=math.tau*k/4+.5;u,v,du,dv=kit.rng.choice(PATCH)
    G['stone'].box((.24*math.cos(a),.24*math.sin(a),ZB/2),(.3,.22,ZB),M['stone'],.012,rot=Matrix.Rotation(a+kit.rng.uniform(-.3,.3),3,'Z'),rect=(u,v,du,dv))
TOP=ZB+.837*S                                   # lid surface
G['water'].poly([(.3*math.cos(math.tau*i/24),.3*math.sin(math.tau*i/24),TOP+.02) for i in range(24)],M['water'])   # rain water just under the rim
# ---- post, gutter and pipe ---------------------------------------------------------------------
PY=.465;oak=lambda c,s:G['timber'].box(c,s,M['timber'],.012,grain=True,band=band())
oak((0,PY,.7),(.11,.11,1.4))                                                      # post, 1.4 m
Z0,Z1=1.295,1.215                                                                   # gutter floor: high at the post, falling to the spout
G['timber'].beam((0,PY-.06,Z0),(0,.0,Z1),.17,.03,M['timber'],band=band())
for s in (-1,1):G['timber'].beam((s*.0875,PY-.06,Z0+.045),(s*.0875,-.03,Z1+.045),.03,.09,M['timber'],band=band())   # side boards
oak((0,PY-.07,Z0+.03),(.2,.03,.10))                                              # back board against the post
G['timber'].beam((0,PY-.05,Z0-.045),(0,.08,Z1-.025),.06,.06,M['timber'],band=band())   # cantilever arm under the gutter
G['timber'].beam((0,PY-.05,.95),(0,.2,1.19),.05,.05,M['timber'],band=band())   # diagonal brace
G['iron'].box((0,PY,1.12),(.126,.126,.035),M['iron'],.004);G['iron'].box((0,PY,.4),(.126,.126,.035),M['iron'],.004)  # post straps
# iron pipe: funnel under the spout, down into the barrel
PX_=-.03+.0;revolve(G['iron'],(0,PX_,Z1+.02),(0,0,-1),[(.07,0),(.05,.03),(.032,.06),(.032,.26),(.04,.27),(.04,.29)],M['iron'],12)
# ---- done ----------------------------------------------------------------------------------------
for g in G.values():g.object()
finish(HERE,'PropRainBarrel',scene,asset,dict(footprint_m=None,front='-Y',ground_contact=True,near=['cottage','farmhouse','stables','blacksmith','market','road'],clearance_m=.9),[(.7,.95),(.7,1.1),(1.3,1.5)])
