"""Original cottage, using the family geometry/material authoring primitives."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from authoring_geometry import *

body=Geometry('SM_GreenlandsCottage_Structure');roofgeo=Geometry('SM_GreenlandsCottage_Roof');detail=Geometry('SM_GreenlandsCottage_Joinery');furniture=Geometry('SM_GreenlandsCottage_Dining')
W,D,EAVE,RIDGE,HALF=8,6,3.35,7.1,3.55;SLOPE=(RIDGE-EAVE)/HALF;SLOPE_LENGTH=math.hypot(HALF,RIDGE-EAVE)
def roof_z(y):return RIDGE-SLOPE*abs(y)
WALL=roof_z(3)-.08
def wall(side,offset,umin,umax,zmin,zmax,holes):
    us=sorted(set([umin,umax]+[max(umin,min(umax,h[k])) for h in holes for k in [0,2]]));zs=sorted(set([zmin,zmax,max(zmin,min(zmax,.7))]+[max(zmin,min(zmax,h[k])) for h in holes for k in [1,3]]))
    for za,zb in zip(zs,zs[1:]):
        for ua,ub in zip(us,us[1:]):
            if any(a<(ua+ub)/2<c and b<(za+zb)/2<d for a,b,c,d in holes):continue
            if side=='y':body.box(((ua+ub)/2,offset,(za+zb)/2),(ub-ua,.18,zb-za),plaster,0)
            else:body.box((offset,(ua+ub)/2,(za+zb)/2),(.18,ub-ua,zb-za),plaster,0)
front_holes=[(-1.02,0,1.02,2.3)]+[(x-.55,.88,x+.55,2.12) for x in [-2.6,2.6]]
wall('y',-3,-4,4,.16,WALL,front_holes);wall('y',3,-4,4,.16,WALL,[(x-.55,.88,x+.55,2.12) for x in [-2.1,2.1]])
for x in [-4,4]:
    wall('x',x,-3,3,.16,WALL,[(-.55,.88,.55,2.12)])
    # Gable triangle with a genuine loft window aperture.
    wall('x',x,-.55,.55,WALL,5.3,[(-.55,4.1,.55,5.3)])
    for sign in [-1,1]:
        pts=[(x,sign*3,WALL),(x,sign*.55,WALL),(x,sign*.55,roof_z(.55)),(x,sign*3,roof_z(3))];body.prism(pts,(1,0,0),.18,plaster)
    body.prism([(x,-.55,5.3),(x,.55,5.3),(x,.55,roof_z(.55)),(x,0,RIDGE),(x,-.55,roof_z(.55))],(1,0,0),.18,plaster)
body.box((0,0,.08),(7.9,5.9,.16),stone,.01)
for centre,size in [((-.875,0,2.64),(6.05,5.8,.14)),((3.55,0,2.64),(.7,5.8,.14)),((2.675,-1.7,2.64),(1.05,2.4,.14)),((2.675,2.6,2.64),(1.05,.6,.14))]:body.box(centre,size,wood,.01)
for step in range(15):
    y=-.45+step*.19;z=.16+(step+1)*(2.55/15);body.box((2.675,y,z),(1.0,.205,.075),wood,.009)
for x in [2.13,3.22]:body.beam((x,-.55,.16),(x,2.4,2.71),.12,.18)
# Guard the open stairwell; the left-side exit stays clear near its upper end.
for x,end in [(2.08,1.55),(3.28,2.2)]:
    for y in [-.5,.15,.8,end]:body.beam((x,y,2.71),(x,y,3.70),.085)
    for z in [3.17,3.66]:body.beam((x,-.5,z),(x,end,z),.075)
for z in [3.17,3.66]:body.beam((2.08,-.5,z),(3.28,-.5,z),.075)
# Framing supports the eaves; the ground-floor front window openings stay clear.
for y in [-3.12,3.12]:
    for x in [-3.95,3.95]:body.beam((x,y,.5),(x,y,WALL),.19,.2)
    body.beam((-4.1,y,WALL-.03),(4.1,y,WALL-.03),.22,.22)
    if y<0:
        for x in [-1.14,1.14]:body.beam((x,y,.16),(x,y,2.4),.2)
        body.beam((-1.24,y,2.41),(1.24,y,2.41),.24)
    else:body.beam((-4,y,.61),(4,y,.61),.16)
    # Broad infill fields become framed panels; members stay above windows.
    body.beam((-4,y,2.48),(4,y,2.48),.16,.18)
    for x in [-1.6,1.6]:body.beam((x,y,.6),(x,y,WALL),.15,.18)
    for x,sign in [(-3.85,1),(3.85,-1)]:body.beam((x,y,2.62),(x+sign*.8,y,WALL-.08),.13,.16)
    for x in [-3.9,3.9]:body.beam((x,y,WALL-.8),(x-math.copysign(.5,x),y,WALL-.12),.11)
for x in [-4.12,4.12]:
    body.beam((x,-3,WALL),(x,3,WALL),.22)
    body.beam((x,-3,roof_z(3)),(x,0,RIDGE),.2);body.beam((x,3,roof_z(3)),(x,0,RIDGE),.2)
    body.beam((x,0,WALL),(x,0,RIDGE-.1),.16)
    body.beam((x,-2.5,WALL),(x,-.7,5.5),.14);body.beam((x,2.5,WALL),(x,.7,5.5),.14)
    body.beam((x,-3,.61),(x,3,.61),.16)
    body.beam((x,-3,2.48),(x,3,2.48),.16)
    for y in [-1.2,1.2]:body.beam((x,y,.6),(x,y,WALL),.15)
    for y,sign in [(-2.9,1),(2.9,-1)]:body.beam((x,y,2.6),(x,y+sign*.85,WALL-.1),.13)
# Individual, staggered fieldstone courses with uneven faces and visible joints.
for side in range(4):
    length=8.15 if side<2 else 6.15
    for course in range(3):
        cursor=-length/2
        while cursor<length/2-.08:
            width=min(rng.uniform(.36,.65),length/2-cursor);mid=cursor+width/2;cursor+=width
            if side==0 and abs(mid)<1.06+width/2:continue
            z=.11+course*.19+rng.uniform(-.008,.008);depth=rng.uniform(.21,.27)
            body.box((mid,(-1 if side==0 else 1)*3.06,z),(width-.014,depth,.178),stone,.025) if side<2 else body.box(((-1 if side==2 else 1)*4.06,mid,z),(depth,width-.014,.178),stone,.025)

def roof_polygon(points):roofgeo.poly(points,roof,[(p[0]/1.5,(RIDGE-p[2])/SLOPE/ HALF*SLOPE_LENGTH/1.5) for p in points])
DC=6.45;DR=1.15;DF=-2.85
def dormer_z(x):return DC-.85*abs(x)
def valley(x):return (dormer_z(x)-RIDGE)/SLOPE
def under_dormer(x,y):return abs(x)<DR and DF<y<valley(x)
# Proper intersecting main/dormer roof decks, including the valley cuts.
for sign in [-1,1]:
    if sign>0:roof_polygon([(-4.5,0,RIDGE),(4.5,0,RIDGE),(4.5,HALF,EAVE),(-4.5,HALF,EAVE)])
    else:
        for xa,xb in [(-4.5,-DR),(DR,4.5)]:roof_polygon([(xa,-HALF,EAVE),(xb,-HALF,EAVE),(xb,0,RIDGE),(xa,0,RIDGE)])
        roof_polygon([(-DR,-HALF,EAVE),(DR,-HALF,EAVE),(DR,DF,roof_z(DF)),(-DR,DF,roof_z(DF))])
        for xa,xb in [(-DR,0),(0,DR)]:roof_polygon([(xa,valley(xa),roof_z(valley(xa))),(xb,valley(xb),roof_z(valley(xb))),(xb,0,RIDGE),(xa,0,RIDGE)])
    normal=Vector((0,sign*math.sin(math.atan(SLOPE)),math.cos(math.atan(SLOPE))))
    rotation=Vector((0,-sign, SLOPE)).to_track_quat('Y','Z').to_matrix()
    # Explicit local basis: tile X along the ridge, Y toward the ridge.
    from mathutils import Matrix
    tangent=Vector((0,-sign/math.sqrt(1+SLOPE*SLOPE),SLOPE/math.sqrt(1+SLOPE*SLOPE)));cross=Vector((1,0,0)).cross(tangent);rotation=Matrix(((1,tangent.x,cross.x),(0,tangent.y,cross.y),(0,tangent.z,cross.z)))
    if cross.z<0:rotation=Matrix(((-1,tangent.x,normal.x),(0,tangent.y,normal.y),(0,tangent.z,normal.z)))
    for row in range(math.ceil(SLOPE_LENGTH/.18)):
        distance=.09+row*.18
        if distance>SLOPE_LENGTH:continue
        y=sign*(HALF-distance/math.sqrt(1+SLOPE*SLOPE));z=roof_z(y)
        for col in range(33):
            x=-4.48+(col+.5)*.28+(row%2)*.14
            if x>4.5 or sign<0 and under_dormer(x,y):continue
            centre=Vector((x,y,z))+normal*.036
            roofgeo.box(centre,(.274,.32,.026),roof,.009,rotation@Matrix.Rotation(-.085,3,'X'),tile_patch(.12 if sign>0 else .035))
    # Exposed rafters are separate structural depth beneath the eaves.
    for i in range(15):
        x=-4.35+i*.62;roofgeo.beam((x,sign*3.65,EAVE-.14),(x,0,RIDGE-.14),.10,.14)
    roofgeo.beam((-4.55,sign*3.55,EAVE-.07),(4.55,sign*3.55,EAVE-.07),.17,.2)
    for x in [-3.9,-1.6,1.6,3.9]:
        roofgeo.beam((x,sign*3.12,EAVE-.65),(x,sign*3.55,EAVE-.12),.14,.18)
for x in [-4.48,4.48]:
    for sign in [-1,1]:roofgeo.beam((x,sign*3.65,EAVE-.06),(x,0,RIDGE+.08),.16,.2)
    roofgeo.beam((x,0,RIDGE-.12),(x,0,RIDGE+.43),.14)
roofgeo.beam((-4.6,0,RIDGE+.05),(4.6,0,RIDGE+.05),.14,.2)
# Cross-gabled front dormer: real recessed window, plaster cheeks and valley join.
wall('y',DF,-1.02,1.02,roof_z(DF),5.6,[(-.55,4.35,.55,5.55)])
body.prism([(-1.02,DF,5.6),(1.02,DF,5.6),(0,DF,DC-.08)],(0,1,0),.18,plaster)
for sign in [-1,1]:
    x=sign*1.02;yb=valley(x);body.prism([(x,DF,roof_z(DF)),(x,yb,roof_z(yb)),(x,DF,5.6)],(1,0,0),.14,plaster)
    roof_polygon([(0,-3.15,DC),(sign*DR,-3.15,dormer_z(DR)),(sign*DR,valley(DR),dormer_z(DR)),(0,valley(0),DC)])
    roofgeo.beam((sign*1.18,-3.17,dormer_z(1.18)),(0,-3.17,DC+.06),.13)
    body.beam((sign*1.0,DF-.06,roof_z(DF)),(sign*1.0,DF-.06,5.6),.13)
    for row in range(8):
        x=sign*(DR-(row+.5)*DR/8);z=dormer_z(x)+.03;yb=valley(x)
        c=1/math.sqrt(1+.85*.85);ss=.85*c;rotation=Matrix(((0,-sign*c,sign*ss),(sign,0,0),(0,ss,c)))
        # Ridge of dormer runs Y; use quads with real thickness as overlapping courses.
        for col in range(math.ceil((yb+3.15)/.27)):
            y=-3.15+(col+.5)*.27
            if y>yb:continue
            roofgeo.box((x,y,z),(.265,.30,.024),roof,.006,rotation@Matrix.Rotation(-.085,3,'X'),tile_patch(.08))
roofgeo.beam((0,-3.3,DC+.04),(0,valley(0),DC+.04),.11,.14)
body.beam((-1.05,DF-.09,5.62),(1.05,DF-.09,5.62),.13)

def window(cx,cy,z,yaw=0,closed=False,flower_box=False):
    def point(x,y,zz):a=math.radians(yaw);return (cx+x*math.cos(a)-y*math.sin(a),cy+x*math.sin(a)+y*math.cos(a),z+zz)
    from mathutils import Matrix
    rotation=Matrix.Rotation(math.radians(yaw),3,'Z')
    def b(x,y,zz,size,mat=wood,bevel=.006,angle=0):detail.box(point(x,y,zz),size,mat,bevel,rotation@Matrix.Rotation(math.radians(angle),3,'Z'))
    for x in [-.59,.59]:b(x,-.06,0,(.105,.21,1.4))
    for zz in [-.65,.65]:b(0,-.06,zz,(1.28,.21,.105))
    b(0,-.13,-.74,(1.42,.38,.12))
    for x in [-.27,.27]:
        for zz in [-.39,0,.39]:b(x,.035,zz,(.54,.012,.42),glass,.001)
    b(0,-.005,0,(.045,.055,1.2));b(0,-.005,-.195,(1.09,.055,.04));b(0,-.005,.195,(1.09,.055,.04))
    for side in [-1,1]:
        angle=0 if closed else side*112;rad=math.radians(angle);hinge=side*.655
        for j in range(4):
            lx=-side*(.067+j*.128);b(hinge+lx*math.cos(rad),-.12+lx*math.sin(rad),0,(.123,.055,1.23),angle=angle)
        for zz in [-.39,.39]:
            lx=-side*.26;b(hinge+lx*math.cos(rad),-.16+lx*math.sin(rad),zz,(.47,.028,.045),iron,angle=angle)
    if flower_box:
        b(0,-.28,-.95,(1.2,.38,.07));b(0,-.46,-.83,(1.2,.05,.28))
        for x in [-.575,.575]:b(x,-.28,-.83,(.05,.38,.28))
        b(0,-.27,-.75,(1.08,.3,.035),soil)
        for x in [-.42,.42]:b(x,-.49,-.83,(.035,.018,.28),iron)
for x in [-2.6,2.6]:window(x,-3.04,1.5,flower_box=True)
for x in [-2.1,2.1]:window(x,3.04,1.5,180,closed=x<0)
for side in [-1,1]:window(side*4.04,0,1.5,side*90);window(side*4.04,0,4.7,side*90)
window(0,DF-.04,4.95,flower_box=True)
# Two open, fully modelled door leaves preserve the existing 2m passage.
for side in [-1,1]:
    angle=side*102;hinge=side*1.0;a=math.radians(angle)
    from mathutils import Matrix
    rotation=Matrix.Rotation(a,3,'Z')
    for i in range(7):
        px=-side*(.07+i*.14);detail.box((hinge+px*math.cos(a),-3.08+px*math.sin(a),1.15),(.135,.075,2.18),wood,.009,rotation)
    for zz in [.4,1.85]:
        px=-side*.5;detail.box((hinge+px*math.cos(a),-3.13+px*math.sin(a),zz),(.91,.035,.09),iron,.008,rotation)
    # Ring handle and backing plate.
    px=-side*.83;centre=Vector((hinge+px*math.cos(a),-3.16+px*math.sin(a),1.1));detail.box(centre,(.1,.025,.18),iron,.008,rotation)
    for j in range(12):
        aa=math.tau*j/12;bb=math.tau*(j+1)/12;p1=centre+rotation@Vector((math.cos(aa)*.055,-.025,math.sin(aa)*.07));p2=centre+rotation@Vector((math.cos(bb)*.055,-.025,math.sin(bb)*.07));detail.beam(p1,p2,.012,material=iron)
# A coursed chimney in the rear-left corner, clear of the stair flight.
CHX,CHY=-3.25,2.35
for course in range(34):
    z=.3+course*.22
    for ix in [-1,1]:
        for iy in [-1,1]:
            if iy<0 and course<5:continue # actual hearth opening, not a black decal
            roofgeo.box((CHX+ix*.2,CHY+iy*.19,z),(.39,.37,.209),stone,.018)
for x in [CHX-.39,CHX+.39]:roofgeo.box((x,CHY,7.58),(.14,.9,.23),stone,.014)
for y in [CHY-.39,CHY+.39]:roofgeo.box((CHX,y,7.58),(.9,.14,.23),stone,.014)
roofgeo.box((CHX,CHY,7.78),(1.02,1.0,.14),stone,.02)
for side in [-1,1]:roofgeo.box((CHX+side*.34,CHY-.25,.65),(.13,.32,.98),stone,.018)
roofgeo.box((CHX,CHY-.25,1.27),(.92,.37,.18),stone,.022)
roofgeo.box((CHX,CHY-.38,.20),(1.14,.91,.10),stone,.024)
detail.box((CHX,CHY-.10,.28),(.57,.38,.055),char,.012)
for i in range(4):
    xx=CHX-.22+i*.14;detail.beam((xx,CHY-.36,.29),(xx,CHY+.08,.29),.075,material=char)
    detail.box((xx,CHY-.18,.33),(.045,.08,.018),ember,.006)
for xx in [-.24,0,.24]:detail.beam((CHX+xx,CHY-.39,.23),(CHX+xx,CHY-.39,.43),.018,material=iron)
detail.beam((CHX-.3,CHY-.39,.42),(CHX+.3,CHY-.39,.42),.018,material=iron)
# Entry lantern: forged frame and a warm, restrained glass core.
for x,z in [(1.36,2.1)]:
    detail.beam((x,-3.17,z+.25),(x,-3.58,z+.25),.045,material=iron)
    detail.box((x,-3.54,z),(.18,.18,.29),warm,.01)
    for xx in [-.11,.11]:
        for yy in [-.11,.11]:detail.beam((x+xx,-3.54+yy,z-.18),(x+xx,-3.54+yy,z+.18),.025,material=iron)
    detail.box((x,-3.54,z-.19),(.27,.27,.055),iron,.015);detail.box((x,-3.54,z+.19),(.28,.28,.055),iron,.015)
detail.beam((-1,0,2.55),(-1,0,2.38),.018,material=iron)
detail.box((-1,0,2.25),(.12,.12,.18),warm,.006)
for x in [-1.085,-.915]:
    for y in [-.085,.085]:detail.beam((x,y,2.12),(x,y,2.38),.018,material=iron)
for z in [2.12,2.38]:detail.box((-1,0,z),(.22,.22,.035),iron,.008)

# Coordinated furniture is separate from the building shell. Room metadata also
# remains available to later UE dressing instead of guessing from mesh bounds.
dressing=json.loads((ROOT/'room-dressing.json').read_text())
anchors=bpy.data.collections.new('Room anchors — authoring only');scene.collection.children.link(anchors)
occupied=[]
def overlaps(a,b):return a[0]<b[2] and a[2]>b[0] and a[1]<b[3] and a[3]>b[1]
for group in dressing['assemblies']:
    room=next(r for r in dressing['rooms'] if r['id']==group['room'])
    ax,ay,floor=group['anchor'];angle=math.radians(group['yaw_degrees']);rotation=Matrix.Rotation(angle,3,'Z')
    rect=group['occupied_xy_local'];corners=[rotation@Vector((x,y,0))+Vector((ax,ay,0)) for x in [rect[0],rect[2]] for y in [rect[1],rect[3]]]
    bounds=[min(p.x for p in corners),min(p.y for p in corners),max(p.x for p in corners),max(p.y for p in corners)]
    usable=room['usable_xy']
    assert bounds[0]>=usable[0] and bounds[1]>=usable[1] and bounds[2]<=usable[2] and bounds[3]<=usable[3],f"Furniture outside room: {group['id']}"
    assert not any(overlaps(bounds,r['xy']) for r in dressing['keep_clear'] if r['room']==group['room']),f"Furniture blocks circulation: {group['id']}"
    assert not any(overlaps(bounds,b) for rid,b in occupied if rid==group['room']),f"Furniture assemblies overlap: {group['id']}"
    occupied.append((group['room'],bounds))
    anchor=bpy.data.objects.new('RoomAnchor_'+group['id'],None);anchors.objects.link(anchor);anchor.location=(ax,ay,floor);anchor.rotation_euler.z=angle;anchor.empty_display_type='ARROWS';anchor.empty_display_size=.3
    anchor['room_id']=group['room'];anchor['assembly_template']=group['template'];anchor['occupied_xy_metres']=bounds
    def point(x,y,z):return rotation@Vector((x,y,z))+Vector((ax,ay,floor))
    def box(x,y,z,size):furniture.box(point(x,y,z),size,wood,.012,rotation)
    def beam(a,b,width):furniture.beam(point(*a),point(*b),width)
    if group['template']=='loft_bed_and_chest':
        box(0,0,.25,(2.12,1.16,.18))
        for x in [-.94,.94]:
            for y in [-.49,.49]:beam((x,y,0),(x,y,.48),.085)
        box(-.97,0,.58,(.09,1.18,.43))
        for i in range(32):
            xa=-.96+i*.06;xb=xa+.06
            for j in range(24):
                ya=-.65+j*.055;yb=ya+.055
                def quilt_z(x,y):return .375+.018*math.sin(x*17+y*4)+.012*math.sin(y*24)-max(0,abs(y)-.48)*1.8
                furniture.poly([point(xa,ya,quilt_z(xa,ya)),point(xb,ya,quilt_z(xb,ya)),point(xb,yb,quilt_z(xb,yb)),point(xa,yb,quilt_z(xa,yb))],linen)
        furniture.box(point(-.62,0,.42),(.42,.74,.12),linen,.05,rotation)
        box(-1.65,-.95,.26,(.94,.54,.46));box(-1.65,-.95,.51,(.98,.57,.08))
        for x in [-1.97,-1.33]:furniture.box(point(x,-.95,.555),(.055,.58,.015),iron,.005,rotation)
        continue
    assert group['template']=='table_bench_two_chairs',f"Unsupported assembly: {group['template']}"
    # 75cm table, 45cm seats; chair space included in the assembly footprint.
    box(0,0,.75,(1.5,.75,.075))
    for x in [-.60,.60]:
        for y in [-.25,.25]:beam((x,y,0),(x,y,.715),.075)
    beam((-.6,0,.23),(.6,0,.23),.07)
    box(0,.82,.45,(1.45,.34,.075))
    for x in [-.57,.57]:box(x,.82,.225,(.085,.28,.40))
    for sign in [-1,1]:
        cx=sign*1.1;box(cx,0,.45,(.46,.46,.065))
        for dx in [-.17,.17]:
            for y in [-.17,.17]:beam((cx+dx,y,0),(cx+dx,y,.425),.055)
        for y in [-.19,.19]:beam((cx+sign*.19,y,.4),(cx+sign*.19,y,.93),.055)
        for z in [.66,.85]:box(cx+sign*.19,0,z,(.055,.43,.12))
    # Supported tabletop props stay inside the dining assembly's existing bounds.
    furniture.vessel(point(-.40,.03,.79),[(.055,0),(.085,.02),(.105,.13),(.06,.20),(.065,.23),(.052,.23),(.047,.20),(.087,.13),(.065,.03),(.055,.025)],clay)
    for x,y in [(.20,-.12),(.46,.17)]:
        furniture.vessel(point(x,y,.79),[(.055,0),(.10,.07),(.105,.08),(.092,.08),(.045,.015),(.055,0)],clay)

# Low ridge caps are curved geometry, not a painted line along the roof peak.
for cap in range(30):
    xa=-4.5+cap*.3;xb=xa+.315;outer=[];inner=[]
    for j in range(9):
        angle=math.pi*j/8;outer.append((math.cos(angle)*.16,RIDGE+.03+math.sin(angle)*.16));inner.append((math.cos(angle)*.135,RIDGE+.03+math.sin(angle)*.135))
    for j in range(8):
        a,b=outer[j],outer[j+1];roofgeo.poly([(xa,*a),(xb,*a),(xb,*b),(xa,*b)],roof)
        a,b=inner[j],inner[j+1];roofgeo.poly([(xa,*b),(xb,*b),(xb,*a),(xa,*a)],roof)
        for x in [xa,xb]:roofgeo.poly([(x,*outer[j]),(x,*outer[j+1]),(x,*inner[j+1]),(x,*inner[j])],roof)

objects=[g.object() for g in [body,roofgeo,detail,furniture]]
# Save a clean editable asset. Camera/lights belong to a separate presentation collection.
presentation=bpy.data.collections.new('Presentation — excluded from exports');scene.collection.children.link(presentation)
def move_to(obj,col):
    for current in list(obj.users_collection):current.objects.unlink(obj)
    col.objects.link(obj)
def camera(name,position,target,ortho=None):
    bpy.ops.object.camera_add(location=position);o=bpy.context.object;o.name=name;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();o.data.lens=44
    if ortho:o.data.type='ORTHO';o.data.ortho_scale=ortho
    move_to(o,presentation);return o
cameras={'Beauty':camera('Beauty — front left',(-12,-16,9),(0,0,3.6)), 'Front':camera('Front',(0,-20,4),(0,0,4),11),'Side':camera('Side',(-20,0,4),(0,0,4),10),'Rear':camera('Rear',(0,20,4),(0,0,4),11)}
scene.camera=cameras['Beauty'];scene.world=bpy.data.worlds.new('Neutral daylight');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.45,.55,.7,1);scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.3
for name,loc,energy,size in [('Key',(-7,-9,13),2200,7),('Fill',(8,-4,8),1400,8),('Rim',(2,7,12),1900,6)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.name=name;o.data.energy=energy;o.data.shape='DISK';o.data.size=size;o.rotation_euler=(Vector((0,0,3))-o.location).to_track_quat('-Z','Y').to_euler();move_to(o,presentation)
bpy.ops.mesh.primitive_plane_add(size=200);ground=bpy.context.object;ground.name='Neutral presentation ground';ground.location.z=-.03;move_to(ground,presentation)
groundmat=bpy.data.materials.new('Neutral ground');groundmat.diffuse_color=(.21,.23,.19,1);ground.data.materials.append(groundmat)
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
for image in bpy.data.images:
    if image.source=='FILE':image.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'GreenlandsCottage.blend'))
report=[]
for obj in objects:
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    # FBX authored-coordinate correction is export-only; saved Blender stays front -Y.
    copy=obj.copy();copy.data=obj.data.copy();asset.objects.link(copy);obj.select_set(False);copy.select_set(True);bpy.context.view_layer.objects.active=copy
    for v in copy.data.vertices:v.co.y=-v.co.y
    bm=bmesh.new();bm.from_mesh(copy.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(copy.data);bm.free()
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE');bpy.data.objects.remove(copy,do_unlink=True)
    obj.data.calc_loop_triangles();report.append(dict(mesh=obj.name,vertices=len(obj.data.vertices),triangles=len(obj.data.loop_triangles),dimensions_metres=list(obj.dimensions)))
bpy.ops.object.select_all(action='DESELECT')
for obj in objects:obj.select_set(True)
bpy.context.view_layer.objects.active=objects[0];bpy.ops.export_scene.gltf(filepath=str(ROOT/'GreenlandsCottage.glb'),export_format='GLB',use_selection=True)
bpy.ops.export_scene.fbx(filepath=str(ROOT/'GreenlandsCottage.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
(ROOT/'asset-manifest.json').write_text(json.dumps(dict(source='Generated concept interpreted as original authored geometry',reference_sha256=hashlib.sha256((ROOT/'Greenlands-Cottage-Concept-v1.png').read_bytes()).hexdigest(),blender=bpy.app.version_string,units='metres',front='-Y',footprint_metres=[8,6],ridge_metres=RIDGE,door_clear_metres=[2.04,2.3],materials=MATERIAL_ROWS,meshes=report,assumptions=['Hero view governs contradictions in secondary generated elevations','Rear windows and interior loft are conservative authored inferences','Asset is static environment scenery, not player-construction topology'],status='Authored prototype; fresh-source render and exchange reimport review pending'),indent=2))
print('COTTAGE_AUTHORED',json.dumps(report),flush=True)
