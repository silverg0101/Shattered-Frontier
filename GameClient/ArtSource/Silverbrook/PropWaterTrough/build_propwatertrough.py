"""Silverbrook water trough: coursed rough-hewn grey granite blocks, still water, a little moss at the base.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propwatertrough.py
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
import hay_props_common as hp
import bpy
scene=kit.reset(5301);rng=kit.rng
asset=bpy.data.collections.new('SilverbrookPropWaterTrough');scene.collection.children.link(asset)
M=dict(granite=kit.scan('SB_PropWaterTrough_Granite','rock_wall_08',dark=(.8,.82,.85),aged=.15),
       moss=kit.scan('SB_PropWaterTrough_Moss','mossy_rock',dark=(.5,.55,.35),damp=False),
       water=kit.solid('SB_PropWaterTrough_Water',(.035,.06,.05),.04,0))
STONES=kit.patches('rock_wall_08',(.14,.1),dark=.6,frac=.04,std=.3,bright=1.0)
G=dict(stone=Geo('SM_SBPropWaterTrough_Granite',asset),moss=Geo('SM_SBPropWaterTrough_Moss',asset),water=Geo('SM_SBPropWaterTrough_Water',asset))
SLOT=(2.2,1.0,.8)   # bounds rounded up to 0.1 m (asserted in finish)
L,W,H=2.0,.8,.7;TS,TE,FL=.16,.18,.14    # length, width, height; side/end wall thickness; floor slab
def block(c,size):G['stone'].box(c,size,M['granite'],.03,rect=rng.choice(STONES),w=(rng.uniform(0,.25),0))
cut=lambda lens,L=L:[sum(lens[:i])-L/2 for i in range(len(lens)+1)]
for k,(z0,lens) in enumerate(((0,[.7,.65,.65]),(.35,[.5,.8,.7]))):
    xs=cut(lens)
    for s in (-1,1):
        for a,b in zip(xs,xs[1:]):block(((a+b)/2,s*(W/2-TS/2),z0+.175+rng.uniform(-.004,.004)),(b-a-.006,TS,.35-.006))
    for s in (-1,1):block((s*(L/2-TE/2),0,z0+.175),(TE,W-2*TS-.006,.35-.006))
block((0,0,FL/2),(L-2*TE,W-2*TS,FL))
# still water: slightly rippled surface just inside the rim
ZW=H-.09;ix,iy=L/2-TE-.005,W/2-TS-.005;NX,NY=16,5
pt=lambda i,j:Vector((-ix+2*ix*i/NX,-iy+2*iy*j/NY,ZW+.0012*math.sin(i*1.3+j*2.1)+.0008*math.sin(i*3.1-j*1.7)))
for i in range(NX):
    for j in range(NY):G['water'].poly([pt(i,j),pt(i+1,j),pt(i+1,j+1),pt(i,j+1)],M['water'],[(0,0)]*4,smooth=True)
# moss: low tufts hugging the base and a few on the rim joints
def tuft(x,y,z,sx,sy,sz):G['moss'].box((x,y,z),(sx,sy,sz),M['moss'],.012,Matrix.Rotation(rng.uniform(-.1,.1),3,'Z'),rect=(rng.uniform(.05,.6),rng.uniform(.05,.6),.2,.2))
for _ in range(40):
    side=rng.random()
    if side<.7:s=rng.choice((-1,1));x,y=rng.uniform(-L/2+.1,L/2-.1),s*(W/2+.005)
    else:s=rng.choice((-1,1));x,y=s*(L/2+.005),rng.uniform(-W/2+.1,W/2-.1)
    (tuft(x,y+(.015 if y>0 else -.015)*(side<.7),rng.uniform(.015,.05),rng.uniform(.18,.4),.06,.07) if side<.7 else tuft(x+(.015 if x>0 else -.015),y,rng.uniform(.015,.05),.06,rng.uniform(.18,.4),.07))
hp.finish(HERE,'PropWaterTrough',G,SLOT,dict(footprint_m=[2.2,1.0],front='-Y',ground_contact=True,near=['stables','farmhouse','square','inn','blacksmith','market','road'],clearance_m=1.0),40000,weld=('SM_SBPropWaterTrough_Water',),beauty=(-2.8,-3.6,2.0),ortho=3.0,zc=.35,crop=(245,705,455,835))
