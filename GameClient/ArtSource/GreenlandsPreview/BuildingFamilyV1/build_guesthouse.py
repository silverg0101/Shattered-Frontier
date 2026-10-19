"""Measured interpretation of generated guesthouse C; original static scenery."""
import sys,json,math,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT.parent/'CottageConceptV1'))
from authoring_geometry import *
ROOT=Path(__file__).resolve().parent;asset.name='Greenlands Guesthouse — original mesh'
rooms=json.loads((ROOT/'guesthouse-rooms.json').read_text());anchors=bpy.data.collections.new('Room anchors — authoring only');scene.collection.children.link(anchors)
def intersects(a,b):return a[0]<b[2] and a[2]>b[0] and a[1]<b[3] and a[3]>b[1]
occupied=[]
for group in rooms['assemblies']:
    room=next(r for r in rooms['rooms'] if r['id']==group['room']);x,y,z=group['anchor'];a=group['footprint_xy'];bounds=[x+a[0],y+a[1],x+a[2],y+a[3]];r=room['usable_xy']
    assert bounds[0]>=r[0] and bounds[1]>=r[1] and bounds[2]<=r[2] and bounds[3]<=r[3],group['id']
    assert not any(intersects(bounds,k['xy']) for k in rooms['keep_clear'] if k['room']==group['room']),group['id']+' obstructs circulation'
    assert not any(intersects(bounds,b) for rid,b in occupied if rid==group['room']),group['id']+' overlaps furnishing'
    occupied.append((group['room'],bounds));o=bpy.data.objects.new('RoomAnchor_'+group['id'],None);anchors.objects.link(o);o.location=(x,y,z);o.empty_display_type='ARROWS';o.empty_display_size=.25;o['room_id']=group['room'];o['occupied_xy_metres']=bounds
body=Geometry('SM_GreenlandsGuesthouse_Structure');tiles=Geometry('SM_GreenlandsGuesthouse_Roof');joinery=Geometry('SM_GreenlandsGuesthouse_Joinery');props=Geometry('SM_GreenlandsGuesthouse_Furnishings');approach=Geometry('SM_GreenlandsGuesthouse_Approach')
RIDGE,EAVE,HALF=7.8,3.5,3.55;SLOPE=(RIDGE-EAVE)/HALF;LENGTH=math.hypot(HALF,RIDGE-EAVE)
def roof_z(y):return RIDGE-SLOPE*abs(y)
WALL=roof_z(3)-.08;DORMERS=[-1.9,1.9];DR=.95;DC=6.8;DF=-3.01
def dormer_z(dx):return DC-abs(dx)
def valley(dx):return (dormer_z(dx)-RIDGE)/SLOPE
def under_dormer(x,y):return any(abs(x-c)<DR and DF<y<valley(x-c) for c in DORMERS)
def wall(axis,offset,lo,hi,zlo,zhi,holes):
    us=sorted(set([lo,hi]+[max(lo,min(hi,h[k])) for h in holes for k in [0,2]]));zs=sorted(set([zlo,zhi]+[max(zlo,min(zhi,h[k])) for h in holes for k in [1,3]]))
    for a,b in zip(us,us[1:]):
        for za,zb in zip(zs,zs[1:]):
            if any(x<(a+b)/2<xx and z<(za+zb)/2<zz for x,z,xx,zz in holes):continue
            body.box(((a+b)/2,offset,(za+zb)/2) if axis=='y' else (offset,(a+b)/2,(za+zb)/2),(b-a,.18,zb-za) if axis=='y' else (.18,b-a,zb-za),plaster,0)
