"""Silverbrook Cottage 1 from docs/Game-Design/concept-art/Silverbrook/Cottage-1.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b -P build_cottage1.py
Original procedural geometry interpreting the elevation (8 m wide incl. eaves, 6 m to
the gable finial): steep-hipped slate roof, central half-timbered cross gable over an
arched stone doorway and porch, right dormer, ridge stack, ashlar ground storey.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,door,lantern
import bpy
scene=kit.reset(4101)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Cottage-1.png'
asset=bpy.data.collections.new('SilverbrookCottage1');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    shutter=kit.scan('SB_PaintedShutter','blue_painted_planks',dark=(.20,.22,.28),damp=False,aged=.85),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBCottage1_Masonry',asset),plaster=Geo('SM_SBCottage1_Plaster',asset),timber=Geo('SM_SBCottage1_Timber',asset),roof=Geo('SM_SBCottage1_Roof',asset),detail=Geo('SM_SBCottage1_Details',asset))
# ---- measured layout (metres) -------------------------------------------------
SLOT=(8,6,6)                   # town-layout slot w x d x h (concept README 'Fits')
W2,D2=3.3,2.3                  # outer wall half-extents
EAVE,RIDGE=2.4,5.35            # main hip roof
EX,EY,RX=3.7,2.6,2.7           # eave half-extents, ridge half-length
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
GH,GW,GE=1.5,1.15,3.6          # front wall gable: roof half, wall half, eave on the main slope
KG=(RIDGE-GE)/GH;GY=-D2-.3
XD,DH,DW,DE,DR=2.2,.62,.5,3.95,4.35   # dormer
YD=-(RIDGE-3.1)/KF
KD=(DR-DE)/DH
XC,YC=2.55,-.25                # ridge stack
PE,PR,PH,PY=2.1,2.95,1.25,-D2-.75  # porch eave, ridge, roof half, front
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return RIDGE-abs(x)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
in_gable=lambda p:abs(p.x)<GW+.04 and p.y<0 and p.z<gable_z(p.x)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.47 and abs(p.y-YC)<.42
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered,deck_cover=None):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],deck_cover if deck_cover is not None else covered)
# ---- main hip roof --------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE);cut=[in_gable,in_dormer,in_stack]
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),cut)
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
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
# ---- cross gable roof, bargeboards and finial ----------------------------------------
LG=math.hypot(GH,RIDGE-GE)
plane((-GH,0,GE),(0,-1,0),(GH,0,RIDGE-GE),LG,lambda t:0,lambda t:-GY,[under_main])
plane((GH,0,GE),(0,1,0),(-GH,0,RIDGE-GE),LG,lambda t:GY,lambda t:0,[under_main])
capped((0,GY,RIDGE+.05),(0,-.4,RIDGE+.05),9)
for s in (-1,1):G['timber'].beam((s*(GH+.05),GY-.04,GE-.08),(-s*.16,GY-.04,RIDGE+.16*KG+.03),.08,.24,M['timber'],band=band())
# cheek walls close the wall gable down to the main slope
yq=-(RIDGE-(gable_z(GW)-.05))/KF
for s in (-1,1):G['plaster'].prism([(s*GW,-D2,main_z(0,-D2)-.04),(s*GW,-D2,gable_z(GW)-.05),(s*GW,yq,gable_z(GW)-.05)],(1,0,0),.1,M['plaster'])
G['timber'].box((0,GY-.06,5.5),(.15,.15,.9),M['timber'],.02,grain=True,band=band())
G['timber'].box((0,GY-.06,5.96),(.2,.2,.05),M['timber'],.02,grain=True,band=band())
# ---- dormer -------------------------------------------------------------------
LD=math.hypot(DH,DR-DE)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:.9,lambda t:-YD+.16,[under_main])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-.9,[under_main])
capped((XD,YD-.16,DR+.04),(XD,-1.0,DR+.04),4)
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
G['stone'].box((XC,YC,4.2),(.86,.76,2.5),M['stone'],.01,w=(0,0))
G['stone'].box((XC,YC,5.1),(.92,.82,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,5.51),(.98,.88,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.2,.2):G['detail'].vessel((XC+dx,YC,5.57),[(.11,0),(.105,.3),(.115,.38),(.13,.42),(.10,.42),(.085,.36),(0,.36)],M['pot'])
# ---- walls ------------------------------------------------------------------------
PLATE=main_z(0,-D2)-.04
STEP,DS=.3,1.6                 # door sill = porch step top; arch spring above it
WIN=.7;WH=.85;WZ=1.05;WO=(WIN+.18,WZ-.09,WZ+WH+.09)
front_holes=[(-2.33,WO[0],WO[1],WO[2]),(0,1.0,STEP,STEP+DS+.5),(2.33,WO[0],WO[1],WO[2])]
wall(G['stone'],M['stone'],(0,-D2,0),(0,-1,0),W2,PLATE,front_holes,arch=(0,1.0,STEP+DS))
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,PLATE,[(-2.0,WO[0],WO[1],WO[2]),(2.0,WO[0],WO[1],WO[2])])
for s in (-1,1):
    wall(G['stone'],M['stone'],(s*W2,0,0),(s,0,0),D2-.3,PLATE,[(1.3,WO[0],WO[1],WO[2])],th=.3)
    top=[(y/10,main_z(W2,y/10)-.05) for y in range(-round(D2*10),round(D2*10)+1) if main_z(W2,y/10)-.05>PLATE+.01]
    G['plaster'].prism([(s*(W2-.15),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['plaster'])
# gable face (plaster infill) above the main wall plate
# gable face (plaster infill) above the main wall plate, with a real window aperture
GWZ=3.3;OB,OT,OH=GWZ-.09,GWZ+.8+.09,(.6+.18)/2;GC=gable_z(GW)-.05
def gx(z):return (RIDGE-.05-z)/KG     # half-width of the gable face at height z
for piece in ([(-GW,PLATE),(GW,PLATE),(GW,OB),(-GW,OB)],
              [(-GW,OB),(-OH,OB),(-OH,OT),(-gx(OT),OT),(-GW,GC)],[(OH,OB),(GW,OB),(GW,GC),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,RIDGE-.05)]):
    G['plaster'].prism([(x,-D2+.15,z) for x,z in piece],(0,1,0),.3,M['plaster'])
# plinth course
for a0,a1 in ((-W2-.04,-.75),(.75,W2+.04)):G['stone'].box(((a0+a1)/2,-D2-.02,.22),(a1-a0,.1,.44),M['stone'],.01)
G['stone'].box((0,D2+.02,.22),(2*W2+.08,.1,.44),M['stone'],.01)
for s in (-1,1):G['stone'].box((s*(W2+.02),0,.22),(.1,2*D2,.44),M['stone'],.01)
# ---- timber framing ---------------------------------------------------------------
def post(x,y,z0,z1,o):G['timber'].box((x,y,(z0+z1)/2),(.2 if o=='y' else .12,.12 if o=='y' else .2,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
y=-D2-.03
for x in (-3.2,-GW,GW,3.2):post(x,y,.44,PLATE if abs(x)>2 else 2.95,'y')
for a,b in ((-W2,-.8),(.8,W2)):rail((a,y,.5),(b,y,.5))
for a,b in ((-W2,-GW),(GW,W2)):rail((a,y,2.05),(b,y,2.05));rail((a,y,PLATE-.08),(b,y,PLATE-.08),.18)
for s in (-1,1):
    rail((s*3.12,y,1.55),(s*2.85,y,2.0),.12)
    rail((s*1.55,y,.6),(s*1.8,y,.95),.12)
# jettied bressumer and half-timbered gable
G['timber'].box((0,y-.04,2.86),(2*GW+.2,.2,.2),M['timber'],.015,grain=True,band=band())
for s in (-1,1):G['timber'].beam((s*1.2,y,2.35),(s*1.2,y-.08,2.76),.1,.14,M['timber'],band=band())
for x in (-GW+.08,GW-.08):post(x,y,2.96,gable_z(GW-.08)-.05,'y')
rail((-GW,y,GWZ-.17),(GW,y,GWZ-.17),.12);zc=OT+.12;rail((-gx(zc)+.05,y,zc),(gx(zc)-.05,y,zc),.14);post(0,y,zc+.07,RIDGE-.08,'y')
for s in (-1,1):rail((s*(GW-.1),y,2.98),(s*.5,y,GWZ-.24),.1)
# side and rear framing
for s in (-1,1):
    x=s*(W2+.03)
    for yy in (-D2+.1,0,D2-.1):post(x,yy,.44,PLATE,'x')
    rail((x,-D2,.5),(x,D2,.5));rail((x,-D2,2.05),(x,D2,2.05))
    rail((x,-D2+.2,1.55),(x,-D2+.5,2.0),.12);rail((x,D2-.2,1.55),(x,D2-.5,2.0),.12)
y=D2+.03
for x in (-3.2,-.6,.6,3.2):post(x,y,.44,PLATE,'y')
rail((-W2,y,.5),(W2,y,.5));rail((-W2,y,2.05),(W2,y,2.05));rail((-W2,y,PLATE-.08),(W2,y,PLATE-.08),.18)
# ---- openings --------------------------------------------------------------------
for x in (-2.33,2.33):window(G,M,(x,-D2,WZ),(0,-1,0),WIN,WH,planter=True,band=band())
for x in (-2.0,2.0):window(G,M,(x,D2,WZ),(0,1,0),WIN,WH,band=band())
for s in (-1,1):window(G,M,(s*W2,s*1.3,WZ),(s,0,0),WIN,WH,band=band())
window(G,M,(0,-D2-.02,GWZ),(0,-1,0),.6,.8,band=band())
door(G,M,(0,-D2,STEP),(0,-1,0),1.0,DS,STONES,band=band())
# ---- porch ------------------------------------------------------------------------
LP=math.hypot(PH,PR-PE)
plane((-PH,0,PE),(0,-1,0),(PH,0,PR-PE),LP,lambda t:D2,lambda t:-PY,[])
plane((PH,0,PE),(0,1,0),(-PH,0,PR-PE),LP,lambda t:PY,lambda t:-D2,[])
capped((0,PY,PR+.04),(0,-D2,PR+.04),5)
kp=(PR-PE)/PH
for s in (-1,1):G['timber'].beam((s*(PH+.04),PY-.03,PE-.07),(-s*.1,PY-.03,PR+.1*kp+.02),.07,.18,M['timber'],band=band())
for s in (-1,1):
    post(s*1.0,PY+.15,.3,PE-.02,'y');rail((s*1.0,PY+.15,PE),(s*1.0,-D2,PE),.16)
    rail((s*1.0,PY+.15,1.6),(s*.62,PY+.15,PE-.03),.1);rail((s*1.0,PY+.15,1.6),(s*1.0,PY+.5,PE-.03),.1)
rail((-1.12,PY+.15,PE+.02),(1.12,PY+.15,PE+.02),.18);post(0,PY+.12,PE+.1,PR-.06,'y')
G['stone'].box((0,(PY+.05-D2)/2,.15),(2.4,abs(PY+.05+D2),.3),M['stone'],.015)
G['stone'].box((0,PY-.08,.075),(1.9,.3,.15),M['stone'],.015)
lantern(G,M,(0,PY+.55,PR-.12))
lantern(G,M,(3.2,-D2-.42,2.15),arm=((3.2,-D2-.09,2.15),(0,-1,0)))
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
# Origin at the slot's ground centre: the porch side is deeper than the back eave.
from mathutils import Matrix
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookCottage1'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a 60 MB GLB.
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
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
shutil.copy2(REF,HERE/'Cottage-1-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
manifest=dict(asset='SilverbrookCottage1',reference=dict(file='Cottage-1-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_cottage1.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookCottage1',slot_m=list(SLOT),outputs=['SilverbrookCottage1.blend','SilverbrookCottage1.glb','SilverbrookCottage1.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('COTTAGE1_BUILT',manifest['bounds_m'],tris)
