"""Silverbrook Townhouse 2 from docs/Game-Design/concept-art/Silverbrook/Townhouse-2.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse2.py
Original procedural geometry interpreting the elevation (6 m wide; the drawn 10 m height
is compressed to the 9 m slot): narrow two-storey townhouse, pale squared-stone ground
storey with a pointed-arch door up three steps, jettied half-timbered left bay, a
two-storey canted oriel under a steep front-gabled wing with a spire finial, hipped
slate main roof with a dormer, side stack with two pots, gutters and downpipes, an
iron bracket sign with a stag banner and a wall lantern.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,tile_plane,deck,wall,window,lantern,frame
import bpy,numpy as np
scene=kit.reset(4208)
_prism=Geo.prism
def _checked_prism(self,points,axis,thickness,mat,**kw):
    """Every prism (ours and the kit's) must close with positive volume: Unreal culls back faces."""
    n0=len(self.f);_prism(self,points,axis,thickness,mat,**kw)
    assert sum(self.v[f[0]].dot(self.v[f[j]].cross(self.v[f[j+1]])) for f in self.f[n0:] for j in range(1,len(f)-1))>0,'inside-out prism'
Geo.prism=_checked_prism
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Townhouse-2.png'
asset=bpy.data.collections.new('SilverbrookTownhouse2');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Townhouse2_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_Townhouse2_Banner','denim_fabric_03',dark=(.16,.19,.34),normal=.6,damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_Townhouse2_Gilt',(.78,.62,.36),.4,.9))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_05',(.06,.045),dark=.7,frac=.01,std=.22)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBTownhouse2_Masonry',asset),plaster=Geo('SM_SBTownhouse2_Plaster',asset),timber=Geo('SM_SBTownhouse2_Timber',asset),roof=Geo('SM_SBTownhouse2_Roof',asset),detail=Geo('SM_SBTownhouse2_Details',asset))
# ---- measured layout (metres; elevation ~95 px/m across, heights scaled ~0.85 into the slot) ----
SLOT=(8,7,9)                   # town-layout slot w x d x h (concept README 'Fits')
W2,D2=2.8,2.4                  # ground-storey outer wall half-extents
JZ,YF=2.9,-2.7                 # jetty height; jettied upper front face (left bay and wing gable)
EAVE,RIDGE=5.6,8.25            # hipped main roof (hip on the left, the wing on the right)
EX,EY,RX=3.0,2.95,1.6
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
XG,GW,GH,GE,GR=1.3,1.5,1.7,5.6,8.6        # front-gabled wing: centre x, wall half, roof half, eave, apex
KG=(GR-GE)/GH;GY,GB=YF-.35,D2+.35         # wing roof front / rear edges
XD,DH,DW,DE,DR=-1.45,.5,.4,7.0,7.55       # dormer
YD=-(RIDGE-6.35)/KF;KD=(DR-DE)/DH
XC,YC=-2.05,0.0                # stack on the left hip
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return GR-abs(x-XG)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
in_gable=lambda p:abs(p.x-XG)<GH+.04 and p.z<gable_z(p.x)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.42 and abs(p.y-YC)<.42
under_main=lambda p:p.x<XG+.05 and p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
def post(x,y,z0,z1,o,s=.2):G['timber'].box((x,y,(z0+z1)/2),(s if o=='y' else .12,.12 if o=='y' else s,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
# ---- main hipped roof (its right end runs into the wing roof) ----------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lambda t:XG,[in_gable,in_dormer,in_stack])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lambda t:-XG,lerp(EX,RX,LF),[in_gable,in_stack])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
XV=XG-(GR-RIDGE)/KG            # where the main ridge dives under the wing roof
capped((-RX,0,RIDGE+.05),(XV,0,RIDGE+.05),11)
for sy in (-1,1):capped((-EX,sy*EY,EAVE+.05),(-RX,0,RIDGE+.05),11)
for sy in (-1,1):G['timber'].beam((-EX,sy*(EY+.02),EAVE-.06),(XG-GH,sy*(EY+.02),EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((-EX-.02,-EY,EAVE-.06),(-EX-.02,EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- wing roof: steep gable front to back, taller than the main ridge ---------------
LG=math.hypot(GH,GR-GE)
plane((XG-GH,0,GE),(0,-1,0),(GH,0,GR-GE),LG,lambda t:-GB,lambda t:-GY,[under_main])
plane((XG+GH,0,GE),(0,1,0),(-GH,0,GR-GE),LG,lambda t:GY,lambda t:GB,[])
capped((XG,GY,GR+.05),(XG,GB,GR+.05),24)
for y in (GY-.04,GB+.04):
    for s in (-1,1):G['timber'].beam((XG+s*(GH+.05),y,GE-.08),(XG-s*.16,y,GR+.16*KG+.03),.08,.24,M['timber'],band=band())
G['timber'].beam((XG+GH+.02,GY,GE-.06),(XG+GH+.02,GB,GE-.06),.04,.2,M['timber'],band=band())
# spire finial (the concept's tall iron spike, kept under the 9 m slot)
G['timber'].box((XG,GY+.02,GR+.03),(.15,.15,.3),M['timber'],.02,grain=True,band=band())
G['timber'].box((XG,GY+.02,GR+.2),(.21,.21,.05),M['timber'],.02,grain=True,band=band())
G['detail'].beam((XG,GY+.02,GR+.22),(XG,GY+.02,8.97),.05,.05,M['iron'],grain=False)
G['detail'].box((XG,GY+.02,GR+.3),(.1,.1,.08),M['iron'],.01)
# ---- dormer on the main front slope ------------------------------------------------
LD=math.hypot(DH,DR-DE)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:.7,lambda t:-YD+.16,[under_main])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-.7,[under_main])
capped((XD,YD-.16,DR+.04),(XD,-.8,DR+.04),4)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
DB=main_z(XD,YD)-.1
wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.58,.13,.71)],th=.12)
G['plaster'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dormer_z(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dormer_z(XD+DW)-.04)],(0,1,0),.12,M['plaster'])
ym=-(RIDGE-(dormer_z(XD+DW)-.04))/KF
for s in (-1,1):G['plaster'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,dormer_z(XD+DW)-.04),(XD+s*DW,ym,dormer_z(XD+DW)-.04)],(1,0,0),.08,M['plaster'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
window(G,M,(XD,YD,DB+.22),(0,-1,0),.4,.4,shutters=False,wall_th=.12,band=band())
# ---- stack -----------------------------------------------------------------------
G['stone'].box((XC,YC,7.5),(.72,.72,1.8),M['stone'],.01)
G['stone'].box((XC,YC,8.3),(.78,.78,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,8.44),(.82,.82,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.17,.17):G['detail'].vessel((XC+dx,YC,8.5),[(.1,0),(.095,.28),(.105,.34),(.12,.38),(.09,.38),(.078,.32),(0,.32)],M['pot'])
# ---- ground storey (squared stone) -------------------------------------------------
STEP,XDOOR,DOOR,DS=.45,-1.35,.85,1.25     # sill = top step; door centre, width, spring above sill
APEX=DS+DOOR*math.sqrt(3)/2               # equilateral pointed arch
WZ,WH,UZ,UH=.9,1.1,3.6,1.0                # ground / upper window bottoms and heights
def hole(a,w,z,h,base=0):return (a,w+.18,z-.09-base,z+h+.09-base)
wall(G['stone'],M['stone'],(0,-D2,0),(0,-1,0),W2,JZ,[(XDOOR,DOOR,STEP,STEP+APEX)])
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,JZ,[hole(1.5,.8,WZ,WH),hole(-XG,.8,WZ,WH)])
wall(G['stone'],M['stone'],(W2,0,0),(1,0,0),D2-.3,JZ,[hole(.9,.7,WZ,WH)])
wall(G['stone'],M['stone'],(-W2,0,0),(-1,0,0),D2-.3,JZ,[hole(-.9,.7,WZ,WH)])
for a0,a1 in ((-W2-.04,XDOOR-.65),(XDOOR+.65,.1),(2.5,W2+.04)):G['stone'].box(((a0+a1)/2,-D2-.02,.22),(a1-a0,.1,.44),M['stone'],.01)
G['stone'].box((0,D2+.02,.22),(2*W2+.08,.1,.44),M['stone'],.01)
for s in (-1,1):G['stone'].box((s*(W2+.02),0,.22),(.1,2*D2,.44),M['stone'],.01)
for k in range(3):G['stone'].box((XDOOR,-D2-.15-.3*k,(3-k)*.075),(1.3,.3,(3-k)*.15),M['stone'],.015)
def pointed_door(c,o,w,spring,wall_th=.3,depth=.22):
    """Pointed (equilateral) arch plank door with dressed jambs and voussoirs. The wall hole is a
    rectangle up to the apex; the spandrels are filled here with convex strips (kit.wall's round
    arch fill does not apply, and concave prisms can come out inside out)."""
    P,R=frame(c,o);r=w/2;o_=Vector(o).normalized();apex=spring+w*math.sqrt(3)/2
    def arc(side,n):return [(-side*r+side*w*math.cos(t),spring+w*math.sin(t)) for t in np.linspace(0,math.pi/3,n)]
    right,left=arc(1,9),arc(-1,9)
    L=Geo(f'SM_SBTownhouse2_Door{len(kit.DOORS)+1}',asset);G[L.name]=L   # the leaf opens: hinge on the left jamb seen from outside
    kit.DOORS.append(dict(geo=L,r=(R@Vector((1,0,0))).normalized(),o=o_,width=w))
    L.prism([P(a,-.2,z) for a,z in [(-r,0),(r,0)]+right+left[::-1][1:]],o,.07,M['door'])
    for z in (.35,spring-.05):L.box(P(-r*.15,-.16,z),(w*.8,.012,.05),M['iron'],.004,R)
    L.box(P(r*.55,-.16,1.0),(.07,.012,.07),M['iron'],.004,R)
    for k in range(10):
        t=math.tau*k/10;L.box(P(r*.55+.06*math.cos(t),-.148,.92+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
    for pts in (right,left):
        for (a0,z0),(a1,z1) in zip(pts,pts[1:]):
            quad=[(a0,z0),(a1,z1)]+([] if apex-z1<.005 else [(a1,apex)])+[(a0,apex)]
            G['stone'].prism([P(a,-wall_th/2,z) for a,z in quad],o,wall_th,M['stone'])
    z=0;i=0
    while z<spring-.05:
        hgt=min(.31,spring-z);qw=.28 if i%2 else .4
        for side in (-1,1):G['stone'].box(P(side*(r-.012+qw/2),-wall_th/2+.025,z+hgt/2),(qw,wall_th+.05,hgt-.012),M['ashlar'],.012,R,rect=kit.rng.choice(STONES))
        z+=hgt;i+=1
    n=5;rm=w-.012+depth/2
    for side in (1,-1):
        for k in range(n):
            t=math.pi/3*(k+.5)/n;phi=t if side==1 else math.pi-t
            radial=R@Vector((math.cos(phi),0,math.sin(phi)));tang=R@Vector((-math.sin(phi),0,math.cos(phi)))
            G['stone'].box(P(-side*r+rm*math.cos(phi),-wall_th/2+.025,spring+rm*math.sin(phi)),(rm*math.pi/3/n-.014,wall_th+.05,depth),M['ashlar'],.012,Matrix((tang,o_,radial)).transposed(),rect=kit.rng.choice(STONES))
pointed_door((XDOOR,-D2,STEP),(0,-1,0),DOOR,DS)
# ---- upper storey (lime plaster) -----------------------------------------------------
PLATE=main_z(0,YF)-.04;PLATE_R=main_z(0,D2)-.04
XU=-1.5                                   # left upper window centre (front and rear)
wall(G['plaster'],M['plaster'],((-W2-.2)/2,YF,JZ),(0,-1,0),(W2-.2)/2,PLATE-JZ,[hole(XU-(-W2-.2)/2,.85,UZ,UH,JZ)])
wall(G['plaster'],M['plaster'],(XG,-D2,JZ),(0,-1,0),GW,5.45-JZ)                    # wing wall behind the oriel
wall(G['plaster'],M['plaster'],((-W2-.2)/2,D2,JZ),(0,1,0),(W2-.2)/2,PLATE_R-JZ,[hole(-(XU-(-W2-.2)/2),.8,UZ,UH,JZ)])
GC=gable_z(XG+GW)-.05
wall(G['plaster'],M['plaster'],(XG,D2,JZ),(0,1,0),GW,GC-JZ,[hole(0,.8,UZ,UH,JZ)])
G['plaster'].prism([(XG-GW,D2-.15,GC),(XG+GW,D2-.15,GC),(XG,D2-.15,GR-.05)],(0,1,0),.3,M['plaster'])
PLATE_W=GE+(GH-GW)*KG-.05
wall(G['plaster'],M['plaster'],(W2,0,JZ),(1,0,0),D2-.3,PLATE_W-JZ,[hole(.9,.7,UZ,UH,JZ)])
YS0,YS1=-D2,D2-.3;ZS=main_z(W2,0)-.02            # the left hip is flat-topped over this wall
wall(G['plaster'],M['plaster'],(-W2,(YS0+YS1)/2,JZ),(-1,0,0),(YS1-YS0)/2,ZS-JZ,[hole(-(.9-(YS0+YS1)/2),.7,UZ,UH,JZ)])
# jettied wing gable over the oriel: bressumer, then the plaster gable with its window
GB0=5.45;GZ,GWW,GWH=6.15,.7,.8
G['timber'].box((XG,(YF-.04-D2)/2,GB0+.1),(2*GW+.1,-D2-YF+.04,.2),M['timber'],.015,grain=True,band=band())
OB,OT,OH=GZ-.09,GZ+GWH+.09,(GWW+.18)/2;G0=GB0+.2
def gx(z):return (GR-.05-z)/KG
for piece in ([(-GW,G0),(GW,G0),(GW,GC),(gx(OB),OB),(-gx(OB),OB),(-GW,GC)],
              [(-gx(OB),OB),(-OH,OB),(-OH,OT),(-gx(OT),OT)],[(OH,OB),(gx(OB),OB),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,GR-.05)]):
    G['plaster'].prism([(XG+x,YF+.15,z) for x,z in piece],(0,1,0),.3,M['plaster'])
# ---- timber framing ------------------------------------------------------------------
G['timber'].box((-1.525,(YF-.04-D2)/2,JZ-.1),(2.65,-D2-YF+.04,.2),M['timber'],.015,grain=True,band=band())  # jetty bressumer
for x in (-2.65,-1.95,-1.05,-.35):        # carved jetty corbels
    G['timber'].box((x,-D2-.15,JZ-.36),(.14,.3,.32),M['timber'],.012,grain=True,band=band())
    G['timber'].box((x,-D2-.08,JZ-.62),(.12,.16,.2),M['timber'],.012,grain=True,band=band())
y=YF-.03
for x in (-2.74,-.26):post(x,y,JZ,PLATE,'y',.16)
for x in (XU-.6,XU+.6):post(x,y,JZ,PLATE,'y',.14)
rail((-W2,y,UZ-.24),(-.2,y,UZ-.24),.12);rail((-W2,y,UZ+UH+.18),(-.2,y,UZ+UH+.18),.12);rail((-W2,y,PLATE-.08),(-.2,y,PLATE-.08),.18)
post(XU,y,UZ+UH+.25,PLATE-.17,'y',.12)
for x0,x1 in ((-2.7,-2.15),(-.3,-.85)):rail((x0,y,JZ+.08),(x1,y,UZ-.3),.1)
for x in (-.12,2.72):post(x,-D2-.03,JZ,GB0,'y',.16)
y=YF-.03                                  # gable framing
for s in (-1,1):post(XG+s*(GW-.08),y,G0,gable_z(XG+GW-.08)-.05,'y',.16)
zc=OT+.12;rail((XG-gx(zc)+.05,y,zc),(XG+gx(zc)-.05,y,zc),.14);post(XG,y,zc+.07,GR-.08,'y',.14)
for s in (-1,1):                          # sides
    x=s*(W2+.03);zt=PLATE_W if s>0 else ZS
    for yy in (-2.3,0,2.3):post(x,yy,JZ,zt-.02,'x',.16)
    rail((x,-D2,JZ+.05),(x,D2,JZ+.05),.18);rail((x,-D2,UZ+UH+.18),(x,D2,UZ+UH+.18),.12)
y=D2+.03                                  # rear
for x in (-2.7,-.2,2.7):post(x,y,JZ,PLATE_R if x<0 else GC,'y',.16)
rail((-W2,y,JZ+.05),(W2,y,JZ+.05),.18);rail((-W2,y,UZ+UH+.18),(W2,y,UZ+UH+.18),.12);rail((-W2,y,PLATE_R-.08),(-.2,y,PLATE_R-.08),.18)
rail((XG-GW,y,GC-.06),(XG+GW,y,GC-.06),.16);post(XG,y,GC,GR-.1,'y',.14)
# ---- two-storey canted oriel under the wing gable ------------------------------------
Y0,BC,BP,OV=-D2,.65,.53,.15     # wall line, centre-face half width, projection, roof overhang
V=[Vector(v) for v in ((XG-BC-BP,Y0,0),(XG-BC,Y0-BP,0),(XG+BC,Y0-BP,0),(XG+BC+BP,Y0,0))]
BZ0,BZ1,BW1,BH1,BW2,BH2,BTOP=.3,2.75,1.05,1.25,3.2,1.25,4.95
pl=[V[0]+Vector((-.071,0,0)),V[1]+Vector((-.021,-.05,0)),V[2]+Vector((.021,-.05,0)),V[3]+Vector((.071,0,0))]
G['stone'].prism([p+Vector((0,0,BZ0/2)) for p in pl],(0,0,1),BZ0,M['stone'])
for i in range(3):
    A,B=V[i],V[i+1];mid=(A+B)/2;d=(B-A).normalized();o=Vector((d.y,-d.x,0));half=(B-A).length/2
    ww=.9 if i==1 else .44;P,R=frame(mid,o)
    wall(G['timber'],M['timber'],mid+Vector((0,0,BZ0)),o,half,BZ1-BZ0,[hole(0,ww,BW1,BH1,BZ0)],th=.2,grain=True,band=band())
    wall(G['timber'],M['timber'],mid+Vector((0,0,BZ1)),o,half,BTOP-BZ1,[hole(0,ww,BW2,BH2,BZ1)],th=.2,grain=True,band=band())
    window(G,M,mid+Vector((0,0,BW1)),o,ww,BH1,shutters=False,planter=True,wall_th=.2,band=band())
    window(G,M,mid+Vector((0,0,BW2)),o,ww,BH2,shutters=False,wall_th=.2,band=band())
    n=3 if i==1 else 1;pw=(2*half-.16)/n
    for k in range(n):                    # raised (carved) panels below each tier
        a=-half+.08+pw*(k+.5)
        for z0,z1 in ((BZ0+.08,BW1-.38),(BW1+BH1+.17,BW2-.17)):
            G['timber'].box(P(a,.012,(z0+z1)/2),(pw-.08,.024,z1-z0),M['timber'],.006,R,grain=True,band=band())
            G['timber'].box(P(a,.03,z1-.06),(pw-.2,.02,.05),M['timber'],.004,R,grain=True,band=band())
    for z,h in ((BW1+BH1+.13,.07),(BTOP-.06,.14)):G['timber'].box(P(0,.03,z),(2*half+.06,.08,h),M['timber'],.01,R,grain=True,band=band())
for v,ang in zip(V,(45,22.5,-22.5,-45)):
    G['timber'].box(v+Vector((0,0,(BZ0+BTOP)/2)),(.16,.16,BTOP-BZ0),M['timber'],.012,Matrix.Rotation(math.radians(ang),3,'Z'),grain=True,band=band())
BE,KB=4.85,.88;T=math.tan(math.radians(22.5));SQ=math.sqrt(1+KB*KB);LB=(BP+OV)*SQ;SL=(BP+OV)*math.sqrt(2)
def h(t):return t/SQ
FL,FR=Vector((XG-BC-OV*T,Y0-BP-OV,BE)),Vector((XG+BC+OV*T,Y0-BP-OV,BE))
QL=FL+Vector((-(BP+OV),BP+OV,0))
def bay_plane(O,e,m,lo,hi):
    u=Vector((m[0],m[1],KB)).normalized();tile_plane(G['roof'],O,e,u,LB,lo,hi,M['slate'],SLATES,lichen=lichen,tone=(.35,.8),size=(.22,.32,.02),exposure=.15)
    deck(G['roof'],O,e,u,LB,lo,hi,M['timber'],cell=.3)
bay_plane(Vector((XG,FL.y,BE)),(1,0,0),(0,1),lambda t:-(BC+OV*T)+T*h(t),lambda t:(BC+OV*T)-T*h(t))
r2=math.sqrt(.5)
bay_plane(QL,(r2,-r2,0),(r2,r2),lambda t:h(t),lambda t:SL-T*h(t))
bay_plane(FR,(r2,r2,0),(-r2,r2),lambda t:T*h(t),lambda t:SL-h(t))
ZW=BE+(BP+OV)*KB;XH=BC+OV*T-T*(BP+OV)
for F,x in ((FL,XG-XH),(FR,XG+XH)):capped(F+Vector((0,0,.04)),(x,Y0-.02,ZW+.04),3)
G['timber'].beam((XG-XH-.1,Y0-.04,ZW-.02),(XG+XH+.1,Y0-.04,ZW-.02),.06,.1,M['timber'],band=band())
# ---- remaining openings --------------------------------------------------------------
window(G,M,(XU,YF,UZ),(0,-1,0),.85,UH,shutters=False,planter=True,band=band())
window(G,M,(XG,YF,GZ),(0,-1,0),GWW,GWH,shutters=False,planter=True,band=band())
for x,z in ((-1.5,WZ),(XG,WZ),(XU,UZ),(XG,UZ)):window(G,M,(x,D2,z),(0,1,0),.8,WH if z<JZ else UH,shutters=False,band=band())
for s in (-1,1):
    window(G,M,(s*W2,.9,WZ),(s,0,0),.7,WH,shutters=False,band=band())
    window(G,M,(s*W2,.9,UZ),(s,0,0),.7,UH,shutters=False,band=band())
lantern(G,M,(XDOOR-.75,-D2-.42,2.3),arm=((XDOOR-.75,-D2-.09,2.3),(0,-1,0)))
# ---- gutters and downpipes -------------------------------------------------------------
def pipe(a,b,s=.08):G['detail'].beam(a,b,s,s,M['iron'],grain=False)
pipe((-EX,-EY-.07,EAVE-.12),(XG-GH,-EY-.07,EAVE-.12),.1)
pipe((XG+GH+.07,GY,GE-.12),(XG+GH+.07,GB,GE-.12),.1)
for (x,y),(xg,yg),xw in (((-2.92,-2.3),(-2.92,-EY-.07),-W2),((2.93,-2.25),(XG+GH+.07,-2.25),W2)):
    pipe((x,y,.06),(x,y,5.15));pipe((x,y,5.1),(xg,yg,EAVE-.14));pipe((x,y,.1),(x+(.15 if x>0 else -.15),y,.06))
    for z in (.9,2.2,3.5,4.8):G['detail'].box(((x+xw)/2,y,z),(abs(x-xw)+.02,.03,.04),M['iron'],.004)
# ---- iron bracket sign with the stag banner (left wall) -----------------------------------
def stag(geo,origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R_,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R_*(a*s)+U*(z*s)
    geo.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        geo.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
AY,AZ,AX=-1.7,2.62,-W2-1.0
pipe((-W2+.02,AY,AZ),(AX,AY,AZ),.045)
pipe((-W2+.02,AY,AZ-.7),(-W2-.75,AY,AZ-.02),.03)
G['detail'].box((-W2-.025,AY,AZ-.35),(.05,.1,.85),M['iron'],.004)
for k in range(8):
    t0,t1=math.tau*k/8,math.tau*(k+1)/8;c=Vector((-W2-.42,AY,AZ-.2));r=.13
    pipe(c+Vector((r*math.cos(t0),0,r*math.sin(t0))),c+Vector((r*math.cos(t1),0,r*math.sin(t1))),.018)
SX=-W2-.68;sb=[(SX-.26,AZ-.14),(SX+.26,AZ-.14),(SX+.26,AZ-.95),(SX,AZ-1.12),(SX-.26,AZ-.95)]
G['detail'].prism([(x,AY,z) for x,z in sb],(0,1,0),.03,M['banner'])
pipe((SX-.32,AY,AZ-.12),(SX+.32,AY,AZ-.12),.03)
for x in (SX-.24,SX+.24):pipe((x,AY,AZ-.02),(x,AY,AZ-.13),.015)
for n in (-1,1):stag(G['detail'],(SX,AY+n*.015,AZ-.62),(-n,0,0),(0,0,1),(0,n,0),.5)
# ---- export, preview scene, manifest ---------------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'not grounded: {lo[2]}'
assert all(h_-l<=s+1e-3 for l,h_,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h_-l,3) for l,h_ in zip(lo,hi)]}'
OUT=HERE/'SilverbrookTownhouse2'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookTownhouse2')  # door leaves: own meshes, hinged; deselected for the main FBX
# Door leaves are their own meshes hinged at the jamb; check each opens inward (+Y, away from the street) above its sill.
for d,row in zip(kit.DOORS,DOORS):
    ob_=bpy.data.objects[d['geo'].name];vs=[ob_.matrix_world@v.co for v in ob_.data.vertices]
    pv=Vector(row['pivot_m']);rot=Matrix.Rotation(math.radians(row['travel']),3,'Z');op=[pv+rot@(v-pv) for v in vs]
    dy=sum(v.y for v in op)/len(op)-sum(v.y for v in vs)/len(vs);print('DOOR_SWING',row['name'],row['pivot_m'],row['travel'],round(dy,3))
    assert dy>.3 and min(v.z for v in op)>pv.z-1e-3,f"{row['name']} does not swing inward"
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-10.5,-14,5.6),(80,0,-37),lens=38);cam('Front',(0,-30,4.5),(90,0,0),9.6);cam('Side',(-30,0,4.5),(90,0,-90),9.6);cam('Rear',(0,30,4.5),(90,0,180),9.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Townhouse2-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'house triangle budget: {tris}'
manifest=dict(asset='SilverbrookTownhouse2',reference=dict(file='Townhouse2-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_townhouse2.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookTownhouse2',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookTownhouse2.blend','SilverbrookTownhouse2.glb','SilverbrookTownhouse2.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('TOWNHOUSE2_BUILT',manifest['bounds_m'],tris)
