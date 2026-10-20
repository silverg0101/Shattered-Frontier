"""Silverbrook Workshop from docs/Game-Design/concept-art/Silverbrook/Workshop.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_workshop.py
Original procedural geometry interpreting the elevation (8 m wide incl. sign and lean-to,
6 m to the chimney pot): steep-hipped slate roof, left half-timbered cross gable over a wide
workshop bay (closed board doors), right dormer, rear ridge stack, rubble-stone ground
storey on timber framing and stone piers, slate lean-to on the right, hanging trade sign.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,lantern
from mathutils import Matrix
import bpy
scene=kit.reset(4108)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Workshop.png'
asset=bpy.data.collections.new('SilverbrookWorkshop');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    rubble=kit.scan('SB_Workshop_Rubble','stone_wall'),
    block=kit.scan('SB_Workshop_BlockStone','rough_block_wall',dark=(.36,.36,.30)),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    shutter=kit.scan('SB_Workshop_Shutter','old_planks_02',dark=(.32,.30,.28),damp=False,aged=.45),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75))
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBWorkshop_Masonry',asset),plaster=Geo('SM_SBWorkshop_Plaster',asset),timber=Geo('SM_SBWorkshop_Timber',asset),roof=Geo('SM_SBWorkshop_Roof',asset),detail=Geo('SM_SBWorkshop_Details',asset))
# ---- measured layout (metres) -------------------------------------------------
SLOT=(8,6,6)                   # town-layout slot w x d x h (concept README 'Fits')
W2,D2=2.75,2.3                 # outer wall half-extents
EAVE,RIDGE=2.7,5.3             # main hip roof
EX,EY,RX=3.15,2.6,1.9          # eave half-extents, ridge half-length
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
XG,GH,GW,GE=-1.3,1.65,1.45,2.95   # left cross gable: centre, roof half, wall half, eave
KG=(RIDGE-GE)/GH;GY=-D2-.3
XD,DH,DW,DE,DR=1.25,.62,.5,3.95,4.4   # dormer
YD=-(RIDGE-3.2)/KF
KD=(DR-DE)/DH
XC,YC=2.1,.4                   # ridge stack (behind the ridge, at the right hip)
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return RIDGE-abs(x-XG)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
in_gable=lambda p:abs(p.x-XG)<GW+.04 and p.y<0 and p.z<gable_z(p.x)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.42 and abs(p.y-YC)<.37
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
def post(x,y,z0,z1,o,w=.2):G['timber'].box((x,y,(z0+z1)/2),(w if o=='y' else .12,.12 if o=='y' else w,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
def pier(x,y,sx,sy,h=.56):G['stone'].box((x,y,h/2),(sx,sy,h),M['block'],.02)
# ---- main hip roof --------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_dormer])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
capped((-RX,0,RIDGE+.05),(RX,0,RIDGE+.05),14)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),10)
for a,b in ((-EX,XG-GW),(XG+GW,EX)):G['timber'].beam((a,-EY-.02,EAVE-.06),(b,-EY-.02,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((-EX,EY+.02,EAVE-.06),(EX,EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- cross gable roof, bargeboards and finial ----------------------------------------
# The gable roof is shallower than the hips, so behind the front slope it runs under the main roof.
LG=math.hypot(GH,RIDGE-GE)
plane((XG-GH,0,GE),(0,-1,0),(GH,0,RIDGE-GE),LG,lambda t:0,lambda t:-GY,[under_main])
plane((XG+GH,0,GE),(0,1,0),(-GH,0,RIDGE-GE),LG,lambda t:GY,lambda t:0,[under_main])
capped((XG,GY,RIDGE+.05),(XG,-.4,RIDGE+.05),9)
for s in (-1,1):G['timber'].beam((XG+s*(GH+.05),GY-.04,GE-.08),(XG-s*.16,GY-.04,RIDGE+.16*KG+.03),.08,.24,M['timber'],band=band())
G['timber'].box((XG,GY-.06,5.5),(.16,.16,.75),M['timber'],.02,grain=True,band=band())
G['timber'].box((XG,GY-.06,5.9),(.22,.22,.06),M['timber'],.02,grain=True,band=band())
# ---- dormer -------------------------------------------------------------------
LD=math.hypot(DH,DR-DE);YR=-(RIDGE-DR)/KF   # dormer ridge meets the main slope at YR
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:-YR+.1,lambda t:-YD+.16,[under_main])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:YR-.1,[under_main])
capped((XD,YD-.16,DR+.04),(XD,YR,DR+.04),4)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
DB=main_z(XD,YD)-.1
wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.58,.13,.71)],th=.12)
G['plaster'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dormer_z(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dormer_z(XD+DW)-.04)],(0,1,0),.12,M['plaster'])
ym=-(RIDGE-(dormer_z(XD+DW)-.04))/KF
for s in (-1,1):G['plaster'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,dormer_z(XD+DW)-.04),(XD+s*DW,ym,dormer_z(XD+DW)-.04)],(1,0,0),.08,M['plaster'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
window(G,M,(XD,YD,DB+.22),(0,-1,0),.4,.4,shutters=False,wall_th=.12,band=band())
# ---- ridge stack ------------------------------------------------------------------
G['stone'].box((XC,YC,4.4),(.8,.7,2.2),M['block'],.01)
G['stone'].box((XC,YC,5.2),(.86,.76,.1),M['block'],.015)
G['stone'].box((XC,YC,5.55),(.92,.82,.1),M['block'],.015,w=(.45,0))
G['detail'].vessel((XC,YC,5.6),[(.11,0),(.105,.24),(.115,.3),(.13,.34),(.10,.34),(.085,.29),(0,.29)],M['pot'])
# ---- walls ------------------------------------------------------------------------
PLATE=main_z(0,-D2)-.04
WIN=.7;WH=.85;WZ=1.05;WO=(WIN+.18,WZ-.09,WZ+WH+.09)
BAY=(-2.35,-.08,2.15)          # workshop bay: left, right, head
wall(G['stone'],M['rubble'],(0,-D2,0),(0,-1,0),W2,PLATE,[((BAY[0]+BAY[1])/2,BAY[1]-BAY[0],0,BAY[2]),(1.4,WO[0],WO[1],WO[2])])
wall(G['stone'],M['rubble'],(0,D2,0),(0,1,0),W2,PLATE,[(-1.3,WO[0],WO[1],WO[2]),(1.3,WO[0],WO[1],WO[2])])
wall(G['stone'],M['rubble'],(-W2,0,0),(-1,0,0),D2-.3,PLATE,[(-1.1,WO[0],WO[1],WO[2])])
wall(G['stone'],M['rubble'],(W2,0,0),(1,0,0),D2-.3,PLATE)
for s in (-1,1):   # hip-end infill under the roof; the left one starts behind the gable face
    y0=-D2+.3 if s<0 else -D2
    top=[(y/10,main_z(W2,y/10)-.05) for y in range(round(y0*10),round(D2*10)+1) if main_z(W2,y/10)-.05>PLATE+.01]
    G['plaster'].prism([(s*(W2-.15),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['plaster'])
# gable face (plaster) above the wall plate, clipped to the gable roof, with a window aperture
GWZ,GWW,GWH=3.55,.55,.6;OB,OT,OH=GWZ-.09,GWZ+GWH+.09,(GWW+.18)/2;GC=gable_z(XG+GW)-.05
def gx(z):return min(GW,(RIDGE-.05-z)/KG)   # half-width of the gable face at height z
def band_poly(xa,xb,z0,z1):
    zs=sorted({z0,z1}|({GC} if z0<GC<z1 else set()))
    pts=[(min(xb,XG+gx(z)),z) for z in zs]+[(max(xa,XG-gx(z)),z) for z in reversed(zs)];out=[]
    for p in pts:
        if not out or abs(p[0]-out[-1][0])+abs(p[1]-out[-1][1])>1e-6:out.append(p)
    if abs(out[0][0]-out[-1][0])+abs(out[0][1]-out[-1][1])<1e-6:out.pop()
    return out
for piece in (band_poly(-9,9,PLATE,OB),band_poly(-9,XG-OH,OB,OT),band_poly(XG+OH,9,OB,OT),band_poly(-9,9,OT,RIDGE-.05)):
    G['plaster'].prism([(x,-D2+.15,z) for x,z in piece],(0,1,0),.3,M['plaster'])
assert gx(OT)>OH+.15
# ---- timber framing and stone piers -------------------------------------------------
y=-D2-.03;HEAD=PLATE-.2      # keep face timber under the eave deck
G['timber'].box((XG,y-.06,PLATE+.02),(2*GW+.06,.2,.22),M['timber'],.015,grain=True,band=band())   # bressumer
pier(-2.57,-D2+.03,.44,.36);pier(.11,-D2+.03,.38,.36);pier(2.57,-D2+.03,.44,.36)
post(-2.55,y,.56,PLATE-.09,'y',.4);post(.05,y,.56,PLATE-.09,'y',.26);post(2.6,y,.56,HEAD+.09,'y',.3)
G['timber'].box(((BAY[0]+BAY[1])/2-.02,y-.02,BAY[2]+.11),(BAY[1]-BAY[0]+.3,.16,.22),M['timber'],.015,grain=True,band=band())  # bay lintel
for s,x in ((1,BAY[0]),(-1,BAY[1])):rail((x+s*.02,y-.06,1.7),(x+s*.45,y-.06,BAY[2]),.12)    # knee braces
for z in (.62,2.1):rail((.18,y,z),(2.45,y,z),.14)
rail((.18,y,HEAD),(2.45,y,HEAD),.16)
rail((.25,y,2.17),(.62,y,HEAD-.08),.1);rail((2.4,y,2.17),(2.03,y,HEAD-.08),.1)
# half-timbered gable
GP=OH+.1
for s in (-1,1):
    post(XG+s*GP,y,PLATE+.13,gable_z(XG+s*(GP+.1))-.05,'y')
    rail((XG+s*(GW-.15),y,PLATE+.15),(XG+s*(GP+.08),y,OB+.3),.1)
rail((XG-GP,y,OB-.07),(XG+GP,y,OB-.07),.12)
zc=OT+.14;rail((XG-gx(zc+.08)+.05,y,zc),(XG+gx(zc+.08)-.05,y,zc),.14);post(XG,y,zc+.07,RIDGE-.08,'y')
# sides and rear
for s in (-1,1):
    x=s*(W2+.03);pier(s*(W2-.03),0,.36,.3);pier(s*2.57,D2-.03,.44,.36)
    for yy in (-D2+.1,0,D2-.1):post(x,yy,.56,HEAD+.08,'x')
    for z in ((.62,2.1) if s<0 else (.62,)):rail((x,-D2,z),(x,D2,z))
    rail((x,-D2,HEAD),(x,D2,HEAD))
    rail((x,-D2+.2,HEAD-.5),(x,-D2+.55,HEAD-.08),.12);rail((x,D2-.2,HEAD-.5),(x,D2-.55,HEAD-.08),.12)
y=D2+.03;pier(0,D2-.03,.3,.36)
for x,w in ((-2.6,.3),(0,.2),(2.6,.3)):post(x,y,.56,HEAD+.08,'y',w)
for z in (.62,2.1,HEAD):rail((-W2,y,z),(W2,y,z),.16 if z==HEAD else .14)
for s in (-1,1):rail((s*2.4,y,2.17),(s*2.05,y,HEAD-.08),.1)
# ---- openings --------------------------------------------------------------------
window(G,M,(1.4,-D2,WZ),(0,-1,0),WIN,WH,band=band())
for x in (-1.3,1.3):window(G,M,(x,D2,WZ),(0,1,0),WIN,WH,band=band())
window(G,M,(-W2,1.1,WZ),(-1,0,0),WIN,WH,band=band())
window(G,M,(XG,-D2-.02,GWZ),(0,-1,0),GWW,GWH,shutters=False,band=band())
# workshop bay: two closed ledged-and-braced board leaves, recessed in the opening
yd=-D2+.12;mid=(BAY[0]+BAY[1])/2
for a0,a1,s in ((BAY[0],mid,-1),(mid,BAY[1],1)):   # leaves meet the reveals and each other: no see-through gaps
    c=(a0+a1)/2;lw=a1-a0;k=len(kit.DOORS)+1
    # Each leaf is its own mesh so it can open; kit.split_doors hinges it on its outer reveal (r points away from the hinge).
    D=G[f'door{k}']=Geo(f'SM_SBWorkshop_Door{k}',asset);kit.DOORS.append(dict(geo=D,r=Vector((-s,0,0)),o=Vector((0,-1,0))))
    if k==1:Batten=D
    D.box((c,yd,BAY[2]/2),(lw,.06,BAY[2]),M['door'],0)
    for z in (.32,1.07,1.82):D.box((c,yd-.05,z),(lw-.08,.04,.14),M['door'],.006)
    for z0,z1 in ((.4,1.0),(1.15,1.75)):D.beam((c+s*(lw/2-.12),yd-.05,z0),(c-s*(lw/2-.12),yd-.05,z1),.12,.04,M['door'],grain=False)
    for z in (.32,1.82):D.box((c+s*(lw/2-.3),yd-.076,z),(.56,.012,.05),M['iron'],.003)   # strap hinges from the outer edge
    D.box(((a1-.1) if s<0 else (a0+.1),yd-.085,1.05),(.05,.03,.12),M['iron'],.004)        # handle plates
Batten.box((mid,yd-.045,BAY[2]/2),(.07,.03,BAY[2]-.04),M['door'],.006)   # cover batten on the meeting stiles, fixed to the left leaf (right leaf opens first)
# ---- lean-to on the right ----------------------------------------------------------
LX0,LX1,LZ0,LZ1,LY=W2,3.82,2.42,1.86,1.55;KL=(LZ0-LZ1)/(LX1-LX0)
def lean_z(x):return LZ1+(LX1-x)*KL
LL=math.hypot(LX1-LX0,LZ0-LZ1)
plane((LX1,0,LZ1),(0,1,0),(LX0-LX1,0,LZ0-LZ1),LL,lambda t:-LY,lambda t:LY,[])
G['timber'].beam((LX1+.02,-LY,LZ1-.06),(LX1+.02,LY,LZ1-.06),.04,.16,M['timber'],band=band())
G['timber'].beam((W2+.07,-LY,lean_z(W2+.14)-.13),(W2+.07,LY,lean_z(W2+.14)-.13),.14,.16,M['timber'],band=band())  # ledger
G['timber'].beam((3.7,-LY,lean_z(3.62)-.13),(3.7,LY,lean_z(3.62)-.13),.14,.16,M['timber'],band=band())           # eave beam
n=Vector((KL,0,1)).normalized()
for yy in (-1.3,-.65,0,.65,1.3):
    a,b=Vector((W2+.02,yy,lean_z(W2+.02))),Vector((LX1-.03,yy,lean_z(LX1-.03)));G['timber'].beam(a-n*.09,b-n*.09,.08,.1,M['timber'],band=band())
for s in (-1,1):
    yy=s*(LY-.1);G['stone'].box((3.7,yy,.06),(.3,.3,.12),M['block'],.015)
    post(3.7,yy,.12,lean_z(3.62)-.21,'x',.16);rail((3.7,yy-s*.42,lean_z(3.62)-.21),(3.7,yy,1.3),.1)
# ---- trade sign and lantern -----------------------------------------------------------
ys=-D2+.25
G['detail'].beam((-W2+.01,ys,2.45),(-3.85,ys,2.45),.04,.04,M['iron'],grain=False)
G['detail'].beam((-W2+.01,ys,2.0),(-3.25,ys,2.45),.03,.03,M['iron'],grain=False)
G['detail'].box((-W2-.03,ys,2.25),(.06,.12,.6),M['iron'],.005)
for xx in (-3.62,-3.08):G['detail'].beam((xx,ys,2.43),(xx,ys,2.19),.014,.014,M['iron'],grain=False)
G['detail'].box((-3.35,ys,1.86),(.68,.05,.66),M['door'],.012)
yh=ys-.032
for d,th in ((1,math.pi/4),(-1,-math.pi/4)):   # crossed hammers: oak handles, iron heads
    lo,hi=Vector((-3.35-d*.2,yh,1.66)),Vector((-3.35+d*.17,yh,2.03))
    G['detail'].beam(lo,hi,.035,.016,M['timber'],band=band())
    G['detail'].box(hi,(.2,.03,.07),M['iron'],.006,Matrix.Rotation(th,3,'Y'))
lantern(G,M,(-2.55,-D2-.42,2.05),arm=((-2.55,-D2-.09,2.05),(0,-1,0)))
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert lo[2]>-1e-3,f'below ground: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookWorkshop'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookWorkshop')  # bay door leaves: own meshes, hinged, left out of the main FBX
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
shutil.copy2(REF,HERE/'Workshop-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'{tris} triangles over the house budget'
manifest=dict(asset='SilverbrookWorkshop',reference=dict(file='Workshop-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_workshop.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookWorkshop',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookWorkshop.blend','SilverbrookWorkshop.glb','SilverbrookWorkshop.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('WORKSHOP_BUILT',manifest['bounds_m'],tris)
