"""Silverbrook Fence Section from docs/Game-Design/concept-art/Silverbrook/Street Props.png (FENCE SECTION).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propfencesection.py
3 m x 1.2 m post-and-rail bay: two identical square oak end posts (chamfered caps) with three split oak
rails laid against their front faces and lashed with hemp rope. Bounds are exactly 3.0 m, so sections butt end to end.
"""
import sys,math,json
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
import bpy
PKG='PropFenceSection'
scene=kit.reset(5101)
asset=bpy.data.collections.new('Silverbrook'+PKG);scene.collection.children.link(asset)
M=dict(timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
       rope=kit.scan('SB_PropFenceSection_Rope','hessian_380',dark=(.30,.40,.52),damp=False,aged=.8))  # no rope scan exists: hessian fibre darkened to grey hemp
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
T=Geo('SM_SBPropFenceSection_Timber',asset);R=Geo('SM_SBPropFenceSection_Rope',asset);G=dict(timber=T,rope=R)
SLOT=(3.0,.3,1.2)
PITCH=3.0                  # tiling length: bounds width, sections butt end to end
PW,PD,PH=.14,.14,1.15      # square post, shaft height (chamfer cap adds .05)
PX=PITCH/2-PW/2-.025      # post centre; rope bands reach exactly the 3.0 m bounds
RAILS=(.38,.68,.98);RY=-(PD/2+.04)   # rails sit against the post fronts
RR=.0095                    # rope radius
def tube(pts,r=RR,sides=6):
    """Round rope strand along a polyline; rings are perpendicular to the local tangent, winding outward from each segment's axis."""
    rings=[]
    for i,p in enumerate(pts):
        t=(pts[min(i+1,len(pts)-1)]-pts[max(i-1,0)]).normalized();u=t.orthogonal().normalized();v=t.cross(u)
        rings.append([p+(u*math.cos(math.tau*k/sides)+v*math.sin(math.tau*k/sides))*r for k in range(sides)])
    for i,(A,B) in enumerate(zip(rings,rings[1:])):
        c=(pts[i]+pts[i+1])/2
        for k in range(sides):
            q=[A[k],A[(k+1)%sides],B[(k+1)%sides],B[k]];n=(q[1]-q[0]).cross(q[3]-q[0])
            if n.dot(sum(q,Vector())/4-c)<0:q.reverse()
            sc=kit.SCALE[M['rope'].name];uv=[(k/sides*.5,i*.02/sc),((k+1)/sides*.5,i*.02/sc),((k+1)/sides*.5,(i+1)*.02/sc),(k/sides*.5,(i+1)*.02/sc)]
            if q[0] is not A[k]:uv.reverse()
            R.poly(q,M['rope'],uv,smooth=True)
def lash(x,z,sign,turns=1.0):
    """One diagonal turn of rope round post and rail (rounded rectangle in plan, climbing 7 cm per turn; rises left to right at the front for sign=1)."""
    hx,y0,y1,rc=PW/2+.012,PD/2+.012,RY-.04-.012,.02;pts=[]
    corners=[(hx-rc,y1+rc,-90),(hx-rc,y0-rc,0),(-hx+rc,y0-rc,90),(-hx+rc,y1+rc,180)]
    path=[]
    for cx_,cy_,a0 in corners:
        for k in range(4):a=math.radians(a0+90*k/3);path.append((cx_+rc*math.cos(a),cy_+rc*math.sin(a)))
    if sign<0:path=[(-p[0],p[1]) for p in path][::-1]
    n=len(path);perim=sum(math.hypot(path[(i+1)%n][0]-path[i][0],path[(i+1)%n][1]-path[i][1]) for i in range(n));acc=0;P=[]
    for i in range(n+1):
        q=path[i%n];P.append(Vector((x+q[0],q[1],z+sign*(-.035+.07*acc/perim)+(.0 if sign>0 else 0))));
        acc+=math.hypot(path[(i+1)%n][0]-q[0],path[(i+1)%n][1]-q[1])
    tube(P)
