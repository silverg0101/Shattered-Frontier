"""Silverbrook Cottage 2 from docs/Game-Design/concept-art/Silverbrook/Cottage-2.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cottage2.py
Original procedural geometry interpreting the elevation (8 m wide, 6 m to the gable
finial): a full-depth rubble-stone cross wing on the left under a steep front gable,
a lower slate range to the right with a hipped end, dormer and ridge-end stack, and a
gabled door hood over a round-headed door on three stone steps.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,tile_plane,deck,wall,window,door,lantern,frame
import bpy
scene=kit.reset(4102)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Cottage-2.png'
asset=bpy.data.collections.new('SilverbrookCottage2');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Cottage2_Rubble','stone_wall'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    shutter=kit.scan('SB_Cottage2_Shutter','wood_planks_grey',dark=(.20,.22,.28),damp=False,aged=.5),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('stone_wall',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBCottage2_Masonry',asset),timber=Geo('SM_SBCottage2_Timber',asset),roof=Geo('SM_SBCottage2_Roof',asset),detail=Geo('SM_SBCottage2_Details',asset))
# ---- measured layout (metres; recentred on the slot at export) ---------------------
SLOT=(8,6,6)                   # town-layout slot w x d x h (concept README 'Fits')
XW,WW,WF,WB=-1.1,2.2,-2.4,2.0  # cross wing: centre x, wall half-width, front and back wall faces
GH,GE,GR=2.5,3.2,5.65          # wing gable: eave half-span, eave, ridge
KG=(GR-GE)/GH;GF,GBK=WF-.3,WB+.3
X0,X1,MD=1.1,3.6,2.0           # right range walls: x from the wing to the hipped end, half-depth
EAVE,RIDGE,EY,EX,RX=2.6,5.35,2.3,3.9,2.4
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX);XL=-.9   # XL: where the range ridge dies into the wing roof
XD,DH,DW,DE,DR=1.95,.6,.46,4.15,4.7   # dormer
YD=-(RIDGE-3.3)/KF;KD=(DR-DE)/DH
XC,YC=2.95,0.0                 # stack at the ridge end
XDOOR,DOORW,STEP,DS=.15,.95,.42,1.25  # door: centre, width, step top (three steps), arch spring above the step
PH,PE,PR,PYF=1.05,2.4,3.15,WF-.8      # door hood: half-span, eave, ridge, front
def wing_z(x):return GR-abs(x-XW)*KG
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,x-RX)*KS)
def dormer_z(x):return DR-abs(x-XD)*KD
in_wing=lambda p:p.x<XW+WW and GF-.05<p.y<GBK+.05 and p.z<wing_z(p.x)-.005   # range slates run under the wing eave to its wall
under_main=lambda p:p.x>XL and abs(p.y)<EY+.05 and p.z<main_z(p.x,p.y)-.01
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.42 and abs(p.y-YC)<.42
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
def post(x,y,z0,z1,o):G['timber'].box((x,y,(z0+z1)/2),(.2 if o=='y' else .12,.12 if o=='y' else .2,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
# ---- cross wing roof: full-depth gable, front bargeboards cross under a finial -------
LG=math.hypot(GH,GR-GE)
plane((XW-GH,0,GE),(0,-1,0),(GH,0,GR-GE),LG,lambda t:-GBK,lambda t:-GF,[])
plane((XW+GH,0,GE),(0,1,0),(-GH,0,GR-GE),LG,lambda t:GF,lambda t:GBK,[under_main])
capped((XW,GF-.02,GR+.05),(XW,GBK,GR+.05),18)
for s in (-1,1):
    G['timber'].beam((XW+s*(GH+.05),GF-.04,GE-.08),(XW-s*.16,GF-.04,GR+.16*KG+.03),.08,.24,M['timber'],band=band())
    G['timber'].beam((XW+s*(GH+.05),GBK+.04,GE-.08),(XW,GBK+.04,GR+.03),.08,.24,M['timber'],band=band())
G['timber'].box((XW,GF-.1,5.5),(.15,.15,.84),M['timber'],.02,grain=True,band=band())
G['timber'].box((XW,GF-.1,5.945),(.2,.2,.05),M['timber'],.02,grain=True,band=band())
G['timber'].beam((XW-GH-.02,GF,GE-.06),(XW-GH-.02,GBK,GE-.06),.04,.2,M['timber'],band=band())
for y0,y1 in ((GF,-1.75),(1.75,GBK)):G['timber'].beam((XW+GH+.02,y0,GE-.06),(XW+GH+.02,y1,GE-.06),.04,.2,M['timber'],band=band())
# ---- right range: slate roof with a hipped end; its ridge dies into the wing ------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lambda t:XL,lerp(EX,RX,LF),[in_wing,in_dormer,in_stack])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lambda t:-XL,[in_wing,in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
capped((XL+.2,0,RIDGE+.05),(RX,0,RIDGE+.05),10)
for sy in (-1,1):capped((EX,sy*EY,EAVE+.05),(RX,0,RIDGE+.05),9)
for sy in (-1,1):G['timber'].beam((XW+GH,sy*(EY+.02),EAVE-.06),(EX,sy*(EY+.02),EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((EX+.02,-EY,EAVE-.06),(EX+.02,EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- dormer (timber-framed, boarded cheeks) -------------------------------------------
LD=math.hypot(DH,DR-DE)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:.5,lambda t:-YD+.16,[under_main])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-.5,[under_main])
capped((XD,YD-.16,DR+.04),(XD,-.6,DR+.04),4)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
DB=main_z(XD,YD)-.1
wall(G['timber'],M['timber'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.6,.13,.76)],th=.12)
G['timber'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dormer_z(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dormer_z(XD+DW)-.04)],(0,1,0),.12,M['timber'])
ym=-(RIDGE-(dormer_z(XD+DW)-.04))/KF
for s in (-1,1):G['timber'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,dormer_z(XD+DW)-.04),(XD+s*DW,ym,dormer_z(XD+DW)-.04)],(1,0,0),.08,M['timber'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
# ---- ridge-end stack in rubble with two clay pots ----------------------------------------
G['stone'].box((XC,YC,4.2),(.76,.76,2.5),M['stone'],.01)
G['stone'].box((XC,YC,5.1),(.82,.82,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,5.51),(.88,.88,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.18,.18):G['detail'].vessel((XC+dx,YC,5.57),[(.11,0),(.105,.3),(.115,.38),(.13,.42),(.10,.42),(.085,.36),(0,.36)],M['pot'])
# ---- walls ----------------------------------------------------------------------------
PW=wing_z(XW-WW)-.04;PLATE=main_z(2,-MD)-.04
WIN,WH,WZ=.7,.85,1.3;UWW,UWH,UWZ=.65,.75,3.65
def hole(a,w=WIN):return (a,w+.18,WZ-.09,WZ+WH+.09)
wall(G['stone'],M['stone'],(XW,WF,0),(0,-1,0),WW,PW,[hole(-1.95-XW),(XDOOR-XW,DOORW,STEP,STEP+DS+.5)],arch=(XDOOR-XW,DOORW,STEP+DS))
wall(G['stone'],M['stone'],(XW,WB,0),(0,1,0),WW,PW,[hole(0)])
YM=(WF+WB)/2
for s in (-1,1):wall(G['stone'],M['stone'],(XW+s*WW,YM,0),(s,0,0),(WB-WF)/2-.3,PW,[hole(0)] if s<0 else [])
XM=(X0+X1)/2
wall(G['stone'],M['stone'],(XM,-MD,0),(0,-1,0),(X1-X0)/2,PLATE,[hole(2.35-XM)])
wall(G['stone'],M['stone'],(XM,MD,0),(0,1,0),(X1-X0)/2,PLATE,[hole(XM-2.35)])
wall(G['stone'],M['stone'],(X1,0,0),(1,0,0),MD-.3,PLATE,[hole(0)])
top=[(y/10,main_z(X1,y/10)-.05) for y in range(-round(MD*10),round(MD*10)+1) if main_z(X1,y/10)-.05>PLATE+.01]
G['stone'].prism([(X1-.15,y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['stone'])
# wing gables: stone up to the roof, the front one around the upper window
OB,OT,OH=UWZ-.09,UWZ+UWH+.09,(UWW+.18)/2
def gx(z):return (GR-.05-z)/KG     # half-width of the gable face at height z
for piece in ([(-gx(PW),PW),(gx(PW),PW),(gx(OB),OB),(-gx(OB),OB)],
              [(-gx(OB),OB),(-OH,OB),(-OH,OT),(-gx(OT),OT)],[(OH,OB),(gx(OB),OB),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,GR-.05)]):
    G['stone'].prism([(XW+x,WF+.15,z) for x,z in piece],(0,1,0),.3,M['stone'])
G['stone'].prism([(XW+x,WB-.15,z) for x,z in [(-gx(PW),PW),(gx(PW),PW),(0,GR-.05)]],(0,1,0),.3,M['stone'])
# ---- timber framing ---------------------------------------------------------------------
y=WF-.03
post(XW-WW+.05,y,0,PW,'y')
rail((XW-WW,y,2.9),(XDOOR-PH-.02,y,2.9),.18)
post(XW-1.45,y,2.99,wing_z(XW-1.45)-.14,'y');post(XW+1.45,y,3.12,wing_z(XW+1.45)-.14,'y')
for s in (-1,1):rail((XW+s*WW,WF-.06,2.7),(XW+s*(GH-.05),WF-.06,GE-.05),.1)   # knee braces under the gable eaves
x=XW-WW-.03;rail((x,WF,PW-.08),(x,WB,PW-.08),.18);post(x,WB-.08,0,PW,'x')
rail((XW-WW,WB+.03,2.9),(XW+WW,WB+.03,2.9),.18)
y=-MD-.03
rail((X0,y,PLATE-.08),(X1,y,PLATE-.08),.18);post(X0+.1,y,0,PLATE,'y');post(X1-.1,y,0,PLATE,'y')
rail((X0,MD+.03,PLATE-.08),(X1,MD+.03,PLATE-.08),.18);post(X1-.1,MD+.03,0,PLATE,'y')
rail((X1+.03,-MD,PLATE-.08),(X1+.03,MD,PLATE-.08),.18)
# ---- openings ---------------------------------------------------------------------------
def win(c,o,w,h,**kw):
    """kit.window plus the concept's diamond cut-outs in each shutter (dark insets)."""
    window(G,M,c,o,w,h,band=band(),**kw);P,R=frame(c,o);sw=w/2+.09
    for side in (-1,1):
        a=side*(w/2+.09+sw/2+.015);G['detail'].prism([P(a-.05,.0625,.6*h),P(a,.0625,.6*h-.075),P(a+.05,.0625,.6*h),P(a,.0625,.6*h+.075)],o,.035,M['iron'])
