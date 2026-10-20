"""Silverbrook River Bridge: a modest late-medieval two-arch road bridge, 22 m nominal.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_riverbridge.py
Two low segmental arches of 8 m clear span meet on one central pier with pointed cutwaters on both
faces (upstream/downstream, +-Y). Dressed voussoir rings, coursed parapets with coping, end posts.
Span runs along X; origin is mid-span, centred in Y, at deck-end ground level (z=0 where the deck
meets the bank at x=+-11). A placer may scale X by 0.85-1.15: nothing is round in plan.
"""
import sys,math,json
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
import bpy
from mathutils import Matrix
scene=kit.reset(2201)
asset=bpy.data.collections.new('SilverbrookRiverBridge');scene.collection.children.link(asset)
M=dict(
    stone=kit.scan('SB_RiverBridge_Masonry','stone_brick_wall_001',dark=(.55,.68,1.0),aged=.9),
    dressed=kit.scan('SB_RiverBridge_Dressed','medieval_blocks_03',dark=(.62,.74,1.0),aged=.85),
    paving=kit.scan('SB_RiverBridge_Paving','cobblestone_floor_01',dark=(.75,.78,.9),damp=False,aged=.35))
# Single-element sampling at true scale: a block of length L and height ~.28 m takes a (L/2, .14) UV patch of the
# 2 m scan (one real block interior, 1:1, no stretch). Voussoirs take the largest clean patch that fits.
LENS=(.45,.55,.65)
BLOCKS={L:kit.patches('medieval_blocks_03',(L/2,.14),dark=.6,frac=.04,std=.3) for L in LENS}
COPING=kit.patches('medieval_blocks_03',(.325,.07),dark=.6,frac=.04,std=.3)
DRESSED=kit.patches('medieval_blocks_03',(.2,.15),dark=.6,frac=.04,std=.3)
def rect():return kit.rng.choice(DRESSED)
G=dict(stone=Geo('SM_SBRiverBridge_Masonry',asset),dressed=Geo('SM_SBRiverBridge_Dressed',asset),deck=Geo('SM_SBRiverBridge_Deck',asset))
S,Dg=G['stone'],G['dressed']
# ---- layout (metres) -----------------------------------------------------------------------
SLOT=(22,7.6,7)
L=11;HW=2.7;PIN=2.25           # half length; body and parapet-inner half widths (5.4 over all, 4.5 walkable)
DC=1.6                         # deck crown above the end-of-deck ground level (x=0, over the pier)
WATER,BED,SPRING=-1.5,-4.0,-1.2
HALF=4.0;RISE=1.5;PIERX=1.0    # arch half-span, rise; pier half-thickness -> arches span |x| in [1,9]
R=(HALF**2+RISE**2)/(2*RISE);ZC=SPRING+RISE-R;CUT=1.0   # arch radius; arch centre height; cutwater projection
def zd(x):
    u=min(1,abs(x)/L);return DC*(.5*(1-u*u)+.25*(1+math.cos(math.pi*u)))
def slope(x):return (zd(x+.01)-zd(x-.01))/.02
def intrados(x):c=PIERX+HALF if x>0 else -(PIERX+HALF);return ZC+math.sqrt(R*R-(x-c)**2)
def xs(a,b,step=.25):n=max(1,math.ceil((b-a)/step));return [a+(b-a)*i/n for i in range(n+1)]
def orient(q,out):
    q=[Vector(p) for p in q];n=sum((a.cross(q[(i+1)%len(q)]) for i,a in enumerate(q)),Vector())
    return q if n.dot(Vector(out))>=0 else q[::-1]