wall('y',-3,-4,4,.16,WALL,[(-1.05,.16,1.05,2.46)]+[(x-.55,1.05,x+.55,2.25) for x in [-2.7,2.7]])
wall('y',3,-4,4,.16,WALL,[(x-.55,1.05,x+.55,2.25) for x in [-2.1,2.1]])
for x in [-4,4]:
    wall('x',x,-3,3,.16,WALL,[(-.55,1.05,.55,2.25)])
    wall('x',x,-.55,.55,WALL,5.75,[(-.55,4.55,.55,5.75)])
    for sign in [-1,1]:body.prism([(x,sign*3,WALL),(x,sign*.55,WALL),(x,sign*.55,roof_z(.55)),(x,sign*3,roof_z(3))],(1,0,0),.18,plaster)
    body.prism([(x,-.55,5.75),(x,.55,5.75),(x,.55,roof_z(.55)),(x,0,RIDGE),(x,-.55,roof_z(.55))],(1,0,0),.18,plaster)
for sign in [-1,1]:
    inner,outer=sign*2.89,sign*3.1
    body.prism([(0,inner,WALL-.05),(0,outer,WALL-.05),(0,outer,roof_z(outer)-.015),(0,inner,roof_z(inner)-.015)],(1,0,0),8,plaster)
body.box((0,0,.08),(7.9,5.9,.16),stone,.012)
# Full upper walking deck, with a rear flight and a protected landing.
for centre,size in [((0,-.825,2.88),(7.8,4.15,.14)),((-3.5,2.075,2.88),(.8,1.65,.14)),((2.45,2.075,2.88),(2.9,1.65,.14)),((-.95,2.675,2.88),(4.3,.45,.14))]:body.box(centre,size,wood,.008)
for i in range(17):body.box((-2.8+i*.22,1.85,.16+(i+1)*2.79/17),( .235,1.0,.075),wood,.008)
for y in [1.3,2.4]:body.beam((-2.96,y,.16),(.94,y,2.95),.12,.17)
for y,end in [(1.22,.65),(2.48,.95)]:
    for x in [-3.05,-2.3,-1.5,-.7,end]:body.beam((x,y,2.95),(x,y,3.94),.085)
    for z in [3.40,3.90]:body.beam((-3.05,y,z),(end,y,z),.075)
for z in [3.40,3.90]:body.beam((-3.1,1.22,z),(-3.1,2.48,z),.075)
# Exterior timber frame, including independent dormer jambs and gable braces.
for y in [-3.12,3.12]:
    for x in [-3.95,-1.3,1.3,3.95]:body.beam((x,y,.55),(x,y,WALL),.17,.20)
    for z in [2.65,2.95,WALL]:body.beam((-4.12,y,z),(4.12,y,z),.19,.22)
    for sign in [-1,1]:body.beam((sign*3.85,y,3.03),(sign*3.12,y,WALL-.05),.13,.17)
for x in [-4.12,4.12]:
    for y in [-2.95,-1.2,1.2,2.95]:body.beam((x,y,.55),(x,y,WALL),.17,.20)
    for z in [.65,2.65,2.95,WALL]:body.beam((x,-3,z),(x,3,z),.19)
    for sign in [-1,1]:body.beam((x,sign*3,roof_z(3)),(x,0,RIDGE),.20,.22)
    body.beam((x,0,5.85),(x,0,RIDGE-.08),.16)
    for sign in [-1,1]:body.beam((x,sign*2.5,WALL),(x,sign*.8,5.9),.14)
for side in range(4):
    length=8.15 if side<2 else 6.15
    for course in range(3):
        cursor=-length/2
        while cursor<length/2-.08:
            width=min(rng.uniform(.38,.65),length/2-cursor);mid=cursor+width/2;cursor+=width
            if side==0 and abs(mid)<1.10+width/2:continue
            body.box((mid,(-1 if side==0 else 1)*3.06,.12+course*.19),(width-.014,.25,.18),stone,.024) if side<2 else body.box(((-1 if side==2 else 1)*4.06,mid,.12+course*.19),(.25,width-.014,.18),stone,.024)
def deck(points,normal):
    tiles.prism(points,normal,.04,roof);tiles.prism([Vector(p)-Vector(normal)*.06 for p in points],normal,.035,wood)
