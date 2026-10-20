"""Silverbrook street prop: hand-cranked sandstone grindstone on an oak frame over a water trough (designed to match Silverbrook; no owner concept).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propgrindstone.py
Wheel axis along X, crank on +X, wheel dips into the trough, front faces -Y.
"""
import sys,math,json
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
import bpy
from mathutils import Matrix
NAME='PropGrindstone';scene=kit.reset(5102)
asset=bpy.data.collections.new('SilverbrookPropGrindstone');scene.collection.children.link(asset)
M=dict(timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    boards=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    stone=kit.scan('SB_PropGrindstone_Sandstone','sandstone_blocks_05',dark=(.36,.33,.30),damp=False,aged=.8,normal=1.0),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    water=kit.solid('SB_PropGrindstone_Water',(.03,.05,.05),.04))
BANDS=kit.bands('weathered_brown_planks');band=lambda:kit.rng.choice(BANDS)
STONES=kit.patches('sandstone_blocks_05',(.07,.07),dark=.7,frac=.03,std=.2)
G=dict(wood=Geo('SM_SBGrindstone_Frame',asset),trough=Geo('SM_SBGrindstone_Trough',asset),stone=Geo('SM_SBGrindstone_Wheel',asset),iron=Geo('SM_SBGrindstone_Iron',asset),water=Geo('SM_SBGrindstone_Water',asset));EXTRA=[]
SCALE=1.2;SMOOTH=['stone'];EXPECT=[(1.0,1.35),(.9,1.3),(1.1,1.3)];BUDGET=40000
PLACEMENT=dict(footprint_m=None,front='-Y',ground_contact=True,near=['blacksmith','workshop','weapons vendor','armor vendor','farmhouse','market'],clearance_m=1.5)
X=Vector((1,0,0));ZC,R,T=.62,.36,.12
# ---- sandstone wheel: one dressed-block interior stretched over the faces, lower half damp --------------
u0,v0,du,dv=kit.rng.choice(STONES);n=72
ring=[(R*math.cos(math.tau*k/n),R*math.sin(math.tau*k/n)) for k in range(n)]
uvc=lambda y,z:(u0+du*(.5+y/(2*R)),v0+dv*(.5+z/(2*R)))
for k in range(n):
    (ya,za),(yb,zb)=ring[k],ring[(k+1)%n];wet=(.5,0) if za+zb<-.2 else (0,0)
    for sg in (1,-1):
        pts=[(sg*T/2,0,ZC)+Vector((0,0,0)) if False else Vector((sg*T/2,0,ZC)),Vector((sg*T/2,ya,ZC+za)),Vector((sg*T/2,yb,ZC+zb))]
        uv=[uvc(0,0),uvc(ya,za),uvc(yb,zb)]
        if sg<0:pts.reverse();uv.reverse()
        G['stone'].poly(pts,M['stone'],uv,wet)
    G['stone'].poly([Vector((-T/2,ya,ZC+za)),Vector((-T/2,yb,ZC+zb)),Vector((T/2,yb,ZC+zb)),Vector((T/2,ya,ZC+za))],M['stone'],
        [(u0+du*.2,v0+dv*(k%8)/8),(u0+du*.2,v0+dv*((k%8)+1)/8),(u0+du*.8,v0+dv*((k%8)+1)/8),(u0+du*.8,v0+dv*(k%8)/8)],wet,smooth=True)
# ---- axle, flanges, crank -------------------------------------------------------
G['iron'].beam((-.34,0,ZC),(.40,0,ZC),.036,.036,M['iron'],grain=False)
for sg in (-1,1):
    G['wood'].box((sg*.075,0,ZC),(.025,.17,.17),M['timber'],.004,band=band(),grain=True)
    G['iron'].box((sg*.235,0,ZC),(.012,.11,.11),M['iron'],.003)
ang=math.radians(35);r_=Vector((0,math.cos(ang),math.sin(ang)));t_=Vector((0,-math.sin(ang),math.cos(ang)));E=Vector((0,0,ZC))
A_=E+X*.385;B_=A_+r_*.22;C_=B_+X*.075   # bent crank: collar, radial arm, offset, handle along the axle
G['iron'].box(E+X*.385,(.03,.07,.07),M['iron'],.006)
G['iron'].beam(A_,B_,.022,.034,M['iron'],grain=False)
G['iron'].beam(B_-r_*.012,B_+X*.095+r_*.012,.026,.026,M['iron'],grain=False)
H_=B_+X*.155
G['wood'].prism([H_+r_*.028*math.cos(math.tau*k/10)+t_*.028*math.sin(math.tau*k/10) for k in range(10)],X,.12,M['timber'],swap=True)
# ---- oak frame: two splayed A-frames, ties ------------------------------------------
ly=lambda z:.40-.34*z/.74
for sg in (-1,1):
    x=sg*.19
    for s in (-1,1):G['wood'].beam((x,s*.40,0),(x,s*.06,.74),.06,.07,M['timber'],band=band())
    G['wood'].box((x,0,ZC),(.06,.2,.17),M['timber'],.005,band=band(),grain=True)
    G['wood'].beam((x,-ly(.3)-.03,.3),(x,ly(.3)+.03,.3),.05,.04,M['timber'],band=band())