def strip(geo,samples,y0,y1,mat,caps=True):
    """Solid between y0 and y1 under an XZ outline: samples (x, bottom z, top z); a repeated x makes a vertical step."""
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
def pyramid(geo,c,sx,sy,h,mat):
    c=Vector(c);b=[c+Vector((x*sx/2,y*sy/2,0)) for x,y in ((-1,-1),(1,-1),(1,1),(-1,1))];top=c+Vector((0,0,h))
    geo.poly(orient(b,(0,0,-1)),mat)
    for i in range(4):p,q=b[i],b[(i+1)%4];geo.poly(orient([p,q,top],((p+q)/2-c)+Vector((0,0,.01))),mat)
# ---- body: one extruded elevation; two segmental barrels cut through, end masses to the bed -------------
bottom=[(-L,BED)]
for a,b in ((-PIERX-2*HALF,-PIERX),(PIERX,PIERX+2*HALF)):
    bottom+=[(x,BED) for x in xs(bottom[-1][0],a)[1:]]
    bottom+=[(a,SPRING)]+[(x,intrados(x)) for x in xs(a,b)[1:-1]]+[(b,SPRING),(b,BED)]
bottom+=[(x,BED) for x in xs(bottom[-1][0],L)[1:]]
strip(S,[(x,z,zd(x)-.25) for x,z in bottom],-HW,HW,M['stone'])
CAM=.06   # crown at the road centre, fading to flush at both deck ends
def zc(x,y):return zd(x)+CAM*(1-(y/PIN)**2)*(1-(x/L)**4)
def deck_slab(geo,mat,nx=88,ny=8):
    X=xs(-L,L,2*L/nx);Y=[-PIN+2*PIN*j/ny for j in range(ny+1)]
    for xa,xb in zip(X,X[1:]):
        for ya,yb in zip(Y,Y[1:]):geo.poly(orient([(xa,ya,zc(xa,ya)),(xb,ya,zc(xb,ya)),(xb,yb,zc(xb,yb)),(xa,yb,zc(xa,yb))],(0,0,1)),mat)
        for y,sg in ((-PIN,-1),(PIN,1)):geo.poly(orient([(xa,y,zd(xa)-.25),(xb,y,zd(xb)-.25),(xb,y,zd(xb)),(xa,y,zd(xa))],(0,sg,0)),mat)
        geo.poly(orient([(xa,-PIN,zd(xa)-.25),(xb,-PIN,zd(xb)-.25),(xb,PIN,zd(xb)-.25),(xa,PIN,zd(xa)-.25)],(0,0,-1)),mat)
    for x,sg in ((-L,-1),(L,1)):geo.poly(orient([(x,-PIN,zd(x)-.25),(x,PIN,zd(x)-.25),(x,PIN,zd(x)),(x,-PIN,zd(x))],(sg,0,0)),mat)
deck_slab(G['deck'],M['paving'])
# ---- arch rings, both faces; keystones project --------------------------------------------------------
DEP=.45
al=math.asin(HALF/R)
for s in (-1,1):
    for xc in (-(PIERX+HALF),PIERX+HALF):
        n=round(2*al*(R+DEP/2)/.5)|1   # odd: a keystone on the crown
        for k in range(n):
            phi=-al+2*al*(k+.5)/n;rm=R+DEP/2;key=k==n//2
            radial=Vector((math.sin(phi),0,math.cos(phi)));tang=Vector((math.cos(phi),0,-math.sin(phi)))
            size=(2*al*rm/n-.02,.4 if key else .34,DEP+(.12 if key else 0))
            Dg.box((xc+rm*math.sin(phi),s*(HW+(.14 if key else .05)-size[1]/2),ZC+(rm+(.06 if key else 0))*math.cos(phi)),size,M['dressed'],.015,Matrix((tang,Vector((0,1,0)),radial)).transposed(),rect=rect())