# Main roof decks have actual paired valley apertures, not floating dormer boxes.
normal_front=Vector((0,-SLOPE,1)).normalized();normal_back=Vector((0,SLOPE,1)).normalized()
deck([(-4.5,0,RIDGE),(4.5,0,RIDGE),(4.5,HALF,EAVE),(-4.5,HALF,EAVE)],normal_back)
for xa,xb in [(-4.5,-2.85),(-.95,.95),(2.85,4.5)]:deck([(xa,-HALF,EAVE),(xb,-HALF,EAVE),(xb,0,RIDGE),(xa,0,RIDGE)],normal_front)
for c in DORMERS:
    deck([(c-DR,-HALF,EAVE),(c+DR,-HALF,EAVE),(c+DR,DF,roof_z(DF)),(c-DR,DF,roof_z(DF))],normal_front)
    for a,b in [(-DR,0),(0,DR)]:deck([(c+a,valley(a),roof_z(valley(a))),(c+b,valley(b),roof_z(valley(b))),(c+b,0,RIDGE),(c+a,0,RIDGE)],normal_front)
for sign in [-1,1]:
    normal=Vector((0,sign*SLOPE,1)).normalized();tangent=Vector((0,-sign,SLOPE)).normalized();across=Vector((-sign,0,0));rotation=Matrix(((across.x,tangent.x,normal.x),(0,tangent.y,normal.y),(0,tangent.z,normal.z)))
    for row in range(math.ceil(LENGTH/.18)):
        distance=.09+row*.18
        if distance>LENGTH:continue
        y=sign*(HALF-distance/math.sqrt(1+SLOPE*SLOPE));z=roof_z(y)
        for col in range(33):
            x=-4.48+(col+.5)*.28+(row%2)*.14
            if x>4.5 or sign<0 and under_dormer(x,y):continue
            tiles.box(Vector((x,y,z))+normal*.036,(.274,.32,.026),roof,.008,rotation@Matrix.Rotation(-.085,3,'X'),tile_patch(.05))
    for x in [-4.45,-3.85,-3.25,-2.65,-2.05,-1.45,-.85,-.25,.35,.95,1.55,2.15,2.75,3.35,3.95,4.45]:
        start=sign*HALF
        if sign<0:
            for c in DORMERS:
                if abs(x-c)<DR:start=valley(x-c)
        tiles.beam((x,start,roof_z(start)-.14),(x,0,RIDGE-.14),.10,.14)
    tiles.beam((-4.55,sign*HALF,EAVE-.1),(4.55,sign*HALF,EAVE-.1),.18,.22)
    for x in [-4.5,4.5]:tiles.beam((x,sign*(HALF+.08),EAVE-.08),(x,0,RIDGE+.08),.20,.22)
tiles.beam((-4.6,0,RIDGE+.05),(4.6,0,RIDGE+.05),.16,.21)
for c in DORMERS:
    wall('y',DF,c-DR,c+DR,WALL-.05,5.85,[(c-.55,4.55,c+.55,5.75)])
    body.prism([(c-DR,DF,5.85),(c+DR,DF,5.85),(c,DF,DC)],(0,1,0),.18,plaster)
    for sign in [-1,1]:
        x=c+sign*DR;body.prism([(x,DF,roof_z(DF)),(x,DF,5.85),(x,valley(sign*DR),5.85)],(1,0,0),.15,plaster)
        body.beam((x,DF-.1,WALL),(x,DF-.1,5.85),.13);body.beam((x,DF-.1,5.85),(c,DF-.1,DC),.15)
        points=[(c,DF-.35,DC),(x,DF-.35,5.85),(x,valley(sign*DR),5.85),(c,valley(0),DC)];normal=Vector((sign,0,1)).normalized();deck(points,normal)
        tangent=Vector((-sign,0,1)).normalized();across=Vector((0,sign,0));rotation=Matrix(((across.x,tangent.x,normal.x),(across.y,tangent.y,0),(0,tangent.z,normal.z)))
        for row in range(8):
            dx=sign*(DR-(row+.5)*DR/8);z=dormer_z(dx);yb=valley(dx)
            for col in range(math.ceil((yb-DF+.35)/.28)):
                y=DF-.35+(col+.5)*.28
                if y>yb:continue
                tiles.box(Vector((c+dx,y,z))+normal*.04,(.274,.28,.026),roof,.007,rotation@Matrix.Rotation(-.085,3,'X'),tile_patch(.05))
        tiles.beam((x,DF-.42,5.82),(c,DF-.42,DC+.06),.14,.17)
    body.beam((c-DR,DF-.1,5.85),(c+DR,DF-.1,5.85),.14)
    tiles.beam((c,DF-.43,DC+.06),(c,valley(0),DC+.06),.12,.16)

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

