"""Silverbrook Cottage 6 from docs/Game-Design/concept-art/Silverbrook/Cottage-6.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cottage6.py
Original procedural geometry interpreting the elevation (8 m wide, 6 m to the stack
pots): steep-hipped slate roof, central half-timbered cross gable with a shuttered
window, two matching gabled dormers with planters, a small gabled porch on stone
piers over an arched stone doorway, right-hand ridge stack, coursed sandstone ground
storey in an oak frame.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,window,door,lantern,frame
import bpy
scene=kit.reset(4106)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Cottage-6.png'
asset=bpy.data.collections.new('SilverbrookCottage6');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Cottage6_Ashlar','sandstone_blocks_05',dark=(.42,.38,.31)),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    shutter=kit.scan('SB_Cottage6_Shutter','wood_planks_grey',dark=(.20,.22,.28),damp=False,aged=.45),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_05',(.05,.035),dark=.7,frac=.01,std=.22)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBCottage6_Masonry',asset),plaster=Geo('SM_SBCottage6_Plaster',asset),timber=Geo('SM_SBCottage6_Timber',asset),roof=Geo('SM_SBCottage6_Roof',asset),detail=Geo('SM_SBCottage6_Details',asset))

# ---- facing-safe solids -----------------------------------------------------------
def volume(geo,start):
    return sum(geo.v[f[0]].dot(geo.v[f[j]].cross(geo.v[f[j+1]])) for f in geo.f[start:] for j in range(1,len(f)-1))
def prism(geo,points,axis,th,mat,**kw):
    """kit.prism orients by its first three points, which flips concave outlines; use the Newell normal."""
    pts=[Vector(p) for p in points];axis=Vector(axis).normalized();n=Vector((0,0,0))
    for i,a in enumerate(pts):n+=a.cross(pts[(i+1)%len(pts)])
    if n.dot(axis)<0:pts.reverse()
    start=len(geo.f);o=axis*th/2   # same faces as kit.prism, without its re-orientation
    geo.poly([p+o for p in pts],mat,**kw);geo.poly([p-o for p in reversed(pts)],mat,**kw)
    for i,a in enumerate(pts):b=pts[(i+1)%len(pts)];geo.poly([a-o,b-o,b+o,a+o],mat,**kw)
    assert volume(geo,start)>0,'inside-out prism'
def wall(geo,mat,c,o,half,height,openings=(),th=.3,arch=None):
    """kit.wall with the arch spandrels built through the facing-safe prism."""
    kit.wall(geo,mat,c,o,half,height,openings,th)
    if arch:
        P,_=frame(c,o);a,w,spring=arch;rad=w/2
        for side in (-1,1):
            arc=[P(a+side*rad*math.cos(t),-th/2,spring+rad*math.sin(t)) for t in [k*math.pi/16 for k in range(9)]]
            prism(geo,arc+[P(a+side*rad,-th/2,spring+rad)],o,th,mat)

# ---- measured layout (metres) -------------------------------------------------
SLOT=(8,6,6)                   # town-layout slot w x d x h (concept README 'Fits')
W2,D2=3.35,2.2                 # outer wall half-extents
EAVE,RIDGE=2.5,5.1             # main hip roof
EX,EY,RX=3.8,2.5,3.15           # eave half-extents, ridge half-length (steep hips, as drawn)
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
GH,GW,GE=1.5,1.25,3.4          # front cross gable: roof half, wall half, eave on the main slope
KG=(RIDGE-GE)/GH;GY=-D2-.3
DXS,DH,DW,DE,DR=(-2.3,2.3),.6,.48,3.95,4.75   # dormers
YD=-(RIDGE-3.15)/KF;KD=(DR-DE)/DH;YDR=-(RIDGE-DR)/KF   # dormer front, roof pitch, where its ridge meets the main roof
XC,YC,CX,CY=2.55,.15,.45,.4    # ridge stack centre, half-extents
PE,PR,PH,PY=2.2,2.95,1.2,-D2-.75  # porch eave, ridge, roof half, front
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return RIDGE-abs(x)*KG
def dormer_z(x,xd):return DR-abs(x-xd)*KD
in_gable=lambda p:abs(p.x)<GW+.04 and p.y<0 and p.z<gable_z(p.x)-.005
in_dormer=[lambda p,xd=xd:abs(p.x-xd)<DH and YD<p.y<0 and p.z<dormer_z(p.x,xd)-.005 for xd in DXS]
in_stack=lambda p:abs(p.x-XC)<CX+.03 and abs(p.y-YC)<CY+.03
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered,deck_cover=None):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],deck_cover if deck_cover is not None else covered)
def finial(x,y,z,h=.3,s=.1):
    G['timber'].box((x,y,z+h/2),(s,s,h),M['timber'],.015,grain=True,band=band())
    prism(G['timber'],[(x-s*.8,y,z+h),(x+s*.8,y,z+h),(x,y,z+h+s*1.6)],(0,1,0),s*1.6,M['timber'])
# ---- main hip roof --------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,*in_dormer,in_stack])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
capped((-RX,0,RIDGE+.05),(RX,0,RIDGE+.05),18)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),11)
for a,b in ((-EX,-GW),(GW,EX)):G['timber'].beam((a,-EY-.02,EAVE-.06),(b,-EY-.02,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((-EX,EY+.02,EAVE-.06),(EX,EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
for s in (-1,1):finial(s*RX,0,RIDGE+.08,.14,.12)   # ridge-end blocks
# ---- cross gable roof, bargeboards and finial ----------------------------------------
LG=math.hypot(GH,RIDGE-GE)
plane((-GH,0,GE),(0,-1,0),(GH,0,RIDGE-GE),LG,lambda t:0,lambda t:-GY,[under_main])
plane((GH,0,GE),(0,1,0),(-GH,0,RIDGE-GE),LG,lambda t:GY,lambda t:0,[under_main])
capped((0,GY,RIDGE+.05),(0,-.4,RIDGE+.05),9)
for s in (-1,1):G['timber'].beam((s*(GH+.05),GY-.04,GE-.08),(-s*.16,GY-.04,RIDGE+.16*KG+.03),.08,.24,M['timber'],band=band())
yq=-(RIDGE-(gable_z(GW)-.05))/KF   # cheek walls close the wall gable down to the main slope
for s in (-1,1):prism(G['plaster'],[(s*GW,-D2,main_z(0,-D2)-.04),(s*GW,-D2,gable_z(GW)-.05),(s*GW,yq,gable_z(GW)-.05)],(1,0,0),.1,M['plaster'])
finial(0,GY-.06,RIDGE+.1,.38,.15)
# ---- dormers ------------------------------------------------------------------
LD=math.hypot(DH,DR-DE)
for XD in DXS:
    dz=lambda x:dormer_z(x,XD)
    plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:-YDR,lambda t:-YD+.16,[under_main])
    plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:YDR,[under_main])
    capped((XD,YD-.16,DR+.04),(XD,YDR-.1,DR+.04),4)
    for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
    finial(XD,YD-.2,DR+.08,.22,.09)
    DB=main_z(XD,YD)-.1
    wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.56,.19,.82)],th=.12)
    prism(G['plaster'],[(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dz(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dz(XD+DW)-.04)],(0,1,0),.12,M['plaster'])
    ym=-(RIDGE-(dz(XD+DW)-.04))/KF
    for s in (-1,1):prism(G['plaster'],[(XD+s*DW,YD,DB),(XD+s*DW,YD,dz(XD+DW)-.04),(XD+s*DW,ym,dz(XD+DW)-.04)],(1,0,0),.08,M['plaster'])
    for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
    G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
    WB=DB+.28;window(G,M,(XD,YD,WB),(0,-1,0),.38,.45,shutters=False,wall_th=.12,band=band())
    # planter box hung on the dormer front just under the sill (the kit planter's brackets would hit the slates)
    pt=WB-.09-.05;G['timber'].box((XD,YD-.11,pt-.08),(.72,.24,.16),M['planks'],.006)
    G['detail'].box((XD,YD-.12,pt-.01),(.66,.17,.02),M['soil'],0)
# ---- ridge stack ------------------------------------------------------------------
G['stone'].box((XC,YC,4.2),(2*CX,2*CY,2.4),M['stone'],.01)
G['stone'].box((XC,YC,5.45),(2*CX+.06,2*CY+.06,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,5.56),(2*CX+.1,2*CY+.1,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.2,.2):G['detail'].vessel((XC+dx,YC,5.62),[(.1,0),(.095,.27),(.105,.31),(.12,.34),(.09,.34),(.075,.29),(0,.29)],M['pot'])
# ---- walls ------------------------------------------------------------------------
PLATE=main_z(0,-D2)-.04
STEP,DWD,DS=.3,.95,1.45        # door sill = porch landing top; door width; arch spring above the sill
WIN,WH,WZ,WX=.7,.8,1.1,2.2;WO=(WIN+.18,WZ-.09,WZ+WH+.09)
front_holes=[(-WX,WO[0],WO[1],WO[2]),(0,DWD,STEP,STEP+DS+DWD/2),(WX,WO[0],WO[1],WO[2])]
wall(G['stone'],M['stone'],(0,-D2,0),(0,-1,0),W2,PLATE,front_holes,arch=(0,DWD,STEP+DS))
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,PLATE,[(-1.8,WO[0],WO[1],WO[2]),(1.8,WO[0],WO[1],WO[2])])
for s in (-1,1):
    wall(G['stone'],M['stone'],(s*W2,0,0),(s,0,0),D2-.3,PLATE,[(.9,WO[0],WO[1],WO[2])])
    top=[(y/10,main_z(W2,y/10)-.05) for y in range(-round(D2*10),round(D2*10)+1) if main_z(W2,y/10)-.05>PLATE+.01]
    prism(G['plaster'],[(s*(W2-.15),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['plaster'])
# gable face (plaster infill) above the main wall plate, with a real window aperture
BZ=3.05;GWZ,GWW,GWH=3.45,.55,.55
OB,OT,OH=GWZ-.09,GWZ+GWH+.09,(GWW+.18)/2;GC=gable_z(GW)-.05
def gx(z):return (RIDGE-.05-z)/KG     # half-width of the gable face at height z
for piece in ([(-GW,PLATE),(GW,PLATE),(GW,OB),(-GW,OB)],
              [(-GW,OB),(-OH,OB),(-OH,OT),(-gx(OT),OT),(-GW,GC)],[(OH,OB),(GW,OB),(GW,GC),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,RIDGE-.05)]):
    prism(G['plaster'],[(x,-D2+.15,z) for x,z in piece],(0,1,0),.3,M['plaster'])
# plinth course
for a0,a1 in ((-W2-.04,-1.2),(1.2,W2+.04)):G['stone'].box(((a0+a1)/2,-D2-.02,.2),(a1-a0,.1,.4),M['stone'],.01)
G['stone'].box((0,D2+.02,.2),(2*W2+.08,.1,.4),M['stone'],.01)
for s in (-1,1):G['stone'].box((s*(W2+.02),0,.2),(.1,2*D2,.4),M['stone'],.01)
# ---- timber framing ---------------------------------------------------------------
def post(x,y,z0,z1,o):G['timber'].box((x,y,(z0+z1)/2),(.2 if o=='y' else .12,.12 if o=='y' else .2,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
y=-D2-.03
for x in (-3.25,3.25):post(x,y,.4,PLATE,'y')
for x in (-GW,GW):post(x,y,.4,BZ-.1,'y')
for a,b in ((-W2,-GW),(GW,W2)):rail((a,y,PLATE-.08),(b,y,PLATE-.08),.18)
for s in (-1,1):
    rail((s*WX-.6,y,WZ+WH+.2),(s*WX+.6,y,WZ+WH+.2),.15)          # window lintels
    rail((s*3.17,y,PLATE-.6),(s*2.85,y,PLATE-.17),.12)            # corner braces
    rail((s*(GW+.08),y,PLATE-.6),(s*(GW+.4),y,PLATE-.17),.12)
# jettied bressumer and half-timbered gable
G['timber'].box((0,y-.04,BZ),(2*GW+.2,.2,.2),M['timber'],.015,grain=True,band=band())
for x in (-GW+.08,GW-.08):post(x,y,BZ+.1,gable_z(GW-.08)-.05,'y')
rail((-GW,y,OB-.08),(GW,y,OB-.08),.12);zc=OT+.12;rail((-gx(zc)+.05,y,zc),(gx(zc)-.05,y,zc),.14);post(0,y,zc+.07,RIDGE-.08,'y')
for s in (-1,1):rail((s*(GW-.1),y,BZ+.12),(s*(GW-.1)-s*.35,y,OB-.12),.1);rail((s*(GW-.12),y,OT-.25),(s*(gx(zc)-.12),y,zc-.06),.1)
# side and rear framing (inferred)
for s in (-1,1):
    x=s*(W2+.03)
    for yy in (-D2+.1,0,D2-.1):post(x,yy,.4,PLATE,'x')
    rail((x,-D2,PLATE-.08),(x,D2,PLATE-.08),.18)
    rail((x,-D2+.2,PLATE-.6),(x,-D2+.5,PLATE-.17),.12);rail((x,D2-.2,PLATE-.6),(x,D2-.5,PLATE-.17),.12)
y=D2+.03
for x in (-3.25,0,3.25):post(x,y,.4,PLATE,'y')
rail((-W2,y,PLATE-.08),(W2,y,PLATE-.08),.18)
for s in (-1,1):rail((s*1.8-.6,y,WZ+WH+.2),(s*1.8+.6,y,WZ+WH+.2),.15)
# ---- openings --------------------------------------------------------------------
for x in (-WX,WX):window(G,M,(x,-D2,WZ),(0,-1,0),WIN,WH,planter=True,band=band())
for x in (-1.8,1.8):window(G,M,(x,D2,WZ),(0,1,0),WIN,WH,band=band())
for s in (-1,1):window(G,M,(s*W2,s*.9,WZ),(s,0,0),WIN,WH,band=band())
window(G,M,(0,-D2-.02,GWZ),(0,-1,0),GWW,GWH,planter=True,band=band())
door(G,M,(0,-D2,STEP),(0,-1,0),DWD,DS,STONES,band=band())
# ---- porch on stone piers -----------------------------------------------------------
LP=math.hypot(PH,PR-PE)
plane((-PH,0,PE),(0,-1,0),(PH,0,PR-PE),LP,lambda t:D2,lambda t:-PY,[])
plane((PH,0,PE),(0,1,0),(-PH,0,PR-PE),LP,lambda t:PY,lambda t:-D2,[])
capped((0,PY,PR+.04),(0,-D2,PR+.04),5)
kp=(PR-PE)/PH
for s in (-1,1):G['timber'].beam((s*(PH+.04),PY-.03,PE-.07),(-s*.1,PY-.03,PR+.1*kp+.02),.07,.18,M['timber'],band=band())
finial(0,PY-.06,PR+.08,.16,.09)
PYP=PY+.25                      # pier line
for s in (-1,1):
    G['stone'].box((s*1.0,PYP,STEP+.14),(.46,.46,.28),M['ashlar'],.015,rect=kit.rng.choice(STONES))   # base block
    z=STEP+.28;i=0
    while z<PE-.12-.05:
        h=min(.28,PE-.12-z);G['stone'].box((s*1.0,PYP,z+h/2),(.38 if i%2 else .4,.38,h-.01),M['ashlar'],.012,rect=kit.rng.choice(STONES));z+=h;i+=1
    G['timber'].box((s*1.0,PYP,PE-.06),(.44,.44,.12),M['timber'],.012,grain=True,band=band())   # pier cap
    rail((s*1.0,PYP,PE+.06),(s*1.0,-D2,PE+.06),.14)
    rail((s*1.0,PYP-.05,PE),(s*.6,PYP-.05,PE+.32),.1)
rail((-1.12,PYP,PE+.08),(1.12,PYP,PE+.08),.16);post(0,PYP-.03,PE+.16,PR-.06,'y')
G['stone'].box((0,(PY-.05-D2)/2,.15),(2.5,abs(PY-.05+D2),.3),M['stone'],.015)
G['stone'].box((0,PY-.2,.075),(2.0,.3,.15),M['stone'],.015)
lantern(G,M,(0,PYP+.2,PR-.12))
for s in (-1,1):lantern(G,M,(s*(W2+.42),-D2+.3,2.05),arm=((s*(W2+.09),-D2+.3,2.05),(s,0,0)))
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
from mathutils import Matrix
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'model does not stand on the ground: zmin {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookCottage6'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a 60 MB GLB.
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookCottage6')  # door leaves: own meshes, hinged
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-9.5,-12.5,4.2),(82,0,-37),lens=38);cam('Front',(0,-30,3.0),(90,0,0),8.6);cam('Side',(-30,0,3.0),(90,0,-90),8.6);cam('Rear',(0,30,3.0),(90,0,180),8.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Cottage6-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'{tris} triangles exceeds the 120k house budget'
manifest=dict(asset='SilverbrookCottage6',reference=dict(file='Cottage6-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_cottage6.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookCottage6',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookCottage6.blend','SilverbrookCottage6.glb','SilverbrookCottage6.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('COTTAGE6_BUILT',manifest['bounds_m'],tris)