# ---- cutwaters on the central pier: pointed in plan, ramped caps rising to the parapet plinth ----------
ZT=.3;ZR=zd(0)-.25
for s in (-1,1):
    tip=(0,s*(HW+CUT));ca,cb=(-PIERX,s*HW),(PIERX,s*HW)
    Dg.prism([(x,y,(BED+ZT)/2) for x,y in (ca,cb,tip)],(0,0,1),ZT-BED,M['dressed'])
    t0=Vector((0,tip[1],ZT));a0,b0=Vector((ca[0],ca[1],ZT)),Vector((cb[0],cb[1],ZT));a1,b1=Vector((ca[0],ca[1],ZR)),Vector((cb[0],cb[1],ZR))
    Dg.poly(orient([t0,b1,a1],(0,s*(ZR-ZT),CUT)),M['dressed'])
    Dg.poly(orient([t0,a0,a1],(-1,0,0)),M['dressed']);Dg.poly(orient([t0,b0,b1],(1,0,0)),M['dressed'])
    Dg.poly(orient([a1,b1,b0,a0],(0,-s,0)),M['dressed'])
# ---- parapets: coursed blocks over a core, plinth course, coping, end posts ---------------------------
YC,TH=(PIN+HW)/2,HW-PIN
COURSES=[.27,.27,.27,.29];XE=10.4
for s in (-1,1):
    strip(S,sheared(-XE,XE,-.25,.85),*sorted((s*YC-.14,s*YC+.14)),M['stone'])
    z0=-.25
    for c,h in enumerate(COURSES):
        x=-XE-(c%2)*.4*kit.rng.random()
        while x<XE-.05:
            L0=kit.rng.choice(LENS);ln=min(L0,XE-x)
            if ln<.25:x+=ln;continue
            xc=x+ln/2;th=math.atan(slope(xc));ext=.04 if c==0 else 0
            Dg.box((xc,s*YC,zd(xc)+z0+h/2),(ln/math.cos(th)-.012,TH+2*ext,h-.012),M['dressed'],.012,Matrix.Rotation(-th,3,'Y'),rect=kit.rng.choice(BLOCKS[L0]))
            x+=ln
        z0+=h
    x=-XE
    while x<XE-.05:
        ln=min(.65,XE-x);xc=x+ln/2;th=math.atan(slope(xc))
        Dg.box((xc,s*YC,zd(xc)+.85+.07),(ln/math.cos(th)-.01,TH+.12,.14),M['dressed'],.012,Matrix.Rotation(-th,3,'Y'),rect=kit.rng.choice(COPING));x+=ln
    for sx in (-1,1):                         # end posts flush with the deck end
        xp=sx*(L-.37);top=1.35
        Dg.box((xp,s*YC,(-.25+top)/2),(.6,.62,top+.25),M['dressed'],.015)
        Dg.box((xp,s*YC,top+.05),(.74,.76,.1),M['dressed'],.015,rect=rect())
        pyramid(Dg,(xp,s*YC,top+.1),.6,.62,.26,M['dressed'])
# ---- export, checks -----------------------------------------------------------------------------------
objs=[g.object() for g in G.values()]
def rewet(o):
    """The kit damps stone up to 0.65 m above z=0; here z=0 is the deck end, so measure the wet band from the water."""
    me=o.data;col=me.color_attributes['Weathering']
    for poly in me.polygons:
        m=me.materials[poly.material_index]
        if m.name not in kit.DAMP:continue
        for li in poly.loop_indices:
            p=me.vertices[me.loops[li].vertex_index].co;damp=max(0,1-(p.z-WATER)/.65)*(.30+.15*math.sin(p.x*7.3+p.y*5.1))
            col.data[li].color=(min(.8,max(damp,kit.AGED.get(m.name,0))),col.data[li].color[1],0,1)
for o in objs:rewet(o)
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
assert abs(lo[0]+hi[0])<1e-6 and abs(lo[1]+hi[1])<1e-6,'must be centred on the origin in X and Y'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
assert abs(zd(L))<1e-9 and abs(zd(-L))<1e-9
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=80_000,f'budget {tris}'
bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();grade=0
for y in (-1.8,0,1.8):
    prev=None
    for i in range(219):
        x=-10.9+i*.1;hit,loc,*_,ob,_m=scene.ray_cast(dg,Vector((x,y,20)),Vector((0,0,-1)))
        assert hit and ob.name=='SM_SBRiverBridge_Deck' and abs(loc.z-zc(x,y))<.01,f'deck not walkable at x={x:.1f} y={y}'
        if prev is not None:grade=max(grade,abs(loc.z-prev)/.1)
        prev=loc.z
