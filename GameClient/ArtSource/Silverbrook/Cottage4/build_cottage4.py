"""Silverbrook Cottage 4 from docs/Game-Design/concept-art/Silverbrook/Cottage-4.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cottage4.py
Original procedural geometry interpreting the elevation (8 m wide incl. eaves, 6 m to
the gable finial): steep-hipped reed-thatch roof laid in stepped courses, a tall
half-timbered cross gable running through the ridge, two small thatched dormers,
a thatched door hood on knee braces, squared-rubble ground storey and a grey
squared-stone stack. Catalogue asset BLD-GreenlandsCottage-01, variant 4.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,door,lantern
import bpy
scene=kit.reset(4104)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Cottage-4.png'
asset=bpy.data.collections.new('SilverbrookCottage4');scene.collection.children.link(asset)

M=dict(
    thatch=kit.scan('SB_Cottage4_Thatch','reed_roof_04',dark=(.25,.20,.14),damp=False),
    rubble=kit.scan('SB_Cottage4_Rubble','rustic_stone_wall'),
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    shutter=kit.scan('SB_Cottage4_Shutter','green_rough_planks',dark=(.20,.22,.20),damp=False,aged=.5),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75))
M['ashlar']=M['stone'];M['planks']=M['timber']
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
# Thatch has no discrete elements: any rect below the scan's mossy ridge strip (top ~8%)
# works. V runs up-slope on every course, so the reeds point down the roof.
TW,TL,TT=1.1,1.0,.1
REEDS=[(kit.rng.uniform(0,1-TW/2.5),kit.rng.uniform(0,.9-TL/2.5),TW/2.5,TL/2.5) for _ in range(60)]
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBCottage4_Masonry',asset),timber=Geo('SM_SBCottage4_Timber',asset),roof=Geo('SM_SBCottage4_Roof',asset),detail=Geo('SM_SBCottage4_Details',asset))
# ---- measured layout (metres; concept scale ~117 px/m) -------------------------
SLOT=(8,6,6)                   # town-layout slot w x d x h (concept README 'Fits')
W2,D2=3.3,2.2                  # outer wall half-extents
EAVE,RIDGE=2.35,5.0            # main hip roof
EX,EY,RX=3.72,2.6,2.55          # eave half-extents, ridge half-length
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
GH,GW,GE,GR=2.1,1.4,3.0,5.5    # cross gable: roof half, wall half, eave, ridge (above the main ridge)
KG=(GR-GE)/GH;GY=D2+.42        # gable roof runs front to back, overhanging both walls
XD,DH,DW,DE,DR=2.55,.48,.42,3.95,4.35   # dormers at +-XD
YD=-1.85;KD=(DR-DE)/DH
XC,YC=2.75,.5                  # stack
PE,PR,PH,PY=2.32,2.9,1.12,-D2-.72  # door hood eave, ridge, half width, front
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return GR-abs(x)*KG
def dormer_z(x,xd):return DR-abs(x-xd)*KD
in_gable=lambda p:abs(p.x)<GW+.04 and p.z<gable_z(p.x)-.005
in_dormer=lambda p:any(abs(p.x-s*XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x,s*XD)-.005 for s in (-1,1))
in_stack=lambda p:abs(p.x-XC)<.44 and abs(p.y-YC)<.4
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered):
    u=Vector(toward).normalized()
    tile_plane(G['roof'],O,e,u,L,lo,hi,M['thatch'],REEDS,covered,size=(TW,TL,TT),exposure=.25,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def roll(a,b,n,w=.34):
    """Thatch ridge roll in short bundles."""
    a,b=Vector(a),Vector(b)
    for i in range(n):G['roof'].beam(a+(b-a)*i/n,a+(b-a)*(i+1.03)/n,w,.16,M['thatch'],grain=False,rect=kit.rng.choice(REEDS),w=(kit.rng.uniform(.2,.5),0))
# ---- main hip roof --------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_dormer,in_stack])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
XG=(GR-RIDGE)/KG               # main ridge disappears into the gable roof inside |x|<XG
for s in (-1,1):roll((s*RX,0,RIDGE+.12),(s*(XG-.1),0,RIDGE+.12),8)
for sx in (-1,1):
    for sy in (-1,1):roll((sx*EX,sy*EY,EAVE+.12),(sx*RX,0,RIDGE+.12),10)
# ---- cross gable roof, bargeboards and finial ----------------------------------------
LG=math.hypot(GH,GR-GE)
plane((-GH,0,GE),(0,-1,0),(GH,0,GR-GE),LG,lambda t:-GY,lambda t:GY,[under_main])
plane((GH,0,GE),(0,1,0),(-GH,0,GR-GE),LG,lambda t:-GY,lambda t:GY,[under_main])
roll((0,-GY,GR+.12),(0,GY,GR+.12),16)
for sy in (-1,1):
    for s in (-1,1):G['timber'].beam((s*(GH+.06),sy*(GY+.05),GE-.1),(-s*.18,sy*(GY+.05),GR+.18*KG+.05),.09,.26,M['timber'],band=band())
G['timber'].box((0,-GY-.08,5.62),(.16,.16,.7),M['timber'],.02,grain=True,band=band())     # front finial
G['timber'].box((0,-GY-.08,5.98-.07),(.24,.24,.14),M['timber'],.02,grain=True,band=band())
# cheek walls close each wall gable down to the main slope
yq=(RIDGE-(gable_z(GW)-.05))/KF
for sy in (-1,1):
    for s in (-1,1):G['stone'].prism([(s*GW,sy*D2,main_z(0,D2)-.04),(s*GW,sy*D2,gable_z(GW)-.05),(s*GW,sy*yq,gable_z(GW)-.05)],(1,0,0),.12,M['rubble'])
# ---- dormers ---------------------------------------------------------------------
LD=math.hypot(DH,DR-DE);DB=main_z(XD,YD)-.1;ym=-(RIDGE-(dormer_z(XD+DW,XD)-.04))/KF
for s in (-1,1):
    xd=s*XD
    plane((xd-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:.55,lambda t:-YD+.2,[under_main])
    plane((xd+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.2,lambda t:-.55,[under_main])
    roll((xd,YD-.2,DR+.1),(xd,-.75,DR+.1),4,.26)
    wall(G['stone'],M['rubble'],(xd,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.66,.21,.81)],th=.12)
    G['timber'].prism([(xd-DW,YD+.06,DE+.02),(xd+DW,YD+.06,DE+.02),(xd+DW,YD+.06,dormer_z(xd+DW,xd)-.04),(xd,YD+.06,DR-.04),(xd-DW,YD+.06,dormer_z(xd+DW,xd)-.04)],(0,1,0),.12,M['timber'])
    for c in (-1,1):G['stone'].prism([(xd+c*DW,YD,DB),(xd+c*DW,YD,dormer_z(xd+DW,xd)-.04),(xd+c*DW,ym,dormer_z(xd+DW,xd)-.04)],(1,0,0),.08,M['rubble'])
    G['timber'].box((xd,YD-.03,DE-.02),(2*DW+.08,.14,.12),M['timber'],.01,grain=True,band=band())
    window(G,M,(xd,YD,DB+.3),(0,-1,0),.48,.42,shutters=False,wall_th=.12,band=band())
# ---- stack ----------------------------------------------------------------------
G['stone'].box((XC,YC,4.3),(.78,.68,2.3),M['stone'],.01)
G['stone'].box((XC,YC,5.5),(.86,.76,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
G['detail'].vessel((XC,YC,5.55),[(.13,0),(.12,.28),(.135,.34),(.15,.38),(.12,.38),(.10,.33),(0,.33)],M['pot'])
# ---- walls ------------------------------------------------------------------------
PLATE=main_z(0,-D2)-.04
STEP,DS=.3,1.45                # door sill = step top; arch spring above it
WIN=.7;WH=.85;WZ=1.05;WO=(WIN+.18,WZ-.09,WZ+WH+.09);WX=2.15
front=[(-WX,WO[0],WO[1],WO[2]),(0,1.0,STEP,STEP+DS+.5),(WX,WO[0],WO[1],WO[2])]
wall(G['stone'],M['rubble'],(0,-D2,0),(0,-1,0),W2,PLATE,front,arch=(0,1.0,STEP+DS))
wall(G['stone'],M['rubble'],(0,D2,0),(0,1,0),W2,PLATE,[(-1.9,WO[0],WO[1],WO[2]),(1.9,WO[0],WO[1],WO[2])])
for s in (-1,1):
    wall(G['stone'],M['rubble'],(s*W2,0,0),(s,0,0),D2-.3,PLATE,[(0,WO[0],WO[1],WO[2])],th=.3)
    top=[(y/10,main_z(W2,y/10)-.05) for y in range(-round(D2*10),round(D2*10)+1) if main_z(W2,y/10)-.05>PLATE+.01]
    G['stone'].prism([(s*(W2-.15),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['rubble'])
# gable faces (rubble infill) above the wall plate, each with a window aperture
GWZ,GWW,GWH=3.5,.7,.7;OB,OT,OH=GWZ-.09,GWZ+GWH+.09,(GWW+.18)/2;GC=gable_z(GW)-.05
def gx(z):return (GR-.05-z)/KG     # half-width of the gable face at height z
for sy in (-1,1):
    for piece in ([(-GW,PLATE),(GW,PLATE),(GW,OB),(-GW,OB)],
                  [(-GW,OB),(-OH,OB),(-OH,OT),(-gx(OT),OT),(-GW,GC)],[(OH,OB),(GW,OB),(GW,GC),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,GR-.05)]):
        G['stone'].prism([(x,sy*(D2-.15),z) for x,z in piece],(0,1,0),.3,M['rubble'])
# plinth course of larger stones
for a0,a1 in ((-W2-.04,-.8),(.8,W2+.04)):G['stone'].box(((a0+a1)/2,-D2-.025,.22),(a1-a0,.1,.44),M['rubble'],.01)
G['stone'].box((0,D2+.025,.22),(2*W2+.08,.1,.44),M['rubble'],.01)
for s in (-1,1):G['stone'].box((s*(W2+.025),0,.22),(.1,2*D2,.44),M['rubble'],.01)
# ---- timber framing ---------------------------------------------------------------
def post(x,y,z0,z1,o):G['timber'].box((x,y,(z0+z1)/2),(.2 if o=='y' else .12,.12 if o=='y' else .2,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
for sy in (-1,1):
    y=sy*(D2+.03)
    for x in (-3.2,-1.1,1.1,3.2):post(x,y,.44,PLATE,'y')
    for a,b in ((-W2,-GW),(GW,W2)):rail((a,y,PLATE-.08),(b,y,PLATE-.08),.18)
    for s in (-1,1):rail((s*3.12,y,1.75),(s*2.75,y,PLATE-.16),.12)
    # jettied bressumer and half-timbered gable
    G['timber'].box((0,y+sy*.04,2.84),(2*GW+.2,.2,.2),M['timber'],.015,grain=True,band=band())
    for x in (-GW+.08,GW-.08):post(x,y,2.94,gable_z(GW-.08)-.05,'y')
    rail((-GW,y,GWZ-.17),(GW,y,GWZ-.17),.12);zc=OT+.1;rail((-gx(zc)+.05,y,zc),(gx(zc)-.05,y,zc),.14);post(0,y,zc+.07,GR-.1,'y')
    for s in (-1,1):rail((s*(GW-.1),y,2.96),(s*.62,y,GWZ-.24),.1);rail((s*(GW-.1),y,OT-.1),(s*(gx(zc)-.1),y,zc-.07),.1)
for s in (-1,1):
    x=s*(W2+.03)
    for yy in (-D2+.1,D2-.1):post(x,yy,.44,PLATE,'x')
    rail((x,-D2,PLATE-.08),(x,D2,PLATE-.08),.18)
    for sy in (-1,1):rail((x,sy*(D2-.2),1.75),(x,sy*(D2-.55),PLATE-.16),.12)
# ---- openings --------------------------------------------------------------------
for x in (-WX,WX):window(G,M,(x,-D2,WZ),(0,-1,0),WIN,WH,planter=True,band=band())
for x in (-1.9,1.9):window(G,M,(x,D2,WZ),(0,1,0),WIN,WH,band=band())
for s in (-1,1):window(G,M,(s*W2,0,WZ),(s,0,0),WIN,WH,band=band())
window(G,M,(0,-D2-.02,GWZ),(0,-1,0),GWW,GWH,planter=True,band=band())
window(G,M,(0,D2+.02,GWZ),(0,1,0),GWW,GWH,shutters=False,band=band())
door(G,M,(0,-D2,STEP),(0,-1,0),1.0,DS,STONES,band=band())
# ---- thatched door hood on knee braces --------------------------------------------
LP=math.hypot(PH,PR-PE)
plane((-PH,0,PE),(0,-1,0),(PH,0,PR-PE),LP,lambda t:D2,lambda t:-PY,[])
plane((PH,0,PE),(0,1,0),(-PH,0,PR-PE),LP,lambda t:PY,lambda t:-D2,[])
roll((0,PY,PR+.1),(0,-D2,PR+.1),4,.24)
kp=(PR-PE)/PH
for s in (-1,1):
    G['timber'].beam((s*(PH+.05),PY-.04,PE-.06),(-s*.1,PY-.04,PR+.1*kp+.03),.07,.18,M['timber'],band=band())
    rail((s*1.1,PY+.12,PE-.06),(s*1.1,-D2-.09,PE-.06),.14)                 # hood plate into the wall post
    rail((s*1.1,-D2-.09,1.45),(s*1.1,PY+.3,PE-.13),.12)                    # knee brace
rail((-1.16,PY+.12,PE-.06),(1.16,PY+.12,PE-.06),.16);post(0,PY+.12,PE+.02,PR-.06,'y')
G['stone'].box((0,(PY+.08-D2)/2,.15),(2.1,abs(PY+.08+D2),.3),M['stone'],.015)
G['stone'].box((0,PY-.06,.075),(1.7,.28,.15),M['stone'],.015)
lantern(G,M,(0,PY+.4,PR-.14))
lantern(G,M,(-W2-.45,-D2+.3,2.05),arm=((-W2-.09,-D2+.3,2.05),(-1,0,0)))
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
from mathutils import Matrix
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert lo[2]>=-1e-3,f'below ground: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookCottage4'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
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
shutil.copy2(REF,HERE/'Cottage4-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'house triangle budget: {tris}'
manifest=dict(asset='SilverbrookCottage4',reference=dict(file='Cottage4-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_cottage4.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookCottage4',slot_m=list(SLOT),outputs=['SilverbrookCottage4.blend','SilverbrookCottage4.glb','SilverbrookCottage4.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('COTTAGE4_BUILT',manifest['bounds_m'],tris)