for s in (-1,1):G['wood'].beam((-.19,s*ly(.1),.1),(.19,s*ly(.1),.1),.045,.045,M['timber'],band=band())
# ---- water trough ---------------------------------------------------------------
G['trough'].box((0,0,.015),(.9,.4,.03),M['boards'],.003,grain=True,band=band())
for s in (-1,1):
    G['trough'].box((0,s*.185,.185),(.9,.03,.31),M['boards'],.003,grain=True,band=band())
    G['trough'].box((s*.435,0,.185),(.03,.34,.31),M['boards'],.003,grain=True,band=band(),swap=True)
    for x in (-.32,.32):G['iron'].box((x,s*.2,.18),(.04,.012,.30),M['iron'],.002)
G['water'].poly([Vector((-.42,-.17,.29)),Vector((.42,-.17,.29)),Vector((.42,.17,.29)),Vector((-.42,.17,.29))],M['water'],[(0,0),(1,0),(1,1),(0,1)])
# ---- export, preview scene, manifest ---------------------------------------------
import math as _m
from mathutils import Matrix
objs=[g.object() for g in G.values()]+EXTRA
import bmesh,hashlib
for k,o in zip(list(G),objs):
    if k in globals().get('SMOOTH',[]):
        me=o.data;bm=bmesh.new();bm.from_mesh(me);bmesh.ops.remove_doubles(bm,verts=bm.verts,dist=1e-5)
        for e in bm.edges:
            if len(e.link_faces)==2 and e.calc_face_angle(0)>.8:e.smooth=False
        bm.to_mesh(me);bm.free()
        for p in me.polygons:p.use_smooth=True
for o in objs:o.data.transform(Matrix.Scale(globals().get('SCALE',1),4))
def bounds():
    pts=[o.matrix_world@Vector(c) for o in objs for c in o.bound_box];return [min(p[k] for p in pts) for k in range(3)],[max(p[k] for p in pts) for k in range(3)]
lo,hi=bounds()
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2])))
lo,hi=bounds();size=[h-l for l,h in zip(lo,hi)]
SLOT=tuple(_m.ceil(round(s*10,4))/10 for s in size)
assert all(s<=t+1e-6 for s,t in zip(size,SLOT)),f'exceeds slot {SLOT}: {size}'
assert all(a<=s<=b for s,(a,b) in zip(size,EXPECT)),f'size {size} outside plausible real-world range {EXPECT}'
OUT=HERE/f'Silverbrook{NAME}'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(_m.radians(52),0,_m.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[_m.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
R_=max(size)*1.0;cz=size[2]/2
cam('Beauty',(-R_*1.15,-R_*1.5,cz+R_*.65),(70,0,-37),lens=50);cam('Front',(0,-20,cz),(90,0,0),R_*1.25);cam('Side',(-20,0,cz),(90,0,-90),R_*1.25);cam('Rear',(0,20,cz),(90,0,180),R_*1.25)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
PLACEMENT['footprint_m']=list(SLOT[:2]);tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=BUDGET,f'{tris} triangles'
manifest=dict(asset=f'Silverbrook{NAME}',reference=dict(file=f'{NAME}-reference.png',sha256=hashlib.sha256((HERE/f'{NAME}-reference.png').read_bytes()).hexdigest(),source='docs/Game-Design/concept-art/Silverbrook/Street Props.png (cropped)'),
    blender=bpy.app.version_string,generator=f'build_{NAME.lower()}.py',method='procedural original geometry',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in size],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans and models)',unreal_mesh=f'SM_Silverbrook{NAME}',parts=[],slot_m=list(SLOT),
    placement=PLACEMENT,outputs=[f'Silverbrook{NAME}.blend',f'Silverbrook{NAME}.glb',f'Silverbrook{NAME}.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('BUILT',NAME,manifest['bounds_m'],tris)
