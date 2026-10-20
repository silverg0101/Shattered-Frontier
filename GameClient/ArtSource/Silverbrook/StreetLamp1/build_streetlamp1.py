"""Silverbrook Street Lamp 1 from docs/Game-Design/concept-art/Silverbrook/Street Lamp-1.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_streetlamp1.py
Original procedural geometry interpreting the elevation (4 m to the post shoulders, ~1 m arm):
stepped rock-faced stone plinth with an iron band, iron-shod oak post with riveted bands and
a spike finial, curved brace and iron scroll under a timber arm, and a chained hexagonal
lantern whose glass is its own emissive slot (SB_StreetLamp1_Glow) for an Unreal point light.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
import bpy
scene=kit.reset(4501)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Street Lamp-1.png'
asset=bpy.data.collections.new('SilverbrookStreetLamp1');scene.collection.children.link(asset)

M=dict(
    stone=kit.scan('SB_StreetLamp1_Plinth','rough_block_wall',dark=(.36,.38,.30),aged=.8),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    glow=kit.solid('SB_StreetLamp1_Glow',(.45,.22,.05),.25,0,(1.0,.4,.08),2.5))
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
G=dict(stone=Geo('SM_SBStreetLamp1_Plinth',asset),timber=Geo('SM_SBStreetLamp1_Timber',asset),detail=Geo('SM_SBStreetLamp1_Details',asset))
# ---- measured layout (metres; concept scale 255 px/m from the 4 m dimension) -----
SLOT=(1.5,1.5,4.2)             # town-layout slot w x d x h (concept README 'Fits')
P2=.11                         # post half-width
FOOT,TOP=.70,3.80              # post foot (plinth top) and top
ARMZ,ARMH=3.49,.16             # arm centre height and depth
LX,LZ=.83,2.55                 # lantern axis x, glass centre (light socket before recentring)

def revolve(geo,origin,axis,profile,mat,seg=8,phase=0,smooth=False):
    """Convex solid of revolution about `axis` from (radius, distance) rings; flat facets by default.
    seg=6 gives the hexagonal lantern. Faces are wound outward from the solid's centroid."""
    o,ax=Vector(origin),Vector(axis).normalized();u=ax.orthogonal().normalized();v=ax.cross(u)
    rings=[[o+ax*d+(u*math.cos(phase+math.tau*j/seg)+v*math.sin(phase+math.tau*j/seg))*r for j in range(seg)] for r,d in profile]
    faces=[list(reversed(rings[0]))] if profile[0][0]>0 else []
    if profile[-1][0]>0:faces.append(rings[-1])
    for (ra,_),(rb,_),A,B in zip(profile,profile[1:],rings,rings[1:]):
        for j in range(seg):
            k=(j+1)%seg;f=[A[j],A[k],B[k],B[j]]
            if ra==0:f=[A[j],B[k],B[j]]
            elif rb==0:f=[A[j],A[k],B[j]]
            faces.append(f)
    c=sum((p for r in rings for p in r),Vector())/(len(rings)*seg);vol=0
    for f in faces:
        n=sum((a.cross(f[(i+1)%len(f)]) for i,a in enumerate(f)),Vector())
        if n.dot(sum(f,Vector())/len(f)-c)<0:f.reverse()
        geo.poly(f,mat,smooth=smooth);vol+=sum((f[0]-c).dot((f[i]-c).cross(f[i+1]-c)) for i in range(1,len(f)-1))
    assert vol>0,'inside-out revolve'
def rivets(geo,z,half,n=2,size=.024):
    """Round-headed rivets (8-sided domes) on all four faces of a square band of half-width `half`."""
    for k in range(4):
        out=Vector((math.cos(k*math.pi/2),math.sin(k*math.pi/2),0));side=Vector((-out.y,out.x,0))
        for i in range(n):
            a=(i-(n-1)/2)*half*1.2/max(n-1,1);revolve(geo,out*(half-.004)+side*a+Vector((0,0,z)),out,[(size/2,0),(size/2,.006),(size/3,.012),(0,.016)],M['iron'])
def iron_band(z,h,half,n=2):G['detail'].box((0,0,z),(2*half,2*half,h),M['iron'],.006);rivets(G['detail'],z,half+.0,n)

