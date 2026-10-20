"""Silverbrook Gatehouse from docs/Game-Design/concept-art/Silverbrook/Gatehouse.png (BLD-GreenlandsGatehouse-01).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_gatehouse.py
Fills the town wall's 10 m gate gap (`wall.gate_gap` in Greenlands/silverbrook-town-layout.json) with a
vaulted gate block between two 8 x 8 x 12 m towers (`wall.tower`): 26 x 8 x 12 m overall. Round-arched
passage with gates swung open and the portcullis raised, crenellated parapet on a corbel table, carved
stag shield; towers with front buttresses, stag banners, machicolated crenellated heads and slate
pyramid roofs with a lit dormer. The towers' outer faces are flat at x=+-13 m so a WallSection end
butts against them; a door at the wall-walk level (z 5.40) opens onto it.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,lantern
import bpy
from mathutils import Matrix
scene=kit.reset(5301)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Gatehouse.png'
LAYOUT=json.loads((HERE.parents[1]/'Greenlands/silverbrook-town-layout.json').read_text())['wall']
asset=bpy.data.collections.new('SilverbrookGatehouse');scene.collection.children.link(asset)

M=dict(
    # identical arguments to WallSection, so the names are shared and the masonry matches the wall it joins
    stone=kit.scan('SB_WallSection_Masonry','rough_block_wall',dark=(.70,.74,.86),aged=.4),
    paving=kit.scan('SB_WallSection_Paving','medieval_blocks_03',dark=(.55,.58,.62),damp=False,aged=.25),
    void=kit.solid('SB_WallSection_LoopVoid',(.012,.012,.014),.95),
    # identical arguments to Cottage1
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    # own
    banner=kit.scan('SB_Gatehouse_Banner','denim_fabric_03',dark=(.12,.14,.30),damp=False,aged=.75),
    gilt=kit.solid('SB_Gatehouse_Gilt',(.80,.55,.18),.35,1.0))
kit.SCALE['SB_WallSection_Masonry']=20/6     # same 3.33 m block repeat as the wall
STONES=kit.patches('rough_block_wall',(.06,.035),dark=.7,frac=.01,std=.25)
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
def rect():return kit.rng.choice(STONES)
w0=lambda:(kit.rng.uniform(.05,.3),0)
G=dict(stone=Geo('SM_SBGatehouse_Masonry',asset),dressed=Geo('SM_SBGatehouse_Dressed',asset),walk=Geo('SM_SBGatehouse_Walk',asset),
       roof=Geo('SM_SBGatehouse_Roof',asset),timber=Geo('SM_SBGatehouse_Timber',asset),plaster=Geo('SM_SBGatehouse_Plaster',asset),detail=Geo('SM_SBGatehouse_Details',asset))
G.update(gateL=Geo('SM_SBGatehouse_GateL',asset),gateR=Geo('SM_SBGatehouse_GateR',asset),port=Geo('SM_SBGatehouse_Portcullis',asset))  # moving parts

# ---- measured layout (metres) -------------------------------------------------
TW,TD,TH=LAYOUT['tower']; GAP=LAYOUT['gate_gap']
SLOT=(GAP+2*TW,TD,TH)          # 26 x 8 x 12
CX=GAP/2+TW/2                  # tower centres at x=+-9
XO=GAP/2+TW                    # outer tower faces x=+-13 (wall butts here)
SY=TD/2-.5                     # shaft half-depth 3.5; heads overhang to TD/2 on corbels
SK=.30                         # skin depth of slits/doors
SHAFT=7.6                      # tower shaft top
HB,HP,HS,HM=7.75,8.5,8.6,9.3   # head floor, parapet sill, merlon base, merlon top (+.1 coping)
EX,EAVE,PEAK=2.7,8.4,11.33     # pyramid roof, eaves hidden behind the parapet (finial tip at PEAK+.65)
KR=(PEAK-EAVE)/EX
BY=3.0                         # gate block half-depth
AW,AS=5.0,3.0                  # arch clear width, spring (crown 5.5)
GW=6.85                        # gate block wall-walk core top (paving +.10)
PF,PB=-3.40,-2.90              # gate block front parapet (overhangs on corbels)
WALK_Z,WALK_Y=5.40,(-.45,1.15) # WallSection wall-walk (README)

def B(geo,x0,x1,y0,y1,z0,z1,mat,bevel=.012,rect=None,w=(0,0)):
    geo.box(((x0+x1)/2,(y0+y1)/2,(z0+z1)/2),(x1-x0,y1-y0,z1-z0),mat,bevel,rect=rect,w=w)

def course(geo,x0,x1,y0,y1,z0,z1,step=.62,along='x'):
    """Run of dressed blocks with 8 mm joints over a hidden backing (copings, string courses)."""
    lo,hi=(x0,x1) if along=='x' else (y0,y1);n=max(1,round((hi-lo)/step));d=(hi-lo)/n
    for i in range(n):
        a,b=lo+i*d+.004,lo+(i+1)*d-.004
        if along=='x':B(geo,a,b,y0,y1,z0,z1,M['stone'],.012,rect(),w0())
        else:B(geo,x0,x1,a,b,z0,z1,M['stone'],.012,rect(),w0())
    B(geo,x0+.03 if along=='y' else x0,x1-.03 if along=='y' else x1,y0+.03 if along=='x' else y0,y1-.03 if along=='x' else y1,z0+.02,z1-.02,M['stone'],0)

def skin(c,o,half,z0,z1,holes,th=SK):
    """Masonry face slab (outer face through c) with rectangular holes (a0,a1,z0,z1,void).
    Slits get a dark back plate; door holes are filled by door()."""
    P,R=kit.frame(c,o);xs=sorted({-half,half,*[max(-half,min(half,v)) for h in holes for v in h[:2]]})
    for a,b in zip(xs,xs[1:]):
        m=(a+b)/2;spans=sorted((h[2],h[3]) for h in holes if h[0]<m<h[1]);z=z0
        for s0,s1 in spans+[(z1,z1)]:
            if s0-z>.004:G['stone'].box(P(m,-th/2,(z+s0)/2),(b-a,th,s0-z),M['stone'],0,R)
            z=max(z,s1)
    for h in holes:
        if h[4]:G['dressed'].box(P((h[0]+h[1])/2,-th,(h[2]+h[3])/2),(h[1]-h[0]+.02,.04,h[3]-h[2]+.02),M['void'],0,R)

def slit(a,z0,z1):return (a-.06,a+.06,z0,z1,True)
def door_hole(a,w,z0,h):return (a-w/2-.2,a+w/2+.2,z0,z0+h+.3,False)
def door(c,o,a,w,z0,h):
    """Ledged plank door set .17 m back in a flush dressed surround (jambs + lintel fill the hole)."""
    P,R=kit.frame(c,o);D=Geo(f'SM_SBGatehouse_Door{len(DOORS)+1}',asset);G[D.name]=D   # each leaf is its own mesh
    DOORS.append(dict(geo=D,r=kit.UP.cross(Vector(o).normalized()),o=Vector(o).normalized()))
    for s in (-1,1):
        zz=z0;i=0
        while zz<z0+h-.01:
            hh=min(.42 if i%2 else .55,z0+h-zz);G['dressed'].box(P(a+s*(w/2+.1),-SK/2,zz+hh/2),(.2,SK,hh),M['stone'],.012,R,rect=rect(),w=w0());zz+=hh;i+=1
    G['dressed'].box(P(a,-SK/2,z0+h+.15),(w+.4,SK,.3),M['stone'],.012,R,rect=rect(),w=w0())
    D.box(P(a,-.2,z0+h/2),(w,.06,h),M['door'],.004,R)
    for z in (z0+.35,z0+h-.4):D.box(P(a-w*.08,-.164,z),(w*.8,.012,.06),M['iron'],.003,R)
    D.box(P(a+w*.32,-.16,z0+1.0),(.07,.02,.07),M['iron'],.004,R)
DOORS=[]

# ---- gate block: vaulted passage, dressed arch rings ------------------------------------
wall(G['stone'],M['stone'],(0,-BY,0),(0,-1,0),GAP/2+.1,GW,[(0,AW,0,AS+AW/2)],th=2*BY,arch=(0,AW,AS))
def arch_ring(y,out):
    P,R=kit.frame((0,y,AS),(0,out,0));o=Vector((0,out,0));rad=AW/2;z=-AS;i=0
    while z<-.01:   # jamb quoins from the ground to the spring
        hh=min(.5,-z);qw=.32 if i%2 else .48
        for s in (-1,1):G['dressed'].box(P(s*(rad-.012+qw/2),-.12,z+hh/2),(qw,.36,hh-.012),M['stone'],.012,R,rect=rect(),w=w0())
        z+=hh;i+=1
    n,depth=13,.6
    for k in range(n):
        t=math.pi*(k+.5)/n;rm=rad-.012+depth/2;radial=R@Vector((math.cos(t),0,math.sin(t)));tang=R@Vector((-math.sin(t),0,math.cos(t)))
        G['dressed'].box(P(rm*math.cos(t),-.12,rm*math.sin(t)),(math.pi*rm/n-.016,.36,depth),M['stone'],.012,Matrix((tang,o,radial)).transposed(),rect=rect(),w=w0())
arch_ring(-BY,-1);arch_ring(BY,1)
# corbel table, front parapet with slit merlons, rear parapet, wall-walk
for k in range(12):
    x=-GAP/2+.45+k*(GAP-.9)/11
    if abs(x)<1.3:continue          # the shield sits between the corbels
    B(G['dressed'],x-.15,x+.15,PF+.05,-BY+.05,GW-.5,GW,M['stone'],.01,rect(),w0())
    B(G['dressed'],x-.12,x+.12,PF+.2,-BY+.05,GW-.9,GW-.5,M['stone'],.01,rect(),w0())
course(G['dressed'],-GAP/2,GAP/2,PF,PB,GW,GW+.15)
B(G['stone'],-GAP/2,GAP/2,PF+.02,PB,GW+.15,7.35,M['stone'],0)
course(G['dressed'],-GAP/2,GAP/2,PF-.02,PB+.04,7.35,7.45,.55)
n,g=6,.55;m=(GAP-(n-1)*g)/n
for k in range(n):
    x0=-GAP/2+k*(m+g);xm=x0+m/2
    B(G['stone'],x0,xm-.05,PF+.02,PB,7.45,7.9,M['stone'],0);B(G['stone'],xm+.05,x0+m,PF+.02,PB,7.45,7.9,M['stone'],0)
    B(G['stone'],xm-.05,xm+.05,PF+.22,PB,7.45,7.9,M['stone'],0);B(G['dressed'],xm-.06,xm+.06,PF+.2,PF+.24,7.44,7.91,M['void'],0)
    course(G['dressed'],x0-.03,x0+m+.03,PF-.02,PB+.04,7.9,8.0,m+.06)
B(G['stone'],-GAP/2,GAP/2,BY-.45,BY,GW,7.7,M['stone'],0)           # inferred town-side parapet
course(G['dressed'],-GAP/2,GAP/2,BY-.49,BY,7.7,7.8)
B(G['walk'],-GAP/2,GAP/2,PB,BY-.45,GW,GW+.10,M['paving'],0)
# carved stag shield over the keystone
def stag(origin,right,up,normal,s,mat):
    o=Vector(origin)+Vector(normal)*.008;R_,U,N=Vector(right),Vector(up),Vector(normal);geo=G['detail']
    def p(a,z):return o+R_*(a*s)+U*(z*s)
    geo.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,mat)
    for sx in (-1,1):
        geo.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,mat)
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,mat,grain=False)
shield=[(-.45,6.4),(.45,6.4),(.45,6.0),(.32,5.75),(0,5.58),(-.32,5.75),(-.45,6.0)]
def shield_slab(y0,y1,scale,geo=G['dressed']):
    """Shield outline extruded y0(front)..y1; the face samples one dressed stone, edges world-mapped."""
    pts=[(x*scale,6.0+(z-6.0)*scale) for x,z in shield];u0,v0,du,dv=rect();zs=[z for _,z in pts];xs=[x for x,_ in pts]
    face=[Vector((x,y0,z)) for x,z in pts]
    if (face[1]-face[0]).cross(face[2]-face[0]).y>0:face.reverse()
    geo.poly(face,M['stone'],[(u0+(p.x-min(xs))/(max(xs)-min(xs))*du,v0+(p.z-min(zs))/(max(zs)-min(zs))*dv) for p in face],w0())
    geo.poly([Vector((p.x,y1,p.z)) for p in reversed(face)],M['stone'])
    for a,b in zip(face,face[1:]+face[:1]):geo.poly([a,Vector((a.x,y1,a.z)),Vector((b.x,y1,b.z)),b],M['stone'])
shield_slab(-BY-.26,-BY+.05,1.0)
rim=[Vector((x,-BY-.285,z)) for x,z in shield]
for a,b in zip(rim,rim[1:]+rim[:1]):G['dressed'].beam(a,b,.07,.05,M['stone'],grain=False)
stag((0,-BY-.26,6.0),(1,0,0),(0,0,1),(0,-1,0),.7,M['stone'])
# ---- open gates and raised portcullis (static) -----------------------------------------
HY,LW,LH,LT=-1.2,2.45,2.95,.12            # hinge line, leaf width/height/thickness
for s in (-1,1):
    GL=G['gateL' if s<0 else 'gateR']    # each leaf is its own mesh: it swings on its pintles
    xo=s*(AW/2-.02);xi=xo-s*LT;xc=(xo+xi)/2;nb=10;bw=LW/nb    # leaf swung back flat against the passage wall
    for i in range(nb):GL.box((xc,HY+(i+.5)*bw,LH/2),(LT,bw-.008,LH),M['door'],.006)
    GL.box((xc,HY+LW/2,LH/2),(LT-.04,LW-.03,LH-.02),M['door'],0)
    xf=xi-s*.025                                                # ledges, brace and iron on the passage face
    for z in (.45,1.5,2.55):
        GL.box((xf,HY+LW/2,z),(.05,LW-.1,.18),M['door'],.006,grain=True)
        GL.box((xf-s*.031,HY+LW/2,z),(.012,LW-.06,.07),M['iron'],.003)
        for y in (HY+.15,HY+LW-.15):GL.box((xf-s*.04,y,z),(.01,.05,.05),M['iron'],.003)
    for z0,z1 in ((.55,1.4),(1.6,2.45)):GL.beam((xf,HY+.15,z0),(xf,HY+LW-.15,z1),.05,.14,M['door'])
    for z in (.45,2.55):G['detail'].box((s*(AW/2-.04),HY-.04,z),(.09,.1,.14),M['iron'],.004)   # pintles into the wall
    r0=Vector((xf-s*.06,HY+LW-.45,1.2))
    for k in range(10):t=math.tau*k/10;GL.box(r0+Vector((0,.07*math.cos(t),.07*math.sin(t))),(.014,.014,.045),M['iron'],0,Matrix.Rotation(t,3,'X'))
    GL.box(r0+Vector((s*.01,0,.08)),(.02,.08,.08),M['iron'],.004)
PY,PZ=-2.2,4.25                            # portcullis groove, raised bottom rail (spike tips 4.0 m)
for k in range(11):
    x=-2.25+k*.45;G['port'].box((x,PY,(PZ+6.2)/2),(.1,.1,6.2-PZ),M['iron'],.006)
    G['port'].prism([(x-.06,PY,PZ),(x+.06,PY,PZ),(x,PY,PZ-.25)],(0,1,0),.08,M['iron'])
for z in (4.4,4.85,5.3,5.75):G['port'].box((0,PY+.09,z),(5.3,.08,.1),M['iron'],.006)
for s in (-1,1):lantern(G,M,(s*3.45,-BY-.42,3.75),arm=((s*3.45,-BY-.03,3.75),(0,-1,0)))
# ---- towers --------------------------------------------------------------------------
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.35+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def fleur(c,s):
    c=Vector(c);p=lambda a,z:c+Vector((a*s,-.01,z*s))
    for pts in ([(0,-.05),(.09,.12),(0,.45),(-.09,.12)],[(.06,.02),(.2,.06),(.3,.2),(.28,.02),(.18,-.1),(.07,-.06)],[(-.06,.02),(-.07,-.06),(-.18,-.1),(-.28,.02),(-.3,.2),(-.2,.06)],[(-.05,-.08),(.05,-.08),(.1,-.3),(-.1,-.3)]):
        G['detail'].prism([p(a,z) for a,z in pts],(0,1,0),.02,M['gilt'])
    G['detail'].box(c+Vector((0,-.012,-.07*s)),(.32*s,.024,.06*s),M['gilt'],.003)
def tower(s):
    cx=s*CX;xi,xo=cx-s*TW/2,cx+s*TW/2;H=TW/2
    # core, front/rear/outer skins with slits and doors, flat inner side against the gate block
    B(G['stone'],min(xi,cx+s*(H-SK)),max(xi,cx+s*(H-SK)),-SY+SK,SY-SK,0,SHAFT,M['stone'],0)
    sl=[slit(a,z0,z0+1.1) for a in (-2.2,2.2) for z0 in (1.8,4.6)]
    skin((cx,-SY,0),(0,-1,0),H,0,SHAFT,sl)
    skin((cx,SY,0),(0,1,0),H,0,SHAFT,sl+[door_hole(0,1.1,0,2.3)]);door((cx,SY,0),(0,1,0),0,1.1,0,2.3)
    a_walk=s*(sum(WALK_Y)/2)           # frame 'a' on the outer face runs along s*y
    skin((xo,0,0),(s,0,0),SY-SK,0,SHAFT,[slit(a,z0,z0+1.1) for a in (-2.5,2.5) for z0 in (1.8,4.6)]+[door_hole(a_walk,1.0,WALK_Z,1.9)])
    door((xo,0,0),(s,0,0),a_walk,1.0,WALK_Z,1.9)
    # front plinth and stepped buttresses at both corners
    B(G['stone'],cx-H+.9,cx+H-.9,-SY-.15,-SY,0,.7,M['stone'],.01)
    for x0 in (cx-H,cx+H-.9):
        B(G['stone'],x0,x0+.9,-TD/2,-SY,0,3.2,M['stone'],0);B(G['dressed'],x0+.02,x0+.88,-TD/2+.02,-SY,3.2,3.3,M['stone'],.012,rect(),w0())
        B(G['stone'],x0+.08,x0+.82,-SY-.32,-SY,3.3,4.4,M['stone'],0);B(G['dressed'],x0+.1,x0+.8,-SY-.34,-SY,4.4,4.5,M['stone'],.012,rect(),w0())
    # machicolated head: corbels front and rear, overhanging floor slab, crenellated parapet
    for k in range(9):
        x=cx-3.6+k*.9
        for sy in (-1,1):
            y0,y1=sorted((sy*(SY-.05),sy*(TD/2-.02)));B(G['dressed'],x-.16,x+.16,y0,y1,7.25,SHAFT,M['stone'],.01,rect(),w0())
            y0,y1=sorted((sy*(SY-.05),sy*(SY+.28)));B(G['dressed'],x-.12,x+.12,y0,y1,6.9,7.25,M['stone'],.01,rect(),w0())
    B(G['stone'],cx-H,cx+H,-TD/2,TD/2,SHAFT,HB,M['stone'],0)
    T,I=.45,TD/2-.45                   # parapet thickness, inner face
    for sy in (-1,1):
        y0,y1=sorted((sy*TD/2,sy*I));B(G['stone'],cx-H,cx+H,y0,y1,HB,HP,M['stone'],0)
        course(G['dressed'],cx-H,cx+H,y0,y1,HP,HS,.6)
    for sx in (-1,1):
        x0,x1=sorted((cx+sx*H,cx+sx*I));B(G['stone'],x0,x1,-I,I,HB,HP,M['stone'],0)
        course(G['dressed'],x0,x1,-I,I,HP,HS,.6,'y')
    nm,gp=5,.5;mw=(TW-(nm-1)*gp)/nm
    for k in range(nm):
        a0=-H+k*(mw+gp);a1=a0+mw
        for sy in (-1,1):
            y0,y1=sorted((sy*TD/2,sy*I));B(G['stone'],cx+a0,cx+a1,y0,y1,HS,HM,M['stone'],0);course(G['dressed'],cx+a0,cx+a1,y0,y1,HM,HM+.1,mw)
        b0,b1=max(a0,-I),min(a1,I)
        if b1-b0>.05:
            for sx in (-1,1):
                x0,x1=sorted((cx+sx*H,cx+sx*I));B(G['stone'],x0,x1,b0,b1,HS,HM,M['stone'],0);course(G['dressed'],x0,x1,b0,b1,HM,HM+.1,b1-b0,'y')
    # roof base inside the parapet walk, paved walk ring
    C=EX-.15;B(G['stone'],cx-C,cx+C,-C,C,HB,EAVE,M['stone'],0)
    for sy in (-1,1):y0,y1=sorted((sy*C,sy*I));B(G['walk'],cx-I,cx+I,y0,y1,HB,HB+.1,M['paving'],0)
    for sx in (-1,1):x0,x1=sorted((cx+sx*C,cx+sx*I));B(G['walk'],x0,x1,-C,C,HB,HB+.1,M['paving'],0)
    # slate pyramid with a front dormer (after Cottage1's dormer)
    XD,DW,DH,DE,DR=cx,.45,.6,10.25,10.75;KD=(DR-DE)/DH;YD=-(PEAK-9.55)/KR
    main_z=lambda x,y:PEAK-max(abs(x-cx),abs(y))*KR;dormer_z=lambda x:DR-abs(x-XD)*KD
    in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
    under_main=lambda p:p.z<main_z(p.x,p.y)-.01
    def plane(O,e,toward,L,lo,hi,covered):
        u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
        deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
    L=math.hypot(EX,PEAK-EAVE)
    plane((cx,-EX,EAVE),(1,0,0),(0,EX,PEAK-EAVE),L,lerp(-EX,0,L),lerp(EX,0,L),[in_dormer])
    plane((cx,EX,EAVE),(-1,0,0),(0,-EX,PEAK-EAVE),L,lerp(-EX,0,L),lerp(EX,0,L),[])
    plane((cx+EX,0,EAVE),(0,1,0),(-EX,0,PEAK-EAVE),L,lerp(-EX,0,L),lerp(EX,0,L),[])
    plane((cx-EX,0,EAVE),(0,-1,0),(EX,0,PEAK-EAVE),L,lerp(-EX,0,L),lerp(EX,0,L),[])
    def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
    def capped(a,b,n):
        a,b=Vector(a),Vector(b)
        for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
    for sx in (-1,1):
        for sy in (-1,1):capped((cx+sx*EX,sy*EX,EAVE+.05),(cx,0,PEAK+.05),12)
    LD=math.hypot(DH,DR-DE);yr=-(PEAK-DR)/KR
    plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:-yr,lambda t:-YD+.16,[under_main])
    plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:yr,[under_main])
    capped((XD,YD-.16,DR+.04),(XD,yr,DR+.04),4)
    for t in (-1,1):G['timber'].beam((XD+t*(DH+.03),YD-.18,DE-.05),(XD-t*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
    DB=main_z(XD,YD)-.1
    wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.58,.13,.71)],th=.12)
    G['plaster'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dormer_z(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dormer_z(XD+DW)-.04)],(0,1,0),.12,M['plaster'])
    ym=-(PEAK-(dormer_z(XD+DW)-.04))/KR
    for t in (-1,1):G['plaster'].prism([(XD+t*DW,YD,DB),(XD+t*DW,YD,dormer_z(XD+DW)-.04),(XD+t*DW,ym,dormer_z(XD+DW)-.04)],(1,0,0),.08,M['plaster'])
    for t in (-1,1):G['timber'].box((XD+t*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
    G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
    # kit.box checks winding with a float32 signed volume, which loses the 9 mm lead cames ~14 m from the
    # origin: build the casement at the origin and move it into place.
    T={k:Geo(k,asset) for k in ('timber','detail')};window(T,M,(0,0,0),(0,-1,0),.4,.4,shutters=False,wall_th=.12,band=band())
    for k,t in T.items():
        g=G[k];base=len(g.v);g.v+=[v+Vector((XD,YD,DB+.22)) for v in t.v];g.f+=[tuple(i+base for i in f) for f in t.f]
        g.uv+=t.uv;g.mat+=t.mat;g.w+=t.w;g.smooth+=t.smooth
    G['detail'].vessel((cx,0,PEAK),[(.14,0),(.12,.1),(.05,.2),(.08,.3),(.03,.4),(.014,.42),(.014,.56),(0,.65)],M['iron'],8)
    # stag banner on an iron rod between the buttresses
    AY=-SY-.26;top,low,tip=6.7,3.5,2.95
    outline=[(cx-.85,top),(cx+.85,top),(cx+.85,low),(cx,tip),(cx-.85,low)]
    G['detail'].prism([(x,AY,z) for x,z in outline],(0,1,0),.02,M['banner'])
    G['detail'].beam((cx-1.05,AY,top+.04),(cx+1.05,AY,top+.04),.04,.04,M['iron'],grain=False)
    for dx in (-.95,.95):
        G['detail'].beam((cx+dx,-SY+.02,top+.04),(cx+dx,AY-.03,top+.04),.05,.05,M['iron'],grain=False)
        G['detail'].beam((cx+dx,-SY+.02,top-.4),(cx+dx,AY+.02,top+.02),.035,.035,M['iron'],grain=False)
    n=Vector((0,-.011,0))
    for a,b in zip(outline,outline[1:]+outline[:1]):G['detail'].beam(Vector((a[0],AY,a[1]))+n,Vector((b[0],AY,b[1]))+n,.02,.05,M['gilt'],grain=False)
    stag((cx,AY-.011,5.55),(1,0,0),(0,0,1),(0,-1,0),.9,M['gilt'])
    fleur((cx,AY-.005,4.05),1.0)
for s in (-1,1):tower(s)
# ---- checks, export, preview scene, manifest -------------------------------------------
objs=[g.object() for g in G.values()]
pts=[o.matrix_world@v.co for o in objs for v in o.data.vertices]
lo=[min(p[k] for p in pts) for k in range(3)];hi=[max(p[k] for p in pts) for k in range(3)]
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)) and abs(lo[0]+XO)<1e-4 and abs(hi[0]-XO)<1e-4 and abs(lo[1]+TD/2)<.01 and lo[2]>-1e-4,f'slot {SLOT}: {lo} {hi}'
# Gate passage open: nothing inside a 4.4 m wide x 3.9 m high clear way through the whole depth.
CLEAR=2.2
assert not [p for p in pts if abs(p.x)<CLEAR and .01<p.z<3.9 and abs(p.y)<TD/2],'gate passage obstructed'
# Wall joint: the outer faces are the slot ends (asserted above); the walk-door threshold is level with the wall-walk.
assert any(abs(p.z-WALK_Z)<1e-4 and abs(abs(p.x)-XO)<1e-4 for p in G['stone'].v),'no threshold at the wall-walk height'
OUT=HERE/'SilverbrookGatehouse'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
# Gate leaves swing on their pintles (closing: 90 deg toward the passage centre); the portcullis drops 4 m to the ground.
ob={o.name:o for o in objs};HX=AW/2-.02
# Tower doors (town-side ground doors, wall-walk doors): hinge at the leaf's -r edge, bottom, inner face; open 90 deg inward (-o).
DOOR_PARTS=[]
for k,d in enumerate(DOORS,1):
    obj=ob[d['geo'].name];vs=[obj.matrix_world@v.co for v in obj.data.vertices];r,o=d['r'],d['o']
    pivot=r*min(v.dot(r) for v in vs)+o*min(v.dot(o) for v in vs)+kit.UP*min(v.z for v in vs)
    travel=90 if kit.UP.cross(r).dot(o)<0 else -90
    q=Matrix.Rotation(math.radians(travel),3,'Z');sw=[pivot+q@(v-pivot) for v in vs]
    assert max(v.dot(o) for v in sw)<=pivot.dot(o)+.07 and min(v.dot(o) for v in sw)<pivot.dot(o)-.9 and min(v.dot(r) for v in sw)>=pivot.dot(r)-1e-3,f'Door{k} does not swing inward'
    print('GATEHOUSE_DOOR',k,[round(x,3) for x in pivot],travel,'open leaf reaches',round(pivot.dot(o)-min(v.dot(o) for v in sw),3),'m inside')
    DOOR_PARTS.append((obj,f'Door{k}',tuple(round(x,4) for x in pivot),(0,0,1),'swing',travel,'closed'))
PARTS=kit.export_parts([(ob['SM_SBGatehouse_GateL'],'GateL',(-HX,HY,0),(0,0,1),'swing',-90,'open'),(ob['SM_SBGatehouse_GateR'],'GateR',(HX,HY,0),(0,0,1),'swing',90,'open'),
    (ob['SM_SBGatehouse_Portcullis'],'Portcullis',(0,PY,PZ-.25),(0,0,-1),'slide',round(PZ-.25,3),'open')]+DOOR_PARTS,str(OUT),'SM_SilverbrookGatehouse')
for o in bpy.data.objects:o.select_set(o in objs and o.name not in ('SM_SBGatehouse_GateL','SM_SBGatehouse_GateR','SM_SBGatehouse_Portcullis',*[d['geo'].name for d in DOORS]))
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
# Preview only: the neighbouring WallSection segments butted against both towers.
wall_blend=HERE.parent/'WallSection/SilverbrookWallSection.blend'
if wall_blend.exists():
    with bpy.data.libraries.load(str(wall_blend)) as (src,dst):dst.objects=[n for n in src.objects if n.startswith('SM_SBWallSection')]
    for o in dst.objects:
        for s in (-1,1):n=o.copy();n.location.x=s*(XO+10);pre.objects.link(n)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-60,-60,0),(60,-60,0),(60,60,0),(-60,60,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-17,-29,6.5),(85,0,-31),lens=30);cam('Front',(0,-60,6),(90,0,0),28);cam('Side',(-60,0,6),(90,0,-90),14);cam('Rear',(0,60,6),(90,0,180),28)
cam('Passage',(0,-13,1.7),(88,0,0),lens=26);cam('Joint',(21,-6,11),(55,0,58),lens=28)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Gatehouse-reference.png')
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=400000,f'{tris} triangles exceeds the 400k landmark budget'
manifest=dict(asset='SilverbrookGatehouse',asset_id='BLD-GreenlandsGatehouse-01',reference=dict(file='Gatehouse-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_gatehouse.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(h-l,3) for l,h in zip(lo,hi)],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookGatehouse',parts=PARTS,slot_m=list(SLOT),
    slot_source='Greenlands/silverbrook-town-layout.json wall.gate_gap (10) + 2 x wall.tower (8 x 8 x 12)',
    wall_joint=dict(wall_end_faces_x=[-XO,XO],walk_top_z=WALK_Z,walk_y=list(WALK_Y),walk_door='closed, sill at walk_top_z, y %.2f..%.2f'%(sum(WALK_Y)/2-.5,sum(WALK_Y)/2+.5),masonry_uv_repeat_m=round(20/6,4)),
    passage=dict(clear_width_m=2*CLEAR,clear_height_m=3.9,arch_width_m=AW,arch_crown_m=AS+AW/2,portcullis_tips_m=PZ-.25,gates='open, swung back against the passage walls'),
    outputs=['SilverbrookGatehouse.blend','SilverbrookGatehouse.glb','SilverbrookGatehouse.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('GATEHOUSE_BUILT',manifest['bounds_m'],tris)
