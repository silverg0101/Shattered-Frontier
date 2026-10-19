"""Original measured workshop from family concept B. Metres, front -Y."""
import sys,json,math,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT.parent/'CottageConceptV1'))
from authoring_geometry import *
ROOT=Path(__file__).resolve().parent
asset.name='Greenlands Carpenter Workshop — original mesh'
body=Geometry('SM_GreenlandsWorkshop_Structure');tiles=Geometry('SM_GreenlandsWorkshop_Roof');joinery=Geometry('SM_GreenlandsWorkshop_Joinery');props=Geometry('SM_GreenlandsWorkshop_Furnishings');approach=Geometry('SM_GreenlandsWorkshop_Approach')
RIDGE,EAVE,HALF=5.65,2.9,3.5;SLOPE=(RIDGE-EAVE)/HALF;WALL=RIDGE-3*SLOPE-.06
def roof_z(y):return RIDGE-SLOPE*abs(y)
def wall(axis,offset,lo,hi,holes):
    us=sorted(set([lo,hi]+[v for h in holes for v in [h[0],h[2]]]));zs=sorted(set([.16,.7,WALL]+[v for h in holes for v in [h[1],h[3]]]))
    for a,b in zip(us,us[1:]):
        for za,zb in zip(zs,zs[1:]):
            if any(x<(a+b)/2<xx and z<(za+zb)/2<zz for x,z,xx,zz in holes):continue
            if axis=='y':body.box(((a+b)/2,offset,(za+zb)/2),(b-a,.18,zb-za),plaster,0)
            else:body.box((offset,(a+b)/2,(za+zb)/2),(.18,b-a,zb-za),plaster,0)
wall('y',-3,-4,4,[(-.2,.16,2.0,2.5),(-3.25,1.05,-1.95,2.25)])
wall('y',3,-4,4,[(-2.8,1.05,-1.5,2.25),(1.5,1.05,2.8,2.25)])
for x in [-4,4]:
    wall('x',x,-3,3,[(-.65,1.05,.65,2.25)])
    body.prism([(x,-3,WALL),(x,3,WALL),(x,0,RIDGE-.08)],(1,0,0),.18,plaster)
body.box((0,0,.08),(7.9,5.9,.16),stone,.012)
# Structural panel framing, sill rails and actual bracing.
for y in [-3.12,3.12]:
    for x in [-3.95,-1.5,2.3,3.95]:body.beam((x,y,.5),(x,y,WALL),.17,.19)
    for z in [2.58,WALL]:body.beam((-4.1,y,z),(4.1,y,z),.18,.20)
    if y>0:body.beam((-4,y,.65),(4,y,.65),.14)
    for x,sign in [(-3.9,1),(3.9,-1)]:body.beam((x,y,2.65),(x+sign*.65,y,WALL-.08),.12)
for x in [-4.12,4.12]:
    for z in [.65,2.58,WALL]:body.beam((x,-3,z),(x,3,z),.17)
    for sign in [-1,1]:body.beam((x,sign*3,WALL),(x,0,RIDGE),.19)
    body.beam((x,0,WALL),(x,0,RIDGE-.12),.15)
    for y in [-1.25,1.25]:body.beam((x,y,.5),(x,y,WALL),.15)
for side in range(4):
    length=8.12 if side<2 else 6.12
    for course in range(3):
        cursor=-length/2
        while cursor<length/2-.1:
            width=min(rng.uniform(.38,.68),length/2-cursor);mid=cursor+width/2;cursor+=width
            if side==0 and -.23-width/2<mid<2.03+width/2:continue
            if side<2:body.box((mid,(-1 if side==0 else 1)*3.06,.13+course*.19),(width-.014,.26,.18),stone,.024)
            else:body.box(((-1 if side==2 else 1)*4.06,mid,.13+course*.19),(.26,width-.014,.18),stone,.024)

