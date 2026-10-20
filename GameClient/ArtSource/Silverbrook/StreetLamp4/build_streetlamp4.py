"""Silverbrook Street Lamp 4 from docs/Game-Design/concept-art/Silverbrook/Street Lamp-4.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_streetlamp4.py
Original procedural geometry interpreting the elevation (5 m to the finial tip, 1.3 m plinth):
stepped dressed-stone pedestal with carved fleur-de-lis panels, iron-banded column with oak
panels, scrolled brackets, and a hexagonal gothic lantern with a spired roof and fleur pinnacles.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
from mathutils import Matrix
import bpy
scene=kit.reset(4504)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Street Lamp-4.png'
asset=bpy.data.collections.new('SilverbrookStreetLamp4');scene.collection.children.link(asset)

M=dict(
    stone=kit.scan('SB_StreetLamp4_Stone','medieval_blocks_03',dark=(.5,.5,.52),aged=.5),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    bronze=kit.solid('SB_StreetLamp4_Bronze',(.30,.20,.085),.42,1.0),
    glow=kit.solid('SB_StreetLamp4_Glow',(.30,.14,.03),.25,0,(1.0,.36,.05),1.1))
STONES=kit.patches('medieval_blocks_03',(.08,.05),dark=.68,frac=.015,std=.25)
BANDS=kit.bands('weathered_brown_planks')
G=dict(ped=Geo('SM_SBStreetLamp4_Pedestal',asset),col=Geo('SM_SBStreetLamp4_Column',asset),lan=Geo('SM_SBStreetLamp4_Lantern',asset))
SLOT=(1.5,1.5,5.0)   # concept labels 5 m overall height (> the 4.2 m default slot); 1.3 m plinth
UP=Vector((0,0,1))
# ---- helpers -------------------------------------------------------------------
def octo(H,k,z):
    """Square turned 45 deg with chamfered corners (a chamfer faces the viewer, as in the concept).
    H: silhouette half-width, k: half chamfer. CCW; even edges are the four main faces."""
    return [Vector((x,y,z)) for x,y in [(k,-H),(H,-k),(H,k),(k,H),(-k,H),(-H,k),(-H,-k),(-k,-H)]]
def hexa(R,z):
    """Hexagon, corner radius R, flat face to the front (-Y). Edge i faces 60i+30 deg."""
    return [Vector((R*math.cos(math.radians(60*i)),R*math.sin(math.radians(60*i)),z)) for i in range(6)]
def loft(geo,rings,mat,rects=None,cap=(True,True),w=(0,0)):
    """Closed solid through CCW rings (bottom to top); a degenerate end ring makes an apex."""
    faces=[];n=len(rings[0]);point=lambda r:(r[0]-r[1]).length<1e-6
    for a,b in zip(rings,rings[1:]):
        for i in range(n):
            j=(i+1)%n;faces.append(([a[i],a[j],b[i]] if point(b) else [a[i],b[j],b[i]] if point(a) else [a[i],a[j],b[j],b[i]],True))
    if cap[0] and not point(rings[0]):faces.append((list(reversed(rings[0])),False))
    if cap[1] and not point(rings[-1]):faces.append((list(rings[-1]),False))
    vol=0
    for q,side in faces:
        uv=None
        if rects:
            u0,v0,du,dv=kit.rng.choice(rects)
            if side:uv=[(u0,v0),(u0+du,v0),(u0+du,v0+dv),(u0,v0+dv)][:len(q)] if len(q)==4 else [(u0,v0),(u0+du,v0),(u0+du/2,v0+dv)]
            else:
                xs,ys=[p.x for p in q],[p.y for p in q];sx,sy=max(xs)-min(xs),max(ys)-min(ys)
                uv=[(u0+du*(p.x-min(xs))/sx,v0+dv*(p.y-min(ys))/sy) for p in q]
        geo.poly(q,mat,uv,w);vol+=sum(q[0].dot(q[j].cross(q[j+1])) for j in range(1,len(q)-1))
    assert vol>0 or not all(cap),'inside-out loft'  # open shells: same CCW winding, checked on closed ones
def course(geo,z0,z1,Hb,Ht,k,mat,rects=None,j=.012,cap=(True,True)):
    """One masonry/iron course with chamfered top and bottom arrises (reads as a joint)."""
    loft(geo,[octo(Hb-j,k,z0),octo(Hb,k,z0+j),octo(Ht,k,z1-j),octo(Ht-j,k,z1)],mat,rects,cap)
def face(bl,br,tl,tr):
    """Frame on a (possibly tapered, tilted) face: F(u,v,d) bilinear point + d along the outward normal."""
    a=(br-bl).normalized();n=(br-bl).cross(tl-bl).normalized();t=n.cross(a)
    F=lambda u,v,d=0:bl.lerp(br,u).lerp(tl.lerp(tr,u),v)+n*d
    return F,Matrix((a,n,t)).transposed(),a,n,t,(br-bl).length,(tr-tl).length,(tl.lerp(tr,.5)-bl.lerp(br,.5)).length
def bez(p0,p1,p2,p3,n=8):
    p0,p1,p2,p3=map(Vector,(p0,p1,p2,p3))
    return [p0*(1-s)**3+p1*3*s*(1-s)**2+p2*3*s*s*(1-s)+p3*s**3 for s in (i/n for i in range(n+1))]
def ribbon(c,w0,w1):
    """2D centreline -> convex quads, width tapering w0..w1."""
    c=[Vector((x,y)) for x,y in c];out=[];pts=[]
    for i,p in enumerate(c):
        d=(c[min(i+1,len(c)-1)]-c[max(i-1,0)]).normalized();nrm=Vector((-d.y,d.x))*(w0+(w1-w0)*i/(len(c)-1))/2;pts.append((p+nrm,p-nrm))
    for (l0,r0),(l1,r1) in zip(pts,pts[1:]):out.append([r0,l0,l1,r1])
    return out
def _fleur2d():
    parts=[[(-.07,.44),(.07,.44),(.14,.58),(.13,.74),(.07,.88),(0,1),(-.07,.88),(-.13,.74),(-.14,.58)],
           [(-.24,.36),(.24,.36),(.24,.445),(-.24,.445)],[(-.06,.37),(.06,.37),(0,.14)]]
    for sx in (1,-1):
        for c,w0,w1 in ((bez((.09,.45),(.20,.78),(.50,.80),(.40,.46)),.11,.025),(bez((.04,.37),(.14,.31),(.26,.24),(.20,.08),6),.08,.02)):
            parts+=[[(sx*p.x,p.y) for p in q] for q in ribbon([(p.x,p.y) for p in c],w0,w1)]
    return parts
FLEUR=_fleur2d()
def fleur(geo,O,a,t,n,s,th,mat):
    """Fleur-de-lis relief/plate: base at O, height s, in the plane (a,t), thickness th along n."""
    O,a,t,n=map(Vector,(O,a,t,n))
    for part in FLEUR:geo.prism([O+a*(x*s)+t*(y*s) for x,y in part],n,th,mat)
def fleur3d(geo,O,s,th,mat):
    """Free-standing finial: two crossed fleur plates."""
    for a,n in (((1,0,0),(0,1,0)),((0,1,0),(1,0,0))):fleur(geo,O,a,UP,n,s,th,mat)
def spike(geo,top,bottom,r,mat):
    """Square pyramid between a base (r half-width) and a point."""
    top,bottom=Vector(top),Vector(bottom);sq=lambda c,s:[c+Vector((s*math.cos(math.radians(45+90*i)),s*math.sin(math.radians(45+90*i)),0)) for i in range(4)]
    loft(geo,[sq(bottom,r),sq(top,0)] if top.z>bottom.z else [sq(top,0),sq(bottom,r)],mat)
def pinnacle(geo,c,up=.30,down=.22,blk=.065,fl=.15):
    """Iron block with a bronze fleur above and an iron drop below (lantern corners, bracket ends)."""
    c=Vector(c);geo.box(c,(blk,blk,blk),M['iron'],.006)
    geo.beam(c+UP*blk*.4,c+UP*(up-fl+.02),.024,.024,M['iron'],grain=False)
    fleur3d(geo,c+UP*(up-fl),fl,.018,M['bronze'])
    spike(geo,c-UP*down,c-UP*blk*.45,.024,M['iron'])
def stud(geo,F,R,u,v,s=.022,mat=None):geo.box(F(u,v,s*.3),(s,s,s),mat or M['bronze'],.004,R)
def xplate(geo,F,R,t,a,u,v,s=.075):
    geo.box(F(u,v,.004),(s,.012,s),M['iron'],.004,R);c=F(u,v,.012)
    for sg in (1,-1):geo.beam(c-a*s*.36-t*s*.36*sg,c+a*s*.36+t*s*.36*sg,.012,.008,M['bronze'],grain=False)
def faces_of(ring0,ring1,main=(0,2,4,6)):
    return [face(ring0[i],ring0[(i+1)%len(ring0)],ring1[i],ring1[(i+1)%len(ring1)]) for i in main]
# ---- pedestal: plinth, tapered carved body, cap course, iron and stone steps (z 0 .. 1.13) ----
P=G['ped'];S=M['stone']
loft(P,[octo(.638,.075,0),octo(.65,.075,.012),octo(.65,.075,.19),octo(.57,.066,.26),octo(.558,.066,.27)],S,STONES)
course(P,.27,.745,.53,.46,.035,S,STONES,cap=(False,True))
for F,R,a,n,t,wb,wt,h in faces_of(octo(.53,.035,.282),octo(.46,.035,.733)):
    W=lambda v:wb+(wt-wb)*v
    for u in (.14,.86):P.box(F(u,.5,.008),(.04,.035,.78*h),S,.008,R,rect=kit.rng.choice(STONES))
    for v in (.11,.89):P.box(F(.5,v,.008),(.72*W(v)+.04,.035,.04),S,.008,R,rect=kit.rng.choice(STONES))
    fleur(P,F(.5,.17,.01),a,t,n,.29,.04,S)
course(P,.745,.875,.49,.49,.045,S,STONES,cap=(False,True))
course(G['col'],.875,.95,.38,.38,.03,M['iron'],j=.008,cap=(False,True))
course(P,.95,1.055,.32,.32,.03,S,STONES,cap=(False,True))
course(G['col'],1.055,1.13,.26,.26,.025,M['iron'],j=.008,cap=(False,True))
for (z0,z1,H) in ((.875,.95,.38),(1.055,1.13,.26)):
    for F,R,a,n,t,*_ in faces_of(octo(H,.03,z0),octo(H,.03,z1)):
        for u in (.2,.4,.6,.8):stud(G['col'],F,R,u,.5)
# ---- column (z 1.13 .. 3.14): iron core, oak panels between studded bands ----------------
C=G['col'];I=M['iron']
def panels(z0,z1,Hb,Ht,fl=None):
    loft(C,[octo(Hb,.02,z0),octo(Ht,.02,z1)],I,cap=(False,True))
    for F,R,a,n,t,wb,wt,h in faces_of(octo(Hb,.02,z0),octo(Ht,.02,z1)):
        w=min(wb,wt);C.box(F(.5,.5,.004),(.8*w,.016,.88*h),M['timber'],.004,R,grain=True,band=kit.rng.choice(BANDS))
        for u in (.09,.91):C.box(F(u,.5,.01),(.15*w,.03,h),I,.005,R)
        for v in (.06,.94):
            for u in (.3,.7):stud(C,F,R,u,v,.018)
        if fl:fleur(C,F(.5,.12,.016),a,t,n,fl,.014,M['bronze'])
def band(z0,z1,H,xp=False,flange=False):
    if flange:
        course(C,z0,z0+.035,H+.012,H+.012,.025,I,j=.006,cap=(False,True));course(C,z0+.035,z1-.035,H,H,.025,I,j=.004,cap=(False,True));course(C,z1-.035,z1,H+.012,H+.012,.025,I,j=.006,cap=(False,True))
    else:course(C,z0,z1,H,H,.025,I,j=.008,cap=(False,True))
    for F,R,a,n,t,*_ in faces_of(octo(H,.025,z0),octo(H,.025,z1)):
        if xp:xplate(C,F,R,t,a,.5,.5)
        for u in ((.18,.82) if xp else (.25,.5,.75)):stud(C,F,R,u,.5)
panels(1.13,1.40,.21,.165,fl=.17)
band(1.40,1.46,.175)
panels(1.46,1.91,.145,.14)
band(1.91,2.04,.175,flange=True)
panels(2.04,2.60,.135,.13)
band(2.60,2.71,.16,xp=True)
panels(2.71,2.92,.125,.125)
band(2.92,3.03,.17,xp=True)
loft(C,[octo(.11,.02,3.03),octo(.11,.02,3.16)],I,cap=(False,True))
# ---- brackets: four scrolled iron arms with fleur pinnacles under the lantern ---------
for ang in range(0,360,90):
    d=Vector((math.cos(math.radians(ang)),math.sin(math.radians(ang)),0))
    Q=lambda r,z:d*r+UP*z
    C.beam(Q(.08,3.20),Q(.52,3.22),.032,.028,I,grain=False)
    pinnacle(C,Q(.52,3.24),up=.24,down=.20,blk=.06,fl=.14)
    curve=bez((.51,3.22),(.49,3.02),(.33,2.90),(.21,2.95))
    cx,cz=.21+.036*.29,2.95+.036*.96
    th0=math.atan2(2.95-cz,.21-cx)
    curl=[(cx+.036*(1-.35*s)*math.cos(th0-s*5.0),cz+.036*(1-.35*s)*math.sin(th0-s*5.0)) for s in (i/10 for i in range(1,11))]
    pts=[Q(p.x,p.y) for p in curve]+[Q(r,z) for r,z in curl]
    for i,(p,q) in enumerate(zip(pts,pts[1:])):e=(q-p)*.12;C.beam(p-e,q+e,.026 if i<8 else .02,.024,I,grain=False)
    m=curve[5];C.beam(Q(.09,3.08),Q(m.x,m.y),.022,.02,I,grain=False)
# ---- lantern (z 3.12 .. 5.0) ------------------------------------------------------------
L=G['lan']
loft(L,[hexa(.10,3.12),hexa(.30,3.27),hexa(.41,3.34)],I,cap=(True,False))
loft(L,[hexa(.41,3.34),hexa(.43,3.355),hexa(.43,3.435),hexa(.41,3.45)],I,cap=(False,True))
GB,GT=3.44,4.135                       # glass body (embedded into both frame bands)
loft(L,[hexa(.37,GB),hexa(.445,GT)],M['glow'])
LIGHT=[0.0,0.0,round((3.45+4.125)/2,3)]   # lantern centre for the Unreal point light
for i in range(6):
    F,R,a,n,t,wb,wt,hf=face(*hexa(.372,3.45)[i:i+1],*hexa(.372,3.45)[(i+1)%6:(i+1)%6+1],*hexa(.4465,4.125)[i:i+1],*hexa(.4465,4.125)[(i+1)%6:(i+1)%6+1])
    Wd=lambda y:wb+(wt-wb)*y/hf
    def X(x,y,d):return F(.5+x/Wd(y),y/hf,d)
    span=.74*wt;ys=hf-.06-.866*span;xl,xr=-span/2,span/2
    def inside(x,y):
        if abs(x)>xl*-1-.012 or y<.04:return False
        return y<ys or ((x-xr)**2+(y-ys)**2<(span-.012)**2 and (x-xl)**2+(y-ys)**2<(span-.012)**2)
    # diamond lattice of lead cames, clipped to the lancet
    for sg in (1,-1):
        for k in range(-20,21):
            y0=k*.075*1.8;run=[]
            for x in [xl+(xr-xl)*s/80 for s in range(81)]+[None]:
                y=None if x is None else y0+sg*1.8*x
                if x is not None and inside(x,y):run.append((x,y));continue
                if len(run)>1:L.beam(X(*run[0],.007),X(*run[-1],.007),.008,.01,M['lead'],grain=False)
                run=[]
    # lancet frame, two sub-arches and a mullion (iron)
    def arc(cx,r,a0,a1,yb,n=7):return [(cx+r*math.cos(math.radians(a0+(a1-a0)*s/n)),yb+r*math.sin(math.radians(a0+(a1-a0)*s/n))) for s in range(n+1)]
    lines=[[(xl,.03),(xl,ys)]+arc(xr,span,180,120,ys),[(xr,.03),(xr,ys)]+arc(xl,span,0,60,ys),
           arc(0,span/2,180,120,ys),arc(xl,span/2,0,60,ys),arc(xr,span/2,180,120,ys),arc(0,span/2,0,60,ys),[(0,.03),(0,ys+.866*span/2)],[(xl,.03),(xr,.03)]]
    for ln in lines:
        for p,q in zip(ln,ln[1:]):
            P0,P1=X(*p,.011),X(*q,.011);e=(P1-P0)*.08
            if (P1-P0).length>1e-4:L.beam(P0-e,P1+e,.018 if ln is lines[0] or ln is lines[1] else .013,.014,I,grain=False)
for i,(p,q) in enumerate(zip(hexa(.383,3.45),hexa(.457,4.125))):L.beam(p,q,.038,.038,I,grain=False)
loft(L,[hexa(.445,4.125),hexa(.50,4.14),hexa(.50,4.22),hexa(.47,4.24)],I)
for F,R,a,n,t,*_ in faces_of(hexa(.50,4.14),hexa(.50,4.22),range(6)):
    for u in (.2,.4,.6,.8):stud(L,F,R,u,.5,.02)
for F,R,a,n,t,*_ in faces_of(hexa(.43,3.355),hexa(.43,3.435),range(6)):
    for u in (.25,.5,.75):stud(L,F,R,u,.5,.018)
RING=[(.52,4.232),(.52,4.25),(.34,4.39),(.17,4.52),(.075,4.595)]
loft(L,[hexa(r,z) for r,z in RING],I)
for i in range(6):
    pts=[hexa(r+.012,z+.008)[i] for r,z in RING[1:]]
    for p,q in zip(pts,pts[1:]):L.beam(p,q,.03,.03,I,grain=False)
    c=(hexa(.30,4.40)[i]+hexa(.30,4.40)[(i+1)%6])/2;o=Vector((c.x,c.y,0)).normalized()
    fleur(L,c-UP*.035,UP.cross(o),UP,o,.15,.018,M['bronze'])
# top finial and corner pinnacles
loft(L,[hexa(.085,4.58),hexa(.085,4.665)],I);loft(L,[hexa(.05,4.665),hexa(.04,4.70)],I,cap=(False,True))
fleur3d(L,Vector((0,0,4.66)),.21,.022,M['bronze'])
spike(L,(0,0,5.0),(0,0,4.80),.03,M['bronze'])
for c in hexa(.585,4.18):
    o=Vector((c.x,c.y,0)).normalized();L.beam(o*.48+UP*4.18,o*.56+UP*4.18,.032,.032,I,grain=False)
    pinnacle(L,c,up=.30,down=.24,blk=.065,fl=.15)
# ---- export, preview scene, manifest -----------------------------------------------
# The concept's 1.3 m label fixes the plinth; above the pedestal it is drawn at the 5 m label's scale,
# so column, brackets and lantern are authored at plinth scale and narrowed here to the uniform scale.
K=.87
for g in G.values():g.v=[Vector((v.x*K,v.y*K,v.z)) if v.z>1.131 else v for v in g.v]
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
LIGHT=[round(LIGHT[0]-(lo[0]+hi[0])/2,3),round(LIGHT[1]-(lo[1]+hi[1])/2,3),LIGHT[2]]
assert abs(lo[2])<1e-4,f'not on the ground: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=40000,f'{tris} triangles > 40k small-prop budget'
OUT=HERE/'SilverbrookStreetLamp4'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
lamp=bpy.data.lights.new('Preview_LanternLight','POINT');lamp.energy=60;lamp.color=(1,.62,.3);lamp.shadow_soft_size=.05;lo_=bpy.data.objects.new('Preview_LanternLight',lamp);lo_.location=LIGHT;pre.objects.link(lo_)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,target,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=(Vector(target)-Vector(loc)).to_track_quat('-Z','Y').to_euler();pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-3.4,-5.4,2.2),(0,0,2.5),lens=36);cam('Front',(0,-30,2.55),(0,0,2.55),5.4);cam('Side',(-30,0,2.55),(0,0,2.55),5.4);cam('Rear',(0,30,2.55),(0,0,2.55),5.4)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=800;scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'StreetLamp4-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
manifest=dict(asset='SilverbrookStreetLamp4',asset_id='BLD-GreenlandsStreetLamp-01',variant=4,reference=dict(file='StreetLamp4-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_streetlamp4.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookStreetLamp4',slot_m=list(SLOT),
    light_socket_m=LIGHT,light_material='SB_StreetLamp4_Glow',outputs=['SilverbrookStreetLamp4.blend','SilverbrookStreetLamp4.glb','SilverbrookStreetLamp4.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('STREETLAMP4_BUILT',manifest['bounds_m'],tris)
