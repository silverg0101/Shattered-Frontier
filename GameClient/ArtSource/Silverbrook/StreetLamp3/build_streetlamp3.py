"""Silverbrook Street Lamp 3 from docs/Game-Design/concept-art/Silverbrook/Street Lamp-3.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_streetlamp3.py
Original procedural geometry interpreting the elevation (4 m to the post top, 1.2 m from
lantern to banner edge): square oak post on a stepped stone plinth with four timber shores,
rope lashings and iron bands, a braced gallows arm hanging a hexagonal iron lantern on a chain,
and an iron rod carrying a navy stag banner. Asset BLD-GreenlandsStreetLamp-01, variant 3.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,UP
from mathutils import Matrix
import bpy
scene=kit.reset(4303)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Street Lamp-3.png'
asset=bpy.data.collections.new('SilverbrookStreetLamp3');scene.collection.children.link(asset)

M=dict(
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    rope=kit.scan('SB_StreetLamp3_Rope','hessian_380',dark=(.34,.24,.13),damp=False,aged=.8),
    banner=kit.scan('SB_StreetLamp3_Banner','denim_fabric_03',dark=(.10,.12,.28),damp=False,aged=.8),
    gilt=kit.solid('SB_StreetLamp3_Gilt',(.80,.55,.18),.35,1.0),
    glow=kit.solid('SB_StreetLamp3_Glow',(.3,.2,.1),.3,0,(1.0,.36,.07),3))
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
G=dict(stone=Geo('SM_SBStreetLamp3_Masonry',asset),timber=Geo('SM_SBStreetLamp3_Timber',asset),detail=Geo('SM_SBStreetLamp3_Details',asset))
S,T,D=G['stone'],G['timber'],G['detail']
# ---- measured layout (metres; concept 4 m vertical label, 1.2 m lantern-to-banner label) ----
SLOT=(1.5,1.5,4.2)             # town-layout slot w x d x h
PW=.2;PB,PT=.64,3.82           # post section, foot (plinth top), head
AZ=3.51;AX=-.63                # gallows arm centre height and outer end
LX,LZ=-.5,2.43                 # lantern axis and glass centre
RZ,RX=3.02,.78                 # banner rod height and spear tip
BX,BW,BT,BB=.44,.38,2.88,1.50  # banner centre, width, top, foot

def lathe(geo,c,prof,mat,n=16,phase=0,rot=Matrix.Identity(3),smooth=True):
    """Solid of revolution about local Z through c. prof: (r, z) from the bottom axis outward,
    up and back in to the top axis (kit.Geo.vessel winding), so every face points outward."""
    c=Vector(c);ring=lambda r,z,j:c+rot@Vector((r*math.cos(phase+math.tau*j/n),r*math.sin(phase+math.tau*j/n),z))
    for (ra,za),(rb,zb) in zip(prof,prof[1:]):
        for j in range(n):
            q=[ring(ra,za,j),ring(ra,za,j+1),ring(rb,zb,j+1),ring(rb,zb,j)]
            if ra==0:q=q[1:]
            elif rb==0:q=q[:3]
            geo.poly(q,mat,smooth=smooth)
def coil(z0,z1,r=.142,amp=.024):
    """Rope lashed round the post: a radius rippling once per turn."""
    k=max(4,round((z1-z0)/.05))*4;lathe(D,(0,0,z0),[(0,0)]+[(r+amp*abs(math.sin(math.pi*i/4)),(z1-z0)*i/k) for i in range(k+1)]+[(0,z1-z0)],M['rope'],16)
def fill(geo,pts,axis,th,mat):
    """Flat slab around a convex planar outline (Townhouse1)."""
    pts=[Vector(p) for p in pts];axis=Vector(axis).normalized()
    if sum((a.cross(pts[(i+1)%len(pts)]) for i,a in enumerate(pts)),Vector()).dot(axis)<0:pts.reverse()
    o=axis*th/2;n=len(pts)
    for i in range(1,n-1):geo.poly([pts[0]+o,pts[i]+o,pts[i+1]+o],mat);geo.poly([pts[0]-o,pts[i+1]-o,pts[i]-o],mat)
    for i,a in enumerate(pts):b=pts[(i+1)%n];geo.poly([a-o,b-o,b+o,a+o],mat)
def stag(origin,right,normal,s):
    """Gilt stag head with antlers (Townhouse1)."""
    o=Vector(origin)+Vector(normal)*.008;Rv,Nn=Vector(right),Vector(normal)
    def p(a,z):return o+Rv*(a*s)+UP*(z*s)
    fill(D,[p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],Nn,.012,M['gilt'])
    for sx in (-1,1):
        fill(D,[p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],Nn,.012,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            D.beam(p(sx*a0,z0),p(sx*a1,z1),.012,.03*s,M['gilt'],grain=False)
def fleur(origin,right,normal,s):
    o=Vector(origin)+Vector(normal)*.008;Rv,Nn=Vector(right),Vector(normal)
    def p(a,z):return o+Rv*(a*s)+UP*(z*s)
    fill(D,[p(a,z) for a,z in [(0,-.1),(.07,.12),(0,.42),(-.07,.12)]],Nn,.012,M['gilt'])
    for sx in (-1,1):fill(D,[p(sx*a,z) for a,z in [(.05,.05),(.25,.22),(.3,.05),(.18,-.05)]],Nn,.012,M['gilt'])
    fill(D,[p(a,z) for a,z in [(-.2,-.02),(.2,-.02),(.2,-.1),(-.2,-.1)]],Nn,.012,M['gilt'])
    fill(D,[p(a,z) for a,z in [(-.05,-.1),(.05,-.1),(0,-.3)]],Nn,.012,M['gilt'])
def link(c,vertical_plane_x,h=.065,w=.035,t=.01):
    """One forged chain link, an open rectangle in the XZ or YZ plane."""
    c=Vector(c);e=Vector((1,0,0)) if vertical_plane_x else Vector((0,1,0));n=Vector((0,1,0)) if vertical_plane_x else Vector((1,0,0))
    for s in (-1,1):D.box(c+e*s*(w/2-t/2),(abs(e.x)*t+abs(n.x)*t,abs(e.y)*t+abs(n.y)*t,h),M['iron'],0)
    for s in (-1,1):D.box(c+UP*s*(h/2-t/2),(abs(e.x)*(w-2*t)+abs(n.x)*t,abs(e.y)*(w-2*t)+abs(n.y)*t,t),M['iron'],0)
def chain(top,bottom,pitch=.05):
    top,bottom=Vector(top),Vector(bottom);k=max(1,round((top-bottom).length/pitch))
    for i in range(k):link(top+(bottom-top)*(i+.5)/k,i%2==0,h=(top-bottom).length/k+.022)

# ---- stone plinth and timber shores ---------------------------------------------------
for i in range(3):
    for j in range(3):
        if i==j==1:continue
        S.box(((i-1)*.23,(j-1)*.23,.105),(.22,.22,.21),M['stone'],.02,rect=kit.rng.choice(STONES),w=(kit.rng.uniform(.1,.5),0))
S.box((0,0,.30),(.30,.30,.60),M['stone'],.025,rect=kit.rng.choice(STONES),w=(.3,0))     # core stone, foot hidden in the course
for a in (-1,1):S.box((a*.09,0,.56),(.175,.36,.16),M['stone'],.015,rect=kit.rng.choice(STONES),w=(kit.rng.uniform(.1,.4),0))
for a in ((1,0),(-1,0),(0,1),(0,-1)):
    foot=Vector((a[0]*.31,a[1]*.31,.21));head=Vector((a[0]*.155,a[1]*.155,.49))
    T.beam(foot+Vector((0,0,-.01)),head,.07,.08,M['timber'],band=band())
    D.box(foot+Vector((0,0,.035)),(.10,.10,.05),M['iron'],.004)
# ---- post, finial, bands, lashings --------------------------------------------------------
T.box((0,0,(PB-.02+PT)/2),(PW,PW,PT-PB+.02),M['timber'],.014,grain=True,band=band())
D.box((0,0,PT+.02),(.235,.235,.05),M['iron'],.006)
lathe(D,(0,0,PT+.04),[(0,0),(.075,0),(.075,.05),(.055,.075),(.09,.13),(0,.25)],M['iron'],4,math.pi/4,smooth=False)
for z0,z1 in ((.645,.80),(2.38,2.52),(3.70,3.78)):
    D.box((0,0,(z0+z1)/2),(.23,.23,z1-z0),M['iron'],.006)
    for k in range(4):
        n=Vector((math.cos(k*math.pi/2),math.sin(k*math.pi/2),0))
        for s in (-1,1):D.box(n*.118+n.cross(UP)*s*.06+UP*(z0+z1)/2,(.022,.022,.022),M['iron'],0)
coil(1.24,1.45);coil(2.60,2.80);coil(3.36,3.62)
# ---- gallows arm, brace, lantern chain -------------------------------------------------------
T.beam((AX,0,AZ),(-.02,0,AZ),.12,.14,M['timber'],band=band())
T.beam((-.31,0,AZ-.05),(-.03,0,2.79),.09,.09,M['timber'],band=band())
D.box((AX+.06,0,AZ),(.05,.155,.165),M['iron'],.004)
D.box((LX,0,AZ-.075),(.07,.07,.012),M['iron'],0)            # eye plate under the arm
chain((LX,0,AZ-.075),(LX,0,2.935))
# ---- hexagonal lantern: glow glass, iron cage with diamond lattice -----------------------------
G0,G1,R=2.19,2.67,.13
lathe(D,(LX,0,0),[(0,G0),(R,G0),(R,G1),(0,G1)],M['glow'],6,smooth=False)
ap=R*math.cos(math.pi/6);vert=[Vector((LX+R*math.cos(math.tau*k/6),R*math.sin(math.tau*k/6),0)) for k in range(7)]
def out(v,d):return Vector((LX,0,v.z))+(v-Vector((LX,0,v.z)))*(1+d/ap)
for v in vert[:6]:D.box(out(v,.006)+UP*(G0+G1)/2,(.02,.02,G1-G0+.02),M['iron'],0)
for z in (G0+.01,(G0+G1)/2,G1-.01):
    for a,b in zip(vert,vert[1:]):D.beam(out(a,.007)+UP*z,out(b,.007)+UP*z,.018,.01,M['iron'],grain=False)
for a,b in zip(vert,vert[1:]):
    for z0 in (G0,(G0+G1)/2):
        h=(G1-G0)/2;D.beam(out(a,.006)+UP*z0,out(b,.006)+UP*(z0+h),.01,.007,M['iron'],grain=False);D.beam(out(b,.006)+UP*z0,out(a,.006)+UP*(z0+h),.01,.007,M['iron'],grain=False)
lathe(D,(LX,0,0),[(0,G1-.005),(.165,G1-.005),(.165,G1+.03),(.12,G1+.09),(.07,G1+.13),(.045,G1+.17),(0,G1+.19)],M['iron'],6,smooth=False)
for v in vert[:6]:lathe(D,Vector((LX,0,G1+.03))+(v-Vector((LX,0,0)))*(.15/R),[(0,0),(.013,0),(0,.05)],M['iron'],4,smooth=False)
for s in (-1,1):D.box((LX+s*.025,0,2.90),(.012,.012,.07),M['iron'],0)
D.box((LX,0,2.932),(.062,.012,.012),M['iron'],0)
lathe(D,(LX,0,0),[(0,1.94),(.03,2.03),(.10,2.11),(.165,2.15),(.165,G0+.005),(0,G0+.005)],M['iron'],6,smooth=False)
SOCKET=Vector((LX,0,LZ))
# ---- banner rod, strut, hangers, batten ------------------------------------------------------
D.box((PW/2+.012,0,RZ),(.024,.10,.17),M['iron'],.004)
D.beam((PW/2,0,RZ),(.70,0,RZ),.032,.032,M['iron'],grain=False)
lathe(D,(.70,0,RZ),[(0,0),(.035,0),(0,.08)],M['iron'],4,rot=Matrix(((0,0,1),(0,1,0),(-1,0,0))),smooth=False)
D.box((.655,0,RZ),(.03,.05,.05),M['iron'],.004)
D.beam((PW/2+.01,0,2.70),(.32,0,RZ-.01),.02,.025,M['iron'],grain=False)
for x in (BX-BW/2+.03,BX,BX+BW/2-.03):
    lathe(D,(x,0,RZ),[(0,-.03),(.024,-.03),(.024,.03),(0,.03)],M['iron'],8,rot=Matrix(((0,0,1),(0,1,0),(-1,0,0))),smooth=False)
    chain((x,0,RZ-.03),(x,0,BT+.03),.045)
T.beam((BX-BW/2-.05,0,BT+.01),(BX+BW/2+.05,0,BT+.01),.04,.04,M['timber'],band=band())
for s in (-1,1):D.box((BX+s*(BW/2+.055),0,BT+.01),(.03,.05,.05),M['iron'],.004)
# ---- banner: navy cloth, gilt edging and fringe, stag and fleur on both faces --------------------
outline=[(-BW/2,BT-.005),(BW/2,BT-.005),(BW/2,BB+.32),(0,BB),(-BW/2,BB+.32)]
D.prism([(BX+x,0,z) for x,z in outline],(0,1,0),.02,M['banner'])
for y in (-.017,.017):
    for (x0,z0),(x1,z1) in zip(outline,outline[1:]+outline[:1]):D.beam((BX+x0,y,z0),(BX+x1,y,z1),.025,.012,M['gilt'],grain=False)
for (x0,z0),(x1,z1) in ((outline[2],outline[3]),(outline[3],outline[4])):
    for k in range(6):
        t0,t1=k/6,(k+1)/6;a=Vector((BX+x0+(x1-x0)*t0,0,z0+(z1-z0)*t0));b=Vector((BX+x0+(x1-x0)*t1,0,z0+(z1-z0)*t1))
        fill(D,[a,b,(a+b)/2-UP*kit.rng.uniform(.04,.07)],(0,1,0),.012,M['gilt'])
for y,r,n in ((-.011,(1,0,0),(0,-1,0)),(.011,(-1,0,0),(0,1,0))):
    stag((BX,y,2.36),r,n,.44);fleur((BX,y,1.93),r,n,.28)
# ---- export, preview scene, manifest ---------------------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
shift=Vector((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0))
for o in objs:o.data.transform(Matrix.Translation(shift))
assert abs(lo[2])<1e-6,f'not on the ground: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
SOCKET+=shift
OUT=HERE/'SilverbrookStreetLamp3'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
pl=bpy.data.lights.new('LanternLight','POINT');pl.energy=25;pl.color=(1,.6,.3);pl.shadow_soft_size=.05;po=bpy.data.objects.new('LanternLight',pl);po.location=SOCKET;pre.objects.link(po)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-2.7,-4.4,2.7),(82,0,-31.5),lens=33);cam('Front',(0,-30,2.06),(90,0,0),4.4);cam('Side',(-30,0,2.06),(90,0,-90),4.4);cam('Rear',(0,30,2.06),(90,0,180),4.4)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'StreetLamp3-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=40000,f'small prop over budget: {tris}'
manifest=dict(asset='SilverbrookStreetLamp3',asset_id='BLD-GreenlandsStreetLamp-01',variant=3,reference=dict(file='StreetLamp3-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_streetlamp3.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookStreetLamp3',slot_m=list(SLOT),
    light_socket_m=[[round(v,3) for v in SOCKET]],light_material='SB_StreetLamp3_Glow',outputs=['SilverbrookStreetLamp3.blend','SilverbrookStreetLamp3.glb','SilverbrookStreetLamp3.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('STREETLAMP3_BUILT',manifest['bounds_m'],tris,manifest['light_socket_m'])