for x in [-2.7,2.7]:window(x,-3.04,1.65,box=True)
for c in DORMERS:window(c,DF-.04,5.15,box=True)
for x in [-2.1,2.1]:window(x,3.04,1.65,180)
for sign in [-1,1]:window(sign*4.04,0,1.65,sign*90);window(sign*4.04,0,5.15,sign*90)
for side in [-1,1]:
    rot=Matrix.Rotation(math.radians(side*103),3,'Z');hinge=side*1.05
    for j in range(7):joinery.box(Vector((hinge,-3.1,1.31))+rot@Vector((-side*(.073+j*.147),0,0)),(.142,.075,2.23),wood,.009,rot)
    for z in [.55,2.05]:joinery.box(Vector((hinge,-3.15,z))+rot@Vector((-side*.52,0,0)),(.98,.035,.08),iron,.005,rot)
for x in [-1.17,1.17]:joinery.beam((x,-3.16,.16),(x,-3.16,2.52),.20)
joinery.beam((-1.28,-3.16,2.57),(1.28,-3.16,2.57),.24)
# A shallow, pitched entrance canopy and a genuine planked porch landing.
pslope=.6/1.33;normal=Vector((0,-pslope,1)).normalized();tangent=Vector((0,1,pslope)).normalized();rotation=Matrix(((1,0,0),(0,tangent.y,normal.y),(0,tangent.z,normal.z)))
points=[(-1.7,-4.55,2.95),(1.7,-4.55,2.95),(1.7,-3.22,3.55),(-1.7,-3.22,3.55)];deck(points,normal)
for row in range(9):
    y=-4.55+(row+.5)*1.33/9;z=2.95+(y+4.55)*pslope
    for col in range(13):
        x=-1.7+(col+.5)*.265
        if x>1.7:continue
        tiles.box(Vector((x,y,z))+normal*.04,(.26,.29,.026),roof,.007,rotation@Matrix.Rotation(-.085,3,'X'),tile_patch(.05))
for x in [-1.42,1.42]:
    body.beam((x,-4.25,.16),(x,-4.25,3.02),.18,.20);body.beam((x,-4.25,2.4),(x-math.copysign(.5,x),-4.25,3.04),.12,.16)
    tiles.beam((x,-4.62,2.87),(x,-3.17,3.52),.13,.16)
