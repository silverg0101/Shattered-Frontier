"""Silverbrook Prop Well from the Street Props concept sheet: round grey rubble curb, four-post oak frame with braces,
steep slate hip roof with finial, winch with iron crank, rope and wooden bucket (about 2 m wide, 3 m tall).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propwell.py
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
from prop_common_wns import revolve,finalize
import bpy
SLOT=(2.0,2.0,3.0)   # slot_m: measured bounds rounded up to 0.1 m (finalize asserts the fit)
scene=kit.reset(5101);asset=bpy.data.collections.new('SilverbrookPropWell');scene.collection.children.link(asset)
M=dict(
    stone=kit.scan('SB_PropWell_Rubble','stone_wall_04',dark=(.55,.58,.64),aged=.55),
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    rope=kit.solid('SB_PropWell_Rope',(.30,.22,.12),.95),
    water=kit.solid('SB_PropWell_Water',(.01,.015,.02),.05))
BANDS=kit.bands('weathered_brown_planks');band=lambda:kit.rng.choice(BANDS)
G=dict(stone=Geo('SM_SBPropWell_Stone',asset),timber=Geo('SM_SBPropWell_Timber',asset),roof=Geo('SM_SBPropWell_Roof',asset),detail=Geo('SM_SBPropWell_Details',asset))
def oak(c,s):G['timber'].box(c,s,M['timber'],.01,grain=True,band=band())
# ---- rubble curb: four courses of irregular blocks plus a coping ring --------------------------
RI,RO=.52,.86
def course(z0,z1,r0,r1,phase,mean,gap=.007):
    cuts=[0];
    while cuts[-1]<math.tau-mean*.6/r1:cuts.append(cuts[-1]+kit.rng.uniform(.65,1.35)*mean/r1)
    cuts[-1]=math.tau
    for a,b in zip(cuts,cuts[1:]):
        a0,a1=phase+a+gap/r1,phase+b-gap/r1;jo=kit.rng.uniform(-.02,.012);ji=kit.rng.uniform(-.01,.01);dz=kit.rng.uniform(-.012,.012)
        pts=[((r0+ji)*math.cos(a0),(r0+ji)*math.sin(a0)),((r1+jo)*math.cos(a0),(r1+jo)*math.sin(a0)),((r1+jo)*math.cos(a1),(r1+jo)*math.sin(a1)),((r0+ji)*math.cos(a1),(r0+ji)*math.sin(a1))]
        G['stone'].prism([(x,y,(z0+z1)/2+dz/2) for x,y in pts],(0,0,1),z1-z0-.01+dz,M['stone'])
for i,z in enumerate((0,.20,.40,.60)):course(z,z+.20,RI,RO-(.015 if i%2 else 0),kit.rng.uniform(0,6),.34)
course(.80,.88,RI-.03,RO+.03,.3,.5)
G['detail'].poly([(RI*math.cos(math.tau*i/32),RI*math.sin(math.tau*i/32),.40) for i in range(32)],M['water'])   # water in the shaft
# ---- four-post oak frame, braces, plates --------------------------------------------------------
CZ,PT=.88,2.02;PP=.56;PS=.12
for sx in (-1,1):
    for sy in (-1,1):
        oak((sx*PP,sy*PP,CZ+(PT-CZ)/2),(PS,PS,PT-CZ))
        for z in (CZ+.05,PT-.1):G['detail'].box((sx*PP,sy*PP,z),(PS+.016,PS+.016,.045),M['iron'],.004)       # iron straps
        G['timber'].beam((sx*PP,sy*PP,1.62),(sx*(PP-.34),sy*PP,PT-.03),.075,.075,M['timber'],band=band())   # braces along X
        G['timber'].beam((sx*PP,sy*PP,1.62),(sx*PP,sy*(PP-.34),PT-.03),.075,.075,M['timber'],band=band())   # braces along Y
for s in (-1,1):
    oak((s*PP,0,PT-.07),(.10,2*PP+.10,.09))       # plates across Y
    oak((0,s*PP,PT-.125),(2*PP+.10,.10,.09))      # plates across X, one course lower so the laps do not share faces
    oak((s*PP,0,1.50),(.09,2*PP-.12,.10))         # axle beams between front and back posts
# ---- steep hip roof: four slate facets, hip caps, king rafters, finial ------------------------
EZ,AZ_,HW=PT+.04,2.76,.95
C=[Vector((sx*HW,sy*HW,EZ)) for sx,sy in ((-1,-1),(1,-1),(1,1),(-1,1))];AP=Vector((0,0,AZ_))
for a,b in zip(C,C[1:]+C[:1]):
    mid=(a+b+AP)/3;nrm=(b-a).cross(AP-a).normalized()
    if nrm.dot(mid-Vector((0,0,1.6)))<0:nrm=-nrm
    G['roof'].prism([a+nrm*.03,b+nrm*.03,AP+nrm*.03],nrm,.05,M['slate'])
for c in C:G['roof'].beam(c+Vector((0,0,.0))*1+(AP-c).normalized()*.0+Vector((0,0,.05)),AP+Vector((0,0,.05)),.085,.07,M['slate'])
for sx in (-1,1):
    for sy in (-1,1):G['timber'].beam((sx*(PP-.02),sy*(PP-.02),PT-.12),(sx*.07,sy*.07,AZ_-.14),.09,.09,M['timber'],band=band())   # hip rafters
oak((0,0,AZ_-.20),(.14,.14,.2))
revolve(G['timber'],(0,0,AZ_+.04),(0,0,1),[(.07,0),(.05,.03),(.03,.10),(0,.19)],M['timber'],8)       # finial
# ---- winch: axle, rope drum, crank, bucket -----------------------------------------------------
AY=1.50;L0=-PP-.12;L1=PP+.30
revolve(G['timber'],(L0,0,AY),(1,0,0),[(.042,0),(.042,L1-L0)],M['timber'],10)
ribs=[(.07,0)]
for i in range(9):ribs+=[(.07,.06*i+.02),(.092,.06*i+.025),(.092,.06*i+.055),(.07,.06*i+.06)]
revolve(G['detail'],(-.27,0,AY),(1,0,0),ribs,M['rope'],10)
for x in (-.285,.28):revolve(G['timber'],(x,0,AY),(1,0,0),[(.13,0),(.13,.03)],M['timber'],12)
cx=L1-.03
G['detail'].box((cx,0,AY-.10),(.04,.045,.24),M['iron'],.005)
revolve(G['detail'],(cx-.09,0,AY-.22),(1,0,0),[(.022,0),(.022,.18)],M['iron'],8)
revolve(G['detail'],(PP+.075,0,AY),(1,0,0),[(.07,0),(.07,.015)],M['iron'],8)
bk=kit.model(asset,'wooden_bucket_01',Matrix(),name='SM_SBPropWell_PH_Bucket')
pts=[Vector(v.co) for o in bk for v in o.data.vertices]
lo=[min(p[k] for p in pts) for k in range(3)];hi=[max(p[k] for p in pts) for k in range(3)]
BZ=1.13
for o in bk:o.matrix_world=Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,BZ-hi[2]))
G['detail'].box((0,0,(AY-.09+BZ)/2),(.026,.026,AY-.09-BZ),M['rope'],.004)   # rope from drum to the bucket handle
for g in G.values():g.object()
finalize(HERE,'PropWell','PROP-SilverbrookWell-01',scene,asset,SLOT,
    dict(footprint_m=[SLOT[0],SLOT[1]],front='-Y',ground_contact=True,near=['square','market','farmhouse','cottage','stables','road'],clearance_m=2.0))