assert grade<=.21,grade
OUT=HERE/'SilverbrookRiverBridge'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
# ---- preview scene (not exported) ---------------------------------------------------------------------
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
def preview(name,verts,faces,mat):
    me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);ob=bpy.data.objects.new(name,me);pre.objects.link(ob);me.materials.append(mat)
prof=[(-60,0),(-12.5,0),(-10.5,-.8),(-9.4,-3.3),(-9,-3.7),(9,-3.7),(9.4,-3.3),(10.5,-.8),(12.5,0),(60,0)]
vs=[(x,y,z) for x,z in prof for y in (-40,40)]
preview('Ground',vs,[(2*i,2*i+2,2*i+3,2*i+1) for i in range(len(prof)-1)],kit.solid('Preview_Ground',(.11,.12,.07),.95))
preview('Water',[(-10,-40,WATER),(10,-40,WATER),(10,40,WATER),(-10,40,WATER)],[(0,1,2,3)],kit.solid('Preview_Water',(.03,.05,.06),.05))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,target,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=(Vector(target)-Vector(loc)).to_track_quat('-Z','Y').to_euler();c.lens=lens;c.clip_end=500;pre.objects.link(o)
cam('Beauty',(-15,-17,4.2),(0,0,.2),30);cam('Front',(0,-36,.8),(0,0,.8),30);cam('Side',(-27,-9,2.6),(0,0,.4),30);cam('Rear',(0,36,.8),(0,0,.8),30)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1600;scene.render.resolution_y=800;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
mins=Vector(lo);maxs=Vector(hi)
manifest=dict(asset='SilverbrookRiverBridge',reference=None,note='designed to fit Silverbrook crossings; no owner concept',context_screenshot='GameClient/ArtSource/Greenlands/Silverbrook-Crossing-InGame.png',
    blender=bpy.app.version_string,generator='build_riverbridge.py',method='procedural original geometry',
    units='metres, Z up, span along X, front -Y, origin mid-span, centred in Y, at deck-end ground level',bounds_m=[round(v,3) for v in maxs-mins],z_range_m=[round(lo[2],3),round(hi[2],3)],
    deck=dict(runs='along X (span); parapet faces -Y/+Y',length_m=2*L,nominal_span_m=2*L,stretch_x_supported=[.85,1.15],walkable_width_m=2*PIN,overall_width_m=2*HW,
              ends_z_m=0.0,midspan_z_m=round(zd(0),3),crown_z_m=DC,max_grade=round(grade,3),water_z_m=WATER,bed_z_m=BED,
              camber_m=CAM,profile='camber +0.06 m at the road centre (y=0), zero at the edges and at both ends; centreline z = 1.6*(0.5*(1-u^2)+0.25*(1+cos(pi*u))), u=|x|/11 (deck surface, between the parapets)',
              arches=f'two segmental, clear span {2*HALF} m, rise {RISE} m, spring z={SPRING}, centres x=+-{PIERX+HALF}; central pier |x|<{PIERX} with cutwaters to y=+-{HW+CUT}',
              abutment_faces_x_m=[-(PIERX+2*HALF),PIERX+2*HALF],abutment_origin_x_m=[-(L+3.0),L+3.0]),
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookRiverBridge',slot_m=list(SLOT),
    asset_id='BLD-SilverbrookRiverBridge-01',outputs=['SilverbrookRiverBridge.blend','SilverbrookRiverBridge.glb','SilverbrookRiverBridge.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('RIVERBRIDGE_BUILT',manifest['bounds_m'],manifest['z_range_m'],tris,'grade',round(grade,3))
