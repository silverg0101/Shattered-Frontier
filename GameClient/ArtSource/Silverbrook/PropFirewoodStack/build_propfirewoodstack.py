"""Silverbrook Firewood Stack from docs/Game-Design/concept-art/Silverbrook/Street Props.png (FIREWOOD STACK).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propfirewoodstack.py
Two courses of split oak firewood (round, half and quarter billets with oak bark and pale split faces,
cut ends facing out) stacked hex-style on two oak bearers between four oak end posts, 2 m wide and 1.6 m high, on a dressed-stone footing inside an oak post frame under a slate gable roof (ridge front to back, gable end showing the log ends). Matches the Street Props concept sheet.
"""
import sys,math,json
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,tile_plane,deck
import bpy
PKG='PropFirewoodStack'
scene=kit.reset(5201)
asset=bpy.data.collections.new('Silverbrook'+PKG);scene.collection.children.link(asset)
M=dict(timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
       slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
       bark=kit.scan('SB_PropFirewoodStack_Bark','bark_brown_02',dark=(.45,.40,.35),damp=False),
       split=kit.scan('SB_PropFirewoodStack_SplitOak','oak_wood_planks',dark=(.62,.57,.50),damp=False,aged=.3),
       end=kit.scan('SB_PropFirewoodStack_EndGrain','sb_endgrain',dark=(.62,.58,.52),damp=False,aged=.35))
BANDS=kit.bands('weathered_brown_planks');SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
def band():return kit.rng.choice(BANDS)
M['stone']=kit.scan('SB_Ashlar','stone_wall_04')
G=dict(stone=Geo('SM_SBPropFirewoodStack_Footing',asset),logs=Geo('SM_SBPropFirewoodStack_Logs',asset),timber=Geo('SM_SBPropFirewoodStack_Timber',asset),roof=Geo('SM_SBPropFirewoodStack_Cover',asset))
SLOT=(2.0,1.1,1.6)
rnd=kit.rng

def stick(geo,cx,cz,y0,y1,outline,spin):
    """Prism along +Y through (cx,cz); outline = [(x,z,kind)] of a convex section, kind ('bark'|'split') = edge to the next point.
    Side faces use that material, the saw-cut ends use the split wood. Winding is made outward from the centroid."""
    c,s=math.cos(spin),math.sin(spin);pts=[(cx+x*c-z*s,cz+x*s+z*c,k) for x,z,k in outline];n=len(pts)
    A=[Vector((x,y0,z)) for x,z,_ in pts];B=[Vector((x,y1,z)) for x,z,_ in pts];cen=(sum(A,Vector())+sum(B,Vector()))/(2*n)
    off=(rnd.random(),rnd.random());cum=0;faces=[]
    for i in range(n):
        j=(i+1)%n;m=M[pts[i][2]];sc=kit.SCALE[m.name];d=math.hypot(pts[j][0]-pts[i][0],pts[j][1]-pts[i][1])
        faces.append(([A[i],A[j],B[j],B[i]],m,[((cum+off[0])/sc,off[1]+y0/sc),((cum+d+off[0])/sc,off[1]+y0/sc),((cum+d+off[0])/sc,off[1]+y1/sc),((cum+off[0])/sc,off[1]+y1/sc)]))
        cum+=d
    sc=kit.SCALE[M['end'].name];j=(rnd.uniform(-.02,.02),rnd.uniform(-.02,.02))   # end grain: rings centred on the log's own pith
    for ring in (A,B):faces.append((list(ring),M['end'],[((p.x-cx)/sc+.5+j[0],(p.z-cz)/sc+.5+j[1]) for p in ring]))
    vol=0
    for f,m,uv in faces:
        nrm=sum((a.cross(f[(i+1)%len(f)]) for i,a in enumerate(f)),Vector())
        if nrm.dot(sum(f,Vector())/len(f)-cen)<0:f.reverse();uv.reverse()
        vol+=sum((f[0]-cen).dot((f[i]-cen).cross(f[i+1]-cen)) for i in range(1,len(f)-1))
        geo.poly(f,m,uv,w=(rnd.uniform(.0,.25) if m is M['bark'] else rnd.uniform(0,.15),0))
    assert vol>0,'inside-out stick'