def roof_plane(xa,xb,ya,yb,za,zb):
    # Roof courses run up the slope in local Y, with a small overlap pitch.
    tangent=Vector((0,yb-ya,zb-za)).normalized();normal=Vector((1,0,0)).cross(tangent)
    if normal.z<0:normal=-normal;xx=Vector((-1,0,0))
    else:xx=Vector((1,0,0))
    rot=Matrix(((xx.x,tangent.x,normal.x),(xx.y,tangent.y,normal.y),(xx.z,tangent.z,normal.z)))
    points=[(xa,ya,za),(xb,ya,za),(xb,yb,zb),(xa,yb,zb)]
    if normal.z>0 and (Vector(points[1])-Vector(points[0])).cross(Vector(points[2])-Vector(points[0])).z<0:points.reverse()
    tiles.prism(points,normal,.045,roof)
    length=math.hypot(yb-ya,zb-za)
    for row in range(math.ceil(length/.18)):
        t=min(1,(row+.5)*.18/length);y=ya+(yb-ya)*t;z=za+(zb-za)*t
        for col in range(math.ceil((xb-xa)/.28)):
            x=xa+(col+.5)*.28+(row%2)*.14
            if x>xb:continue
            tiles.box(Vector((x,y,z))+normal*.04,(.274,.32,.026),roof,.008,rot@Matrix.Rotation(-.085,3,'X'),tile_patch(.04))
for sign in [-1,1]:
    roof_plane(-4.5,4.5,sign*HALF,0,EAVE,RIDGE)
    tiles.beam((-4.6,sign*HALF,EAVE-.10),(4.6,sign*HALF,EAVE-.10),.18,.21)
    for x in [-4.45,4.45]:tiles.beam((x,sign*3.6,EAVE-.08),(x,0,RIDGE+.08),.18,.2)
    for i in range(16):
        x=-4.4+i*.59;tiles.beam((x,sign*3.6,EAVE-.16),(x,0,RIDGE-.14),.1,.14)
    for i in range(24):
        y=sign*(i+.5)*HALF/24;z=roof_z(y)-.075
        tiles.box((0,y,z),(9,.185,.035),wood,.004,Matrix.Rotation(-sign*math.atan(SLOPE),3,'X'))
tiles.beam((-4.6,0,RIDGE+.06),(4.6,0,RIDGE+.06),.16,.2)
for x in [-2.8,1.3]:
    body.beam((x,-3.0,WALL-.1),(x,3.0,WALL-.1),.19,.23)
    body.beam((x,0,WALL-.1),(x,0,RIDGE-.12),.16)
    for sign in [-1,1]:body.beam((x,sign*2.8,WALL),(x,0,RIDGE-.12),.15,.19)
# Distinct open side canopy, with a lower lean-to roof and knee-braced posts.
for y in [-3.2,0,3.1]:
    body.beam((6.05,y,.16),(6.05,y,2.65),.20,.20)
    body.beam((6.05,y,1.95),(5.5,y,2.77),.13,.16)
    body.beam((6.05,y,1.95),(6.05,y+(.55 if y<1 else -.55),2.58),.13,.16)
body.beam((6.05,-3.4,2.65),(6.05,3.35,2.65),.21,.23)
body.beam((4.12,-3.4,3.5),(4.12,3.35,3.5),.21,.23)
# Reuse the roof-course builder in rotated coordinates for the lean-to.
canopy=Geometry('canopy temporary');old=tiles;tiles=canopy
roof_plane(-3.45,3.4,6.35,3.95,2.55,3.55)
tiles=old
for p in canopy.v:tiles.v.append((p[1],p[0],p[2]))
for face,uv,index in zip(canopy.faces,canopy.uv,canopy.mi):tiles.faces.append(tuple(reversed([i+len(tiles.v)-len(canopy.v) for i in face])));tiles.uv.append(list(reversed(uv)));tiles.mi.append(index)
for y in [-3.4,-2.3,-1.2,-.1,1,2.1,3.3]:body.beam((3.95,y,3.48),(6.4,y,2.48),.12,.18)
for i in range(15):
    x=3.99+(i+.5)*2.35/15;z=3.55-(x-3.95)/2.4-.07
    tiles.box((x,-.025,z),(.168,6.85,.035),wood,.004,Matrix.Rotation(math.atan(1/2.4),3,'Y'))
# Protected plank work floor; clear front aisle remains separate from benches.
for i in range(10):body.box((4.14+i*.215,0,.12),(.207,6.5,.12),wood,.005)

