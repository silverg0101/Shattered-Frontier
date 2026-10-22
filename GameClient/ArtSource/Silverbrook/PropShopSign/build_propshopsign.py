"""Silverbrook street prop: SHOP SIGN. Wrought-iron scroll bracket (0.7 m) on a wall plate, a painted oak board hanging from two rings.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propshopsign.py
Concept: concept/shop-sign-sheet.png. Origin = centre of the wall plate's back face (touches the wall at y=0); the arm projects along -Y; the board faces +-X.
Board face: PNG cut from the sheet's bottom row (bread; boots and ale are kept beside it as variants for Unreal MIs).
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
from prop_common_wns import revolve
from prop_common_street import torus
import prop_common_sheet as sheet
from mathutils.geometry import tessellate_polygon
import bpy
SHEET=HERE/'concept'/'shop-sign-sheet.png'
BOARDS=dict(bread=(172,653,573,998),boots=(632,632,1040,995),ale=(1090,630,1503,995))   # board bounding boxes in the sheet (px, top-left origin)
for n,b in BOARDS.items():sheet.crop_png(SHEET,b,HERE/f'PropShopSign-board-{n}.png',(512,512))
scene=kit.reset(5301);asset=bpy.data.collections.new('SilverbrookPropShopSign');scene.collection.children.link(asset)
M=dict(timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    face=sheet.image_mat('SB_PropShopSignBoard',HERE/'PropShopSign-board-bread.png',.75,variants=dict(Boots='PropShopSign-board-boots.png',Ale='PropShopSign-board-ale.png')))
BANDS=kit.bands('weathered_brown_planks');band=lambda:kit.rng.choice(BANDS)
G=dict(iron=Geo('SM_SBShopSign_Iron',asset),timber=Geo('SM_SBShopSign_Board',asset),face=Geo('SM_SBShopSign_BoardFace',asset))
ARM_Z,BOARD_TOP,BW,BH,BT=.12,-.015,.60,.55,.04     # arm height, board top, board width/height (0.45 m body + point), thickness
Y0=-.08                                              # board's wall-side edge
def bar(pts,w=.016):
    for a,b in zip(pts,pts[1:]):G['iron'].beam(a,b,w,w,M['iron'],grain=False)
def yz(y,z):return Vector((0,y,z))
def spiral(c,r0,a0,turns=1.3,n=14,sign=-1,k=.85):
    return [yz(c[0]+r0*(1-k*i/n)*math.cos(a0+sign*math.tau*turns*i/n),c[1]+r0*(1-k*i/n)*math.sin(a0+sign*math.tau*turns*i/n)) for i in range(n+1)]
# ---- wall plate, arm, scrolls ---------------------------------------------------------------------
G['iron'].box((0,-.01,0),(.07,.02,.5),M['iron'],.003)
for z in (-.2,-.08,.08,.2):G['iron'].box((0,-.024,z),(.022,.012,.022),M['iron'],0,rot=Matrix.Rotation(math.pi/4,3,'Y'))   # diamond-head nails
bar([yz(-.02,ARM_Z),yz(-.62,ARM_Z)],.026)
revolve(G['iron'],(0,-.62,ARM_Z),(0,-1,0),[(0,0),(.022,.025),(.02,.045),(0,.08)],M['iron'],4)                  # spear point, tip at y=-0.70
for y in (-.08,-.5):G['iron'].box((0,y,ARM_Z),(.04,.03,.04),M['iron'],.004)                                       # collars
c=(-.57,.215);a0=math.atan2(.17-c[1],-.50-c[0]);r0=math.hypot(-.50-c[0],.17-c[1])
bar([yz(-.02,.23),yz(-.2,.21),yz(-.36,.19),yz(-.50,.17)]+spiral(c,r0,a0)[1:],.014)                                 # upper bar into the end scroll
arc=[yz(-.32+.3*math.cos(t),-.20+.3*math.sin(t)) for t in [i*math.pi/2/10 for i in range(11)]]
bar(arc,.018)                                                                                                       # brace, plate foot to arm
bar(spiral((-.07,-.215),.035,0.0,1.1,10,1,.8),.012)                                                                # foot scroll
bar([yz(-.02,.0),yz(-.05,.07),yz(-.075,ARM_Z-.01)],.012)                                                           # short stay
# ---- hangers: collar on the arm, ring, strap eye ----------------------------------------------------
HY=[Y0-.16*BW,Y0-.82*BW]
for y in HY:
    G['iron'].box((0,y,ARM_Z),(.044,.044,.044),M['iron'],.004)
    torus(G['iron'],(0,y,ARM_Z-.045),(0,1,0),(0,0,1),.024,.006,M['iron'],14,6)
    G['iron'].box((0,y,BOARD_TOP+.012),(.016,.03,.03),M['iron'],.002)
    G['iron'].box((0,y,BOARD_TOP-.05),(BT+.012,.046,.10),M['iron'],.004)                                           # strap over the board top
    for s in (-1,1):G['iron'].box((s*(BT/2+.009),y,BOARD_TOP-.06),(.01,.016,.016),M['iron'],0,rot=Matrix.Rotation(math.pi/4,3,'X'))
# ---- board: scalloped outline cut from the sheet panel ---------------------------------------------
OUT=[(0,0),(1,0),(1,.82),(.945,.85),(.8,.87),(.66,.9),(.6,.93),(.5,1),(.4,.93),(.34,.9),(.19,.87),(.065,.85),(0,.82)]   # (u,v) from top-left; image reads left to right for a viewer on -X
INS=.006;OUT=[(min(max(u,INS),1-INS),min(max(v,INS),1-INS)) for u,v in OUT]
P=lambda u,v,x:Vector((x,Y0-u*BW,BOARD_TOP-v*BH))
def tri_fan(side):
    x=side*BT/2;pts=[P(u,v,x) for u,v in OUT];uv=[(u,1-v) for u,v in OUT]
    # viewer on -X: right = -Y (u grows); viewer on +X: right = +Y, so mirror u in the texture to read correctly there
    if side>0:uv=[(1-u,w) for u,w in uv]
    for i,j,k in tessellate_polygon([[(p.y,p.z,0) for p in pts]]):
        t=[pts[i],pts[j],pts[k]];tuv=[uv[i],uv[j],uv[k]]
        if (t[1]-t[0]).cross(t[2]-t[0]).x*side<0:t.reverse();tuv.reverse()
        G['face'].poly(t,M['face'],tuv)
for s in (-1,1):tri_fan(s)
cen=sum((P(u,v,0) for u,v in OUT),Vector())/len(OUT)
for (u0,v0),(u1,v1) in zip(OUT,OUT[1:]+OUT[:1]):
    q=[P(u0,v0,-BT/2),P(u1,v1,-BT/2),P(u1,v1,BT/2),P(u0,v0,BT/2)]
    if (q[1]-q[0]).cross(q[3]-q[0]).dot((q[0]+q[2])/2-cen)<0:q.reverse()
    G['timber'].poly(q,M['timber'],w=(.2,0))
for g in G.values():g.object()
sheet.finish(HERE,'PropShopSign',scene,asset,dict(footprint_m=None,front='-Y',ground_contact=False,mount='wall',near=['shop','inn','bakery','cobbler'],clearance_m=.1),[(.03,.15),(.65,.8),(.7,.95)],'shop-sign-sheet.png',
    'centre of the wall plate back face (wall at y=0, arm projects -Y, board faces +-X, hangs below the arm)',
    extra=dict(variants=dict(board_textures=['PropShopSign-board-bread.png','PropShopSign-board-boots.png','PropShopSign-board-ale.png'],note='one mesh; Boots/Ale are MIs of M_SB_Card on slot SB_PropShopSignBoard'),pivot_m=[0,0,0]),ground=False,wall=True)