def billet(r):
    kind=rnd.choice(['round','round','half','half','quarter']);a0=rnd.uniform(0,math.tau)
    rad=lambda:r*rnd.uniform(.94,1.06);P=lambda a,rr:(rr*math.cos(a),rr*math.sin(a))
    if kind=='round':return [(*P(a0+math.tau*i/12,rad()),'bark') for i in range(12)]
    if kind=='half':
        arc=[(*P(a0+math.pi*i/6,rad()),'bark') for i in range(7)];arc[-1]=(*arc[-1][:2],'split');return arc
    arc=[(*P(a0+math.pi/2*i/4,rad()),'bark') for i in range(5)];return [(0,0,'split')]+arc[:-1]+[(*arc[-1][:2],'split')]
PX,PYS=.88,.36             # post x and y (four posts)
PS=.12                     # post section
FT=.2                      # stone footing height
EZ=1.12                    # roof surface at the eaves (x=+-1.0)
HW=.94
RZ=1.56                    # roof surface at the ridge
def zr(x):return RZ-(RZ-EZ)*abs(x)/HW   # roof top surface
for yc,y0,y1,dx,dz in ((-.2,-.4,-.02,0,0),(.2,.02,.4,.0725,.063)):  # two courses; the back one is offset so there is no sight-line through
    k=0
    while True:
        z=FT+.085+(k+dz/.126)*.126
        if z>1.04:break
        shift=(.0725 if k%2 else 0)+dx
        for i in range(12):
            x=-.80+i*.145+shift+rnd.uniform(-.004,.004);r=rnd.uniform(.060,.080)
            if abs(x)+r>.82:continue
            stick(G['logs'],x,z+rnd.uniform(-.004,.004),y0+rnd.uniform(-.03,.02),y1+rnd.uniform(-.02,.03),billet(r),rnd.uniform(0,math.tau))
        k+=1
# stone footing: dressed blocks under the whole frame
bx=[-.98+i*.327 for i in range(7)]
for a,b in zip(bx,bx[1:]):
    for yy in (-.2,.2):G['stone'].box(((a+b)/2,yy,FT/2),(b-a-.012,.43,FT-rnd.uniform(0,.012)),M['stone'],.014,w=(0,0))
for sx in (-PX,PX):
    for sy in (-PYS,PYS):G['timber'].box((sx,sy,(FT+1.12)/2),(PS,PS,1.12-FT),M['timber'],.01,grain=True,band=band())
for sy in (-PYS,PYS):
    G['timber'].box((0,sy,1.12),(2*PX+PS+.1,.1,.1),M['timber'],.01,grain=True,band=band())                          # tie beam
    for s in (-1,1):G['timber'].beam((s*(PX+.04),sy,1.15),(0,sy,zr(0)-.08),.07,.09,M['timber'],band=band())          # rafters of the gable truss
    G['timber'].beam((0,sy,1.12),(0,sy,zr(0)-.08),.08,.08,M['timber'],band=band())                                   # king post
    for s in (-1,1):G['timber'].beam((s*PX,sy,1.0),(s*.45,sy,1.12),.06,.07,M['timber'],band=band())                  # braces
for sx in (-PX,PX):G['timber'].beam((sx,-PYS,1.02),(sx,PYS,1.02),.08,.1,M['timber'],band=band())                     # side plate
G['timber'].beam((0,-.55,zr(0)-.07),(0,.55,zr(0)-.07),.1,.1,M['timber'],band=band())                                    # ridge beam
L=math.hypot(HW,RZ-EZ);ro=(0,0,-.045)
for s in (-1,1):
    u=Vector((-s*HW,0,RZ-EZ)).normalized();e=(0,s,0);O=(s*HW,-s*.52,EZ)
    tile_plane(G['roof'],O,e,u,L,lambda t:0,lambda t:1.04,M['slate'],SLATES,[],lichen=lambda p:rnd.uniform(.3,1) if rnd.random()<.35 else 0,tone=(.3,.7))
    deck(G['roof'],O,e,u,L,lambda t:0,lambda t:1.04,M['timber'],[])
    G['timber'].beam((s*HW,-.53,EZ-.06),(0,-.53,RZ-.04),.04,.16,M['timber'],band=band())                              # bargeboard
    G['timber'].beam((s*HW,.53,EZ-.06),(0,.53,RZ-.04),.04,.16,M['timber'],band=band())
G["roof"].beam((0,-.55,RZ+.02),(0,.55,RZ+.02),.2,.04,M["slate"],grain=False,rect=SLATES[0])  # ridge cap
PLACEMENT=dict(footprint_m=[2.0,1.0],front='-Y',ground_contact=True,near=['cottage','farmhouse','blacksmith','stables','mill','workshop'],clearance_m=.8)
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
