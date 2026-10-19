"""Original herbalist cottage A, 6x8m, front gable and left preparation porch."""
import sys,json,math,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT.parent/'CottageConceptV1'))
from authoring_geometry import *
ROOT=Path(__file__).resolve().parent;asset.name='Greenlands Herbalist — original mesh'
rooms=json.loads((ROOT/'herbalist-rooms.json').read_text());assert rooms['schema_version']==1 and rooms['units']=='metres'
anchors=bpy.data.collections.new('Room anchors — authoring only');scene.collection.children.link(anchors)
occupied=[]
def intersects(a,b):return a[0]<b[2] and a[2]>b[0] and a[1]<b[3] and a[3]>b[1]
for group in rooms['assemblies']:
    room=next(r for r in rooms['rooms'] if r['id']==group['room']);x,y,z=group['anchor'];r=group['footprint_xy'];bounds=[x+r[0],y+r[1],x+r[2],y+r[3]];usable=room['usable_xy']
    assert bounds[0]>=usable[0] and bounds[1]>=usable[1] and bounds[2]<=usable[2] and bounds[3]<=usable[3],group['id']+' outside room'
    assert not any(intersects(bounds,k['xy']) for k in rooms['keep_clear'] if k['room']==group['room']),group['id']+' blocks circulation'
    assert not any(intersects(bounds,b) for rid,b in occupied if rid==group['room']),group['id']+' overlaps another assembly'
    occupied.append((group['room'],bounds));anchor=bpy.data.objects.new('RoomAnchor_'+group['id'],None);anchors.objects.link(anchor);anchor.location=(x,y,z);anchor.empty_display_type='ARROWS';anchor.empty_display_size=.25;anchor['room_id']=group['room'];anchor['occupied_xy_metres']=bounds
body=Geometry('SM_GreenlandsHerbalist_Structure');tiles=Geometry('SM_GreenlandsHerbalist_Roof');joinery=Geometry('SM_GreenlandsHerbalist_Joinery');props=Geometry('SM_GreenlandsHerbalist_Furnishings');approach=Geometry('SM_GreenlandsHerbalist_Approach')
RIDGE,EAVE,HALF=6.8,3.2,3.45;SLOPE=(RIDGE-EAVE)/HALF
def roof_z(x):return RIDGE-SLOPE*abs(x)
WALL=roof_z(3)-.06
def wall(axis,offset,lo,hi,zlo,zhi,holes):
    us=sorted(set([lo,hi]+[v for h in holes for v in [h[0],h[2]]]));zs=sorted(set([zlo,zhi,max(zlo,min(zhi,.7))]+[v for h in holes for v in [h[1],h[3]]]))
    for a,b in zip(us,us[1:]):
        for za,zb in zip(zs,zs[1:]):
            if any(x<(a+b)/2<xx and z<(za+zb)/2<zz for x,z,xx,zz in holes):continue
            if axis=='y':body.box(((a+b)/2,offset,(za+zb)/2),(b-a,.18,zb-za),plaster,0)
            else:body.box((offset,(a+b)/2,(za+zb)/2),(.18,b-a,zb-za),plaster,0)
wall('y',-4,-3,3,.16,WALL,[(-1,.16,1,2.46)]+[(x-.5,.95,x+.5,2.15) for x in [-1.95,1.95]])
wall('y',4,-3,3,.16,WALL,[(x-.5,.95,x+.5,2.15) for x in [-1.4,1.4]])
for x in [-3,3]:wall('x',x,-4,4,.16,WALL,[(y-.5,.95,y+.5,2.15) for y in [-1.8,1.8]])
# Close the roof junction across the wall thickness, following the actual pitch.
for sign in [-1,1]:
    inner,outer=sign*2.89,sign*3.10
    body.prism([(inner,0,WALL-.05),(outer,0,WALL-.05),(outer,0,roof_z(outer)-.015),(inner,0,roof_z(inner)-.015)],(0,1,0),8,plaster)
