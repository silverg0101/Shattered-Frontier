"""Silverbrook Stone Bridge from docs/Game-Design/concept-art/Silverbrook/Stone Bridge.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_stonebridge.py
Original procedural geometry interpreting the elevation (drawn 24 m long, 6 m to the pillar
spikes), fitted to the 30 x 7 m deck slot over the 24 m Silverbrook River: three round arches
(a wide centre arch between two smaller ones) with dressed voussoir rings and keystones, piers
with pilasters rising into tall pillars carrying carved lozenge plaques, pyramid caps, iron
spikes and navy stag banners, end pillars on the abutments, a lancet-arcaded parapet with
coping over a string course, and a humped flagstone deck.
Span runs along X; origin is the centre of the span at riverbank ground level (deck ends z=0).
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,UP,frame
import bpy
from mathutils import Matrix
scene=kit.reset(4161)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Stone Bridge.png'
asset=bpy.data.collections.new('SilverbrookStoneBridge');scene.collection.children.link(asset)

M=dict(
    # The scans read warm; the concept's stone is cool grey, so a blue-grey multiply at `aged` cools it.
    stone=kit.scan('SB_StoneBridge_Masonry','stone_brick_wall_001',dark=(.55,.68,1.0),aged=.9),
    dressed=kit.scan('SB_StoneBridge_Dressed','medieval_blocks_03',dark=(.62,.74,1.0),aged=.85),
    paving=kit.scan('SB_StoneBridge_Paving','medieval_blocks_03',dark=(.55,.64,.85),damp=False,aged=.8),
    banner=kit.scan('SB_StoneBridge_Banner','denim_fabric_03',dark=(.10,.12,.28),damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    gilt=kit.solid('SB_StoneBridge_Gilt',(.80,.55,.18),.35,1.0))
DRESSED=kit.patches('medieval_blocks_03',(.05,.035),dark=.7,frac=.01,std=.22)
def rect():return kit.rng.choice(DRESSED)
G=dict(stone=Geo('SM_SBStoneBridge_Masonry',asset),dressed=Geo('SM_SBStoneBridge_Dressed',asset),
       deck=Geo('SM_SBStoneBridge_Deck',asset),detail=Geo('SM_SBStoneBridge_Details',asset))
S,D=G['stone'],G['detail']
# ---- measured layout (metres) -------------------------------------------------
# Town layout (GameClient/ArtSource/Greenlands/silverbrook-town-layout.json): river 24 m wide,
# bridge_length 30, bridge_width 7; heightfield.py puts the bank 1.5 m above the water
# (WATER_BANK) and the channel bed 2 m below it (CHANNEL_DEPTH). z=0 is that bank.
SLOT=(30,7,10)                 # deck 30 x 7; height not in the layout: 6 m above water (concept) + footings
L=15;HW=3.3;PIN=2.9            # half length; body and parapet-inner half widths
DC=3.6                         # deck crown above the bank; deck ends meet the bank at z=0
WATER,BED,SPRING=-1.5,-3.5,-1.3
ARCHES=[(-9.2,2.1),(0,3.9),(9.2,2.1)]   # centre x, intrados radius (semicircular)
PIER=5.5                       # pier / pillar centre |x|
def zd(x):
    """Deck walking surface: a hump that is flat where it meets the bank."""
    u=min(1,abs(x)/L);return DC*(.5*(1-u*u)+.25*(1+math.cos(math.pi*u)))
def xs(a,b,step=.25):n=max(1,math.ceil((b-a)/step));return [a+(b-a)*i/n for i in range(n+1)]
def orient(q,out):
    q=[Vector(p) for p in q];n=sum((a.cross(q[(i+1)%len(q)]) for i,a in enumerate(q)),Vector())
    return q if n.dot(Vector(out))>=0 else q[::-1]
def strip(geo,samples,y0,y1,mat,caps=True):
    """Solid between y0 and y1 under an XZ outline: samples (x, bottom z, top z), x non-decreasing;
    a repeated x makes a vertical step in the bottom (pier faces)."""
    for (xa,ba,ta),(xb,bb,tb) in zip(samples,samples[1:]):
        dx=xb-xa
        if dx>1e-6:
            assert ta>ba and tb>bb,(xa,ba,ta)
            for y,s in ((y0,-1),(y1,1)):geo.poly(orient([(xa,y,ba),(xb,y,bb),(xb,y,tb),(xa,y,ta)],(0,s,0)),mat)
            geo.poly(orient([(xa,y0,ta),(xb,y0,tb),(xb,y1,tb),(xa,y1,ta)],(-(tb-ta),0,dx)),mat)
        if dx>1e-6 or abs(bb-ba)>1e-6:geo.poly(orient([(xa,y0,ba),(xb,y0,bb),(xb,y1,bb),(xa,y1,ba)],(bb-ba,0,-dx)),mat)
    if caps:
        for (x,b,t),s in ((samples[0],-1),(samples[-1],1)):geo.poly(orient([(x,y0,b),(x,y1,b),(x,y1,t),(x,y0,t)],(s,0,0)),mat)
def sheared(a,b,lo,hi,step=.25):return [(x,zd(x)+lo,zd(x)+hi) for x in xs(a,b,step)]
def fill(geo,pts,axis,th,mat):
    """Flat slab around a planar outline, caps fanned from pts[0] (Townhouse1)."""
    pts=[Vector(p) for p in pts];axis=Vector(axis).normalized()
    if sum((a.cross(pts[(i+1)%len(pts)]) for i,a in enumerate(pts)),Vector()).dot(axis)<0:pts.reverse()
    o=axis*th/2;n=len(pts)
    for i in range(1,n-1):geo.poly([pts[0]+o,pts[i]+o,pts[i+1]+o],mat);geo.poly([pts[0]-o,pts[i+1]-o,pts[i]-o],mat)
    for i,a in enumerate(pts):b=pts[(i+1)%n];geo.poly([a-o,b-o,b+o,a+o],mat)
def pyramid(geo,c,sx,sy,h,mat):
    c=Vector(c);b=[c+Vector((x*sx/2,y*sy/2,0)) for x,y in ((-1,-1),(1,-1),(1,1),(-1,1))];top=c+UP*h
    geo.poly(orient(b,(0,0,-1)),mat)
    for i in range(4):p,q=b[i],b[(i+1)%4];geo.poly(orient([p,q,top],((p+q)/2-c)+UP*.01),mat)
# ---- bridge body: one extruded elevation, arch barrels cut through ------------------------------
bottom=[(-L,BED)]
for c,r in ARCHES:
    bottom+=[(x,BED) for x in xs(bottom[-1][0],c-r)[1:]]+[(c-r,SPRING)]
    n=math.ceil(math.pi*r/.25);bottom+=[(c+r*math.cos(math.pi*(1-i/n)),SPRING+r*math.sin(math.pi*(1-i/n))) for i in range(1,n)]
    bottom+=[(c+r,SPRING),(c+r,BED)]
bottom+=[(x,BED) for x in xs(bottom[-1][0],L)[1:]]
strip(S,[(x,z,zd(x)-.25) for x,z in bottom],-HW,HW,M['stone'])
# ---- deck: continuous flagstone walking surface between the parapets --------------------------------
strip(G['deck'],sheared(-L,L,-.25,0),-PIN,PIN,M['paving'])
# ---- faces: voussoir rings, keystones, string course, pilasters, buttresses ---------------------------
for s in (-1,1):
    for c,r in ARCHES:
        depth=.47 if r>3 else .4;n=round(math.pi*(r+depth/2)/.45);rm=r-.03+depth/2
        for k in range(n):
            t=math.pi*(k+.5)/n;radial=Vector((math.cos(t),0,math.sin(t)));tang=Vector((-math.sin(t),0,math.cos(t)))
            key=k==n//2;size=(math.pi*rm/n-.02,.4 if key else .35,depth+(.12 if key else 0))
            G['dressed'].box((c+rm*math.cos(t),s*(HW+(.15 if key else .05)-size[1]/2),SPRING+(rm+(.06 if key else 0))*math.sin(t)),size,M['dressed'],.015,Matrix((tang,Vector((0,1,0)),radial)).transposed(),rect=rect())
    strip(G['dressed'],sheared(-L,L,-.47,-.25),*sorted((s*HW,s*(HW+.12))),M['dressed'])
    for xc in (-PIER,PIER):
        S.box((xc,s*(HW+.04),(BED+zd(abs(xc)+.9)-.47)/2),(1.8,.08,zd(abs(xc)+.9)-.47-BED),M['stone'],.01)
    for xc in (-12.5,12.5):
        S.box((xc,s*(HW+.04),(BED+zd(13.2)-.47)/2),(1.4,.08,zd(13.2)-.47-BED),M['stone'],.01)
# ---- parapets: base course, lancet arcade, coping -------------------------------------------------
PILLARS=[(-PIER,1.1),(PIER,1.1),(-14.35,.9),(14.35,.9)]
W_O,SILL,SPR=.16,.3,.62
APEX=SPR+W_O*math.sqrt(3)/2
for s in (-1,1):
    y0,y1=sorted((s*PIN,s*HW));yc=(y0+y1)/2;Dg=G['dressed']
    strip(S,sheared(-L,L,-.25,SILL),y0,y1,M['stone'])
    strip(Dg,sheared(-L,L,.85,1.05),y0-.05,y1+.05,M['dressed'])
    for i in range(60):
        xa,xb=-L+i*.5,-L+(i+1)*.5;xc=(xa+xb)/2
        if any(xb>px-w/2-.1 and xa<px+w/2+.1 for px,w in PILLARS):
            strip(Dg,sheared(xa,xb,SILL,.85),y0,y1,M['dressed']);continue
        o0,o1=xc-W_O/2,xc+W_O/2
        strip(Dg,sheared(xa,o0,SILL,SPR,.5),y0,y1,M['dressed']);strip(Dg,sheared(o1,xb,SILL,SPR,.5),y0,y1,M['dressed'])
        P=lambda x,h:(x,yc,zd(x)+h)
        left=[(xa,.85),(xa,SPR),(o0,SPR)]+[(o1+W_O*math.cos(a),SPR+W_O*math.sin(a)) for a in (math.pi*(1-j/12) for j in range(1,5))]+[(xc,.85)]
        right=[(xb,.85),(xb,SPR),(o1,SPR)]+[(o0+W_O*math.cos(a),SPR+W_O*math.sin(a)) for a in (math.pi*j/12 for j in range(1,5))]+[(xc,.85)]
        for pts in (left,right):fill(Dg,[P(x,h) for x,h in pts],(0,1,0),HW-PIN,M['dressed'])
# ---- pillars: plaques, cornices, pyramid caps, iron spikes, banners ---------------------------------
def banner(c,o,ztop,zbot,w,d=.06):
    """Navy banner with a pointed foot, gilt edging and rod on iron brackets (NorthridgeHouse1), gilt stag."""
    P,R=frame(c,o);pts=[(-w/2,ztop),(w/2,ztop),(w/2,zbot+.35),(0,zbot),(-w/2,zbot+.35)]
    D.prism([P(x,d,z) for x,z in pts],o,.02,M['banner'])
    for (x0,z0),(x1,z1) in zip(pts,pts[1:]+pts[:1]):D.beam(P(x0,d+.012,z0),P(x1,d+.012,z1),.03,.012,M['gilt'],grain=False)
    D.beam(P(-w/2-.08,d,ztop+.05),P(w/2+.08,d,ztop+.05),.035,.035,M['gilt'],grain=False)
    for s in (-1,1):D.box(P(s*(w/2+.04),d/2,ztop+.05),(.04,d+.03,.04),M['iron'],.004,R)
    stag(D,P(0,d+.011,(ztop+zbot)/2+.15),R@Vector((1,0,0)),UP,Vector(o).normalized(),w*1.05)
def stag(geo,origin,right,up,normal,s):
    """Gilt stag head with antlers (Townhouse1)."""
    o=Vector(origin)+Vector(normal)*.008;Rv,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+Rv*(a*s)+U*(z*s)
    fill(geo,[p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        fill(geo,[p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def lozenge(c,o,s):
    """Carved plaque: raised lozenge frame with a three-petal fleur (concept's pillar panels)."""
    P,R=frame(c,o);Dg=G['dressed']
    Dg.box(P(0,.02,0),(.72*s,.04,.62*s),M['dressed'],.012,R,rect=rect())
    for k,(h,w,d) in enumerate([(.27,.2,.06),(.2,.14,.08)]):
        fill(Dg,[P(x*s,d,z*s) for x,z in ((0,h),(w,0),(0,-h),(-w,0))],o,.02,M['dressed'])
    for pts in ([(0,.15),(.035,0),(0,-.05),(-.035,0)],[(.03,-.01),(.1,.08),(.08,-.06)],[(-.03,-.01),(-.08,-.06),(-.1,.08)],[(-.07,-.07),(.07,-.07),(.07,-.1),(-.07,-.1)]):
        fill(Dg,[P(x*s,.1,z*s) for x,z in pts],o,.02,M['dressed'])
def pillar(xc,w,z0,top,sc=1.0):
    dep=.68*sc;yc=HW+.08-dep/2;Dg=G['dressed']
    for s in (-1,1):
        Dg.box((xc,s*yc,(z0+top)/2),(w,dep,top-z0),M['dressed'],.015)
        Dg.box((xc,s*yc,top+.07*sc),(w+.16*sc,dep+.12*sc,.14*sc),M['dressed'],.015,rect=rect())
        pyramid(Dg,(xc,s*yc,top+.14*sc),w,dep,.5*sc,M['dressed'])
        D.vessel((xc,s*yc,top+.14*sc+.46*sc),[(0,0),(.05*sc,.01),(.05*sc,.07*sc),(.085*sc,.13*sc),(.05*sc,.2*sc),(.03*sc,.28*sc),(0,.62*sc)],M['iron'],8)
        lozenge((xc,s*(HW+.08),top-.45*sc),(0,s,0),sc)
        if sc==1.0:banner((xc,s*(HW+.08),0),(0,s,0),top-.95,zd(PIER)-1.55,.7)
for xc,w in PILLARS:
    big=w>1;x_low=abs(xc)+(.9 if big else .45)
    pillar(xc,w,zd(x_low)-.47 if big else -.7,zd(abs(xc))+(2.2 if big else 1.9),1.0 if big else .85)
# ---- export, checks, preview scene, manifest -----------------------------------------------------
objs=[g.object() for g in G.values()]
def rewet(o):
    """The kit damps stone up to 0.65 m above z=0; here z=0 is the bank, so measure the wet band from the water."""
    me=o.data;col=me.color_attributes['Weathering']
    for poly in me.polygons:
        m=me.materials[poly.material_index]
        if m.name not in kit.DAMP:continue
        for li in poly.loop_indices:
            p=me.vertices[me.loops[li].vertex_index].co;damp=max(0,1-(p.z-WATER)/.65)*(.30+.15*math.sin(p.x*7.3+p.y*5.1))
            col.data[li].color=(min(.8,max(damp,kit.AGED.get(m.name,0))),col.data[li].color[1],0,1)
for o in objs:rewet(o)
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
assert abs(zd(L))<1e-9 and abs(zd(-L))<1e-9,'deck ends must meet the bank at z=0'
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=400_000,f'landmark triangle budget: {tris}'
# Walkable deck: cast straight down along three lines; every hit is the paving at zd(x), grade <= 0.35.
bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();grade=0
for y in (-2.5,0,2.5):
    prev=None
    for i in range(299):
        x=-14.9+i*.1;hit,loc,*_,ob,_m=scene.ray_cast(dg,Vector((x,y,20)),Vector((0,0,-1)))
        assert hit and ob.name=='SM_SBStoneBridge_Deck' and abs(loc.z-zd(x))<.01,f'deck not walkable at x={x:.1f} y={y}: {ob.name if hit else None} {loc.z if hit else None}'
        if prev is not None:grade=max(grade,abs(loc.z-prev)/.1)
        prev=loc.z
assert grade<=.35,f'deck too steep: {grade:.2f}'
OUT=HERE/'SilverbrookStoneBridge'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a huge GLB.
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
def preview(name,verts,faces,mat):
    me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);ob=bpy.data.objects.new(name,me);pre.objects.link(ob);me.materials.append(mat)