def window(cx,cy,z,yaw=0):
    rot=Matrix.Rotation(math.radians(yaw),3,'Z')
    def point(x,y,zz):return Vector((cx,cy,z))+rot@Vector((x,y,zz))
    for x in [-.70,.70]:joinery.box(point(x,-.07,0),(.12,.22,1.38),wood,.009,rot)
    for zz in [-.65,.65]:joinery.box(point(0,-.07,zz),(1.50,.22,.12),wood,.009,rot)
    joinery.box(point(0,-.16,-.73),(1.62,.42,.13),wood,.014,rot)
    joinery.box(point(0,.04,0),(1.32,.014,1.23),glass,.002,rot)
    for x in [-.22,.22]:joinery.box(point(x,-.015,0),(.04,.06,1.23),wood,.004,rot)
    joinery.box(point(0,-.015,0),(1.32,.06,.045),wood,.004,rot)
    for sign in [-1,1]:
        for j in range(4):joinery.box(point(sign*(.86+j*.14),-.05,0),(.135,.055,1.24),wood,.006,rot)
        for zz in [-.40,.40]:joinery.box(point(sign*1.07,-.09,zz),(.58,.025,.045),iron,.004,rot)
window(-2.6,-3.04,1.65)
for x in [-2.15,2.15]:window(x,3.04,1.65,180)
for sign in [-1,1]:window(sign*4.04,0,1.65,sign*90)
# Broad two-leaf entrance; open door panels articulate individual planks/hardware.
for side in [-1,1]:
    hinge=.9+side*1.10;angle=math.radians(side*103);rot=Matrix.Rotation(angle,3,'Z')
    for j in range(8):
        p=Vector((hinge,-3.1,1.31))+rot@Vector((-side*(.065+j*.135),0,0));joinery.box(p,(.13,.075,2.23),wood,.009,rot)
    for z in [.55,2.05]:joinery.box(Vector((hinge,-3.15,z))+rot@Vector((-side*.54,0,0)),(1.02,.035,.08),iron,.006,rot)
for x in [-.32,2.12]:joinery.beam((x,-3.15,.16),(x,-3.15,2.56),.20)
joinery.beam((-.42,-3.15,2.61),(2.22,-3.15,2.61),.23)

def workbench(x,y,angle=0):
    rot=Matrix.Rotation(math.radians(angle),3,'Z')
    def p(a,b,z):return Vector((x,y,z))+rot@Vector((a,b,0))
    props.box((x,y,1.02),(2.1,.82,.13),wood,.025,rot)
    for a in [-.85,.85]:
        for b in [-.28,.28]:props.beam(p(a,b,.16),p(a,b,.96),.10)
    props.beam(p(-.85,0,.35),p(.85,0,.35),.10)
    # Joiner's vice with timber screw housing and two iron guides.
    props.box(p(.55,-.51,.91),(.46,.15,.25),wood,.015,rot)
    for a in [.40,.70]:props.beam(p(a,-.43,.94),p(a,-.64,.94),.025,material=iron)
    props.beam(p(.55,-.68,.81),p(.55,-.68,1.08),.026)
    props.box(p(-.25,0,1.11),(.62,.12,.065),wood,.012,rot)
    props.box(p(.30,.13,1.115),(.25,.16,.06),iron,.012,rot)
workbench(-2.35,.35,90);workbench(4.95,.6,90)
# Rear rack, stacked irregular boards and visible pegs; no logs imply resources.
for x in [-3.5,-1.4]:
    props.beam((x,2.62,.16),(x,2.62,2.6),.13)
    for z in [.65,1.35,2.05]:props.beam((x,2.62,z),(x,2.05,z),.09)
for layer,z in enumerate([.73,1.43,2.13]):
    for course in range(4):
        for i in range(5):props.box((-2.4+rng.uniform(-.09,.09),2.16+i*.075,z+course*.065),(2.55+rng.uniform(-.2,.2),.068,.058),wood,.008)
for i in range(9):props.box((4.87,2.55,.23+i*.075),(1.55+rng.uniform(-.12,.15),.66,.068),wood,.01)
# A real wall-mounted lantern and its practical light anchor.
joinery.beam((2.45,-3.14,2.35),(2.45,-3.5,2.35),.035,material=iron)
joinery.box((2.45,-3.47,2.17),(.13,.13,.22),warm,.006)
for x in [2.36,2.54]:
    for y in [-3.56,-3.38]:joinery.beam((x,y,2.02),(x,y,2.31),.018,material=iron)