body.beam((-1.6,-4.25,3.01),(1.6,-4.25,3.01),.18,.20)
for i in range(13):body.box((-1.44+i*.24,-3.65,.09),(.23,1.3,.14),wood,.006)
# Paired dining groups and a service counter leave the central aisle clear.
def cup(x,y,z):props.vessel((x,y,z),[(.06,0),(.07,.02),(.075,.13),(.065,.15),(.05,.15),(.05,.03)],clay)
def dining(x,y,length):
    props.box((x,y,.94),(length,.76,.09),wood,.018)
    for dx in [-length*.4,length*.4]:
        for dy in [-.26,.26]:props.beam((x+dx,y+dy,.16),(x+dx,y+dy,.9),.075)
    for side in [-1,1]:
        cy=y+side*.66;props.box((x,cy,.62),(length*.85,.30,.09),wood,.012)
        for dx in [-length*.32,length*.32]:
            for dy in [-.09,.09]:props.beam((x+dx,cy+dy,.16),(x+dx,cy+dy,.58),.06)
        props.box((x,cy+side*.13,.89),(length*.85,.065,.38),wood,.012)
    for j in [-1,1]:cup(x+j*.34,y+j*.13,.985)
    props.vessel((x,y,.985),[(.12,0),(.14,.02),(.16,.07),(.145,.10),(.125,.10),(.12,.05)],clay)
dining(-2.2,-.7,1.7);dining(2.25,-.8,1.5)
props.box((2.7,.8,1.14),(1.8,.60,.11),wood,.018)
for x in [1.87,3.53]:props.box((x,.8,.67),(.13,.5,.96),wood,.012)
props.box((2.7,1.05,.67),(1.8,.07,.96),wood,.012)
for x in [2.05,2.35,2.65,2.95,3.25]:cup(x,.8,1.20)
# Two occupied sleeping spaces, with a low privacy screen and a clear rear corridor.
props.box((0,-1.55,3.75),(.085,2.25,1.6),wood,.014)
for cx in [-1.9,1.9]:
    cy=-1.25;props.box((cx,cy,3.18),(1.16,2.12,.17),wood,.018)
    props.box((cx,cy,3.26),(1.1,1.97,.13),linen,.035)
    for dx in [-.48,.48]:
        for dy in [-.95,.95]:props.beam((cx+dx,cy+dy,2.95),(cx+dx,cy+dy,3.42),.08)
    props.box((cx,cy-.98,3.56),(1.17,.08,.56),wood,.014)
    for i in range(24):
        for j in range(32):
            xa=cx-.55+i*1.1/24;xb=cx-.55+(i+1)*1.1/24;ya=cy-.95+j*1.9/32;yb=cy-.95+(j+1)*1.9/32
            def quilt(x,y):return 3.34+.018*math.sin(x*9+y*13)+.012*math.sin(y*22-x*5)
            props.poly([(xa,ya,quilt(xa,ya)),(xb,ya,quilt(xb,ya)),(xb,yb,quilt(xb,yb)),(xa,yb,quilt(xa,yb))],linen)
    for sign in [-1,1]:
        x=cx+sign*.55
        for j in range(32):
            ya=cy-.95+j*1.9/32;yb=cy-.95+(j+1)*1.9/32
            props.poly([(x,ya,quilt(x,ya)),(x,yb,quilt(x,yb)),(x+sign*.025,yb,3.17+.015*math.sin(yb*18)),(x+sign*.025,ya,3.17+.015*math.sin(ya*18))],linen)
    props.box((cx,cy-.63,3.41),(.76,.39,.14),linen,.055)
for cx in [-3.2,3.2]:
    props.box((cx,-.7,3.18),(.60,1.16,.42),wood,.025)
    for y in [-1.08,-.32]:props.box((cx,y,3.41),(.64,.055,.045),iron,.008)
# Masonry hearth and rear-right flue; kept away from stairs/upper landing.
for x in [3.02,3.68]:body.box((x,1.93,1.06),(.22,.76,1.80),stone,.024)
body.box((3.35,2.22,1.06),(.80,.18,1.80),stone,.024);body.box((3.35,1.93,2.05),(.96,.85,.22),stone,.024)
props.box((3.35,1.85,.26),(.65,.57,.14),iron,.016);props.box((3.35,1.85,.345),(.43,.32,.03),warm,.006)
for x in [3.12,3.27,3.42,3.57]:props.beam((x,1.49,.33),(x,1.49,.75),.018,material=iron)
for row in range(33):
    z=2.27+row*.19
    for sign in [-1,1]:
        body.box((3.35+sign*.33,1.93,z),(.18,.79,.18),stone,.022)
        body.box((3.35,1.93+sign*.31,z),(.49,.18,.18),stone,.022)
