"""Silverbrook Street Lamp 2 from docs/Game-Design/concept-art/Silverbrook/Street Lamp-2.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_streetlamp2.py
Original procedural geometry interpreting the elevation (labels: 1.5 m wide, 4.5 m tall): a forged-iron
column on a tapered, bolted pedestal with fleur-de-lis panels, two scrolled arms with fleur-de-lis
ornaments, a fleur-de-lis and spear finial, and two hexagonal Gothic lanterns hung on short chains.
Every part is placed in the concept's own pixel coordinates (P below) so it can be traced to the image.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
from mathutils import Matrix
import bpy
scene=kit.reset(4141)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Street Lamp-2.png'
asset=bpy.data.collections.new('SilverbrookStreetLamp2');scene.collection.children.link(asset)

M=dict(
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    bronze=kit.solid('SB_StreetLamp2_Bronze',(.30,.19,.07),.45,1.0),
    glow=kit.solid('SB_StreetLamp2_Glow',(.6,.3,.06),.2,0,(1.0,.42,.08),2.5))
G=dict(iron=Geo('SM_SBStreetLamp2_Iron',asset),lantern=Geo('SM_SBStreetLamp2_Lanterns',asset))
# ---- concept pixels -> metres ---------------------------------------------------------
# The two labels disagree with the drawing's proportions (1.5 m spans 435 px, 4.5 m spans 1050 px),
# so each axis gets its own scale: 4.5 m from the ground line (y 1190) to the spear tip (y 95), and
# 1.5 m across the lantern bands (x 327..792). Depth uses the horizontal scale.
SLOT=(1.5,1.5,4.5)
KX,KZ=.0032,4.49/(1190-95)
def X(px):return (px-560)*KX
def Z(py):return (1190-py)*KZ
def P(px,py,y=0):return Vector((X(px),y,Z(py)))

def emit(geo,verts,faces,mat,smooth=False):
    """Closed mesh from index faces; flipped as a whole if it came out inside-out."""
    vol=sum(verts[f[0]].dot(verts[f[j]].cross(verts[f[j+1]])) for f in faces for j in range(1,len(f)-1))
    if vol<0:faces=[f[::-1] for f in faces]
    assert abs(vol)>1e-9,'degenerate part'
    for f in faces:geo.poly([verts[i] for i in f],mat,smooth=smooth)
def lathe(geo,c,profile,mat,seg=24,phase=0,smooth=None):
    """Solid of revolution about Z through c; profile (r,z) runs bottom axis -> top axis."""
    c=Vector(c);verts=[];rings=[]
    for r,z in profile:
        if r==0:rings.append([len(verts)]);verts.append(c+Vector((0,0,z)))
        else:
            rings.append(list(range(len(verts),len(verts)+seg)))
            verts+=[c+Vector((r*math.cos(phase+math.tau*j/seg),r*math.sin(phase+math.tau*j/seg),z)) for j in range(seg)]
    faces=[]
    for a,b in zip(rings,rings[1:]):
        for j in range(seg):
            j1=(j+1)%seg
            if len(a)==1:faces.append((a[0],b[j],b[j1]))
            elif len(b)==1:faces.append((a[j],a[j1],b[0]))
            else:faces.append((a[j],a[j1],b[j1],b[j]))
    emit(geo,verts,faces,mat,seg>8 if smooth is None else smooth)
def smooth_path(pts,n=4):
    """Catmull-Rom through the control points."""
    pts=[Vector(p) for p in pts];ext=[pts[0]]+pts+[pts[-1]];out=[]
    for i in range(1,len(ext)-2):
        p0,p1,p2,p3=ext[i-1:i+3]
        for k in range(n):
            t=k/n;out.append(.5*((2*p1)+(-p0+p2)*t+(2*p0-5*p1+4*p2-p3)*t*t+(-p0+3*p1-3*p2+p3)*t**3))
    return out+[pts[-1]]
def tube(geo,pts,r,mat,normal=(0,1,0),seg=8,closed=False,taper=1.0):
    """Round bar swept along a planar path (plane normal `normal`), capped unless closed."""
    n=Vector(normal).normalized();verts=[];N=len(pts)
    for i,p in enumerate(pts):
        t=(pts[(i+1)%N]-pts[i-1]) if closed else (pts[min(i+1,N-1)]-pts[max(i-1,0)]);t.normalize();b=t.cross(n);rr=r*(1-(1-taper)*i/(N-1))
        verts+=[p+rr*(math.cos(math.tau*j/seg)*n+math.sin(math.tau*j/seg)*b) for j in range(seg)]
    faces=[(i*seg+j,i*seg+(j+1)%seg,((i+1)%N)*seg+(j+1)%seg,((i+1)%N)*seg+j) for i in range(N if closed else N-1) for j in range(seg)]
    if not closed:faces+=[tuple(range(seg))[::-1],tuple(range((N-1)*seg,N*seg))]
    emit(geo,verts,faces,mat,True)
def scroll(geo,ctrl,r,mat,**kw):tube(geo,smooth_path([P(*q) for q in ctrl]),r,mat,**kw)
def mirror(ctrl):return [(1120-x,y) for x,y in ctrl]
def fleur(geo,o,U,V,W,h,mat,th=None):
    """Fleur-de-lis of height h from convex pieces: lancet petal, band, two curled petals, tail."""
    o,U,V,W=Vector(o),Vector(U),Vector(V),Vector(W);th=th or h*.16
    def Q(u,v):return o+U*u*h+V*v*h
    geo.prism([Q(u,v) for u,v in ((0,1),(.09,.78),(.11,.6),(.07,.42),(0,.34),(-.07,.42),(-.11,.6),(-.09,.78))],W,th,mat)
    geo.prism([Q(u,v) for u,v in ((-.07,.36),(.07,.36),(0,.04))],W,th*.8,mat)
    geo.prism([Q(u,v) for u,v in ((-.2,.35),(.2,.35),(.2,.45),(-.2,.45))],W,th*1.3,mat)
    for s in (-1,1):
        tube(geo,smooth_path([Q(s*u,v) for u,v in ((.05,.47),(.2,.56),(.33,.62),(.43,.57),(.44,.47),(.37,.42),(.31,.47))],3),h*.05,mat,W,6,taper=.6)
        tube(geo,smooth_path([Q(s*u,v) for u,v in ((.04,.31),(.15,.25),(.2,.17),(.15,.1))],3),h*.035,mat,W,6,taper=.6)
def stud(geo,p,s=.022,mat=None):geo.box(p,(s,s,s),mat or M['bronze'],.004)
I,B=G['iron'],M['bronze']
# ---- pedestal (orig y 1190..842) ----------------------------------------------------------
I.box((0,0,.078),(.59,.59,.156),M['iron'],.012)                                   # plinth slab
for sx in (-1,1):
    for sy in (-1,1):I.box((sx*.24,sy*.24,.083),(.13,.13,.166),M['iron'],.01);stud(I,(sx*.24,sy*.305,.09),.03)
R2=math.sqrt(2);HB,HT,ZB,ZT=.19,.135,.15,.70                                        # tapered body
lathe(I,(0,0,0),[(0,ZB),(HB*R2,ZB),(HT*R2,ZT),(0,ZT)],M['iron'],4,math.pi/4,False)
for sx in (-1,1):
    for sy in (-1,1):                                                                # flared corner pilasters
        d=Vector((sx,sy,0))
        I.beam(d*.225+Vector((0,0,ZB)),d*.165+Vector((0,0,.36)),.06,.06,M['iron'],grain=False)
        I.beam(d*.165+Vector((0,0,.34)),d*.138+Vector((0,0,ZT)),.055,.055,M['iron'],grain=False)
tilt=math.atan2(HB-HT,ZT-ZB)
for k in range(4):                                                                   # bronze-framed fleur panels
    a=math.pi/2*k;out=Vector((math.sin(a),-math.cos(a),0));U=Vector((math.cos(a),math.sin(a),0));V=(Vector((0,0,1))*math.cos(tilt)+out*math.sin(tilt))
    def F(u,v,d=.0):return out*(HB-(v-ZB)*math.tan(tilt)+d)+U*u+Vector((0,0,v))
    for (u0,v0),(u1,v1) in (((-.1,.22),(.1,.22)),((-.1,.62),(.1,.62)),((-.1,.22),(-.1,.62)),((.1,.22),(.1,.62))):
        I.beam(F(u0,v0,.004),F(u1,v1,.004),.016,.012,B,grain=False)
    fleur(I,F(0,.29,.008),U,V,out,.22,B,.02)
    for u in (-.1,.1):
        for v in (.22,.62):stud(I,F(u,v,.012),.02)
I.box((0,0,.745),(.39,.39,.11),M['iron'],.02)                                      # pedestal cap (orig 995..1020)
for k in range(4):
    a=math.pi/2*k
    for u in (-.12,0,.12):stud(I,(Matrix.Rotation(a,3,'Z')@Vector((u,-.2,.745))),.026)
lathe(I,(0,0,0),[(0,.79),(.17,.79),(.17,.82),(.105,.975),(.1,1.02),(0,1.02)],M['iron'],8,math.pi/8,False)   # bell
lathe(I,(0,0,0),[(0,1.015),(.13,1.015),(.13,1.10),(0,1.10)],M['iron'],8,math.pi/8,False)                   # bolted band
for k in range(8):a=math.tau*(k+.5)/8;stud(I,(.128*math.cos(a),.128*math.sin(a),1.057),.022)
lathe(I,(0,0,0),[(0,1.095),(.1,1.095),(.072,1.29),(0,1.29)],M['iron'],8,math.pi/8,False)                    # taper
lathe(I,(0,0,0),[(0,1.285),(.09,1.285),(.1,1.31),(.1,1.36),(.088,1.39),(.075,1.395),(.075,1.43),(0,1.43)],M['iron'],24)
lathe(I,(0,0,0),[(0,1.355),(.103,1.355),(.103,1.37),(0,1.37)],B,24)                                          # bronze bead
# ---- column (orig y 842..367) -----------------------------------------------------------------
lathe(I,(0,0,0),[(0,1.42),(.055,1.42),(.055,3.39),(0,3.39)],M['iron'],16)
for z0,z1,r in ((Z(672),Z(645),.074),(Z(470),Z(437),.072),(Z(400),Z(380),.066)):
    lathe(I,(0,0,0),[(0,z0),(r-.008,z0),(r,z0+.01),(r,z1-.01),(r-.008,z1),(0,z1)],M['iron'],24)
    lathe(I,(0,0,0),[(0,(z0+z1)/2-.007),(r+.004,(z0+z1)/2-.007),(r+.004,(z0+z1)/2+.007),(0,(z0+z1)/2+.007)],B,24)
def collar(z0,z1,half):
    """Square bolted collar with side flanges for the arms (orig y 345..367 and 252..272)."""
    I.box((0,0,(z0+z1)/2),(.15,.15,z1-z0),M['iron'],.008);I.box((0,0,(z0+z1)/2),(2*half,.09,z1-z0-.012),M['iron'],.008)
    for x in (0,-half+.03,half-.03):
        for y in ((-.077,.077) if x==0 else (-.047,.047)):stud(I,(x,y,(z0+z1)/2),.026)
collar(Z(367),Z(345),.15)
for s in (-1,1):lathe(I,(s*.115,0,0),[(0,Z(385)),(.012,Z(380)),(.026,Z(372)),(.024,Z(366)),(0,Z(364))],B,12)   # pendant bosses
I.box((0,0,(Z(345)+Z(272))/2),(.124,.124,Z(272)-Z(345)+.01),M['iron'],.006)      # square section
for y in (-.064,.064):fleur(I,(0,y,Z(345)+.07),(1,0,0),(0,0,1),(0,math.copysign(1,y),0),.13,B,.012)
collar(Z(272),Z(252),.165)
I.box((0,0,(Z(252)+Z(225))/2),(.112,.112,Z(225)-Z(252)+.006),M['iron'],.006)
I.box((0,0,Z(225)+.015),(.16,.16,.035),M['iron'],.008)
lathe(I,(0,0,0),[(0,Z(222)),(.035,Z(222)),(.035,Z(212)),(.046,Z(205)),(.046,Z(196)),(.032,Z(190)),(.03,Z(178)),(.042,Z(175)),(0,Z(173))],M['iron'],16)
for s in (-1,1):scroll(I,[(560+s*6,203),(560+s*25,192),(560+s*38,192),(560+s*42,202),(560+s*35,208),(560+s*29,202)],.011,M['iron'])
fleur(I,(0,0,Z(178)),(1,0,0),(0,0,1),(0,1,0),Z(135)-Z(178),B,.03)
fleur(I,(0,0,Z(178)),(0,1,0),(0,0,1),(-1,0,0),Z(135)-Z(178),B,.03)                 # inferred: crossed for depth
lathe(I,(0,0,0),[(0,Z(140)),(.045,Z(120)),(0,Z(95))],B,4,0,False)                  # spear point
# ---- arms (right side traced, left mirrored) -----------------------------------------------------
MAIN=[(570,262),(595,245),(625,210),(665,187),(705,180),(740,185),(762,202),(770,227),(760,250),(738,258),(725,247),(723,233),(732,225),(742,229),(742,238),(735,240)]
TOP=[(700,184),(712,170),(720,158),(716,149),(709,151),(710,158)]
INNER=[(708,186),(724,198),(731,214),(731,226)]
LOWER=[(572,360),(588,330),(605,290),(625,272),(648,268),(662,280),(662,298),(650,305),(641,298),(643,288),(651,289)]
for s in (1,-1):
    f=(lambda c:c) if s>0 else mirror
    scroll(I,f(MAIN),.02,M['iron']);scroll(I,f(TOP),.013,M['iron'],taper=.7);scroll(I,f(INNER),.01,M['iron']);scroll(I,f(LOWER),.016,M['iron'],taper=.75)
    I.beam(P(*f([(700,181)])[0]),P(*f([(712,181)])[0]),.055,.05,B,grain=False)     # bronze band on the arm
    fleur(I,P(*f([(660,279)])[0]),(1,0,0),(0,0,1),(0,1,0),Z(228)-Z(279),B,.028)
# ---- lanterns ------------------------------------------------------------------------------------------
L=G['lantern'];RB,RG,CX=.17,.148,.564
SOCKETS=[]
for s in (1,-1):
    c=Vector((s*CX,0,0));zt=Z(258)                                                   # chain from the arm loop
    for k in range(3):
        zc=zt-.035-k*.07;ax=(0,1,0) if k%2==0 else (1,0,0);side=Vector((1,0,0)) if k%2==0 else Vector((0,1,0))
        path=[c+Vector((0,0,zc))+side*.022*math.cos(t)+Vector((0,0,.044))*math.sin(t) for t in [math.tau*i/16 for i in range(16)]]
        tube(L,path,.0065,M['iron'],ax,6,closed=True)
    zk=zt-.035-2*.07-.04
    lathe(L,c,[(0,Z(345)-.005),(.06,Z(345)-.005),(.07,Z(345)+.015),(.04,Z(345)+.06),(.03,Z(345)+.08),(.036,zk-.01),(.014,zk+.02),(0,zk+.02)],B,16)
    lathe(L,c,[(0,Z(387)-.005),(RB-.002,Z(387)-.005),(.06,Z(345)),(0,Z(345))],M['iron'],6,0,False)   # roof
    lathe(L,c,[(0,Z(397)),(RB,Z(397)),(RB,Z(385)),(0,Z(385))],M['iron'],6,0,False)                 # top band
    lathe(L,c,[(0,Z(506)),(RG,Z(506)),(RG,Z(396)),(0,Z(396))],M['glow'],6,0,False)                 # glass
    lathe(L,c,[(0,Z(520)),(RB,Z(520)),(RB,Z(505)),(0,Z(505))],M['iron'],6,0,False)                 # base band
    lathe(L,c,[(0,Z(550)),(.045,Z(550)),(.14,Z(521)),(0,Z(519))],M['iron'],6,0,False)              # bottom cone
    lathe(L,c,[(0,Z(580)),(.012,Z(572)),(.03,Z(562)),(.034,Z(553)),(.02,Z(546)),(0,Z(546))],B,12)  # drip
    for j in range(6):
        a=math.tau*j/6;cor=c+Vector((math.cos(a),math.sin(a),0))
        L.box(c+Vector((RG*math.cos(a),RG*math.sin(a),(Z(506)+Z(396))/2)),(.022,.022,Z(396)-Z(506)),M['iron'],.004,Matrix.Rotation(a,3,'Z'))
        lathe(L,c+Vector(((RB-.008)*math.cos(a),(RB-.008)*math.sin(a),0)),[(0,Z(386)),(.013,Z(386)),(.01,Z(372)),(0,Z(355))],B,4,a,False)  # corner spikes
        for zz in (Z(391),Z(512)):stud(L,c+Vector(((RB+.004)*math.cos(a),(RB+.004)*math.sin(a),zz)),.016)
        # face tracery: pointed arch over a diamond lattice, on the glass face between corners a and a+60
        a1=math.tau*(j+1)/6;p0=c+Vector((RG*math.cos(a),RG*math.sin(a),0));p1=c+Vector((RG*math.cos(a1),RG*math.sin(a1),0))
        mid=(p0+p1)/2;tan=(p1-p0).normalized();nrm=Vector((mid.x-c.x,mid.y-c.y,0)).normalized();half=(p1-p0).length/2-.012
        def T(u,z):return mid+tan*u+nrm*.004+Vector((0,0,z))
        z0,z1=Z(506)+.01,Z(396)-.012;spring=z1-.005-2*half*math.sin(math.pi/3)   # equilateral pointed arch
        for sg in (-1,1):tube(L,[T(sg*(2*half*math.cos(t)-half),spring+2*half*math.sin(t)) for t in [i*math.pi/3/6 for i in range(7)]],.0045,M['iron'],nrm,6)
        tube(L,[T(-half,z0),T(-half,spring)],.0045,M['iron'],nrm,6);tube(L,[T(half,z0),T(half,spring)],.0045,M['iron'],nrm,6)
        slope=2.2;step=.09
        for sg in (1,-1):
            for k in range(-8,9):
                b=z0+.02+k*step;pts=[]
                for u in (-half,half):
                    z=b+sg*slope*u
                    if z0<=z<=spring:pts.append((u,z))
                for z in (z0,spring):
                    u=(z-b)/(sg*slope)
                    if -half<u<half:pts.append((u,z))
                if len(pts)>=2 and math.dist(pts[0],pts[1])>.02:tube(L,[T(*pts[0]),T(*pts[1])],.0035,M['iron'],nrm,4)
    SOCKETS.append(c+Vector((0,0,(Z(506)+Z(396))/2)))
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
shift=Vector((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2]))
for o in objs:o.data.transform(Matrix.Translation(shift))
SOCKETS=[p+shift for p in SOCKETS]
assert abs(lo[2])<.005,f'base not on the ground: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookStreetLamp2'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
for i,p in enumerate(SOCKETS):                                                       # preview of the Unreal point lights
    pl=bpy.data.lights.new(f'Lamp{i}','POINT');pl.energy=20;pl.color=(1,.62,.3);pl.shadow_soft_size=.05;po=bpy.data.objects.new(f'Lamp{i}',pl);po.location=p;pre.objects.link(po)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-3.2,-4.6,2.6),(86,0,-35),lens=30);cam('Front',(0,-30,2.3),(90,0,0),4.9);cam('Side',(-30,0,2.3),(90,0,-90),4.9);cam('Rear',(0,30,2.3),(90,0,180),4.9)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'StreetLamp2-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=40000,f'small prop over budget: {tris}'
manifest=dict(asset='SilverbrookStreetLamp2',asset_id='BLD-GreenlandsStreetLamp-01',variant=2,reference=dict(file='StreetLamp2-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_streetlamp2.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookStreetLamp2',slot_m=list(SLOT),
    light_socket_m=[[round(v,3) for v in p] for p in SOCKETS],light_material='SB_StreetLamp2_Glow',
    outputs=['SilverbrookStreetLamp2.blend','SilverbrookStreetLamp2.glb','SilverbrookStreetLamp2.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('STREETLAMP2_BUILT',manifest['bounds_m'],tris,manifest['light_socket_m'])