for z in [2.02,2.31]:joinery.box((2.45,-3.47,z),(.24,.24,.04),iron,.008)
joinery.beam((-2.8,.2,WALL-.18),(-2.8,.2,2.81),.018,material=iron)
joinery.box((-2.8,.2,2.65),(.13,.13,.22),warm,.006)
for x in [-2.89,-2.71]:
    for y in [.11,.29]:joinery.beam((x,y,2.5),(x,y,2.79),.018,material=iron)
for z in [2.5,2.79]:joinery.box((-2.8,.2,z),(.24,.24,.04),iron,.008)
# A short, graded earth spur joins the existing path to the offset threshold.
cross=[-1.1,-.85,-.5,0,.5,.85,1.1]
for row in range(9):
    for col in range(len(cross)-1):
        points=[]
        for r,c in [(row,col),(row,col+1),(row+1,col+1),(row+1,col)]:
            t=r/9;u=cross[c];x=.9*t+u;y=-4.6+1.62*t;centre=-.13+.30*t;shoulder=max(0,(abs(u)-.75)/.35);z=centre*(1-shoulder)+(-.13)*shoulder
            points.append((x,y,z))
        approach.poly(points,stone,[(p[0]/2,p[1]/2) for p in points])

objects=[g.object() for g in [body,tiles,joinery,props,approach]]
presentation=bpy.data.collections.new('Presentation — excluded from exports');scene.collection.children.link(presentation)
def move(obj):
    for col in list(obj.users_collection):col.objects.unlink(obj)
    presentation.objects.link(obj)
for label,loc,target,ortho in [('Beauty',(13,-16,9),(1,0,2.5),None),('Front',(1,-22,3),(1,0,3),13),('Side',(23,0,3),(1,0,3),10),('Rear',(1,22,3),(1,0,3),13)]:
    bpy.ops.object.camera_add(location=loc);o=bpy.context.object;o.name=label;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();o.data.lens=45
    if ortho:o.data.type='ORTHO';o.data.ortho_scale=ortho
    move(o)
scene.camera=bpy.data.objects['Beauty'];scene.world=bpy.data.worlds.new('Workshop neutral daylight');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.3
for loc,power,size in [((-5,-8,12),2400,7),((8,-2,8),1500,7),((0,7,10),1800,6)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(Vector((1,0,2))-o.location).to_track_quat('-Z','Y').to_euler();move(o)
bpy.ops.mesh.primitive_plane_add(size=200);ground=bpy.context.object;ground.location.z=-.03;move(ground)
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX'
for image in bpy.data.images:
    if image.source=='FILE':image.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'GreenlandsWorkshop.blend'))
report=[]
for obj in objects:
    bpy.ops.object.select_all(action='DESELECT');copy=obj.copy();copy.data=obj.data.copy();asset.objects.link(copy);copy.select_set(True);bpy.context.view_layer.objects.active=copy
    for v in copy.data.vertices:v.co.y=-v.co.y
    bm=bmesh.new();bm.from_mesh(copy.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(copy.data);bm.free()
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE');bpy.data.objects.remove(copy,do_unlink=True)
    obj.data.calc_loop_triangles();report.append(dict(mesh=obj.name,triangles=len(obj.data.loop_triangles),dimensions_metres=list(obj.dimensions)))
bpy.ops.object.select_all(action='DESELECT')
for obj in objects:obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(ROOT/'GreenlandsWorkshop.glb'),export_format='GLB',use_selection=True)
bpy.ops.export_scene.fbx(filepath=str(ROOT/'GreenlandsWorkshop.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
(ROOT/'workshop-manifest.json').write_text(json.dumps(dict(units='metres',front='-Y',main_shell_metres=[8,6],canopy_extension_metres=2.4,ridge_metres=RIDGE,door_clear_metres=[2.2,2.34],reference_sha256=hashlib.sha256((ROOT/'Greenlands-Building-Family-v1.png').read_bytes()).hexdigest(),materials=MATERIAL_ROWS,meshes=report,status='Prototype; source render, exchange and game review pending'),indent=2))
print('WORKSHOP_AUTHORED',json.dumps(report),flush=True)