body.box((3.35,1.93,8.52),(.99,.94,.14),stone,.025)
# Hanging bed sign is environmental storytelling, not an implemented inn service.
joinery.beam((3.8,-3.13,3.83),(4.9,-3.13,3.83),.065,material=iron)
for x in [4.05,4.65]:joinery.beam((x,-3.13,3.8),(x,-3.13,3.53),.025,material=iron)
joinery.box((4.35,-3.13,3.2),(1.10,.10,.66),wood,.025)
for x in [3.86,4.84]:joinery.box((x,-3.2,3.2),(.05,.025,.60),iron,.006)
for z in [2.93,3.47]:joinery.box((4.35,-3.2,z),(1.03,.025,.05),iron,.006)
joinery.box((4.35,-3.2,3.16),(.69,.025,.10),plaster,.008)
for x in [4.02,4.68]:joinery.box((x,-3.2,3.14),(.05,.025,.31),plaster,.006)
joinery.box((4.16,-3.2,3.27),(.21,.025,.11),plaster,.012)
for x,y,z in [(-1.7,-3.55,2.25),(-.55,.1,2.38),(0,.6,5.20)]:
    joinery.box((x,y,z),(.13,.13,.22),warm,.006)
    for dx in [-.09,.09]:
        for dy in [-.09,.09]:joinery.beam((x+dx,y+dy,z-.15),(x+dx,y+dy,z+.14),.018,material=iron)
    for zz in [z-.15,z+.14]:joinery.box((x,y,zz),(.24,.24,.04),iron,.008)
joinery.beam((-1.7,-3.14,2.48),(-1.7,-3.55,2.48),.035,material=iron)
joinery.beam((-1.7,-3.55,2.48),(-1.7,-3.55,2.39),.018,material=iron)
joinery.beam((-.55,.1,2.52),(-.55,.1,2.81),.018,material=iron)
joinery.beam((0,.6,5.34),(0,.6,roof_z(.6)-.17),.018,material=iron)
# Continuous low ramp meets the existing northern footpath/porch landing.
for row in range(9):
    for col in range(6):
        points=[]
        for r,c in [(row,col),(row,col+1),(row+1,col+1),(row+1,col)]:
            t=r/9;x=-1.15+c*2.3/6;y=-5.2+t*.96;z=-.13+.29*t;points.append((x,y,z))
        approach.poly(points,stone,[(p[0]/2,p[1]/2) for p in points])
objects=[g.object() for g in [body,tiles,joinery,props,approach]]
# Maintained age follows actual exposure: sill runoff, low splash and flue soot.
# Preserve PBR scans; these masks tint local surfaces instead of uniform dirt.
for obj in objects:
    if obj.name.endswith('_Approach'):continue
    colours=obj.data.color_attributes['CottageWeathering']
    for face in obj.data.polygons:
        material=obj.data.materials[face.material_index]
        for li in face.loop_indices:
            x,y,z=obj.data.vertices[obj.data.loops[li].vertex_index].co;wear=colours.data[li].color[0]
            n=.5+.25*math.sin(x*13.7+y*4.3)+.25*math.sin(z*11.5-x*8.1)
            if material==plaster:
                if abs(y+3)<.25:
                    for wx,sill in [(-2.7,.90),(2.7,.90),(-1.9,4.40),(1.9,4.40)]:
                        runoff=math.exp(-((x-wx)/.57)**2)*max(0,1-abs(z-(sill-.29))/.50)
                        wear=max(wear,runoff*(.22+.24*n))
                if abs(abs(x)-4)<.25:wear=max(wear,math.exp(-(y/.62)**2)*max(0,1-abs(z-4.08)/.52)*(.2+.22*n))
            elif material==stone and x>2.8 and y>1.4 and z>7.7:wear=max(wear,min(.70,(z-7.7)*.80)*(.65+.35*n))
            elif material==wood and z<.9:wear=max(wear,max(0,1-(z-.16)/.85)*(.18+.20*n))
            colours.data[li].color=(wear,wear,wear,1)
