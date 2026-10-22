"""Silverbrook street prop: LADDER. 3.2 m ash ladder, two squared rails and ten round rungs through them, hemp lashing at every joint.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propladder.py
Concept: concept/ladder-sheet.png. Modelled straight along +Z, rails along X (0.4 m centres, 0.5 m overall); origin = ground centre between the feet. The placer leans it (about 75 degrees).
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
from prop_common_wns import revolve
from prop_common_street import torus
import prop_common_sheet as sheet
import bpy
scene=kit.reset(5303);asset=bpy.data.collections.new('SilverbrookPropLadder');scene.collection.children.link(asset)
M=dict(ash=kit.scan('SB_AshLadder','old_planks_02',dark=(.4,.37,.34),aged=.15),rope=kit.solid('SB_HempRope',(.3,.21,.11),.95))
BANDS=kit.bands('old_planks_02');band=lambda:kit.rng.choice(BANDS)
G=dict(wood=Geo('SM_SBLadder_Wood',asset),rope=Geo('SM_SBLadder_Lashing',asset))
H,RX,N=3.2,.2,10
for s in (-1,1):G['wood'].box((s*RX,0,H/2),(.05,.06,H),M['ash'],.009,grain=True,band=band(),w=(.1,0))                 # rails (rungs pass through them)
for i in range(N):
    z=.3+i*(H-.55)/(N-1);ph=kit.rng.uniform(0,1)
    revolve(G['wood'],(-RX-.035,0,z),(1,0,0),[(.022,0),(.024,.03),(.024,.44),(.022,.47)],M['ash'],10,phase=ph)   # rung, ends 3.5 cm past the rails
    for s in (-1,1):torus(G['rope'],(s*RX,0,z),(1,0,0),(0,1,0),.04,.009,M['rope'],10,5)                           # lashing round rail and rung
for g in G.values():g.object()
sheet.finish(HERE,'PropLadder',scene,asset,dict(footprint_m=None,front='-Y',ground_contact=True,near=['cottage','barn','granary','mill'],clearance_m=.2,lean_deg=75),[(.4,.6),(.05,.2),(3.1,3.3)],'ladder-sheet.png',
    'ground centre between the feet (straight, along +Z, rails along X; the placer leans it)')