for y in [-4,4]:
    wall('y',y,-.58,.58,WALL,5.42,[(-.58,4.18,.58,5.42)])
    for sign in [-1,1]:body.prism([(sign*3,y,WALL),(sign*.58,y,WALL),(sign*.58,y,roof_z(.58)),(sign*3,y,roof_z(3))],(0,1,0),.18,plaster)
    body.prism([(-.58,y,5.42),(.58,y,5.42),(.58,y,roof_z(.58)),(0,y,RIDGE),(-.58,y,roof_z(.58))],(0,1,0),.18,plaster)
body.box((0,0,.08),(5.9,7.9,.16),stone,.012)
# Loft aperture and flight are placed where the sloping roof has real headroom.
for centre,size in [((-.95,0,2.68),(3.9,7.8,.14)),((2.5,0,2.68),(.8,7.8,.14)),((1.55,-2.15,2.68),(1.1,3.5,.14)),((1.55,3.3,2.68),(1.1,1.2,.14))]:body.box(centre,size,wood,.01)
for i in range(15):body.box((1.55,-.2+i*.2,.16+(i+1)*2.59/15),(1.0,.215,.075),wood,.009)
for x in [1.0,2.1]:body.beam((x,-.35,.16),(x,2.78,2.75),.12,.17)
for x,end in [(.95,1.8),(2.15,2.6)]:
    for y in [-.4,.3,1.0,end]:body.beam((x,y,2.75),(x,y,3.74),.085)
    for z in [3.20,3.70]:body.beam((x,-.4,z),(x,end,z),.075)
for z in [3.20,3.70]:body.beam((.95,-.4,z),(2.15,-.4,z),.075)
# Exposed structural infill framework and heavy front-gable brackets.
for y in [-4.12,4.12]:
    for x in [-2.95,2.95]:body.beam((x,y,.5),(x,y,WALL),.18,.20)
    for z in [2.65,WALL]:body.beam((-3.12,y,z),(3.12,y,z),.19,.21)
    for sign in [-1,1]:
        body.beam((sign*3,y,WALL),(0,y,RIDGE),.20,.22)
        body.beam((sign*2.8,y,2.75),(sign*2.15,y,WALL-.05),.14,.17)
        body.beam((sign*2.3,y,WALL),(sign*.7,y,5.6),.14,.17)
    body.beam((0,y,5.5),(0,y,RIDGE-.08),.15)
for x in [-3.12,3.12]:
    for y in [-3.95,-.7,3.95]:body.beam((x,y,.5),(x,y,WALL),.17,.20)
    for z in [.65,2.65,WALL]:body.beam((x,-4,z),(x,4,z),.17,.19)
for side in range(4):
    length=6.1 if side<2 else 8.1
    for course in range(3):
        cursor=-length/2
        while cursor<length/2-.1:
            width=min(rng.uniform(.38,.65),length/2-cursor);mid=cursor+width/2;cursor+=width
            if side==0 and abs(mid)<1.04+width/2:continue
            if side<2:body.box((mid,(-1 if side==0 else 1)*4.06,.13+course*.19),(width-.014,.25,.18),stone,.024)
            else:body.box(((-1 if side==2 else 1)*3.06,mid,.13+course*.19),(.25,width-.014,.18),stone,.024)

def roof_section(xa,xb,za,zb,ya,yb):
    # Ridge/courses run along Y; slope is along X. Upper ends lap lower courses.
    tangent=Vector((xb-xa,0,zb-za)).normalized();ridge=Vector((0,1,0));normal=ridge.cross(tangent)
    if normal.z<0:normal=-normal;ridge=-ridge
    rot=Matrix(((ridge.x,tangent.x,normal.x),(ridge.y,tangent.y,normal.y),(ridge.z,tangent.z,normal.z)))
    points=[(xa,ya,za),(xa,yb,za),(xb,yb,zb),(xb,ya,zb)];tiles.prism(points,normal,.045,roof)
    length=math.hypot(xb-xa,zb-za)
    for row in range(math.ceil(length/.18)):
        t=min(1,(row+.5)*.18/length);x=xa+(xb-xa)*t;z=za+(zb-za)*t
        for col in range(math.ceil((yb-ya)/.28)):
            y=ya+(col+.5)*.28+(row%2)*.14
            if y>yb:continue
            tiles.box(Vector((x,y,z))+normal*.04,(.274,.32,.026),roof,.008,rot@Matrix.Rotation(-.085,3,'X'),tile_patch(.05))
    rows=math.ceil(length/.18)
    for i in range(rows):
        t=(i+.5)/rows;centre=Vector((xa+(xb-xa)*t,(ya+yb)/2,za+(zb-za)*t))-normal*.075
        tiles.box(centre,(yb-ya,length/rows-.001,.035),wood,.004,rot)
