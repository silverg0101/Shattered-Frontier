"""Silverbrook town-wall section from docs/Game-Design/concept-art/Silverbrook/Wall Section.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_wallsection.py
Modular curtain segment 20 m long (X), 3 m deep, 8 m to the turret merlons, sized from the
`wall` entry of Greenlands/silverbrook-town-layout.json. Square battered wall-turrets every
20/3 m: full ones at x=+-3.33, half ones at both ends, so two segments butted together close a
turret over the joint. Every part is mirror-symmetric about x=0 and cut flat at x=+-10, so both
end faces carry the same section. Crenellated parapet on a corbel table, cross arrow loops,
continuous flagged wall-walk behind the parapet.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
import bpy
from mathutils import Matrix
scene=kit.reset(2001)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Wall Section.png'
LAYOUT=json.loads((HERE.parents[1]/'Greenlands/silverbrook-town-layout.json').read_text())['wall']
asset=bpy.data.collections.new('SilverbrookWallSection');scene.collection.children.link(asset)

M=dict(
    stone=kit.scan('SB_WallSection_Masonry','rough_block_wall',dark=(.70,.74,.86),aged=.4),  # cooled toward the concept's pale grey
    paving=kit.scan('SB_WallSection_Paving','medieval_blocks_03',dark=(.55,.58,.62),damp=False,aged=.25),
    void=kit.solid('SB_WallSection_LoopVoid',(.012,.012,.014),.95))
# 6 repeats per segment (3.33 m instead of the scan's 3.04 m: blocks ~0.47 m like the concept) so world-projected masonry
# continues across the joint; the 2 m flagstone scan already divides 20 m.
kit.SCALE['SB_WallSection_Masonry']=20/6
STONES=kit.patches('rough_block_wall',(.06,.035),dark=.7,frac=.01,std=.25)
G=dict(stone=Geo('SM_SBWallSection_Masonry',asset),dressed=Geo('SM_SBWallSection_Dressed',asset),walk=Geo('SM_SBWallSection_Walk',asset))
# ---- measured layout (metres) -------------------------------------------------
L,T,H=LAYOUT['segment_length'],LAYOUT['thickness'],LAYOUT['height']
SLOT=(L,T,H);E=L/2;YF,YB=-T/2,T/2     # ends at x=+-E; slot front/back faces
CF=-.70                        # curtain outer face
SK=.30                         # loop skin depth (loops are recesses with a dark back)
WALK=5.30;PAVE=.10             # core top, flagstones on top -> walk surface 5.40
PF,PB=-.95,-.45                # parapet front/back (overhangs the curtain on the corbel table)
SILL,MER,COP=6.45,7.20,.10     # crenel sill, merlon top, coping thickness
RB=1.15                        # rear parapet inner face (inferred)
TOW=[-E,-E/3,E/3,E]            # turret centres; the end ones are halves
SW,SF=.70,-1.30                # shaft half-width, shaft front
BW,BH=1.20,2.30                # batter half-width at ground, batter top
HW,HF,HT=.85,-1.44,7.60        # turret head half-width, front, top of head wall
STR=(2.25,2.40)                # lower string course
LOOP=(3.25,4.25)               # cross loop in the curtain and shaft
w0=lambda:(kit.rng.uniform(.05,.3),0)

def B(geo,x0,x1,y0,y1,z0,z1,mat,bevel=.012,rect=None,w=(0,0)):
    """Axis-aligned box cut to the segment; a box cut at an end face gets no chamfer so the face stays flat."""
    a,b=max(x0,-E),min(x1,E)
    if b-a<.004:return
    if a!=x0 or b!=x1:bevel=0
    geo.box(((a+b)/2,(y0+y1)/2,(z0+z1)/2),(b-a,y1-y0,z1-z0),mat,bevel,rect=rect,w=w)

def frustum(geo,bot,top,z0,z1,mat):
    """Battered base: bot/top = (x0,x1,y0,y1) rectangles, cut to the segment."""
    cl=lambda r:(max(r[0],-E),min(r[1],E),r[2],r[3])
    (a0,a1,b0,b1),(c0,c1,d0,d1)=cl(bot),cl(top)
    v=[Vector(p) for p in [(a0,b0,z0),(a1,b0,z0),(a1,b1,z0),(a0,b1,z0),(c0,d0,z1),(c1,d0,z1),(c1,d1,z1),(c0,d1,z1)]]
    faces=[[3,2,1,0],[4,5,6,7]]+[[i,(i+1)%4,(i+1)%4+4,i+4] for i in range(4)]
    vol=0
    for f in faces:
        geo.poly([v[i] for i in f],mat);vol+=sum(v[f[0]].dot(v[f[j]].cross(v[f[j+1]])) for j in range(1,len(f)-1))
    assert vol>0,'inside-out batter'

def holed(geo,x0,x1,yf,depth,z0,z1,holes,mat):
    """Masonry slab [x0,x1]x[yf,yf+depth]x[z0,z1] with rectangular holes (x0,x1,z0,z1) through it,
    backed by a dark void so the loop reads as an opening."""
    xs=sorted({x0,x1,*[max(x0,min(x1,v)) for h in holes for v in h[:2]]})
    for a,b in zip(xs,xs[1:]):
        m=(a+b)/2;spans=sorted((h[2],h[3]) for h in holes if h[0]<m<h[1]);z=z0
        for s0,s1 in spans+[(z1,z1)]:
            if s0-z>.004:B(geo,a,b,yf,yf+depth,z,s0,mat,0)
            z=max(z,s1)
    for h in holes:B(G['dressed'],h[0]-.01,h[1]+.01,yf+depth-.02,yf+depth+.02,h[2]-.01,h[3]+.01,M['void'],0)

def cross(x,z0,z1,bar=True):
    """Cross loop: tall slit plus, low down, a short cross-bar (as in the concept)."""
    out=[(x-.05,x+.05,z0,z1)]
    if bar:zb=z0+(z1-z0)*.38;out+=[(x-.16,x-.05,zb,zb+.08),(x+.05,x+.16,zb,zb+.08)]
    return out

def surround(x,yf,z0,z1):
    """Dressed jambs, head and sill framing a loop, 5 cm proud of the face."""
    for a,b in ((x-.36,x-.19),(x+.19,x+.36)):B(G['dressed'],a,b,yf-.05,yf+.06,z0-.12,z1+.12,M['stone'],.01,kit.rng.choice(STONES),w0())
    for za,zb in ((z0-.22,z0-.06),(z1+.06,z1+.22)):B(G['dressed'],x-.4,x+.4,yf-.06,yf+.06,za,zb,M['stone'],.01,kit.rng.choice(STONES),w0())

def course(geo,x0,x1,y0,y1,z0,z1,step=.62):
    """A run of dressed blocks (copings, string courses), joint every `step`; joints symmetric about x=0."""
    n=max(1,round((x1-x0)/step));d=(x1-x0)/n
    for i in range(n):B(geo,x0+i*d+.004,x0+(i+1)*d-.004,y0,y1,z0,z1,M['stone'],.012,kit.rng.choice(STONES),w0())
    # hidden backing so the 8 mm joints show shadow, not daylight
    B(geo,x0,x1,y0+.03,y1-.03,z0+.02,z1-.02,M['stone'],0)

# ---- curtain core, loops, rear ---------------------------------------------------
B(G['stone'],-E,E,CF+SK,YB,0,WALK,M['stone'],0)
holes=[]
bays=[(a+SW,b-SW) for a,b in zip(TOW,TOW[1:])]
for a,b in bays:
    c=(a+b)/2
    for x in (c-1.6,c,c+1.6):holes+=cross(x,*LOOP);surround(x,CF,*LOOP)
holed(G['stone'],-E,E,CF,SK,0,WALK,holes,M['stone'])
for a,b in bays:course(G['dressed'],a,b,CF-.08,CF+.06,*STR)
# corbel table under the parapet: two-stage stone brackets
for a,b in bays:
    for k in range(4):
        x=a+(b-a)*(k+.5)/4
        B(G['dressed'],x-.15,x+.15,PF-.03,CF+.05,4.85,WALK,M['stone'],.01,kit.rng.choice(STONES),w0())
        B(G['dressed'],x-.12,x+.12,PF+.12,CF+.05,4.45,4.85,M['stone'],.01,kit.rng.choice(STONES),w0())
# ---- parapet with merlons ----------------------------------------------------------
for t0,t1 in zip(TOW,TOW[1:]):
    a,b=t0+HW,t1-HW
    course(G['dressed'],a,b,PF-.05,PB,WALK,WALK+.15)               # string course on the corbels
    B(G['stone'],a,b,PF,PB,WALK+.15,SILL,M['stone'],0)
    course(G['dressed'],a,b,PF-.04,PB+.04,SILL,SILL+COP,.55)        # crenel sills
    n=5;g=.5;m=(b-a-(n-1)*g)/n
    for k in range(n):
        x=a+k*(m+g)
        B(G['stone'],x,x+m,PF,PB,SILL+COP,MER,M['stone'],0)
        course(G['dressed'],x-.04,x+m+.04,PF-.05,PB+.05,MER,MER+COP,m+.1)
# rear parapet (inferred: the concept shows only the field face)
B(G['stone'],-E,E,RB,YB,WALK,WALK+.85,M['stone'],0)
course(G['dressed'],-E,E,RB-.04,YB,WALK+.85,WALK+.95)
# ---- wall-walk: one continuous flagged surface end to end ------------------------------
B(G['walk'],-E,E,PB,RB,WALK,WALK+PAVE,M['paving'],0)
# ---- turrets --------------------------------------------------------------------------
for c in TOW:
    frustum(G['stone'],(c-BW,c+BW,YF,CF+.1),(c-SW,c+SW,SF,CF+.1),0,BH,M['stone'])
    holed(G['stone'],c-SW,c+SW,SF,.25,BH,WALK,cross(c,*LOOP),M['stone']);surround(c,SF,*LOOP)
    B(G['stone'],c-SW,c+SW,SF+.25,CF+.1,BH,WALK,M['stone'],0)
    B(G['dressed'],c-SW-.08,c+SW+.08,SF-.08,CF+.05,*STR,M['stone'],.012,kit.rng.choice(STONES),w0())
    # corbels carrying the overhanging head: two on the front, one on each side
    for x in (c-.42,c+.42):B(G['dressed'],x-.13,x+.13,HF+.02,SF+.05,4.80,WALK,M['stone'],.01,kit.rng.choice(STONES),w0())
    for s in (-1,1):B(G['dressed'],c+s*SW-.13 if s<0 else c+SW-.05,c-SW+.05 if s<0 else c+SW+.13,-1.12,-.88,4.80,WALK,M['stone'],.01,kit.rng.choice(STONES),w0())
    # head: ring course, walls with a plain loop, corner merlons
    course(G['dressed'],c-HW-.05,c+HW+.05,YF,PB,WALK,WALK+.15,.6)
    holed(G['stone'],c-HW,c+HW,HF,.25,WALK+.15,HT,cross(c,6.55,7.25,bar=False),M['stone']);surround(c,HF,6.55,7.25)
    B(G['stone'],c-HW,c+HW,HF+.25,PB,WALK+.15,HT,M['stone'],0)
    for x0,x1 in ((c-HW-.03,c-HW+.45),(c+HW-.45,c+HW+.03)):
        for y0,y1 in ((HF-.03,HF+.40),(PB-.40,PB+.03)):
            B(G['stone'],x0,x1,y0,y1,HT,H-COP,M['stone'],0)
            B(G['dressed'],x0-.02,x1+.02,y0-.02 if y0<-1 else y0,y1+.02 if y1>-.5 else y1,H-COP,H,M['stone'],.01,kit.rng.choice(STONES),w0())
# ---- checks, export, preview scene, manifest ----------------------------------------
objs=[g.object() for g in G.values()]
pts=[o.matrix_world@v.co for o in objs for v in o.data.vertices]
lo=[min(p[k] for p in pts) for k in range(3)];hi=[max(p[k] for p in pts) for k in range(3)]
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)) and abs(lo[0]+E)<1e-4 and abs(hi[0]-E)<1e-4 and lo[2]>-1e-4,f'slot {SLOT}: {lo} {hi}'
# Tiling: the section at x=-E must equal the section at x=+E (both end faces flush and matching).
end=lambda s:sorted({(round(p.y,4),round(p.z,4)) for p in pts if abs(p.x-s*E)<1e-4})
assert end(-1)==end(1) and len(end(1))>20,'end faces differ'
# Wall-walk: one top face spanning the full segment.
walk_tops=[p for p in G['walk'].v if abs(p.z-WALK-PAVE)<1e-4];assert min(p.x for p in walk_tops)==-E and max(p.x for p in walk_tops)==E,'walk not continuous'
OUT=HERE/'SilverbrookWallSection'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
for o in objs:   # neighbouring segments share the mesh: shows the joint turret and walk in the renders
    for dx in (-L,L):n=o.copy();n.location.x=dx;pre.objects.link(n)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-60,-60,0),(60,-60,0),(60,60,0),(-60,60,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-9,-16,4.5),(84,0,-30),lens=26);cam('Front',(0,-40,4),(90,0,0),21);cam('Side',(-40,0,4),(90,0,-90),9);cam('Rear',(0,40,4),(90,0,180),21);cam('Walk',(-13,6,10),(62,0,-118),lens=28)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'WallSection-reference.png')
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=120000,tris
manifest=dict(asset='SilverbrookWallSection',reference=dict(file='WallSection-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_wallsection.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(h-l,3) for l,h in zip(lo,hi)],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookWallSection',slot_m=list(SLOT),
    tiling=dict(axis='X',segment_length_m=L,end_faces='identical sections at x=+-%g'%E,walk_top_z=WALK+PAVE,walk_y=[PB,RB],masonry_uv_repeat_m=round(20/6,4)),
    outputs=['SilverbrookWallSection.blend','SilverbrookWallSection.glb','SilverbrookWallSection.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('WALLSECTION_BUILT',manifest['bounds_m'],tris)