presentation=bpy.data.collections.new('Presentation — excluded from exports');scene.collection.children.link(presentation)
def move(obj):
    for col in list(obj.users_collection):col.objects.unlink(obj)
    presentation.objects.link(obj)
for label,loc,target,ortho in [('Beauty',(13,-17,10),(0,0,3.7),None),('Front',(0,-24,4),(0,0,4),13),('Side',(24,0,4),(0,0,4),11),('Rear',(0,24,4),(0,0,4),13)]:
    bpy.ops.object.camera_add(location=loc);o=bpy.context.object;o.name=label;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();o.data.lens=45
    if ortho:o.data.type='ORTHO';o.data.ortho_scale=ortho
    move(o)
scene.camera=bpy.data.objects['Beauty'];scene.world=bpy.data.worlds.new('Guesthouse neutral daylight');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.3
for loc,power,size in [((-5,-8,12),2400,7),((8,-2,8),1500,7),((0,7,10),1800,6)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(Vector((1,0,2))-o.location).to_track_quat('-Z','Y').to_euler();move(o)
bpy.ops.mesh.primitive_plane_add(size=200);ground=bpy.context.object;ground.location.z=-.03;move(ground)
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX'
for image in bpy.data.images:
    if image.source=='FILE':image.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'GreenlandsGuesthouse.blend'))
report=[]
for obj in objects:
    bpy.ops.object.select_all(action='DESELECT');copy=obj.copy();copy.data=obj.data.copy();asset.objects.link(copy);copy.select_set(True);bpy.context.view_layer.objects.active=copy
    for v in copy.data.vertices:v.co.y=-v.co.y
    bm=bmesh.new();bm.from_mesh(copy.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(copy.data);bm.free()
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE');bpy.data.objects.remove(copy,do_unlink=True)
    obj.data.calc_loop_triangles();report.append(dict(mesh=obj.name,triangles=len(obj.data.loop_triangles),dimensions_metres=list(obj.dimensions)))
bpy.ops.object.select_all(action='DESELECT')
for obj in objects:obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(ROOT/'GreenlandsGuesthouse.glb'),export_format='GLB',use_selection=True)
bpy.ops.export_scene.fbx(filepath=str(ROOT/'GreenlandsGuesthouse.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
(ROOT/'guesthouse-manifest.json').write_text(json.dumps(dict(units='metres',front='-Y',main_shell_metres=[8,6],porch_extension_metres=1.55,ridge_metres=RIDGE,door_clear_metres=[2.1,2.3],reference_sha256=hashlib.sha256((ROOT/'Greenlands-Building-Family-v1.png').read_bytes()).hexdigest(),materials=MATERIAL_ROWS,meshes=report,status='Prototype; source render, exchange and game review pending'),indent=2))
manifest_path=ROOT/'guesthouse-manifest.json';manifest=json.loads(manifest_path.read_text());manifest.update(blender=bpy.app.version_string,room_descriptor_sha256=hashlib.sha256((ROOT/'guesthouse-rooms.json').read_bytes()).hexdigest(),method='Original measured geometry from generated family concept C; shared CC0 PBR; independently authored furniture/porch',assumptions=['Hero governs inconsistent generated rear view','Upper deck, paired dormer valleys and rear guarded stair flight are measured authoring inferences','Environment scenery only; no inn service or inventory implementation']);manifest_path.write_text(json.dumps(manifest,indent=2))
print('GUESTHOUSE_AUTHORED',json.dumps(report),flush=True)