win((-1.95,WF,WZ),(0,-1,0),WIN,WH,planter=True)
win((XW,WF,UWZ),(0,-1,0),UWW,UWH,planter=True)
win((2.35,-MD,WZ),(0,-1,0),WIN,WH,planter=True)
win((XW,WB,WZ),(0,1,0),WIN,WH);win((2.35,MD,WZ),(0,1,0),WIN,WH)
win((XW-WW,YM,WZ),(-1,0,0),WIN,WH);win((X1,0,WZ),(1,0,0),WIN,WH)
window(G,M,(XD,YD,DB+.22),(0,-1,0),.42,.45,shutters=False,wall_th=.12,band=band())
door(G,M,(XDOOR,WF,STEP),(0,-1,0),DOORW,DS,STONES,band=band())
# ---- door hood on knee braces, three stone steps ---------------------------------------
LP=math.hypot(PH,PR-PE);kp=(PR-PE)/PH
plane((XDOOR-PH,0,PE),(0,-1,0),(PH,0,PR-PE),LP,lambda t:-WF,lambda t:-PYF,[])
plane((XDOOR+PH,0,PE),(0,1,0),(-PH,0,PR-PE),LP,lambda t:PYF,lambda t:WF,[])
capped((XDOOR,PYF,PR+.04),(XDOOR,WF,PR+.04),4)
for s in (-1,1):
    G['timber'].beam((XDOOR+s*(PH+.04),PYF-.03,PE-.07),(XDOOR-s*.1,PYF-.03,PR+.1*kp+.02),.07,.18,M['timber'],band=band())
    x=XDOOR+s*.92;post(x,WF-.06,1.75,PE-.02,'y');rail((x,WF-.06,PE),(x,PYF+.15,PE),.16);rail((x,WF-.12,1.85),(x,PYF+.45,PE-.03),.1)