# ---- stepped stone plinth, iron band ----------------------------------------------------------
for z0,z1,s in ((0,.13,.68),(.12,.27,.62),(.26,.39,.58),(.49,FOOT,.50)):G['stone'].box((0,0,(z0+z1)/2),(s,s,z1-z0),M['stone'],.02)
G['stone'].box((0,0,.44),(.54,.54,.12),M['stone'],0)          # core behind the band
iron_band(.44,.11,.30,3)
# ---- iron-shod oak post ------------------------------------------------------------------------
G['timber'].box((0,0,(FOOT-.01+TOP)/2),(2*P2,2*P2,TOP-FOOT+.01),M['timber'],.015,grain=True,band=band())
G['detail'].box((0,0,FOOT+.025),(.32,.32,.05),M['iron'],.006)  # shoe plate on the plinth
revolve(G['timber'],(0,0,FOOT+.045),(0,0,1),[(.145*math.sqrt(2),0),(P2*math.sqrt(2),.2)],M['timber'],4,math.pi/4)   # flared foot
for k in range(4):              # flared straps: one on each face, one on each corner
    out=Vector((math.cos(k*math.pi/2),math.sin(k*math.pi/2),0));diag=Vector((math.cos(k*math.pi/2+math.pi/4),math.sin(k*math.pi/2+math.pi/4),0))*math.sqrt(2)
    G['detail'].beam(out*.15+Vector((0,0,FOOT+.03)),out*(P2+.004)+Vector((0,0,FOOT+.24)),.06,.012,M['iron'],grain=False)
    G['detail'].beam(diag*.15+Vector((0,0,FOOT+.03)),diag*(P2+.002)+Vector((0,0,FOOT+.24)),.03,.03,M['iron'],grain=False)
iron_band(FOOT+.25,.06,P2+.012,2)
for z in (1.57,2.68):iron_band(z,.12,P2+.012)
iron_band(ARMZ,.20,P2+.014)
iron_band(TOP-.05,.12,P2+.014)
G['detail'].box((0,0,TOP+.025),(.15,.15,.05),M['iron'],.006)
revolve(G['detail'],(0,0,TOP+.05),(0,0,1),[(.05,0),(.05,.03),(.085,.06),(.095,.13),(0,.31)],M['iron'],8,math.pi/8)
# ---- arm, brace, scroll ------------------------------------------------------------------------
G['timber'].beam((-.02,0,ARMZ),(.93,0,ARMZ),.13,ARMH,M['timber'],band=band())
for x in (.74,.80):G['detail'].box((x,0,ARMZ),(.035,.15,ARMH+.02),M['iron'],.004)
revolve(G['detail'],(.92,0,ARMZ),(1,0,0),[(.045,0),(.065,.03),(.07,.07),(0,.18)],M['iron'],8,math.pi/8)
AB=ARMZ-ARMH/2                 # arm underside
C,RX,RZ=Vector((.50,0,2.80)),.40,AB-2.80+.01
arc=[C+Vector((-RX*math.cos(t),0,RZ*math.sin(t))) for t in [i*math.pi/20 for i in range(11)]]
for a,b in zip(arc,arc[1:]):G['timber'].beam(a+(a-b)*.04,b+(b-a)*.04,.10,.085,M['timber'],band=band())
G['timber'].box((P2+.04,0,2.76),(.09,.10,.10),M['timber'],.012,grain=True,band=band())   # brace foot nub
S0=Vector((.64,0,3.31));spiral=[S0+Vector((math.cos(t),0,math.sin(t)))*(.09*(1-(t-math.pi*.5)/10)) for t in [math.pi*.5+i*.2 for i in range(44)]]
spiral=[Vector((.66,0,AB+.01))]+spiral   # foot welded to the arm underside
for a,b in zip(spiral,spiral[1:]):G['detail'].beam(a,b,.022,.018,M['iron'],grain=False)
# ---- chain and lantern -------------------------------------------------------------------------
G['detail'].box((LX,0,AB-.012),(.06,.04,.03),M['iron'],.004)      # hanger eye plate
def link(z,turn):
    rot=Matrix.Rotation(turn,3,'Z');pts=[Vector((.022*math.cos(t),0,.04*math.sin(t))) for t in [i*math.tau/8 for i in range(8)]]
    for i,p in enumerate(pts):a,b=rot@p+Vector((LX,0,z)),rot@pts[(i+1)%8]+Vector((LX,0,z));G['detail'].beam(a+(a-b)*.08,b+(b-a)*.08,.011,.011,M['iron'],grain=False)
