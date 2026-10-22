"""Silverbrook hay bales: a small stack of four twine-tied rectangular straw bales.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_prophaybales.py
Designed to match Silverbrook; no owner concept.
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
import hay_props_common as hp
import bpy
scene=kit.reset(5201);rng=kit.rng
asset=bpy.data.collections.new('SilverbrookPropHayBales');scene.collection.children.link(asset)
M=dict(straw=kit.scan('SB_PropHayBales_Straw','sb_straw_face',dark=(.55,.45,.3),damp=False,normal=1.0),   # make_straw_scan.py (WP6d fix 5)
       twine=kit.solid('SB_PropHayBales_Twine',(.30,.22,.12),.9))                                          # dark tarred jute
G=dict(bales=Geo('SM_SBPropHayBales_Bales',asset),twine=Geo('SM_SBPropHayBales_Twine',asset),stalks=Geo('SM_SBPropHayBales_Stalks',asset))
SLOT=(2.4,1.1,1.6)   # bounds rounded up to 0.1 m (asserted in finish); 0.12-0.25 m loose stalks widen it
L,W,H=.95,.6,.56
def bale(c,yaw,tilt=0):
    R=Matrix.Rotation(math.radians(yaw),3,'Z')@Matrix.Rotation(math.radians(tilt),3,'X');C=Vector(c);P=lambda v:C+R@Vector(v)
    G['bales'].box(C,(L,W,H),M['straw'],.07,R,rect=(rng.uniform(.02,.38),rng.uniform(.02,.4),.6,.5))
    for x in (-.25,.25):
        for sz,c_ in (((.012,W+.012,.012),(x,0,H/2+.002)),((.012,W+.012,.012),(x,0,-H/2-.002)),((.012,.012,H+.012),(x,W/2+.002,0)),((.012,.012,H+.012),(x,-W/2-.002,0))):
            G['twine'].box(P(c_),sz,M['twine'],.004,R)
    for _ in range(330):   # loose stalks: poking from the cut ends mostly, some from the faces
        f=rng.random()
        if f<.55:s=rng.choice((-1,1));p=Vector((s*L/2,rng.uniform(-W/2+.04,W/2-.04),rng.uniform(-H/2+.04,H/2-.04)));d=Vector((s,rng.uniform(-.3,.3),rng.uniform(-.3,.3)))
        elif f<.8:s=rng.choice((-1,1));p=Vector((rng.uniform(-L/2+.05,L/2-.05),s*W/2,rng.uniform(-H/2+.04,H/2-.04)));d=Vector((rng.uniform(-.6,.6),s,rng.uniform(-.4,.2)))
        else:s=rng.choice((-1,1));p=Vector((rng.uniform(-L/2+.05,L/2-.05),rng.uniform(-W/2+.04,W/2-.04),s*H/2));d=Vector((rng.uniform(-.6,.6),rng.uniform(-.6,.6),s))
        d=(R@d).normalized();a=C+R@p-d*.03;b=a+d*rng.uniform(.12,.25)
        rot=(b-a).to_track_quat('Z','Y').to_matrix();G['stalks'].box((a+b)/2,(.0035,.0035,(b-a).length),M['straw'],0,rot,rect=(.3+rng.random()*.3,.3+rng.random()*.3,.02,.02))
bale((-.49,0,H/2),1.5);bale((.49,.01,H/2),-1.0)                       # two on the ground, end to end, a 2 cm wider shadowed gap
bale((.02,.02,H*1.5-.01),-3.5)                                      # one on top, centred over the joint
hp.finish(HERE,'PropHayBales',G,SLOT,dict(footprint_m=[2.4,1.1],front='-Y',ground_contact=True,near=['stables','farmhouse','granary','market','cottage','road','square'],clearance_m=.8),40000,beauty=(-2.2,-3.4,2.0),ortho=3.0,crop=(20,680,250,830))
