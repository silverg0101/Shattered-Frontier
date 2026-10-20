"""Silverbrook Farmhouse from docs/Game-Design/concept-art/Silverbrook/Farmhouse.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_farmhouse.py
Original procedural geometry interpreting the elevation (12 m wide incl. the lean-to,
7 m to the chimney pots): long steep-hipped slate roof, off-centre half-timbered cross
gable, left dormer, gabled porch over an arched stone doorway, right ridge stack, open
slated lean-to on the right end, squared-rubble ground storey with oak framing.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,door,lantern
import bpy
scene=kit.reset(4112)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Farmhouse.png'
asset=bpy.data.collections.new('SilverbrookFarmhouse');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Farmhouse_Rubble','rough_block_wall'),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    shutter=kit.scan('SB_Farmhouse_Shutter','old_planks_02',dark=(.32,.30,.28),damp=False,aged=.45),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('rough_block_wall',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBFarmhouse_Masonry',asset),plaster=Geo('SM_SBFarmhouse_Plaster',asset),timber=Geo('SM_SBFarmhouse_Timber',asset),roof=Geo('SM_SBFarmhouse_Roof',asset),detail=Geo('SM_SBFarmhouse_Details',asset))
# ---- measured layout (metres; main house centred on x=0, recentred at export) ------
SLOT=(12,8,7)                  # concept README 'Fits' w x d x h
W2,D2=4.4,2.7                  # outer wall half-extents
EAVE,RIDGE=2.75,5.85           # main hip roof
EX,EY,RX=4.8,3.05,3.4          # eave half-extents, ridge half-length
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
XG,GH,GW,GE=1.7,1.6,1.05,4.3   # cross gable: centre x, roof half, wall half, eave on the main slope
KG=(RIDGE-GE)/GH;GY=-D2-.3
XD,DH,DW,DE,DR=-1.7,.95,.62,4.45,5.35   # dormer
YD=-(RIDGE-3.5)/KF;KD=(DR-DE)/DH
XC,YC,CX,CY=3.0,-.35,.95,.85   # ridge stack centre and plan size
XP,PE,PR,PH=.6,2.25,3.35,1.45  # porch centre x, eave, ridge, roof half
PY=-D2-1.0
LH,LL,LX,LY0,LY1=2.55,1.83,6.85,-D2-.35,1.3   # lean-to: wall head, eave z, eave x, y extent
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return RIDGE-abs(x-XG)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
in_gable=lambda p:abs(p.x-XG)<GW+.04 and p.y<0 and p.z<gable_z(p.x)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<CX/2+.02 and abs(p.y-YC)<CY/2+.02
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered,deck_cover=None):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],deck_cover if deck_cover is not None else covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
def post(x,y,z0,z1,o):G['timber'].box((x,y,(z0+z1)/2),(.2 if o=='y' else .12,.12 if o=='y' else .2,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
# ---- main hip roof --------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE);cut=[in_gable,in_dormer,in_stack]
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),cut)
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
capped((-RX,0,RIDGE+.05),(RX,0,RIDGE+.05),20)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),12)
for a,b in ((-EX,XG-GW),(XG+GW,EX)):G['timber'].beam((a,-EY-.02,EAVE-.06),(b,-EY-.02,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((-EX,EY+.02,EAVE-.06),(EX,EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- cross gable roof, bargeboards and finial ----------------------------------------
LG=math.hypot(GH,RIDGE-GE)
plane((XG-GH,0,GE),(0,-1,0),(GH,0,RIDGE-GE),LG,lambda t:0,lambda t:-GY,[under_main])
plane((XG+GH,0,GE),(0,1,0),(-GH,0,RIDGE-GE),LG,lambda t:GY,lambda t:0,[under_main])
capped((XG,GY,RIDGE+.05),(XG,-.4,RIDGE+.05),9)
for s in (-1,1):G['timber'].beam((XG+s*(GH+.05),GY-.04,GE-.08),(XG-s*.16,GY-.04,RIDGE+.16*KG+.03),.08,.24,M['timber'],band=band())
GC=gable_z(XG+GW)-.08         # gable wall head (clear of the boarding under the slates)
yq=-(RIDGE-GC)/KF
for s in (-1,1):G['plaster'].prism([(XG+s*GW,-D2,main_z(0,-D2)-.04),(XG+s*GW,-D2,GC),(XG+s*GW,yq,GC)],(1,0,0),.1,M['plaster'])
G['timber'].box((XG,GY-.06,RIDGE+.12),(.15,.15,.9),M['timber'],.02,grain=True,band=band())
G['timber'].box((XG,GY-.06,RIDGE+.595),(.2,.2,.05),M['timber'],.02,grain=True,band=band())
# ---- dormer -------------------------------------------------------------------
LD=math.hypot(DH,DR-DE)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:.9,lambda t:-YD+.16,[under_main])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-.9,[under_main])
capped((XD,YD-.16,DR+.04),(XD,-1.0,DR+.04),4)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
DB=main_z(XD,YD)-.1;DT=dormer_z(XD+DW)-.07
wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.84,.11,.91)],th=.12)
G['plaster'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,DT),(XD,YD+.06,DR-.07),(XD-DW,YD+.06,DT)],(0,1,0),.12,M['plaster'])
ym=-(RIDGE-DT)/KF
for s in (-1,1):G['plaster'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,DT),(XD+s*DW,ym,DT)],(1,0,0),.08,M['plaster'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
window(G,M,(XD,YD,DB+.2),(0,-1,0),.66,.62,shutters=False,wall_th=.12,band=band())
# ---- ridge stack (squared rubble, as the concept) -------------------------------------
G['stone'].box((XC,YC,5.45),(CX,CY,1.7),M['stone'],.01,w=(0,0))
G['stone'].box((XC,YC,6.3),(CX+.06,CY+.06,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,6.41),(CX+.12,CY+.12,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.22,.22):G['detail'].vessel((XC+dx,YC,6.47),[(.11,0),(.105,.3),(.115,.38),(.13,.42),(.10,.42),(.085,.36),(0,.36)],M['pot'])
# ---- walls ------------------------------------------------------------------------
PLATE=main_z(0,-D2)-.18       # clear of the roof boarding at the eave-side wall face
STEP,DS,DWD=.25,1.5,1.1        # door sill = porch landing top; arch spring above it
def holes(xs,w,z,h):return [(x,w+.18,z-.09,z+h+.09) for x in xs]
front_holes=holes([-2.4],1.0,1.0,.95)+[(XP,DWD,STEP,STEP+DS+DWD/2)]
wall(G['stone'],M['stone'],(0,-D2,0),(0,-1,0),W2,PLATE,front_holes,arch=(XP,DWD,STEP+DS))
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,PLATE,holes([-2.4,2.4],.8,1.0,.9))
for s in (-1,1):
    wall(G['stone'],M['stone'],(s*W2,0,0),(s,0,0),D2-.3,PLATE,holes([0],.8,1.0,.9),th=.3)
    # side wall head under the steep hip, kept below the roof boarding
    top=[(y/10,main_z(W2,y/10)-.2) for y in range(-round(D2*10),round(D2*10)+1) if main_z(W2,y/10)-.2>PLATE+.01]
    G['stone'].prism([(s*(W2-.15),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['stone'])
# gable face (plaster infill) above the front wall plate, with a window aperture
GWZ,GWW,GWH=3.85,.5,.62;OB,OT,OH=GWZ-.09,GWZ+GWH+.09,(GWW+.18)/2
for piece in ([(-GW,PLATE),(GW,PLATE),(GW,OB),(-GW,OB)],[(-GW,OB),(-OH,OB),(-OH,OT),(-GW,OT)],[(OH,OB),(GW,OB),(GW,OT),(OH,OT)],
              [(-GW,OT),(GW,OT),(GW,GC),(0,RIDGE-.08),(-GW,GC)]):
    G['plaster'].prism([(XG+x,-D2+.15,z) for x,z in piece],(0,1,0),.3,M['plaster'])
# ---- timber framing ---------------------------------------------------------------
y=-D2-.03;RAIL=2.35
for x in (-W2+.1,2.7,W2-.1):post(x,y,0,PLATE,'y')
rail((-W2,y,RAIL),(W2,y,RAIL));rail((-W2,y,PLATE-.09),(XG-GW,y,PLATE-.09),.18);rail((XG+GW,y,PLATE-.09),(W2,y,PLATE-.09),.18)
for x in [-W2+.9+.8*i for i in range(10)]:
    if abs(x-XP)>PH+.15 and abs(x-2.7)>.3 and x<W2-.4:post(x,y,RAIL+.08,PLATE-.18,'y')
for s in (-1,1):rail((s*(W2-.1),y,1.55),(s*(W2-.65),y,RAIL-.07),.12)
# jettied bressumer and half-timbered gable
G['timber'].box((XG,y-.04,PLATE+.06),(2*GW+.2,.2,.2),M['timber'],.015,grain=True,band=band())
for x in (XG-GW+.08,XG+GW-.08):post(x,y,PLATE+.16,GC-.04,'y')
rail((XG-GW,y,OB-.08),(XG+GW,y,OB-.08),.12);rail((XG-GW+.1,y,OT+.08),(XG+GW-.1,y,OT+.08),.14);post(XG,y,OT+.15,RIDGE-.12,'y')
for s in (-1,1):
    G['timber'].beam((XG+s*OH,y,OB-.02),(XG+s*OH,y,OT+.02),.1,.1,M['timber'],band=band())
    rail((XG+s*(GW-.12),y,OT+.15),(XG+s*.15,y,RIDGE-.55),.1)
G['detail'].box((XG,-D2-.04,3.4),(.62,.05,.42),M['door'],.006,swap=True)   # boarded loft hatch (inferred purpose)
for z in (3.27,3.53):G['detail'].box((XG,-D2-.07,z),(.5,.012,.04),M['iron'],.004)
# side and rear framing
for s in (-1,1):
    x=s*(W2+.03)
    for yy in (-D2+.1,D2-.1):post(x,yy,0,PLATE,'x')
    rail((x,-D2,RAIL),(x,D2,RAIL));rail((x,-D2,PLATE-.09),(x,D2,PLATE-.09),.18)
    rail((x,-D2+.2,1.55),(x,-D2+.75,RAIL-.07),.12);rail((x,D2-.2,1.55),(x,D2-.75,RAIL-.07),.12)
y=D2+.03
for x in (-W2+.1,0,W2-.1):post(x,y,0,PLATE,'y')
rail((-W2,y,RAIL),(W2,y,RAIL));rail((-W2,y,PLATE-.09),(W2,y,PLATE-.09),.18)
# ---- openings --------------------------------------------------------------------
window(G,M,(-2.4,-D2,1.0),(0,-1,0),1.0,.95,planter=True,band=band())
for x in (-2.4,2.4):window(G,M,(x,D2,1.0),(0,1,0),.8,.9,band=band())
for s in (-1,1):window(G,M,(s*W2,0,1.0),(s,0,0),.8,.9,band=band())
window(G,M,(XG,-D2-.02,GWZ),(0,-1,0),GWW,GWH,band=band())
door(G,M,(XP,-D2,STEP),(0,-1,0),DWD,DS,STONES,band=band())
# ---- porch ------------------------------------------------------------------------
LP=math.hypot(PH,PR-PE);kp=(PR-PE)/PH
plane((XP-PH,0,PE),(0,-1,0),(PH,0,PR-PE),LP,lambda t:D2,lambda t:-PY,[])
plane((XP+PH,0,PE),(0,1,0),(-PH,0,PR-PE),LP,lambda t:PY,lambda t:-D2,[])
capped((XP,PY,PR+.04),(XP,-D2,PR+.04),5)
for s in (-1,1):G['timber'].beam((XP+s*(PH+.04),PY-.03,PE-.07),(XP-s*.1,PY-.03,PR+.1*kp+.02),.07,.18,M['timber'],band=band())
for s in (-1,1):
    post(XP+s*1.15,PY+.15,STEP,PE-.02,'y');rail((XP+s*1.15,PY+.15,PE),(XP+s*1.15,-D2,PE),.16)
    rail((XP+s*1.15,PY+.15,1.75),(XP+s*.75,PY+.15,PE-.03),.1);rail((XP+s*1.15,PY+.15,1.75),(XP+s*1.15,PY+.5,PE-.03),.1)
rail((XP-1.27,PY+.15,PE+.02),(XP+1.27,PY+.15,PE+.02),.18);post(XP,PY+.12,PE+.1,PR-.06,'y')
G['stone'].box((XP,(PY+.05-D2)/2,STEP/2),(2.7,abs(PY+.05+D2),STEP),M['stone'],.015)
G['stone'].box((XP,PY-.08,STEP/4),(1.9,.3,STEP/2),M['stone'],.015)
for s in (-1,1):G['stone'].box((XP+s*1.15,PY+.15,STEP+.12),(.3,.3,.24),M['ashlar'],.012,rect=kit.rng.choice(STONES))   # post pads
lantern(G,M,(XP,PY+.55,PR-.12))
for x in (-1.05,2.25,3.6):lantern(G,M,(x,-D2-.42,2.05),arm=((x,-D2-.09,2.05),(0,-1,0)))
# ---- open lean-to on the right end (concept: slated pent roof on oak posts) ----------
LK=(LH-LL)/(LX-W2)
def lean_z(x):return LH-(x-W2)*LK
LPX=LX-.3
G['timber'].box((W2+.07,(LY0+LY1)/2,LH-.14),(.14,LY1-LY0,.16),M['timber'],.012,grain=True,band=band())   # ledger on the wall
for yy in (LY0+.2,(LY0+LY1)/2,LY1-.2):
    post(LPX,yy,0,lean_z(LPX)-.2,'x')
    G['timber'].beam((W2+.14,yy,LH-.14),(LX,yy,lean_z(LX)-.1),.1,.14,M['timber'],band=band())   # rafter-tie
G['timber'].box((LPX,(LY0+LY1)/2,lean_z(LPX)-.12),(.16,LY1-LY0,.16),M['timber'],.012,grain=True,band=band())
for yy,d in ((LY0+.2,1),(LY1-.2,-1),((LY0+LY1)/2,1),((LY0+LY1)/2,-1)):
    rail((LPX,yy,lean_z(LPX)-.75),(LPX,yy+d*.55,lean_z(LPX)-.2),.1)
LLn=math.hypot(LX-W2-.02,LH-LL)
plane((LX,0,LL),(0,1,0),(W2+.02-LX,0,LH-LL),LLn,lambda t:LY0,lambda t:LY1,[])
G['timber'].beam((LX+.02,LY0,LL-.06),(LX+.02,LY1,LL-.06),.04,.18,M['timber'],band=band())
for yy in (LY0-.02,LY1+.02):G['timber'].beam((W2+.05,yy,LH-.02),(LX,yy,LL-.04),.04,.16,M['timber'],band=band())
tie=lambda x:LH-.14+(x-W2-.14)*((lean_z(LX)-.1)-(LH-.14))/(LX-W2-.14)
lantern(G,M,(5.6,(LY0+LY1)/2,tie(5.6)-.08))   # hangs from the middle rafter-tie
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
# Wall heads must stay under the hip/front roof boarding (no stone or plaster poking through).
def deck_under(x,y):
    k=KF if RIDGE-abs(y)*KF<=RIDGE-max(0,abs(x)-RX)*KS else KS
    return main_z(x,y)-.035*math.sqrt(1+k*k)
def exempt(p):return (abs(p.x-XG)<GH+.15 and p.y<-.3) or (abs(p.x-XD)<DH+.1 and p.y<0) or (abs(p.x-XC)<CX/2+.1 and abs(p.y-YC)<CY/2+.1)
bad=[(round(p.x,2),round(p.y,2),round(p.z-deck_under(p.x,p.y),3)) for o in objs if o.name.split('_')[-1] in ('Masonry','Plaster','Timber')
     for p in (o.matrix_world@v.co for v in o.data.vertices) if abs(p.x)<=W2+.1 and abs(p.y)<=D2+.1 and not exempt(p) and p.z>deck_under(p.x,p.y)-.005]
assert not bad,f'wall tops poke through the roof: {bad[:8]}'
from mathutils import Matrix
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookFarmhouse'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookFarmhouse')  # door leaves: own meshes, hinged
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-13,-17.5,5.2),(80,0,-37),lens=38);cam('Front',(0,-30,3.6),(90,0,0),12.8);cam('Side',(-30,0,3.6),(90,0,-90),12.8);cam('Rear',(0,30,3.6),(90,0,180),12.8)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Farmhouse-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'{tris} triangles exceeds the house budget'
manifest=dict(asset='SilverbrookFarmhouse',reference=dict(file='Farmhouse-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_farmhouse.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookFarmhouse',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookFarmhouse.blend','SilverbrookFarmhouse.glb','SilverbrookFarmhouse.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('FARMHOUSE_BUILT',manifest['bounds_m'],tris)