for sign in [-1,1]:
    roof_section(sign*HALF,0,EAVE,RIDGE,-4.55,4.5)
    tiles.beam((sign*HALF,-4.6,EAVE-.08),(sign*HALF,4.55,EAVE-.08),.18,.21)
    for y in [-4.5,4.45]:tiles.beam((sign*3.55,y,EAVE-.05),(0,y,RIDGE+.08),.18,.22)
    for i in range(16):
        y=-4.4+i*.59;tiles.beam((sign*3.55,y,EAVE-.15),(0,y,RIDGE-.14),.10,.14)
tiles.beam((0,-4.65,RIDGE+.06),(0,4.6,RIDGE+.06),.16,.20)
# Sheltered herb-preparation porch with independent, knee-braced supports.
roof_section(-4.95,-3.20,2.45,3.19375,-3.55,1.1)
for y in [-3.35,1.0]:
    body.beam((-4.75,y,.16),(-4.75,y,2.55),.19,.21)
    body.beam((-4.75,y,1.85),(-4.2,y,2.66),.13,.16)
body.beam((-4.75,-3.5,2.55),(-4.75,1.05,2.55),.20,.22)
for y in [-3.4,-2.4,-1.4,-.4,.9]:body.beam((-4.97,y,2.35),(-3.15,y,3.116),.11,.16)
for i in range(9):body.box((-4.76+i*.207,-1.2,.13),(.20,4.6,.16),wood,.005)

def window(cx,cy,z,yaw=0,box=False):
    rot=Matrix.Rotation(math.radians(yaw),3,'Z')
    def p(x,y,zz):return Vector((cx,cy,z))+rot@Vector((x,y,zz))
    for x in [-.58,.58]:joinery.box(p(x,-.07,0),(.105,.22,1.36),wood,.009,rot)
    for zz in [-.64,.64]:joinery.box(p(0,-.07,zz),(1.26,.22,.105),wood,.009,rot)
    joinery.box(p(0,-.15,-.73),(1.42,.38,.12),wood,.012,rot);joinery.box(p(0,.04,0),(1.10,.014,1.20),glass,.001,rot)
    joinery.box(p(0,-.01,0),(.045,.06,1.20),wood,.004,rot)
    for zz in [-.20,.20]:joinery.box(p(0,-.01,zz),(1.1,.06,.04),wood,.004,rot)
    for sign in [-1,1]:
        for j in range(4):joinery.box(p(sign*(.74+j*.13),-.06,0),(.125,.055,1.22),wood,.006,rot)
        for zz in [-.40,.40]:joinery.box(p(sign*.94,-.10,zz),(.50,.025,.045),iron,.004,rot)
    if box:
        joinery.box(p(0,-.3,-.94),(1.24,.39,.07),wood,.012,rot);joinery.box(p(0,-.48,-.82),(1.24,.05,.29),wood,.01,rot)
        for x in [-.59,.59]:joinery.box(p(x,-.3,-.82),(.05,.39,.29),wood,.01,rot)
        joinery.box(p(0,-.3,-.75),(1.12,.31,.035),soil,.003,rot)
for x in [-1.95,1.95]:window(x,-4.04,1.55,box=True)
window(0,-4.04,4.8,box=True);window(0,4.04,4.8,180)
for x in [-1.4,1.4]:window(x,4.04,1.55,180)
for sign in [-1,1]:
    for y in [-1.8,1.8]:window(sign*3.04,y,1.55,sign*90)
for side in [-1,1]:
    angle=math.radians(side*103);rot=Matrix.Rotation(angle,3,'Z');hinge=side*1.0
    for j in range(7):joinery.box(Vector((hinge,-4.1,1.31))+rot@Vector((-side*(.07+j*.14),0,0)),(.135,.075,2.23),wood,.009,rot)
    for z in [.55,2.05]:joinery.box(Vector((hinge,-4.15,z))+rot@Vector((-side*.49,0,0)),(.92,.035,.08),iron,.006,rot)