def post(x):
    kit.rng.seed(77)       # end posts identical
    b=BANDS[3 % len(BANDS)]
    T.box((x,0,PH/2),(PW,PD,PH),M['timber'],.012,grain=True,band=b)
    a,c=PW/2,PD/2;apex=Vector((x,0,PH+.05));ring=[Vector((x+sx*a,sy*c,PH)) for sx,sy in ((-1,-1),(1,-1),(1,1),(-1,1))]
    for p,q in zip(ring,ring[1:]+ring[:1]):T.poly([p,q,apex],M['timber'])
for s in (-1,1):post(s*PX)
kit.rng.seed(5101)
for z in RAILS:
    pts=[(-PITCH/2,RY,z)]+[(x,RY+kit.rng.uniform(-.01,.01),z+kit.rng.uniform(-.012,.012)) for x in (-.5,.5)]+[(PITCH/2,RY,z)]
    for a,b in zip(pts,pts[1:]):T.beam(a,b,.09,.08,M['timber'],band=band())
    for s in (-1,1):
        for dz in (-.012,.012):lash(s*PX,z+dz,1);lash(s*PX,z+dz,-1)   # two turns, each crossing diagonally on the front
PLACEMENT=dict(footprint_m=[PITCH,.3],front='-Y',ground_contact=True,near=['farmhouse','stables','cottage','granary','road','square'],clearance_m=.4)
EXTRA=dict(tiling=dict(axis='X',length_m=PITCH,note='bounds width: place sections 3.0 m apart to butt end posts; or overlap by one post width (0.14 m) to share a post',end_post_width_m=PW))
# ---- recentre, assert slot, export, preview scene, manifest -------------------------
from mathutils import Matrix
import hashlib
SHEET=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Street Props.png'
objs=[g.object() for g in G.values()]
def bounds():
    pts=[o.matrix_world@Vector(c) for o in objs for c in o.bound_box];return Vector([min(p[k] for p in pts) for k in range(3)]),Vector([max(p[k] for p in pts) for k in range(3)])
lo,hi=bounds()
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2])))
lo,hi=bounds();size=hi-lo
assert all(s<=t+5e-3 for s,t in zip(size,SLOT)),f'exceeds slot {SLOT}: {[round(s,3) for s in size]}'
assert tuple(math.ceil(s*10-.05)/10 for s in size)==tuple(SLOT),f'slot_m must be bounds rounded up to 0.1 m: {[round(s,3) for s in size]}'
OUT=HERE/('Silverbrook'+PKG)
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot=None,ortho=None,lens=40,look=None):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;pre.objects.link(o)
    if look is not None:o.rotation_euler=(Vector(look)-Vector(loc)).to_track_quat('-Z','Y').to_euler()
    else:o.rotation_euler=[math.radians(a) for a in rot]
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
mid=(lo+hi)/2;md=max(size)
cam('Beauty',mid+Vector((-.75,-1.0,.55))*md*1.25,look=mid,lens=40)
cam('Front',(mid.x,-30,mid.z),(90,0,0),max(size[0],size[2])*1.2);cam('Side',(-30,mid.y,mid.z),(90,0,-90),max(size[1],size[2])*1.2);cam('Rear',(mid.x,30,mid.z),(90,0,180),max(size[0],size[2])*1.2)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=40000,f'{tris} triangles over the 40k prop budget'
manifest=dict(asset='Silverbrook'+PKG,reference=dict(file=PKG+'-reference.png',sha256=hashlib.sha256((HERE/(PKG+'-reference.png')).read_bytes()).hexdigest(),source='docs/Game-Design/concept-art/Silverbrook/Street Props.png (cropped to this prop)',source_sha256=hashlib.sha256(SHEET.read_bytes()).hexdigest()),blender=bpy.app.version_string,generator=f'build_{PKG.lower()}.py',
    method='procedural original geometry (concept interpretation, not image reconstruction)',units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in size],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_Silverbrook'+PKG,parts=[],slot_m=list(SLOT),
    placement=PLACEMENT,outputs=[f'Silverbrook{PKG}.blend',f'Silverbrook{PKG}.glb',f'Silverbrook{PKG}.fbx'],**EXTRA)
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(PKG.upper()+'_BUILT',manifest['bounds_m'],tris)