for i in range(4):link(AB-.055-i*.065,i*math.pi/2)
# body (absolute z): roof 2.86-3.05, eave plate 2.83-2.86, bands, glass 2.32-2.78, base 2.22-2.26, drop 2.12
L=Vector((LX,0,0));H=math.pi/6
revolve(G['detail'],L+Vector((0,0,3.05)),(0,0,1),[(.028,0),(.028,.06),(0,.09)],M['iron'],6,H)     # hanger knob into the last link
revolve(G['detail'],L+Vector((0,0,2.855)),(0,0,1),[(.20,0),(.035,.20)],M['iron'],6,H)
revolve(G['detail'],L+Vector((0,0,2.83)),(0,0,1),[(.215,0),(.215,.03)],M['iron'],6,H)
revolve(G['detail'],L+Vector((0,0,2.775)),(0,0,1),[(.18,0),(.18,.06)],M['iron'],6,H)
revolve(G['detail'],L+Vector((0,0,2.315)),(0,0,1),[(.155,0),(.155,.465)],M['glow'],6,H)
revolve(G['detail'],L+Vector((0,0,2.26)),(0,0,1),[(.18,0),(.18,.06)],M['iron'],6,H)
revolve(G['detail'],L+Vector((0,0,2.22)),(0,0,1),[(.20,0),(.20,.045)],M['iron'],6,H)
revolve(G['detail'],L+Vector((0,0,2.225)),(0,0,-1),[(.07,0),(.05,.035),(0,.105)],M['iron'],6,H)
def hexpt(j,r):return L+Vector((math.cos(H+j*math.pi/3),math.sin(H+j*math.pi/3),0))*r
for j in range(6):
    G['detail'].beam(hexpt(j,.163)+Vector((0,0,2.30)),hexpt(j,.163)+Vector((0,0,2.79)),.026,.026,M['iron'],grain=False)
    c,d=hexpt(j,.155),hexpt(j+1,.155);out=((c+d)/2-L).normalized()*.004    # cames lie on the glass face
    q=lambda f,z:c+(d-c)*f+out+Vector((0,0,z))
    G['detail'].beam(q(.5,2.32),q(.5,2.775),.012,.008,M['iron'],grain=False)               # mullion
    for (f0,z0),(f1,z1) in (((.06,2.55),(.5,2.76)),((.5,2.76),(.94,2.55)),((.94,2.55),(.5,2.34)),((.5,2.34),(.06,2.55)),((.06,2.68),(.5,2.775)),((.5,2.775),(.94,2.68))):
        G['detail'].beam(q(f0,z0),q(f1,z1),.010,.008,M['iron'],grain=False)              # diamond and pointed-arch cames
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
shift=Vector((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0))
for o in objs:o.data.transform(Matrix.Translation(shift))
assert lo[2]>-1e-6,f'below ground: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
SOCKET=Vector((LX,0,LZ))+shift
OUT=HERE/'SilverbrookStreetLamp1'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
pl=bpy.data.lights.new('LanternLight','POINT');pl.energy=25;pl.color=(1,.6,.25);pl.shadow_soft_size=.05;po=bpy.data.objects.new('LanternLight',pl);po.location=SOCKET;pre.objects.link(po)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-3.4,-4.6,2.3),(85,0,-36),lens=34);cam('Front',(0,-30,2.1),(90,0,0),4.5);cam('Side',(-30,0,2.1),(90,0,-90),4.5);cam('Rear',(0,30,2.1),(90,0,180),4.5)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'StreetLamp1-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=40000,f'small-prop budget: {tris} triangles'
manifest=dict(asset='SilverbrookStreetLamp1',asset_id='BLD-GreenlandsStreetLamp-01',variant=1,reference=dict(file='StreetLamp1-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_streetlamp1.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookStreetLamp1',slot_m=list(SLOT),
    light_socket_m=[round(v,3) for v in SOCKET],light_material='SB_StreetLamp1_Glow',
    outputs=['SilverbrookStreetLamp1.blend','SilverbrookStreetLamp1.glb','SilverbrookStreetLamp1.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('STREETLAMP1_BUILT',manifest['bounds_m'],tris,manifest['light_socket_m'])