for x in [-1.12,1.12]:joinery.beam((x,-4.16,.16),(x,-4.16,2.52),.20)
joinery.beam((-1.23,-4.16,2.56),(1.23,-4.16,2.56),.23)
# Human-scale preparation tables, apothecary shelves and rounded earthenware.
def jar(x,y,z,r=.085,h=.22):props.vessel((x,y,z),[(r*.65,0),(r,.03),(r*1.1,h*.6),(r*.65,h*.85),(r*.7,h),(r*.54,h),(r*.5,h*.86),(r*.95,h*.6),(r*.7,.04),(r*.65,.025)],clay)
for x,y,width,length in [(-1.95,.5,.72,1.5),(-3.38,-1.8,.52,1.25)]:
    props.box((x,y,.98),(width,length,.09),wood,.018)
    for dx in [-width*.35,width*.35]:
        for dy in [-length*.4,length*.4]:props.beam((x+dx,y+dy,.16),(x+dx,y+dy,.94),.075)
    for j in range(3):jar(x-.1+(j%2)*.2,y-.4+j*.3,1.025,.07,.17+j*.035)
for x in [-2.5,.2]:props.beam((x,3.60,.16),(x,3.60,2.38),.11)
for z in [.65,1.25,1.85]:
    props.box((-1.15,3.5,z),(2.88,.42,.065),wood,.012)
    for i in range(8):jar(-2.32+i*.33,3.45,z+.035,.07+rng.random()*.025,.17+rng.random()*.10)
for y in [-.75,.75]:props.beam((-3.30,y,.16),(-3.30,y,2.05),.09)
for z in [.7,1.4]:
    props.box((-3.34,0,z),(.4,1.70,.065),wood,.01)
    for i in range(5):jar(-3.36,-.62+i*.3,z+.035,.075,.22)
# A loft bed and trunk, kept clear of the central landing/headroom envelope.
props.box((-1.25,-.3,3.0),(1.15,2.1,.16),wood,.018)
for x in [-1.72,-.78]:
    for y in [-1.22,.62]:props.beam((x,y,2.75),(x,y,3.25),.085)
props.box((-1.25,-1.3,3.30),(1.16,.08,.4),wood,.015)
for i in range(24):
    for j in range(32):
        xa=-1.9+i*.055;xb=xa+.055;ya=-1.25+j*.06;yb=ya+.06
        def quilt_z(x,y):return 3.11+.016*math.sin(y*17+x*4)+.012*math.sin(x*24)-max(0,abs(x+1.25)-.49)*1.7
        props.poly([(xa,ya,quilt_z(xa,ya)),(xb,ya,quilt_z(xb,ya)),(xb,yb,quilt_z(xb,yb)),(xa,yb,quilt_z(xa,yb))],linen)
props.box((-1.25,-1.0,3.16),(.75,.42,.13),linen,.05)
props.box((-1.6,1.55,3.0),(.95,.55,.48),wood,.024);props.box((-1.6,1.55,3.27),(.99,.58,.08),wood,.024)
# Full-height corner flue avoids the stairs and preparation shelves.
for course in range(32):
    for ix in [-1,1]:
        for iy in [-1,1]:body.box((2.5+ix*.18,3.0+iy*.18,.27+course*.22),(.35,.35,.21),stone,.018)
body.box((2.5,3.0,7.27),(.92,.92,.14),stone,.018)
for x,y,z in [(1.35,-4.45,2.10),(-1.1,1.3,2.25)]:
    joinery.box((x,y,z),(.13,.13,.22),warm,.006)
    for dx in [-.09,.09]:
        for dy in [-.09,.09]:joinery.beam((x+dx,y+dy,z-.15),(x+dx,y+dy,z+.14),.018,material=iron)
    for zz in [z-.15,z+.14]:joinery.box((x,y,zz),(.24,.24,.04),iron,.008)
    if y<0:joinery.beam((x,-4.15,z+.23),(x,y,z+.23),.035,material=iron)
    else:joinery.beam((x,y,z+.14),(x,y,2.61),.018,material=iron)
