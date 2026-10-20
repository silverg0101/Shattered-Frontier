"""Silverbrook Prop Notice Board: oak board on two posts, small slate gable roof, pinned parchment notices (ink strips, no readable text).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propnoticeboard.py
Designed to match Silverbrook; no owner concept. The board faces -Y; ridge runs along X.
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
from prop_common_wns import revolve,finalize
import bpy
SLOT=(1.9,1.0,2.5)
scene=kit.reset(5102);asset=bpy.data.collections.new('SilverbrookPropNoticeBoard');scene.collection.children.link(asset)
M=dict(
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    paper=kit.solid('SB_PropNoticeBoard_Parchment',(.50,.40,.25),.9),
    ink=kit.solid('SB_PropNoticeBoard_Ink',(.035,.027,.02),.8),
    wax=kit.solid('SB_PropNoticeBoard_Wax',(.30,.03,.02),.45))
BANDS=kit.bands('weathered_brown_planks');band=lambda:kit.rng.choice(BANDS)
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
G=dict(stone=Geo('SM_SBPropNoticeBoard_Stone',asset),timber=Geo('SM_SBPropNoticeBoard_Timber',asset),board=Geo('SM_SBPropNoticeBoard_Board',asset),roof=Geo('SM_SBPropNoticeBoard_Roof',asset),detail=Geo('SM_SBPropNoticeBoard_Details',asset))
def oak(c,s):G['timber'].box(c,s,M['timber'],.01,grain=True,band=band())
PX,PT,RZ=.82,2.05,2.30
for s in (-1,1):
    oak((s*PX,0,PT/2),(.12,.12,PT))                                                       # post, set straight in the ground
    oak((s*PX,0,1.96),(.10,.70,.11))                                                      # tie beam across Y
    oak((s*PX,0,2.19),(.09,.09,.28))                                                      # king post
    for t in (-1,1):G['timber'].beam((s*PX,t*.05,1.77),(s*PX,t*.27,1.96),.08,.08,M['timber'],band=band())
oak((0,0,RZ),(1.85,.10,.10))
for t in (-1,1):oak((0,t*.30,1.96),(1.6,.08,.11))
# ---- board: vertical plank back, oak frame ----------------------------------------------------
BX,BZ0,BZ1=PX-.06,.62,1.88
G['board'].box((0,0,(BZ0+BZ1)/2),(2*BX,.04,BZ1-BZ0),M['door'],.004,grain=True,band=kit.bands('old_planks_02')[0])
for i in range(1,8):G['detail'].box((-BX+2*BX*i/8,-.0205,(BZ0+BZ1)/2),(.004,.002,BZ1-BZ0-.02),M['ink'],0)   # plank seams
for z in (BZ0+.045,BZ1-.04):oak((0,0,z),(2*BX,.07,.09))
for s in (-1,1):oak((s*(BX-.04),0,(BZ0+BZ1)/2),(.08,.07,BZ1-BZ0-.18))
# ---- roof: small slate gable ------------------------------------------------------------------
RIDGE=Vector((0,0,RZ));EAVE=Vector((0,.30,1.96+.06));d=(EAVE-RIDGE).normalized();L=(EAVE-RIDGE).length+.2
for x in (-.82,0,.82):
    for t in (-1,1):G['timber'].beam((x,0,RZ-.04),(x,t*.30,2.01),.07,.07,M['timber'],band=band())
for t in (1,-1):
    dd=Vector((0,d.y*t,d.z));nn=Vector((0,-d.z*t,d.y));rot=Matrix((Vector((t,0,0)),dd,nn)).transposed();assert abs(rot.determinant()-1)<1e-6
    G['roof'].box(RIDGE+dd*(L/2-.02)+nn*.07,(1.8,L,.06),M['slate'],.01,rot)
G['roof'].box((0,0,RZ+.08),(1.8,.15,.05),M['slate'],.01,Matrix.Rotation(math.pi/4,3,'X'))
# ---- notices: parchment sheets with illegible ink strips, iron pins, wax seals -------------------
YF=-.0205   # front of the plank back
def notice(cx,cz,w,h,ang,lines=True,seal=False):
    R=Matrix.Rotation(ang,3,'Y');at=lambda x,z,y:Vector((cx,0,cz-.11))+R@Vector((x,y,z))
    G['detail'].box(at(0,0,YF-.002),(w,.006,h),M['paper'],.002,R)
    if lines:
        for i in range(int((h-.05)/.032)):
            lw=(w-.06)*(1 if i%5 else .55)*kit.rng.uniform(.8,1);G['detail'].box(at(-(w-.06)/2+lw/2,h/2-.04-i*.032,YF-.0052),(lw,.0015,.009),M['ink'],0,R)
    for sx in (-1,1) if h>.2 else (0,):revolve(G['detail'],at(sx*(w/2-.03),h/2-.03,YF-.005),(0,-1,0),[(.009,0),(.009,.004),(.005,.008),(0,.01)],M['iron'],6)
    if seal:revolve(G['detail'],at(0,-h/2+.04,YF-.005),(0,-1,0),[(.022,0),(.022,.006),(.012,.011),(0,.012)],M['wax'],10)
notice(-.50,1.66,.40,.54,.04,seal=True);notice(.0,1.62,.36,.62,-.03);notice(.50,1.68,.38,.50,.05)
notice(-.52,1.14,.34,.38,-.05);notice(-.06,1.12,.42,.32,.02,seal=True);notice(.45,1.16,.32,.40,-.04);notice(.02,1.88,.2,.14,.1,lines=False)
for g in G.values():g.object()
finalize(HERE,'PropNoticeBoard','PROP-SilverbrookNoticeBoard-01',scene,asset,SLOT,
    dict(footprint_m=[SLOT[0],SLOT[1]],front='-Y',ground_contact=True,near=['square','market','road','guild','townhall','gatehouse'],clearance_m=1.5))
