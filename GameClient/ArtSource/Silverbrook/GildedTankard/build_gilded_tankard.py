"""The Gilded Tankard from docs/Game-Design/concept-art/Silverbrook/The Gilded Tankard.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_gilded_tankard.py
Original procedural geometry interpreting the elevation (slot 14 x 12 x 14 m): a two-storey alehouse.
Ashlar ground storey with a wide round-headed double door in a dressed surround and a stone step; an
oak-framed plaster upper storey with two shuttered leaded windows; a hanging gilt tankard sign on an iron
bracket above the door; a steep hip slate roof with a chimney stack on the back slope; wall lanterns either
side of the door; a bench and two barrels on the street.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,door,lantern
import bpy
scene=kit.reset(5171)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/The Gilded Tankard.png'
asset=bpy.data.collections.new('SilverbrookGildedTankard');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',dark=(.22,.17,.12),aged=.5),
    sign=kit.scan('SB_GildedTankard_SignBoard','old_planks_02',dark=(.10,.10,.11),damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_GildedTankard_Gilt',(.62,.45,.18),.4,.85))
M['ashlar']=M['stone'];M['planks']=M['timber'];M['shutter']=M['timber']   # concept shutters are dark oak like the frame
kit.SCALE['SB_Ashlar']*=1.6   # ashlar blocks ~0.53 x 0.26 m, not coursed rubble
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBGildedTankard_Masonry',asset),plaster=Geo('SM_SBGildedTankard_Plaster',asset),timber=Geo('SM_SBGildedTankard_Timber',asset),roof=Geo('SM_SBGildedTankard_Roof',asset),detail=Geo('SM_SBGildedTankard_Details',asset))
# ---- measured layout (metres) -------------------------------------------------
SLOT=(14,12,14)                  # town-layout slot w x d x h (building_types.tavern)
W2,D2=4.6,3.6                    # outer wall half-extents: footprint 9.2 x 7.2 m
EAVE,RIDGE=7.4,11.0              # hip roof: EAVE at the overhang edge, RIDGE on the centre line (about 42 deg)
EX,EY,RX=W2+.4,D2+.4,3.4         # eave half-extents (0.4 m overhang), ridge half-length
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
STONE_TOP=3.7                    # ashlar ground storey; oak-framed plaster above
TILE=dict(size=(.42,.58,.028),exposure=.27)
XC,YC=3.9,1.5                    # chimney stack, right side end of the back slope
XDOOR,DWID,DS,STEP=0.0,2.6,1.8,.2   # double door: width, spring above the step, step top
WIN,WH,WZ=1.15,1.6,4.6           # upper windows: width, height, sill
WO=(WIN+.18,WZ-.09-STONE_TOP,WZ-STONE_TOP+WH+.09)   # plaster-wall opening (a is set per use), relative to STONE_TOP
SY=-D2-1.2                       # hanging sign board plane, 1.2 m out from the street face
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
PLATE=main_z(0,-D2)-.04          # wall plate under the hip roof
in_stack=lambda p:abs(p.x-XC)<.52 and abs(p.y-YC)<.47
def lichen(p):return kit.rng.uniform(.15,.45) if kit.rng.random()<.12 else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered,deck_cover=None):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8),**TILE)
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],deck_cover if deck_cover is not None else covered)
# ---- hip roof -------------------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
def cap(a,b,w=.32):G['roof'].beam(a,b,w,.06,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
capped((-RX,0,RIDGE+.06),(RX,0,RIDGE+.06),10)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.06),(sx*RX,0,RIDGE+.06),8)
G['timber'].beam((-EX,-EY-.02,EAVE-.07),(EX,-EY-.02,EAVE-.07),.05,.24,M['timber'],band=band())
G['timber'].beam((-EX,EY+.02,EAVE-.07),(EX,EY+.02,EAVE-.07),.05,.24,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.07),(sx*(EX+.02),EY,EAVE-.07),.05,.24,M['timber'],band=band())
for x in (-4.0,-2.6,-1.3,0,1.3,2.6,4.0):G['timber'].box((x,-D2-.2,EAVE-.36),(.16,.4,.3),M['timber'],.012,grain=True,band=band())   # eave corbels under the fascia
# ---- walls: ashlar ground storey, oak-framed plaster above ---------------------------
wall(G['stone'],M['stone'],(0,-D2,0),(0,-1,0),W2,STONE_TOP,[(XDOOR,DWID,STEP,STEP+DS+DWID/2)],arch=(XDOOR,DWID,STEP+DS))
wall(G['plaster'],M['plaster'],(0,-D2,STONE_TOP),(0,-1,0),W2,PLATE-STONE_TOP,[(-2.6,*WO),(2.6,*WO)])
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,STONE_TOP,[])
wall(G['plaster'],M['plaster'],(0,D2,STONE_TOP),(0,1,0),W2,PLATE-STONE_TOP,[(-2.6,*WO),(2.6,*WO)])
for s in (-1,1):
    wall(G['stone'],M['stone'],(s*W2,0,0),(s,0,0),D2,STONE_TOP,[])
    wall(G['plaster'],M['plaster'],(s*W2,0,STONE_TOP),(s,0,0),D2,PLATE-STONE_TOP,[(0,*WO)])
    top=[(y/10,main_z(W2,y/10)-.05) for y in range(-round(D2*10),round(D2*10)+1) if main_z(W2,y/10)-.05>PLATE+.01]
    G['plaster'].prism([(s*(W2-.15),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['plaster'])
# ---- chimney stack on the back slope ----------------------------------------------------
G['stone'].box((XC,YC,10.0),(.9,.7,4.0),M['stone'],.01)
G['stone'].box((XC,YC,12.05),(1.0,.8,.1),M['stone'],.015,rect=kit.rng.choice(STONES))
for dx in (-.22,.22):G['detail'].vessel((XC+dx,YC,12.1),[(.12,0),(.115,.34),(.125,.43),(.14,.47),(.11,.47),(.095,.41),(0,.41)],M['pot'])
# ---- oak framing of the plaster storey (posts, sill and wall-plate rails) ---------------------
def post(x,y,z0,z1,along_x):G['timber'].box((x,y,(z0+z1)/2),(.2 if along_x else .13,.13 if along_x else .2,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.13,s,M['timber'],band=band())
y=-D2-.06
for x in (-4.25,-1.1,1.1,4.25):post(x,y,STONE_TOP,PLATE-.1,True)
G['timber'].beam((-W2-.2,-D2-.18,STONE_TOP+.15),(W2+.2,-D2-.18,STONE_TOP+.15),.36,.3,M['timber'],band=band())   # jetty beam at the storey line
for x in (-4.0,-2.6,2.6,4.0):G['timber'].box((x,-D2-.2,STONE_TOP-.22),(.2,.4,.44),M['timber'],.012,grain=True,band=band())   # corbels, clear of the arch
rail((-W2,-D2-.04,WZ-.17),(W2,-D2-.04,WZ-.17),.12)   # sill rail under the shutters
rail((-W2,-D2-.04,PLATE-.3),(W2,-D2-.04,PLATE-.3),.16)
rail((-W2,D2+.04,PLATE-.3),(W2,D2+.04,PLATE-.3),.16)
for s in (-1,1):rail((s*(W2+.04),-D2,PLATE-.3),(s*(W2+.04),D2,PLATE-.3),.16)
# ---- openings ---------------------------------------------------------------------------
for x in (-2.6,2.6):window(G,M,(x,-D2,WZ),(0,-1,0),WIN,WH,band=band())
for x in (-2.6,2.6):window(G,M,(x,D2,WZ),(0,1,0),WIN,WH,band=band())
for s in (-1,1):window(G,M,(s*W2,0,WZ),(s,0,0),WIN,WH,band=band())
door(G,M,(XDOOR,-D2,STEP),(0,-1,0),DWID,DS,STONES,band=band())
G['stone'].box((XDOOR,-D2-.225,STEP/2),(DWID+.8,.45,STEP),M['ashlar'],.015,rect=kit.rng.choice(STONES))
# ---- hanging tankard sign: iron bracket, navy-black board, gilt tankard both faces --------------
G['detail'].box((0,-D2-.03,5.9),(.12,.06,.12),M['iron'],.004)
G['detail'].beam((0,-D2-.06,5.4),(0,-D2-.8,5.9),.035,.035,M['iron'],grain=False)
G['detail'].beam((0,-D2-.06,5.9),(0,SY-.02,5.9),.05,.05,M['iron'],grain=False)
G['detail'].beam((-.32,SY,5.9),(.32,SY,5.9),.03,.03,M['iron'],grain=False)
for x in (-.32,.32):G['detail'].beam((x,SY,5.9),(x,SY,5.65),.02,.02,M['iron'],grain=False)
board=[(-.52,5.65),(.52,5.65),(.52,4.6),(0,4.0),(-.52,4.6)]
G['detail'].prism([(x,SY,z) for x,z in board],(0,1,0),.05,M['sign'])
tank=[(-.2,4.35),(.2,4.35),(.2,4.5),(.38,4.5),(.44,4.7),(.44,5.0),(.38,5.2),(.2,5.2),(.2,5.35),(-.2,5.35)]   # solid silhouette, handle included
for dy in (-.035,.035):G['detail'].prism([(x,SY+dy,z) for x,z in tank],(0,1,0),.02,M['gilt'])
# ---- street dressing: bench and two barrels; wall lanterns either side of the door ---------------
BY=-D2-.55
G['timber'].box((2.7,BY,.45),(1.8,.42,.07),M['planks'],.01,grain=True,band=band())
for lx in (2.7-.75,2.7+.75):G['timber'].box((lx,BY,.21),(.08,.36,.42),M['timber'],.01,grain=True,band=band())
for bx,by in ((-3.0,-D2-.8),(-2.25,-D2-.6)):G['timber'].vessel((bx,by,0),[(.28,0),(.36,.25),(.40,.55),(.36,.85),(.28,1.1),(0,1.1)],M['timber'])
for x in (-2.3,2.3):lantern(G,M,(x,-D2-.42,2.55),arm=((x,-D2-.09,2.55),(0,-1,0)))
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
from mathutils import Matrix
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
assert abs(lo[2])<1e-3,f'model does not stand on the ground: min z {lo[2]}'
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookGildedTankard'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a huge GLB.
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookGildedTankard')  # door leaves: own meshes, hinged
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-40,-40,0),(40,-40,0),(40,40,0),(-40,40,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-13,-17.5,6.2),(84,0,-36),lens=34);cam('Front',(0,-40,7.0),(90,0,0),14.6);cam('Side',(-40,0,7.0),(90,0,-90),14.6);cam('Rear',(0,40,7.0),(90,0,180),14.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'GildedTankard-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'{tris} triangles exceeds the 120k house budget'
manifest=dict(asset='SilverbrookGildedTankard',reference=dict(file='GildedTankard-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_gilded_tankard.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookGildedTankard',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookGildedTankard.blend','SilverbrookGildedTankard.glb','SilverbrookGildedTankard.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('GILDEDTANKARD_BUILT',manifest['bounds_m'],tris)