# Graded spur reaches the old path without moving the region's through-roads.
cross=[-1.05,-.8,-.45,0,.45,.8,1.05]
for row in range(9):
    for col in range(6):
        points=[]
        for r,c in [(row,col),(row,col+1),(row+1,col+1),(row+1,col)]:
            t=r/9;u=cross[c];centre=-.13+.30*t;shoulder=max(0,(abs(u)-.75)/.30);points.append((-.7*(1-t)+u,-5.2+1.21*t,centre*(1-shoulder)-.13*shoulder))
        approach.poly(points,stone,[(p[0]/2,p[1]/2) for p in points])

objects=[g.object() for g in [body,tiles,joinery,props,approach]]
presentation=bpy.data.collections.new('Presentation — excluded from exports');scene.collection.children.link(presentation)
def move(obj):
    for col in list(obj.users_collection):col.objects.unlink(obj)
    presentation.objects.link(obj)
for label,loc,target,ortho in [('Beauty',(-12,-16,9),(0,0,3.1),None),('Front',(1,-22,3),(1,0,3),13),('Side',(-23,0,3),(0,0,3),11),('Rear',(1,22,3),(1,0,3),13)]:
    bpy.ops.object.camera_add(location=loc);o=bpy.context.object;o.name=label;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();o.data.lens=45
    if ortho:o.data.type='ORTHO';o.data.ortho_scale=ortho
    move(o)
scene.camera=bpy.data.objects['Beauty'];scene.world=bpy.data.worlds.new('Herbalist neutral daylight');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.3
for loc,power,size in [((-5,-8,12),2400,7),((8,-2,8),1500,7),((0,7,10),1800,6)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(Vector((1,0,2))-o.location).to_track_quat('-Z','Y').to_euler();move(o)
bpy.ops.mesh.primitive_plane_add(size=200);ground=bpy.context.object;ground.location.z=-.03;move(ground)
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX'
for image in bpy.data.images:
    if image.source=='FILE':image.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'GreenlandsHerbalist.blend'))
report=[]
for obj in objects:
    bpy.ops.object.select_all(action='DESELECT');copy=obj.copy();copy.data=obj.data.copy();asset.objects.link(copy);copy.select_set(True);bpy.context.view_layer.objects.active=copy
    for v in copy.data.vertices:v.co.y=-v.co.y
    bm=bmesh.new();bm.from_mesh(copy.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(copy.data);bm.free()
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE');bpy.data.objects.remove(copy,do_unlink=True)
    obj.data.calc_loop_triangles();report.append(dict(mesh=obj.name,triangles=len(obj.data.loop_triangles),dimensions_metres=list(obj.dimensions)))
bpy.ops.object.select_all(action='DESELECT')
for obj in objects:obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(ROOT/'GreenlandsHerbalist.glb'),export_format='GLB',use_selection=True)
bpy.ops.export_scene.fbx(filepath=str(ROOT/'GreenlandsHerbalist.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
(ROOT/'herbalist-manifest.json').write_text(json.dumps(dict(units='metres',front='-Y',main_shell_metres=[6,8],porch_extension_metres=1.95,ridge_metres=RIDGE,door_clear_metres=[2.0,2.3],reference_sha256=hashlib.sha256((ROOT/'Greenlands-Building-Family-v1.png').read_bytes()).hexdigest(),materials=MATERIAL_ROWS,meshes=report,status='Prototype; source render, exchange and game review pending'),indent=2))
manifest_path=ROOT/'herbalist-manifest.json';manifest=json.loads(manifest_path.read_text());manifest.update(blender=bpy.app.version_string,room_descriptor_sha256=hashlib.sha256((ROOT/'herbalist-rooms.json').read_bytes()).hexdigest(),method='Original measured geometry from generated family concept A; shared CC0 PBR; independently authored furniture/porch',assumptions=['Hero governs inconsistent generated rear view','Loft and guarded stair flight are measured authoring inferences','Environment scenery only; no herbalism service or inventory implementation']);manifest_path.write_text(json.dumps(manifest,indent=2))
print('HERBALIST_AUTHORED',json.dumps(report),flush=True)
