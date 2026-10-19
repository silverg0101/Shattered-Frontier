"""Silverbrook Market Stall from docs/Game-Design/concept-art/Silverbrook/Market Stall.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_marketstall.py
Original procedural geometry interpreting the elevation (4 m wide, 4 m to the post finials):
four oak posts, a striped canvas shed awning lashed to rails with jute rope, a ragged
valance, a plank counter with a red counter cloth, a bracket lantern and a stag banner.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,lantern
import bpy
scene=kit.reset(4401)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Market Stall.png'
asset=bpy.data.collections.new('SilverbrookMarketStall');scene.collection.children.link(asset)

M=dict(
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    red=kit.scan('SB_MarketStall_AwningRed','velour_velvet',dark=(.45,.28,.24),normal=.7,damp=False,aged=.4),
    cream=kit.scan('SB_MarketStall_AwningCream','hessian_380',dark=(.50,.40,.28),normal=.7,damp=False,aged=.12),
    rope=kit.scan('SB_MarketStall_Rope','hessian_380',dark=(.45,.36,.22),normal=1.0,damp=False,aged=.35),
    banner=kit.scan('SB_MarketStall_Banner','denim_fabric_03',dark=(.10,.12,.28),damp=False,aged=.5),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    gilt=kit.solid('SB_MarketStall_Gilt',(.80,.55,.18),.35,1.0))
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
G=dict(timber=Geo('SM_SBMarketStall_Timber',asset),cloth=Geo('SM_SBMarketStall_Cloth',asset),detail=Geo('SM_SBMarketStall_Details',asset))
# ---- measured layout (metres) -------------------------------------------------
SLOT=(4,4,4)                   # concept README 'Fits'
PX,PS=1.4,.16                  # post centre x, post section
YF,YR=-.75,.8                  # front / rear post lines
ZT,ZF=3.5,2.62                 # top rails (rear + sides), front awning rail
CX=1.31                        # awning half-width (clear of the posts)
CT,CY0,CY1=.9,-.82,-.2         # counter top, front and back edge
RAIL=.09
def post(x,y,top,band_=None):G['timber'].box((x,y,top/2),(PS,PS,top),M['timber'],.014,grain=True,band=band_ or band())
def rail(a,b,s=RAIL):G['timber'].beam(a,b,s,s,M['timber'],band=band())
def iron_band(x,y,z,h=.1):G['detail'].box((x,y,z),(PS+.02,PS+.02,h),M['iron'],.004)
def rope(x,y,z,turns=4):
    for k in range(turns):G['detail'].vessel((x,y,z+(k-turns/2)*.028),[(.113,0),(.122,.014),(.113,.028)],M['rope'],12)
def spire(geo,x,y,rings,tip,mat):
    """Square-section finial: rings [(z, half)] bottom-up, then a point at z=tip."""
    sq=lambda z,h:[(x-h,y-h,z),(x+h,y-h,z),(x+h,y+h,z),(x-h,y+h,z)]
    geo.poly(list(reversed(sq(*rings[0]))),mat)
    for lo,hi in zip(rings,rings[1:]):
        a,b=sq(*lo),sq(*hi)
        for i in range(4):geo.poly([a[i],a[(i+1)%4],b[(i+1)%4],b[i]],mat)
    top=sq(*rings[-1])
    for i in range(4):geo.poly([top[i],top[(i+1)%4],(x,y,tip)],mat)
# ---- posts, rails, finials ----------------------------------------------------------
FT=3.72
for s in (-1,1):
    post(s*PX,YF,FT);post(s*PX,YR,ZT+.06)
    G['timber'].box((s*PX,YF,FT+.03),(.2,.2,.06),M['timber'],.01,grain=True,band=band())
    spire(G['timber'],s*PX,YF,[(FT+.06,.035),(FT+.1,.035),(FT+.17,.075),(FT+.2,.075)],4.0,M['timber'])
    G['timber'].box((s*PX,YR,ZT+.08),(.19,.19,.04),M['timber'],.01,grain=True,band=band())
    rail((s*PX,YF-.1,ZT),(s*PX,YR+.1,ZT))                               # side rail
    for y in (YF,YR):
        iron_band(s*PX,y,.06,.12);iron_band(s*PX,y,ZT-.11,.08);rope(s*PX,y,ZT)
    iron_band(s*PX,YF,FT-.08,.06);rope(s*PX,YF,ZF);iron_band(s*PX,YF,CT-.07,.1)
rail((-PX-.1,YR,ZT),(PX+.1,YR,ZT));rail((-PX,YF,ZF),(PX,YF,ZF))
# ---- awning: five stripes of canvas over the rails, ragged valance --------------------
TH=.006;R=RAIL/2+TH
path=[(YR+R,ZT-.08),(YR+R*.7,ZT+R*.7),(YR,ZT+R)]               # tucked over the rear rail
for k in range(1,12):
    t=k/12;path.append((YR+(YF-YR)*t,ZT+R+(ZF-ZT)*t-.11*math.sin(math.pi*t)))
path+=[(YF,ZF+R),(YF-R*.7,ZF+R*.7),(YF-R,ZF)]                  # over the front rail
VAL=len(path)-1
for k in range(1,9):path.append((YF-R-.006*k,ZF-.062*k))           # valance, hem added per column
def sheet(rows,xs,mats,grime=lambda x,v:0):
    """Two-sided cloth from a vertex grid rows[i][j] (i across, j down the drape); mats[i] per column.
    Both faces are offset along shared vertex normals, so they weld and shade smooth."""
    I,J=len(rows),len(rows[0]);arc=[0]
    for a,b in zip(rows[0],rows[0][1:]):arc.append(arc[-1]+(b-a).length)
    def nrm(i,j):return (rows[min(i+1,I-1)][j]-rows[max(i-1,0)][j]).cross(rows[i][min(j+1,J-1)]-rows[i][max(j-1,0)]).normalized()
    N=[[nrm(i,j) for j in range(J)] for i in range(I)]
    for i in range(I-1):
        mat=mats[i];sc=kit.SCALE[mat.name]
        for j in range(J-1):
            ij=[(i,j),(i+1,j),(i+1,j+1),(i,j+1)];uv=[(xs[a]/sc,arc[b]/sc) for a,b in ij];w=(grime(xs[i],arc[j]),0)
            G['cloth'].poly([rows[a][b]-N[a][b]*TH/2 for a,b in reversed(ij)],mat,list(reversed(uv)),w)   # outer face (up / front)
            G['cloth'].poly([rows[a][b]+N[a][b]*TH/2 for a,b in ij],mat,uv,w)                            # underside
STRIPES=['red','cream','red','cream','red'];NS=16;SW=2*CX/len(STRIPES)
xs=[-CX+2*CX*i/(NS*len(STRIPES)) for i in range(NS*len(STRIPES)+1)];rows=[]
for i,x in enumerate(xs):
    fold=.022*math.sin(x*21+.7);local=(i%NS)/NS;ring=[]
    for j,(y,z) in enumerate(path):
        f=max(0,j-VAL)/(len(path)-1-VAL);t=min(1,max(0,(j-3)/11))   # t: 0..1 between the rails; each panel sags between seams
        ring.append(Vector((x,y-fold*f,z-.035*math.sin(math.pi*local)*math.sin(math.pi*t))))
    y,z=path[-1];ring.append(Vector((x,y-.012-fold,ZF-.5-kit.rng.uniform(0,.1)-(.08*kit.rng.random() if kit.rng.random()<.35 else 0))));rows.append(ring)
def grime(x,v):return .12+.12*math.sin(x*2.3+v*3.1)**2+(.12 if v>2.0 else 0)
sheet(rows,xs,[M[STRIPES[i//NS]] for i in range(len(xs)-1)],grime)
# side drops hang from both sloping edges, clear of the rails
def edge_z(y):t=(YR-y)/(YR-YF);return ZT+R+(ZF-ZT)*t-.11*math.sin(math.pi*t)
for sx in (-1,1):
    ys=[YR-.06+(YF-YR+.12)*k/14 for k in range(15)][::sx]            # left: rear to front; right: front to rear
    rows=[[Vector((sx*(CX-.003)-sx*.012*d*math.sin(y*19),y,edge_z(y)-.004-d*(.3+(kit.rng.uniform(0,.08) if d==1 else 0)))) for d in (0,.35,.7,1)] for y in ys]
    sheet(rows,[-sx*y for y in ys],[M['red']]*14)
# ---- counter: plank top on a braced frame, lower shelf ---------------------------------
CW=PX-PS/2+.02                 # counter planks run 2 cm into the posts
for k in range(4):                                             # top planks, front to back
    w=(CY1-CY0)/4;y=CY0+w*(k+.5);G['timber'].box((0,y,CT-.025),(2*CW,w-.006,.05),M['timber'],.008,grain=True,band=band())
for x in (-CW+.04,0,CW-.04):G['timber'].box((x,(CY0+CY1)/2,CT-.085),(.08,CY1-CY0-.04,.07),M['timber'],.008,grain=True,band=band())
G['timber'].box((0,CY0+.03,CT-.13),(2*CW,.04,.16),M['timber'],.008,grain=True,band=band())       # apron
for x in (-.55,.55):G['timber'].box((x,CY0+.08,(CT-.05)/2),(.12,.12,CT-.05),M['timber'],.01,grain=True,band=band())
for x in (-CW+.05,-.55,.55,CW-.05):G['timber'].box((x,CY1-.05,(CT-.05)/2),(.09,.09,CT-.05),M['timber'],.01,grain=True,band=band())
for k in range(3):G['timber'].box((0,CY0+.12+k*.17,.16),(2*CW,.16,.04),M['timber'],.006,grain=True,band=band())    # shelf
for y in (CY0+.07,CY1-.05):rail((-CW,y,.11),(CW,y,.11),.07)
for s in (-1,1):                                               # X braces in the end bays
    a,b=s*CW,s*.62;rail((a,CY0+.06,.2),(b,CY0+.06,CT-.22),.06);rail((b,CY0+.035,.2),(a,CY0+.035,CT-.22),.06)
# counter cloth over the front middle
cc=[(CY0+.32,CT+TH),(CY0,CT+TH),(CY0-.012,CT-.02),(CY0-.014,CT-.15),(CY0-.016,CT-.3)]
cx=[-.95+1.9*i/16 for i in range(17)]
sheet([[Vector((x,y,z)) for y,z in cc]+[Vector((x,CY0-.018,CT-.38-kit.rng.uniform(0,.1)))] for x in cx],cx,[M['red']]*16)
# ---- lantern bracket (left) and stag banner on a bracket arm (right) ---------------------
LZ=2.45
G['timber'].beam((-PX-PS/2+.02,YF,LZ+.08),(-PX-.36,YF,LZ+.08),.07,.07,M['timber'],band=band())
lantern(G,M,(-PX-.32,YF,LZ))
AZ,AX=3.25,1.98
G['timber'].beam((PX+PS/2-.02,YF,AZ),(AX,YF,AZ),.08,.08,M['timber'],band=band())
G['timber'].beam((PX+PS/2-.02,YF,AZ-.32),(PX+.3,YF,AZ-.03),.06,.06,M['timber'],band=band())
rope(PX,YF,AZ,3)
BX=1.76;bz=[(BX-.2,2.96),(BX+.2,2.96),(BX+.2,2.3),(BX,2.02),(BX-.2,2.3)]
G['detail'].prism([(x,YF,z) for x,z in bz],(0,1,0),.02,M['banner'])
G['detail'].beam((BX-.23,YF,2.975),(BX+.21,YF,2.975),.02,.02,M['iron'],grain=False)
for x in (BX-.15,BX+.15):G['detail'].beam((x,YF,AZ-.04),(x,YF,2.985),.012,.012,M['iron'],grain=False)
def stag(geo,origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R_,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R_*(a*s)+U*(z*s)
    geo.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.012,M['gilt'])
    for sx in (-1,1):
        geo.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.012,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.012,.045*s,M['gilt'],grain=False)
for n in (-1,1):
    yb=YF+n*.01
    for a,b in zip(bz,bz[1:]+bz[:1]):G['detail'].beam(Vector((a[0],yb,a[1]))+Vector((0,n*.006,0)),Vector((b[0],yb,b[1]))+Vector((0,n*.006,0)),.012,.022,M['gilt'],grain=False)
    stag(G['detail'],(BX,yb,2.52),(-n,0,0),(0,0,1),(0,n,0),.4)
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
# Cloth: weld shared stripe seams and shade smooth so folds do not read as facets.
import bmesh
cl=next(o for o in objs if o.name=='SM_SBMarketStall_Cloth');bm=bmesh.new();bm.from_mesh(cl.data);bmesh.ops.remove_doubles(bm,verts=bm.verts,dist=1e-5);bm.to_mesh(cl.data);bm.free()
for p in cl.data.polygons:p.use_smooth=True
from mathutils import Matrix
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2])))
assert abs(lo[2])<1e-3,f'ground contact {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookMarketStall'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-4.9,-6.6,2.3),(84,0,-37),lens=36);cam('Front',(0,-30,2.05),(90,0,0),4.5);cam('Side',(-30,0,2.05),(90,0,-90),4.5);cam('Rear',(0,30,2.05),(90,0,180),4.5)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'MarketStall-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=40000,f'small-prop budget: {tris} triangles'
manifest=dict(asset='SilverbrookMarketStall',reference=dict(file='MarketStall-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_marketstall.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookMarketStall',slot_m=list(SLOT),outputs=['SilverbrookMarketStall.blend','SilverbrookMarketStall.glb','SilverbrookMarketStall.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('MARKETSTALL_BUILT',manifest['bounds_m'],tris)
