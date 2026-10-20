"""Silverbrook Watchtower from docs/Game-Design/concept-art/Silverbrook/Watchtower.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_watchtower.py
Original procedural geometry interpreting the elevation (slot 8 x 8 x 9 m): chamfered-square
rough-block tower on battered corner buttresses, arched plank door with flanking lanterns,
string course, pointed arrow slits, oak hoarding brackets carrying a crenellated parapet
with corner turrets and fire baskets, an oak lookout with arched braces under a shingled
pyramid roof, flagpole with swallowtail pennant, and two stag banners on oak arms.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,frame,door,lantern,tile_plane,deck
import bpy
scene=kit.reset(4801)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Watchtower.png'
asset=bpy.data.collections.new('SilverbrookWatchtower');scene.collection.children.link(asset)

M=dict(
    stone=kit.scan('SB_Watchtower_Stone','seaworn_stone_tiles',dark=(.40,.41,.43)),
    shingle=kit.scan('SB_Watchtower_Shingle','wood_planks_grey',dark=(.32,.30,.28),damp=False,aged=.25),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_Watchtower_Banner','denim_fabric_03',dark=(.12,.14,.30),damp=False,aged=.75),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    flame=kit.solid('SB_Watchtower_Flame',(.40,.10,.02),.4,0,(1.0,.20,.02),1.6),
    gilt=kit.solid('SB_Watchtower_Gilt',(.80,.55,.18),.35,1.0))
M['ashlar']=M['stone']
STONES=kit.patches('seaworn_stone_tiles',(.05,.035),dark=.65,frac=.02,std=.3)
SHINGLES=kit.patches('wood_planks_grey',(.06,.12),dark=.55,frac=.02,std=.3)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
def rect():return kit.rng.choice(STONES)

G=dict(stone=Geo('SM_SBWatchtower_Masonry',asset),timber=Geo('SM_SBWatchtower_Timber',asset),roof=Geo('SM_SBWatchtower_Roof',asset),detail=Geo('SM_SBWatchtower_Details',asset))
# ---- measured layout (metres; concept read at ~120 px/m, flagpole shortened to fit 9 m) -----
SLOT=(8,8,9)
A,F,TH=3.4,2.3,.45              # shaft: across flats, flat-face width, wall thickness
TOP=5.25                        # shaft walls run up into the hoarding deck
STEP,DW,DS=.17,.95,1.3          # door sill (step top), width, arch spring above sill
SC0,SC1=2.6,2.75                # string course
SL0,SL1,SLW=3.3,4.3,.2          # arrow slits
S2,RB,DECK0,DECK1=2.35,.35,5.22,5.5   # hoarding ring-beam outer half-extent, ring width, deck
PB,PH,CREN=5.55,1.05,.45        # parapet base, height, crenel depth
TT,TZ=.43,6.5                   # corner turret half-size, wall top (cap above)
LP,PLATE=1.0,7.52               # lookout post half-spacing, plate top
EX,EAVE,PEAK=1.35,7.42,8.15     # pyramid roof
V=[(F/2,-A/2),(A/2,-F/2),(A/2,F/2),(F/2,A/2),(-F/2,A/2),(-A/2,F/2),(-A/2,-F/2),(-F/2,-A/2)]
def faces():
    """(centre, outward normal, half width, flat?) for the 8 shaft faces, front first."""
    for i in range(8):
        a,b=Vector((*V[i-1],0)),Vector((*V[i],0));d=b-a
        yield (a+b)/2,Vector((d.y,-d.x,0)).normalized(),d.length/2,i%2==0
def offset(d):
    out=[]
    for i,p in enumerate(V):
        a,b,c=Vector(V[i-1]),Vector(p),Vector(V[(i+1)%8]);n1=Vector(((b-a).y,-(b-a).x)).normalized();n2=Vector(((c-b).y,-(c-b).x)).normalized();bis=(n1+n2).normalized()
        out.append(b+bis*d/bis.dot(n1))
    return out

def wall(geo,c,o,half,height,openings=(),th=TH):
    """kit.wall with several openings per span (door + slit stacked on the front face)."""
    P,R=frame(c,o);cuts=sorted({-half,half,*[x for a,w,_,_ in openings for x in (a-w/2,a+w/2)]})
    for a0,a1 in zip(cuts,cuts[1:]):
        spans=[(0,height)]
        for a,w,z0,z1 in openings:
            if abs(a-(a0+a1)/2)<w/2:spans=[s for lo,hi in spans for s in ((lo,min(hi,z0)),(max(lo,z1),hi))]
        for z0,z1 in spans:
            if z1-z0>.005:geo.box(P((a0+a1)/2,-th/2,(z0+z1)/2),(a1-a0,th,z1-z0),M['stone'],0,R)
    return P
def arch(P,o,a,w,spring,th=TH):
    rad=w/2
    for side in (-1,1):
        pts=[P(a+side*rad*math.cos(t),0,spring+rad*math.sin(t)) for t in [k*math.pi/16 for k in range(9)]]+[P(a+side*rad,0,spring+rad)]
        G['stone'].prism([p-o*th/2 for p in pts],o,th,M['stone'])
def slit(P,R,o,a=0,z0=SL0,z1=SL1,w=SLW,th=TH,surround=True):
    """Pointed arrow slit: stone spandrels close the head inside the opening, dressed surround outside."""
    for side in (-1,1):G['stone'].prism([p-o*th/2 for p in (P(a+side*w/2,0,z1-.16),P(a,0,z1),P(a+side*w/2,0,z1))],o,th,M['stone'])
    if not surround:return
    for side in (-1,1):G['stone'].box(P(a+side*(w/2+.09),.0,(z0+z1)/2),(.18,.12,z1-z0+.04),M['ashlar'],.012,R,rect=rect())
    G['stone'].box(P(a,.01,z0-.07),(w+.44,.14,.14),M['ashlar'],.015,R,rect=rect())
    G['stone'].box(P(a,.0,z1+.1),(w+.4,.12,.2),M['ashlar'],.015,R,rect=rect())

# ---- shaft: eight walls, door, slits, buttresses, string course ---------------------------------
for k,(c,o,half,flat) in enumerate(faces()):
    holes=[]
    if k==0:holes.append((0,DW,STEP,STEP+DS+DW/2))
    if flat:holes.append((0,SLW,SL0,SL1))
    P=wall(G['stone'],c,o,half,TOP,holes);_,R=frame(c,o)
    if k==0:arch(P,o,0,DW,STEP+DS)
    if flat:slit(P,R,o)
    else:   # battered corner buttress, its sloped face ending under the string course
        G['stone'].prism([P(0,d,z) for d,z in ((-.15,0),(.7,0),(.05,SC0+.02),(-.15,SC0+.02))],(P(1,0,0)-P(0,0,0)),2*half,M['stone'])
# solid stair core: what the slits look onto, so the tower never reads see-through
G['stone'].box((0,0,(TOP-.05)/2),(A-2*TH-.5,A-2*TH-.5,TOP-.05),M['stone'],0)
G['stone'].prism([(*p,(SC0+SC1)/2) for p in offset(.07)],(0,0,1),SC1-SC0,M['ashlar'])
G['stone'].box((0,-A/2-.25,STEP/2),(1.5,.5,STEP),M['ashlar'],.015,rect=rect())
door(G,M,(0,-A/2,STEP),(0,-1,0),DW,DS,STONES,wall_th=TH,band=band())
for x in (-1.05,1.05):lantern(G,M,(x,-A/2-.3,1.62),arm=((x,-A/2-.02,1.62),(0,-1,0)))

# ---- hoarding: joists, hanging posts, braces, ring beam, deck --------------------------------------
def tbox(centre,size,R=None):G['timber'].box(centre,size,M['timber'],.012,R,grain=True,band=band())
def tbeam(a,b,w=.14,d=None):G['timber'].beam(a,b,w,d or w,M['timber'],band=band())
for c,o,half,flat in faces():
    P,R=frame(c,o)
    if flat:
        out=S2-A/2
        for a in (-.8,0,.8):
            tbeam(P(a,-.05,DECK0-.11),P(a,out-.02,DECK0-.11),.16,.22)
            tbox(P(a,.05,4.8),(.16,.14,.6),R)
            if a:tbeam(P(a,.1,4.6),P(a,out-.12,DECK0-.12),.1)
    else:   # diagonal joist and long brace to each deck corner
        corner=Vector((math.copysign(S2-.1,c.x),math.copysign(S2-.1,c.y),0))
        tbeam(c-o*.05+Vector((0,0,DECK0-.11)),corner+Vector((0,0,DECK0-.11)),.16,.22)
        tbeam(c-o*.02+Vector((0,0,4.2)),corner-(corner-c)*.15+Vector((0,0,DECK0-.12)),.14)
G['timber'].box((0,0,(DECK0+DECK1)/2),(2*S2-.5,2*S2-.5,DECK1-DECK0),M['door'],.01,grain=True)
RZ=(DECK0-.02+PB)/2;RH=PB-DECK0+.02
for s in (-1,1):
    tbox((0,s*(S2-RB/2),RZ),(2*S2,RB,RH))
    tbox((s*(S2-RB/2),0,RZ),(RB,2*S2-2*RB,RH))

# ---- parapet: crenellated walls between square corner turrets, oak rails in the crenels -----------
TC=S2-TT+.05
for o in ((0,-1,0),(1,0,0),(0,1,0),(-1,0,0)):
    o=Vector(o);c=o*S2+Vector((0,0,PB));half=TC-TT+.01
    P=wall(G['stone'],c,o,half,PH,[(-.6,.42,PH-CREN,9),(.6,.42,PH-CREN,9)],th=.28)
    for a in (-.6,.6):
        tbeam(P(a-.23,-.14,PH-.27),P(a+.23,-.14,PH-.27),.07)
        for da in (-.12,0,.12):tbox(P(a+da,-.14,PH-CREN+.08),(.05,.05,.16),frame(c,o)[1])
for sx in (-1,1):
    for sy in (-1,1):
        tc=Vector((sx*TC,sy*TC,PB))
        for o in ((0,-1,0),(1,0,0),(0,1,0),(-1,0,0)):
            o=Vector(o);outer=o.x*sx>0 or o.y*sy>0;side=abs(o.x)>0
            P=wall(G['stone'],tc+o*TT,o,TT-(.2 if side else 0),TZ-PB,[(0,.09,.3,.72)] if outer else [],th=.2)
            if outer:slit(P,frame(tc+o*TT,o)[1],o,0,.3,.72,.09,.2,surround=False)
        G['stone'].box((tc.x,tc.y,TZ+.075),(2*TT+.1,2*TT+.1,.15),M['ashlar'],.02,rect=rect())
        # fire basket: iron stem, cage of splayed bars and rings, glowing coals and flame
        b=Vector((tc.x,tc.y,TZ+.15))
        G['detail'].box(b+Vector((0,0,.05)),(.08,.08,.12),M['iron'],.004)
        ring=lambda r,z:[b+Vector((r*math.cos(math.tau*k/8),r*math.sin(math.tau*k/8),z)) for k in range(8)]
        lo,hi,tip=ring(.12,.1),ring(.26,.38),ring(.3,.48)
        for k in range(8):
            G['detail'].beam(lo[k],tip[k],.025,.025,M['iron'],grain=False)
            for r in (lo,hi):G['detail'].beam(r[k],r[(k+1)%8],.025,.025,M['iron'],grain=False)
        G['detail'].vessel(b+Vector((0,0,.1)),[(0,0),(.12,0),(.24,.25),(0,.25)],M['iron'],12)
        G['detail'].vessel(b+Vector((0,0,.3)),[(0,0),(.2,0),(.17,.15),(.1,.3),(0,.42)],M['flame'],12)

# ---- lookout: posts, plates, arched braces, shingled pyramid roof, flagpole -------------------------
for sx in (-1,1):
    for sy in (-1,1):tbox((sx*LP,sy*LP,(DECK1-.02+PLATE-.22)/2),(.2,.2,PLATE-.22-DECK1+.02))
for s in (-1,1):
    tbox((0,s*LP,PLATE-.11),(2*LP+.24,.2,.22));tbox((s*LP,0,PLATE-.11),(.2,2*LP-.2,.22))
    for o in (Vector((0,s,0)),Vector((s,0,0))):
        r=Vector((0,0,1)).cross(o);base=o*LP
        pts=[base+r*(-.9*math.cos(t))+Vector((0,0,6.55+.75*math.sin(t))) for t in [k*math.pi/12 for k in range(13)]]
        for p,q in zip(pts,pts[1:]):G['timber'].beam(p,q,.12,.14,M['timber'],band=band())
L=math.hypot(EX,PEAK-EAVE)
def plane(O,e,toward):
    u=Vector(toward).normalized();lo=lambda t:-EX*(1-t/L);hi=lambda t:EX*(1-t/L)
    tile_plane(G['roof'],O,e,u,L,lo,hi,M['shingle'],SHINGLES,size=(.2,.36,.022),exposure=.15,tone=(.05,.4))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'])
plane((0,-EX,EAVE),(1,0,0),(0,EX,PEAK-EAVE));plane((0,EX,EAVE),(-1,0,0),(0,-EX,PEAK-EAVE))
plane((EX,0,EAVE),(0,1,0),(-EX,0,PEAK-EAVE));plane((-EX,0,EAVE),(0,-1,0),(EX,0,PEAK-EAVE))
for sx in (-1,1):
    for sy in (-1,1):G['timber'].beam((sx*EX,sy*EX,EAVE+.05),(0,0,PEAK+.05),.12,.07,M['timber'],band=band())
for s in (-1,1):
    G['timber'].beam((-EX-.02,s*(EX+.02),EAVE-.06),(EX+.02,s*(EX+.02),EAVE-.06),.04,.17,M['timber'],band=band())
    G['timber'].beam((s*(EX+.02),-EX,EAVE-.06),(s*(EX+.02),EX,EAVE-.06),.04,.17,M['timber'],band=band())
tbox((0,0,PEAK+.06),(.22,.22,.2))
FT=8.78
G['detail'].beam((0,0,PEAK),(0,0,FT),.06,.06,M['iron'],grain=False)
G['detail'].vessel((0,0,FT),[(0,0),(.05,0),(.05,.03),(0,.18)],M['iron'],8)

# ---- stag banners and pennant (gilt cut-out stag after the Golden Stag Inn) ----------------------
def stag(origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R,U,N=Vector(right),Vector(up),Vector(normal);geo=G['detail']
    def p(a,z):return o+R*(a*s)+U*(z*s)
    geo.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        geo.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def trim(pts,normal,w=.04):
    n=Vector(normal)*.008
    for a,b in zip(pts,pts[1:]+pts[:1]):G['detail'].beam(Vector(a)+n,Vector(b)+n,.02,w,M['gilt'],grain=False)
AY,AZ,BX=-1.0,4.75,2.5
for s in (-1,1):
    tbeam((s*(A/2-.05),AY,AZ),(s*2.95,AY,AZ),.14,.16)
    tbeam((s*(A/2-.02),AY,4.05),(s*2.3,AY,AZ-.06),.1)
    xc=s*BX;outline=[(xc-.36,4.5),(xc+.36,4.5),(xc+.36,2.95),(xc,2.6),(xc-.36,2.95)]
    G['detail'].prism([(x,AY,z) for x,z in outline],(0,1,0),.02,M['banner'])
    G['detail'].beam((xc-.42,AY,4.53),(xc+.42,AY,4.53),.035,.035,M['iron'],grain=False)
    for dx in (-.3,.3):G['detail'].beam((xc+dx,AY,4.52),(xc+dx,AY,AZ-.07),.025,.025,M['iron'],grain=False)
    for n in (-1,1):
        trim([(x,AY+n*.01,z) for x,z in outline],(0,n,0))
        stag((xc,AY+n*.01,3.72),(-n,0,0),(0,0,1),(0,n,0),.62)
FX,FZ=.03,8.52
pennant=[[(FX,8.76),(FX+1.0,8.71),(FX+1.0,8.31),(FX,8.27)],[(FX+1.0,8.71),(FX+1.5,8.68),(FX+1.0,8.52)],[(FX+1.0,8.51),(FX+1.5,8.33),(FX+1.0,8.31)]]
for piece in pennant:G['detail'].prism([(x,0,z) for x,z in piece],(0,1,0),.02,M['banner'])
for z in (8.32,8.7):G['detail'].box((0,0,z),(.1,.1,.04),M['iron'],.004)
for n in (-1,1):stag((FX+.5,n*.01,FZ-.05),(-n,0,0),(0,0,1),(0,n,0),.33)

# ---- export, preview scene, manifest -------------------------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2])))
assert abs(lo[2])<.01,f'model does not stand on the ground: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookWatchtower'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookWatchtower')  # door leaves: own meshes, hinged
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-9.4,-12.2,4.6),(88,0,-37.5),lens=28);cam('Front',(0,-30,4.6),(90,0,0),9.6);cam('Side',(-30,0,4.6),(90,0,-90),9.6);cam('Rear',(0,30,4.6),(90,0,180),9.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Watchtower-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'{tris} triangles exceeds the 120k house budget'
manifest=dict(asset='SilverbrookWatchtower',reference=dict(file='Watchtower-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_watchtower.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookWatchtower',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookWatchtower.blend','SilverbrookWatchtower.glb','SilverbrookWatchtower.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('WATCHTOWER_BUILT',manifest['bounds_m'],tris)