# Banks as heightfield.py carves them: water edge 0.3 m above the water, rising to the bank, bed 2 m down.
prof=[(-60,0),(-14,0),(-12,WATER+.3),(-8,BED),(8,BED),(12,WATER+.3),(14,0),(60,0)]
vs=[(x,y,z) for x,z in prof for y in (-40,40)]
preview('Ground',vs,[(2*i,2*i+2,2*i+3,2*i+1) for i in range(len(prof)-1)],kit.solid('Preview_Ground',(.11,.12,.07),.95))
preview('Water',[(-12.5,-40,WATER),(12.5,-40,WATER),(12.5,40,WATER),(-12.5,40,WATER)],[(0,1,2,3)],kit.solid('Preview_Water',(.03,.05,.06),.05))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-17,-26,8.5),(76,0,-33),lens=30);cam('Front',(0,-60,1.4),(90,0,0),31.5);cam('Side',(-60,0,1.4),(90,0,-90),31.5);cam('Rear',(0,60,1.4),(90,0,180),31.5)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1600;scene.render.resolution_y=800;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'StoneBridge-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
manifest=dict(asset='SilverbrookStoneBridge',reference=dict(file='StoneBridge-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_stonebridge.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, span along X, front -Y, origin at the centre of the span at riverbank ground level',bounds_m=[round(v,3) for v in maxs-mins],
    z_range_m=[round(mins.z,3),round(maxs.z,3)],
    deck=dict(runs='along X (span); parapet faces -Y/+Y',length_m=2*L,walkable_width_m=2*PIN,ends_z_m=0.0,crown_z_m=DC,max_grade=round(grade,3),
              water_z_m=WATER,bed_z_m=BED,profile='z = 3.6*(0.5*(1-u^2)+0.25*(1+cos(pi*u))), u=|x|/15'),
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookStoneBridge',slot_m=list(SLOT),
    asset_id='BLD-GreenlandsStoneBridge-01',outputs=['SilverbrookStoneBridge.blend','SilverbrookStoneBridge.glb','SilverbrookStoneBridge.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('STONEBRIDGE_BUILT',manifest['bounds_m'],manifest['z_range_m'],tris,'grade',round(grade,3))
