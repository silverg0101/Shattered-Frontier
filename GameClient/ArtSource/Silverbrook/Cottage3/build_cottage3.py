"""Silverbrook Cottage 3 from docs/Game-Design/concept-art/Silverbrook/Cottage-3.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cottage3.py
Original procedural geometry interpreting the elevation (8 m wide, 6 m to the gable
finial): a fully half-timbered cream-plaster cottage on a squared-rubble plinth, under a
steep-hipped slate roof. An off-centre front cross gable rises just above the main ridge
(its back is hipped into the rear slope) over a gabled porch on stone piers and an arched
plank door in a timber surround; right dormer, rubble ridge stack with two pots.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,tile_plane,deck,wall,window,lantern,frame
import bpy
scene=kit.reset(4103)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Cottage-3.png'
asset=bpy.data.collections.new('SilverbrookCottage3');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Cottage3_Rubble','rough_block_wall'),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    shutter=kit.scan('SB_Cottage3_Shutter','wood_planks_grey',dark=(.30,.32,.36),damp=False,aged=.3),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75))
M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBCottage3_Masonry',asset),plaster=Geo('SM_SBCottage3_Plaster',asset),timber=Geo('SM_SBCottage3_Timber',asset),roof=Geo('SM_SBCottage3_Roof',asset),detail=Geo('SM_SBCottage3_Details',asset))
# ---- measured layout (metres; concept ~114 px/m across, ~120 px/m up) ---------------
SLOT=(8,6,6)                   # town-layout slot w x d x h (concept README 'Fits')
W2,D2=3.3,2.3                  # outer wall half-extents
EAVE,RIDGE=2.45,5.35           # main hip roof
EX,EY,RX=3.75,2.6,2.95         # eave half-extents, ridge half-length
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
XG=-.4                         # the gable/porch/door axis sits left of the wall centre in the concept
GH,GW,GE,GR=1.7,1.3,3.45,5.65  # cross gable: roof half, wall half, eave, ridge (0.3 m above the main ridge)
KG=(GR-GE)/GH;KH=3.0;GY=-D2-.3 # KH: slope of the small hip that closes the gable's back
XD,DH,DW,DE,DR=1.98,.6,.44,4.05,4.85  # dormer
YD=-(RIDGE-3.1)/KF
KD=(DR-DE)/DH
XC,YC=2.65,-.3                 # ridge stack
PE,PR,PH,PY=2.2,3.0,1.25,-D2-.65  # porch eave, ridge, roof half, front
PB=.58                         # rubble plinth height
STEP,DS,DWD=.33,1.25,.95       # door sill = porch landing top; arch spring above it; door width
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def wing_z(x,y):return GR-max(abs(x-XG)*KG,max(y,0)*KH)
def gable_z(dx):return GR-abs(dx)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
in_wing=lambda p:abs(p.x-XG)<GW+.04 and p.y>GY-.05 and p.z<wing_z(p.x,p.y)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.47 and abs(p.y-YC)<.42
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
# ---- main hip roof --------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_wing,in_dormer,in_stack])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_wing,in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
capped((-RX,0,RIDGE+.05),(XG-.28,0,RIDGE+.05),10);capped((XG+.28,0,RIDGE+.05),(XC-.45,0,RIDGE+.05),5)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),11)
for sx in (-1,1):G['timber'].box((sx*RX,0,RIDGE+.2),(.13,.13,.36),M['timber'],.015,grain=True,band=band())   # ridge-end stubs
for a,b in ((-EX,XG-GH),(XG+GH,EX)):G['timber'].beam((a,-EY-.02,EAVE-.06),(b,-EY-.02,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((-EX,EY+.02,EAVE-.06),(EX,EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- cross gable: two front slopes, a steep back hip, bargeboards and finial ------------
LG=math.hypot(GH,GR-GE);YH=GH*KG/KH;LH=math.hypot(YH,GR-GE)
def hip_y(t):return GH*(1-t/LG)*KG/KH          # where the side slope meets the back hip, at slope distance t
plane((XG-GH,0,GE),(0,-1,0),(GH,0,GR-GE),LG,lambda t:-hip_y(t),lambda t:-GY,[under_main])
plane((XG+GH,0,GE),(0,1,0),(-GH,0,GR-GE),LG,lambda t:GY,hip_y,[under_main])
plane((XG,YH,GE),(-1,0,0),(0,-YH,GR-GE),LH,lambda t:-GH*(1-t/LH),lambda t:GH*(1-t/LH),[under_main])
capped((XG,GY,GR+.05),(XG,0,GR+.05),9)
for s in (-1,1):cap((XG,0,GR+.05),(XG+s*.42,.42*KG/KH,GR+.05-.42*KG))
for s in (-1,1):G['timber'].beam((XG+s*(GH+.05),GY-.04,GE-.08),(XG-s*.16,GY-.04,GR+.16*KG+.03),.08,.24,M['timber'],band=band())
G['timber'].box((XG,GY-.06,5.55),(.15,.15,.8),M['timber'],.02,grain=True,band=band())
G['timber'].box((XG,GY-.06,5.955),(.2,.2,.05),M['timber'],.02,grain=True,band=band())
# cheek walls close the gable's eaves down to the main slope behind the gable face
GC=gable_z(GW)-.05;yq=-(RIDGE-GC)/KF
for s in (-1,1):G['plaster'].prism([(XG+s*GW,-D2+.3,main_z(XG+s*GW,-D2+.3)-.04),(XG+s*GW,-D2+.3,GC),(XG+s*GW,yq,GC)],(1,0,0),.1,M['plaster'])
# ---- dormer -------------------------------------------------------------------
LD=math.hypot(DH,DR-DE)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:.4,lambda t:-YD+.16,[under_main])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-.4,[under_main])
capped((XD,YD-.16,DR+.04),(XD,-.5,DR+.04),4)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
G['timber'].box((XD,YD-.2,DR+.15),(.1,.1,.3),M['timber'],.01,grain=True,band=band())
DB=main_z(XD,YD)-.1
wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.6,.16,.76)],th=.12)
G['plaster'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dormer_z(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dormer_z(XD+DW)-.04)],(0,1,0),.12,M['plaster'])
ym=-(RIDGE-(dormer_z(XD+DW)-.04))/KF
for s in (-1,1):G['plaster'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,dormer_z(XD+DW)-.04),(XD+s*DW,ym,dormer_z(XD+DW)-.04)],(1,0,0),.08,M['plaster'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
window(G,M,(XD,YD,DB+.25),(0,-1,0),.42,.42,shutters=False,planter=True,wall_th=.12,band=band())
# ---- ridge stack: squared rubble with a projecting cap course ------------------------
G['stone'].box((XC,YC,4.17),(.86,.76,2.4),M['stone'],.01)
G['stone'].box((XC,YC,5.435),(.98,.88,.13),M['stone'],.015,w=(.45,0))
for dx in (-.2,.2):G['detail'].vessel((XC+dx,YC,5.5),[(.11,0),(.105,.34),(.115,.4),(.13,.45),(.10,.45),(.085,.39),(0,.39)],M['pot'])
# ---- walls: rubble plinth, plaster panels above ------------------------------------
PLATE=main_z(0,-D2)-.04
WZ,WH=1.15,.7
def holes(specs):return [(a,w+.18,WZ-.09-PB,WZ+WH+.09-PB) for a,w in specs]
FRONT=[(-2.44,.54),(2.1,.6)];REAR=[(-2.0,.62),(2.0,.62)];SIDE=(1.3,.6)
door_hole=(XG,DWD,0,STEP+DS+DWD/2-PB)
wall(G['stone'],M['stone'],(0,-D2-.06,0),(0,-1,0),W2+.06,PB,[(XG,DWD,STEP,PB+1)],th=.36)
wall(G['stone'],M['stone'],(0,D2+.06,0),(0,1,0),W2+.06,PB,th=.36)
for s in (-1,1):wall(G['stone'],M['stone'],(s*(W2+.06),0,0),(s,0,0),D2-.3,PB,th=.36)
wall(G['plaster'],M['plaster'],(0,-D2,PB),(0,-1,0),W2,PLATE-PB,holes(FRONT)+[door_hole],arch=(XG,DWD,STEP+DS-PB))
wall(G['plaster'],M['plaster'],(0,D2,PB),(0,1,0),W2,PLATE-PB,holes(REAR))
for s in (-1,1):
    wall(G['plaster'],M['plaster'],(s*W2,0,PB),(s,0,0),D2-.3,PLATE-PB,holes([SIDE]))
    top=[(y/10,main_z(W2,y/10)-.05) for y in range(-round(D2*10),round(D2*10)+1) if main_z(W2,y/10)-.05>PLATE+.01]
    G['plaster'].prism([(s*(W2-.15),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['plaster'])
# gable face (plaster infill) above the wall plate, with a real window aperture
GWZ,GWW,GWH=3.4,.6,.75;OB,OT,OH=GWZ-.09,GWZ+GWH+.09,(GWW+.18)/2
def gx(z):return (GR-.05-z)/KG     # half-width of the gable face at height z
for piece in ([(-GW,PLATE),(GW,PLATE),(GW,OB),(-GW,OB)],
              [(-GW,OB),(-OH,OB),(-OH,OT),(-gx(OT),OT),(-GW,GC)],[(OH,OB),(GW,OB),(GW,GC),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,GR-.05)]):
    G['plaster'].prism([(XG+x,-D2+.15,z) for x,z in piece],(0,1,0),.3,M['plaster'])
# ---- timber framing ---------------------------------------------------------------
def post(x,y,z0,z1,o='y',w=.2):G['timber'].box((x,y,(z0+z1)/2),(w if o=='y' else .12,.12 if o=='y' else w,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
y=-D2-.03;GP=GW-.08              # gable posts run down to the plinth beside the porch
for x in (-3.2,3.2):post(x,y,PB,PLATE)
for x in (XG-GP,XG+GP):post(x,y,PB,PLATE,'y',.14);post(x,y,PLATE,gable_z(GP)-.05,'y',.14)
for a,b in ((-W2,XG-DWD/2-.25),(XG+DWD/2+.25,W2)):rail((a,y,PB+.06),(b,y,PB+.06))
for a,b in ((-W2,XG-GP-.07),(XG+GP+.07,W2)):rail((a,y,2.08),(b,y,2.08),.14)
rail((-W2,y,PLATE-.08),(W2,y,PLATE-.08),.18)
for s in (-1,1):rail((s*3.12,y,2.18),(s*2.72,y,PLATE-.2),.11)       # upper-panel braces
rail((-1.75,y,2.18),(-2.12,y,PLATE-.2),.11);rail((.95,y,2.18),(1.32,y,PLATE-.2),.11)
rail((.98,y,PB+.16),(1.24,y,2.0),.1)                                    # long brace beside the right window
# half-timbered gable: sill rail, collar, king post, struts
rail((XG-GW,y,GWZ-.17),(XG+GW,y,GWZ-.17),.12);zc=OT+.12;rail((XG-gx(zc)+.05,y,zc),(XG+gx(zc)-.05,y,zc),.14);post(XG,y,zc+.07,GR-.08,'y')
for s in (-1,1):
    rail((XG+s*(GW-.12),y,PLATE+.06),(XG+s*.5,y,GWZ-.24),.1)
    rail((XG+s*.06,y,zc+.1),(XG+s*(gx(zc+.45)-.12),y,zc+.45),.09)
# side and rear framing
for s in (-1,1):
    x=s*(W2+.03)
    for yy in (-D2+.1,0,D2-.1):post(x,yy,PB,PLATE,'x')
    rail((x,-D2,PB+.06),(x,D2,PB+.06));rail((x,-D2,2.08),(x,D2,2.08),.14)
    rail((x,-D2+.2,2.18),(x,-D2+.55,PLATE-.2),.11);rail((x,D2-.2,2.18),(x,D2-.55,PLATE-.2),.11)
y=D2+.03
for x in (-3.2,-.6,.6,3.2):post(x,y,PB,PLATE)
rail((-W2,y,PB+.06),(W2,y,PB+.06));rail((-W2,y,2.08),(W2,y,2.08),.14);rail((-W2,y,PLATE-.08),(W2,y,PLATE-.08),.18)
# ---- openings --------------------------------------------------------------------
for x,w in FRONT:window(G,M,(x,-D2,WZ),(0,-1,0),w,WH,planter=True,band=band())
for x,w in REAR:window(G,M,(x,D2,WZ),(0,1,0),w,WH,band=band())
for s in (-1,1):window(G,M,(s*W2,s*SIDE[0],WZ),(s,0,0),SIDE[1],WH,band=band())
window(G,M,(XG,-D2-.02,GWZ),(0,-1,0),GWW,GWH,planter=True,band=band())
def plank_door(c,w,spring):
    """Round-headed ledged plank door in a timber jamb-and-arch surround (the concept has no stone dressing)."""
    P,R=frame(c,(0,-1,0));rad=w/2
    # The leaf (boards, straps, ring) is its own mesh so it can open; kit.split_doors hinges it on the left jamb (-R x).
    D=G['door1']=Geo('SM_SBCottage3_Door1',asset);kit.DOORS.append(dict(geo=D,r=R@Vector((1,0,0)),o=Vector((0,-1,0))))
    D.prism([P(-rad,-.2,0),P(rad,-.2,0)]+[P(rad*math.cos(t),-.2,spring+rad*math.sin(t)) for t in kit.np.linspace(0,math.pi,17)],(0,-1,0),.07,M['door'])
    for z in (.3,spring-.12):
        D.box(P(-rad*.1,-.15,z),(w*.85,.012,.06),M['iron'],.004,R)
        D.prism([P(rad*.32,-.144,z-.06),P(rad*.32,-.144,z+.06),P(rad*.5,-.144,z)],(0,-1,0),.012,M['iron'])
    for k in range(10):
        t=math.tau*k/10;D.box(P(rad*.55+.06*math.cos(t),-.13,.95+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
    D.box(P(rad*.55,-.155,1.01),(.07,.01,.07),M['iron'],.004,R)
    for side in (-1,1):G['timber'].box(P(side*(rad+.06),-.1,spring/2),(.16,.3,spring),M['timber'],.012,R,grain=True,band=band())
    n=5;depth=.16
    for k in range(n):
        t=math.pi*(k+.5)/n;rm=rad-.02+depth/2;radial=R@Vector((math.cos(t),0,math.sin(t)));tang=R@Vector((-math.sin(t),0,math.cos(t)))
        G['timber'].box(P(rm*math.cos(t),-.1,spring+rm*math.sin(t)),(math.pi*rm/n-.01,.3,depth),M['timber'],.012,Matrix((tang,Vector((0,-1,0)),radial)).transposed(),grain=True,band=band())
plank_door((XG,-D2,STEP),DWD,DS)
# ---- porch on rubble piers ------------------------------------------------------------
LP=math.hypot(PH,PR-PE)
plane((XG-PH,0,PE),(0,-1,0),(PH,0,PR-PE),LP,lambda t:D2,lambda t:-PY,[])
plane((XG+PH,0,PE),(0,1,0),(-PH,0,PR-PE),LP,lambda t:PY,lambda t:-D2,[])
capped((XG,PY,PR+.04),(XG,-D2,PR+.04),5)
kp=(PR-PE)/PH
for s in (-1,1):G['timber'].beam((XG+s*(PH+.04),PY-.03,PE-.07),(XG-s*.1,PY-.03,PR+.1*kp+.02),.07,.18,M['timber'],band=band())
PIER=.45;py=PY+.15
for s in (-1,1):
    G['stone'].box((XG+s*.95,py,STEP+PIER/2),(.38,.38,PIER),M['stone'],.015)
    post(XG+s*.95,py,STEP+PIER,PE-.02,'y',.16);rail((XG+s*.95,py,PE),(XG+s*.95,-D2-.09,PE),.16)
    rail((XG+s*.95,py,1.75),(XG+s*.6,py,PE-.03),.1);rail((XG+s*.95,py,1.75),(XG+s*.95,py+.35,PE-.03),.1)
rail((XG-1.07,py,PE+.02),(XG+1.07,py,PE+.02),.18);post(XG,py-.03,PE+.1,PR-.06,'y')
for s in (-1,1):rail((XG,py-.03,PR-.25),(XG+s*.55,py-.03,PE+.12),.09)
G['stone'].box((XG,(PY-.05-D2-.06)/2,STEP/2),(2.3,abs(PY-.05+D2+.06),STEP),M['stone'],.015)   # landing stops at the plinth face
G['stone'].box((XG,PY-.05-.13,STEP/4),(1.9,.26,STEP/2),M['stone'],.015)
lantern(G,M,(XG,PY+.55,PR-.12))
lantern(G,M,(W2+.45,-D2+.45,2.1),arm=((W2+.04,-D2+.45,2.1),(1,0,0)))
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
# Origin at the slot's ground centre (the gable axis and porch are off the wall centre).
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert lo[2]>=-1e-4,f'below ground: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookCottage3'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a 60 MB GLB.
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookCottage3')  # door leaf: own mesh, hinged, left out of the main FBX
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-9.5,-12.5,4.2),(82,0,-37),lens=38);cam('Front',(0,-30,3.15),(90,0,0),8.6);cam('Side',(-30,0,3.15),(90,0,-90),8.6);cam('Rear',(0,30,3.15),(90,0,180),8.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Cottage3-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'house triangle budget: {tris}'
manifest=dict(asset='SilverbrookCottage3',reference=dict(file='Cottage3-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_cottage3.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookCottage3',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookCottage3.blend','SilverbrookCottage3.glb','SilverbrookCottage3.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('COTTAGE3_BUILT',manifest['bounds_m'],tris)
