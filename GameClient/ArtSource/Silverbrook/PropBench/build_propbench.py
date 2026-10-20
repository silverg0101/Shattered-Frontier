"""Silverbrook Bench from docs/Game-Design/concept-art/Silverbrook/Street Props.png (BENCH).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propbench.py
Sturdy 1.8 m oak plank bench (Street Props concept sheet): a thick seat plank with chamfered edges, four stout legs splayed in both
directions and through-tenoned into the seat (wedged, end grain showing), an end rail pinning each leg
pair and a long stretcher between the pairs.
"""
import sys,math,json
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
import bpy
PKG='PropBench'
scene=kit.reset(5301)
asset=bpy.data.collections.new('Silverbrook'+PKG);scene.collection.children.link(asset)
M=dict(timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
       end=kit.scan('SB_PropBench_EndGrain','sb_endgrain',dark=(.62,.58,.52),damp=False,aged=.4))
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
G=dict(timber=Geo('SM_SBPropBench_Timber',asset))
SLOT=(1.8,.4,.5)
L,D,T,SEAT=1.8,.3,.07,.474       # seat length, depth, thickness, top height
T_=G['timber']
T_.box((0,0,SEAT-T/2),(L,D,T),M['timber'],.012,grain=True,band=BANDS[2%len(BANDS)])
LX,LY,SX,SY=.70,.07,.10,.05         # leg top (x,y) and extra foot splay
for sx in (-1,1):
    tops=[]
    for sy in (-1,1):
        top=Vector((sx*LX,sy*LY,SEAT-.01));foot=Vector((sx*(LX+SX),sy*(LY+SY),0))
        T_.beam(foot,top,.09,.09,M['timber'],band=band());tops.append((top,foot))
        T_.box((top.x,top.y,SEAT+.008),(.06,.06,.016),M['end'],.004,rect=(.38,.38,.24,.24))    # wedged tenon end
    f=lambda z:[(a.x+(b.x-a.x)*(a.z-z)/(a.z-b.z),a.y+(b.y-a.y)*(a.z-z)/(a.z-b.z),z) for a,b in tops] # leg axis at height z
    (p0,p1)=f(.20);T_.beam((p0[0],p0[1]-.04,.20),(p1[0],p1[1]+.04,.20),.05,.07,M['timber'],band=band())  # end rail between the pair
T_.beam((-LX-.04,0,.20),(LX+.04,0,.20),.06,.05,M['timber'],band=band())  # long stretcher, pins the end rails
for sx in (-1,1):T_.box((sx*(LX+.025),0,.20),(.025,.03,.04),M['end'],.003,rect=(.4,.4,.2,.2))
PLACEMENT=dict(footprint_m=[1.8,.3],front='-Y',ground_contact=True,near=['square','inn','stables','farmhouse','cottage','market','road','temple'],clearance_m=.6)
EXTRA=dict()
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