rail((XDOOR-PH-.02,PYF+.15,PE+.02),(XDOOR+PH+.02,PYF+.15,PE+.02),.18);post(XDOOR,PYF+.12,PE+.1,PR-.06,'y')
for k,(w_,d_) in enumerate(((1.98,1.01),(1.84,.73),(1.7,.45))):
    G['stone'].box((XDOOR,WF-d_/2,STEP*(k+1)/6),(w_,d_,STEP*(k+1)/3),M['stone'],.015)
lantern(G,M,(XDOOR,PYF+.45,PR-.12))
lantern(G,M,(X1+.42,-MD+.4,2.3),arm=((X1+.09,-MD+.4,2.3),(1,0,0)))
# Grey shutter boards run vertically in the scan; undo kit.window's quarter-turn for them.
d=G['detail'];d.uv=[[(b,a) for a,b in uv] if m is M['shutter'] else uv for uv,m in zip(d.uv,d.mat)]
# ---- export, preview scene, manifest -------------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
assert abs(lo[2])<1e-6,f'base not on the ground: {lo[2]}'
OUT=HERE/'SilverbrookCottage2'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a huge GLB.
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookCottage2')  # door leaves: own meshes, hinged
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
shutil.copy2(REF,HERE/'Cottage2-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'house triangle budget: {tris}'
manifest=dict(asset='SilverbrookCottage2',reference=dict(file='Cottage2-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_cottage2.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookCottage2',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookCottage2.blend','SilverbrookCottage2.glb','SilverbrookCottage2.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('COTTAGE2_BUILT',manifest['bounds_m'],tris)
