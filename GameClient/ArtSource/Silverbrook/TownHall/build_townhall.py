"""Silverbrook Town Hall from docs/Game-Design/concept-art/Silverbrook/Town Hall.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhall.py
Original procedural geometry interpreting the elevation (22 m wide, 28 m to the spire
finial): Gothic ashlar hall with a projecting gabled entrance bay, a central clock and
bell tower with slate spire and corner pinnacles, two corner towers and two roof turrets
with pyramidal spires, lower hipped wings with half-timbered gablets, banners and lanterns.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,UP,tile_plane,deck,wall,frame,lantern
import bpy,numpy as np
scene=kit.reset(4122)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Town Hall.png'
asset=bpy.data.collections.new('SilverbrookTownHall');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_TownHall_Ashlar','sandstone_blocks_08',dark=(.40,.36,.30)),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_TownHall_Banner','denim_fabric_04',dark=(.10,.14,.32),damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    gilt=kit.solid('SB_TownHall_Gilt',(.80,.55,.22),.32,1),
    bronze=kit.solid('SB_TownHall_Bronze',(.36,.22,.11),.4,1))
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_08',(.05,.035),dark=.8,frac=.01,std=.2)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
def stone():return kit.rng.choice(STONES)
G=dict(stone=Geo('SM_SBTownHall_Masonry',asset),plaster=Geo('SM_SBTownHall_Plaster',asset),timber=Geo('SM_SBTownHall_Timber',asset),roof=Geo('SM_SBTownHall_Roof',asset),detail=Geo('SM_SBTownHall_Details',asset))
S,D=G['stone'],G['detail']
# ---- measured layout (metres; concept elevation at ~40.4 px/m) -----------------
SLOT=(22,18,28)                       # town-layout slot w x d x h (concept README 'Fits')
FY,RY,EM,KM=-4.5,5.5,8.0,1.308        # main hall front/rear wall faces, wall top, roof pitch
MEX,MEY,MCY=6.35,5.35,.5              # main hip roof eave half-extents and centre y
BF,BW,BTOP,KB=-7.0,3.0,11.8,1.267     # entrance bay front face, half width, wall top, gable pitch
BR=BTOP+BW*KB+.05                     # bay roof ridge (gable apex 15.6 + roof build-up)
TC,TH=(0,2.0),2.0                     # central tower centre and half width
CT=(6.9,-4.6,1.2,10.4)                # corner towers: |x|, y, half, shaft top
TU=(3.9,-1.2,.85,16.3)                # roof turrets: |x|, y, half, shaft top
WX0,WX1,WFY,WRY,WTOP,KW=6.0,10.6,-4.5,5.0,5.5,1.6   # wings
def hipz(cx,cy,ex,ey,eave,k,p):return eave+k*min(ex-abs(p.x-cx),ey-abs(p.y-cy))
main_z=lambda p:hipz(0,MCY,MEX,MEY,EM,KM,p)
bay_z=lambda x:BR-abs(x)*KB
def box_in(cx,cy,h,top):return lambda p:abs(p.x-cx)<h+.02 and abs(p.y-cy)<h+.02 and p.z<top
in_ctr=lambda p:abs(p.x)<TH+.02 and -.02<p.y<2*TH+.02 and p.z<21.8
in_ct=lambda p:abs(abs(p.x)-CT[0])<CT[2]+.02 and abs(p.y-CT[1])<CT[2]+.02 and p.z<CT[3]+.2
in_tu=lambda p:abs(abs(p.x)-TU[0])<TU[2]+.02 and abs(p.y-TU[1])<TU[2]+.02 and p.z<TU[3]+.3
in_bay=lambda p:abs(p.x)<BW+.05 and p.y<.05 and p.z<bay_z(p.x)-.005
under_main=lambda p:abs(p.x)<MEX and FY-.4<p.y<RY+.4 and p.z<main_z(p)-.01
in_hall=lambda p:abs(p.x)<WX0 and FY<p.y<RY and p.z<EM
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.4 else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
BIG=dict(size=(.36,.5,.022),exposure=.24)
def plane(O,e,u,L,lo,hi,covered,**kw):
    u=Vector(u).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8),**kw)
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n,cover=(),w=.26):
    a,b=Vector(a),Vector(b)
    for i in range(n):
        p,q=a+(b-a)*i/n,a+(b-a)*(i+1.04)/n
        if not any(c((p+q)/2) for c in cover):cap(p,q,w)
def hip(cx,cy,ex,ey,eave,ridge,cover=(),tiles={},capw=.26):
    """Hipped (or, with ex == ey, pyramidal) slate roof; ridge along the longer axis."""
    es,rise,rl,alongx=min(ex,ey),ridge-eave,abs(ex-ey),ex>=ey;L=math.hypot(es,rise)
    for out in [Vector((0,-1,0)),Vector((0,1,0)),Vector((1,0,0)),Vector((-1,0,0))]:
        side=out.x==0;half=ex if side else ey;r=rl if side==alongx else 0
        plane(Vector((cx,cy,eave))+out*(ey if side else ex),UP.cross(out),-out*es+UP*rise,L,lerp(-half,-r,L),lerp(half,r,L),cover,**tiles)
    ends=[Vector((cx+s*rl,cy,ridge+.05)) if alongx else Vector((cx,cy+s*rl,ridge+.05)) for s in (-1,1)]
    n=max(2,round(L/.45))
    if rl>.05:capped(ends[0],ends[1],max(1,round(2*rl/.45)),cover,capw)
    for sx in (-1,1):
        for sy in (-1,1):
            end=ends[(sx if alongx else sy)>0];capped((cx+sx*ex,cy+sy*ey,eave+.05),end,n,cover,capw)
def pyramid(g,c,half,z0,apex,mat):
    """Solid stone or slate pinnacle cap (world-projected scan)."""
    cs=[Vector((c[0]+x*half,c[1]+y*half,z0)) for x,y in ((-1,-1),(1,-1),(1,1),(-1,1))];top=Vector((c[0],c[1],apex))
    for i in range(4):g.poly([cs[i],cs[(i+1)%4],top],mat)
    g.poly(cs[::-1],mat)
def finial(x,y,z0,z1):
    D.beam((x,y,z0),(x,y,z1),.045,.045,M['gilt'],grain=False)
    for z,r in ((z0+(z1-z0)*.4,.09),(z1-.05,.05)):D.vessel((x,y,z-r),[(0,0),(r*.7,r*.3),(r,r),(r*.7,r*1.7),(0,2*r)],M['gilt'],10)
def pinnacle(x,y,z0,shaft,half,spike=1.2):
    S.box((x,y,z0+shaft/2),(2*half,2*half,shaft),M['stone'],.015,rect=stone())
    S.box((x,y,z0+shaft+.06),(2*half+.12,2*half+.12,.12),M['stone'],.015,rect=stone())
    top=z0+shaft+.12;pyramid(G['roof'],(x,y),half+.04,top,top+spike,M['slate']);finial(x,y,top+spike-.05,top+spike+.45)
# ---- pointed (two-centred) arches ---------------------------------------------------
def arcpts(w,spring,off=0,n=9):
    """Right-hand arc of a pointed arch (radius .75 w), springing to apex, as (offset from axis, z)."""
    r,h=.75*w,w/2;c=h-r;R=r+off;t1=math.acos(-c/R)
    return [(c+R*math.cos(t),spring+R*math.sin(t)) for t in np.linspace(0,t1,n)]
def apex(w,spring,off=0):return arcpts(w,spring,off,2)[-1][1]
def seg(g,A,B,o,radial,depth,mat):
    t=B-A;L=t.length;t.normalize();n=Vector(o).normalized();rot=Matrix((n.cross(t),n,t)).transposed()
    g.box((A+B)/2,(radial,depth,L+.012),mat,.01,rot,rect=stone())
def ring(P,R,o,a,w,spring,off,radial,dc,depth,jamb=None,n=9):
    pts=arcpts(w,spring,off,n)
    for s in (1,-1):
        for (x0,z0),(x1,z1) in zip(pts,pts[1:]):seg(S,P(a+s*x0,dc,z0),P(a+s*x1,dc,z1),o,radial,depth,M['stone'])
        if jamb is not None:S.box(P(a+s*(w/2+off),dc,(jamb+spring)/2),(radial,depth,spring-jamb),M['stone'],.012,R,rect=stone())
def pwall(g,c,o,half,height,openings,th):
    """kit.wall with pointed heads: openings are (a, width, bottom, spring)."""
    P,R=frame(c,o);holes=sorted((b,a,w,apex(w,s)) for a,w,b,s in openings)   # kit.wall allows one hole per column
    cuts=sorted({-half,half,*[a+d*w/2 for _,a,w,_ in holes for d in (-1,1)]})
    for a0,a1 in zip(cuts,cuts[1:]):
        z=0;spans=[]
        for b,a,w,top in holes:
            if abs(a-(a0+a1)/2)<w/2:spans.append((z,b));z=top
        for z0,z1 in spans+[(z,height)]:
            if z1-z0>.005:g.box(P((a0+a1)/2,-th/2,(z0+z1)/2),(a1-a0,th,z1-z0),M['stone'],0,R)
    for a,w,b,s in openings:
        top=apex(w,s)
        for sd in (1,-1):g.prism([P(a+sd*w/2,-th/2,top)]+[P(a+sd*x,-th/2,z) for x,z in arcpts(w,s)],o,th,M['stone'])   # corner first: kit.prism orients by the first three points
def lancet(c,o,a,w,bottom,spring,th=.5,lights=2,decor=False,sill=True,hood=True):
    """Leaded Gothic window: lit glass, stone mullions (and tracery when lights >= 4),
    lead cames, a moulded hood and sill. decor=True lays it on a solid face instead of in a hole."""
    P,R=frame(c,o);dg=.03 if decor else -th/2;pts=arcpts(w,spring);top=pts[-1][1]
    outline=[P(a-w/2,dg,bottom),P(a+w/2,dg,bottom)]+[P(a+x,dg,z) for x,z in pts]+[P(a-x,dg,z) for x,z in pts[::-1][1:]]
    D.prism(outline,o,.02,M['glass'])
    def zarc(x):r,h=.75*w,w/2;return spring+math.sqrt(max(0,r*r-(abs(x)+r-h)**2))
    for i in range(1,lights):
        x=-w/2+i*w/lights;zt=zarc(x)
        if lights>=4:zt=apex(w/2,spring) if i!=lights//2 else spring+.37*w   # sub-arch apex / oculus foot
        S.box(P(a+x,dg,(bottom+zt)/2),(.11,.16,zt-bottom),M['stone'],.012,R,rect=stone())
    if lights>=4:
        for s in (-1,1):
            q=arcpts(w/2,spring,0,7)
            for (x0,z0),(x1,z1) in zip(q,q[1:]):
                for m in (1,-1):seg(S,P(a+s*w/4+m*x0,dg,z0),P(a+s*w/4+m*x1,dg,z1),o,.09,.14,M['stone'])
        oc=spring+.53*w;rr=.16*w;k=14
        for j in range(k):
            t0,t1=math.tau*j/k,math.tau*(j+1)/k;seg(S,P(a+rr*math.cos(t0),dg,oc+rr*math.sin(t0)),P(a+rr*math.cos(t1),dg,oc+rr*math.sin(t1)),o,.08,.14,M['stone'])
    z=bottom+.3
    while z<spring-.08:D.beam(P(a-w/2,dg+.02,z),P(a+w/2,dg+.02,z),.014,.012,M['lead'],grain=False);z+=.3
    for x in np.arange(-w/2+.2,w/2-.1,.2):
        if spring-bottom>.1 and (lights<2 or min(abs(x-(-w/2+i*w/lights)) for i in range(1,lights))>.1):D.beam(P(a+x,dg+.02,bottom),P(a+x,dg+.02,min(spring,zarc(x)-.03)),.014,.012,M['lead'],grain=False)
    if hood:ring(P,R,o,a,w,spring,.09,.15,.03 if not decor else .06,.1 if not decor else .08,jamb=bottom)
    if sill:S.box(P(a,.05 if not decor else .08,bottom-.06),(w+.36,.2 if not decor else .14,.12),M['stone'],.012,R,rect=stone())
def roundel(c,o,a,zc,rad,clock=False):
    """Round window on a solid face: glass disc, stone ring; clock adds an iron star and gilt hub."""
    P,R=frame(c,o);k=20
    D.prism([P(a+rad*math.cos(math.tau*j/k),.03,zc+rad*math.sin(math.tau*j/k)) for j in range(k)],o,.02,M['glass'])
    for j in range(k):
        t0,t1=math.tau*j/k,math.tau*(j+1)/k;seg(S,P(a+(rad+.07)*math.cos(t0),.06,zc+(rad+.07)*math.sin(t0)),P(a+(rad+.07)*math.cos(t1),.06,zc+(rad+.07)*math.sin(t1)),o,.16,.12,M['stone'])
    for j in range(8 if clock else 4):
        t=math.pi*j/(8 if clock else 4)+(0 if clock else math.pi/4)
        for s in (1,-1):D.beam(P(a,.055,zc),P(a+s*rad*math.cos(t),.05,zc+s*rad*math.sin(t)),.035 if clock else .06,.02,M['gilt'] if clock else M['stone'],grain=False)
    if clock:D.box(P(a,.07,zc),(.16,.04,.16),M['gilt'],.01,R)
def banner(c,o,a,ztop,zbot,w,d=.14):
    """Hanging cloth banner with a pointed foot, gilt edging and rod on iron brackets."""
    P,R=frame(c,o);pts=[(-w/2,ztop),(w/2,ztop),(w/2,zbot+.4),(0,zbot),(-w/2,zbot+.4)]
    D.prism([P(a+x,d,z) for x,z in pts],o,.02,M['banner'])
    for (x0,z0),(x1,z1) in zip(pts,pts[1:]+pts[:1]):D.beam(P(a+x0,d+.012,z0),P(a+x1,d+.012,z1),.035,.012,M['gilt'],grain=False)
    D.beam(P(a-w/2-.1,d,ztop+.05),P(a+w/2+.1,d,ztop+.05),.045,.045,M['gilt'],grain=False)
    for s in (-1,1):D.box(P(a+s*(w/2+.04),d/2,ztop+.05),(.04,d+.04,.04),M['iron'],.004,R)
def dormer(x,yf,base,half,pitch,apex_z,roof_y,face,timbered=False):
    """Gabled dormer/gablet: face in the wall plane, two slate slabs and cheeks running back into the roof."""
    zt=apex_z-half*pitch;ov=.22;zs=lambda u:apex_z+.06-abs(u-x)*pitch
    g=G['plaster'] if timbered else S
    g.prism([(x-half,yf+.15,base),(x+half,yf+.15,base),(x+half,yf+.15,zt),(x,yf+.15,apex_z),(x-half,yf+.15,zt)],(0,1,0),.3,face)
    y0,y1=yf-.2,roof_y(apex_z+.25)
    for s in (-1,1):
        e=x+s*(half+ov);G['roof'].prism([(e,y0,zs(e)),(x,y0,zs(x)),(x,y1,zs(x)),(e,y1,zs(e))],(s*pitch,0,1),.1,M['slate'])
        g.prism([(x+s*(half-.1),yf+.3,base),(x+s*(half-.1),yf+.3,zt),(x+s*(half-.1),roof_y(zt+.1),zt)],(1,0,0),.2,face)
        G['timber'].beam((e+s*.02,yf-.23,zs(e)-.05),(x-s*.06,yf-.23,apex_z+.12+.06*pitch),.07,.2,M['timber'],band=band())
    if timbered:
        y=yf-.03
        for s in (-1,1):G['timber'].box((x+s*(half-.07),y,(base+zt)/2),(.14,.1,zt-base),M['timber'],.012,grain=True,band=band())
        G['timber'].box((x,y,base+.07),(2*half,.1,.14),M['timber'],.012,grain=True,band=band())
        G['timber'].box((x,y,zt-.05),(2*half,.1,.12),M['timber'],.012,grain=True,band=band())
        G['timber'].box((x,y,(zt+apex_z)/2),(.12,.1,apex_z-zt-.08),M['timber'],.012,grain=True,band=band())
        for s in (-1,1):G['timber'].beam((x+s*(half-.1),y,base+.14),(x,y,zt-.1),.1,.1,M['timber'],band=band())
    return lambda p:abs(p.x-x)<half+ov+.02 and p.y<y1+.05 and p.z<zs(p.x)-.005
# ---- main hall --------------------------------------------------------------------
main_y=lambda z:MCY-MEY+(z-EM)/KM
dorm=[]
for s in (-1,1):
    x=s*4.35
    pwall(S,(x,FY,0),(0,-1,0),1.35,EM,[(0,1.2,4.4,6.35),(0,.7,1.3,2.55)],.5)
    lancet((x,FY,0),(0,-1,0),0,1.2,4.4,6.35);lancet((x,FY,0),(0,-1,0),0,.7,1.3,2.55,sill=True)
    S.box((x,FY-.07,EM-.12),(2.7,.64,.34),M['stone'],.015)            # eave cornice
    S.box((x,FY-.25,4.21),(1.7,.5,.15),M['stone'],.015,rect=stone())    # balconette
    for dx in (-.55,.55):S.box((x+dx,FY-.15,3.98),(.2,.3,.3),M['stone'],.012,rect=stone())
    for dx in np.linspace(-.8,.8,9):D.box((x+dx,FY-.45,4.55),(.025,.025,.55),M['iron'],.003)
    D.beam((x-.82,FY-.45,4.84),(x+.82,FY-.45,4.84),.04,.04,M['iron'],grain=False)
    dorm.append(dormer(x,FY,EM,.75,1.3,11.05,main_y,M['stone']))
    lancet((x,FY,EM),(0,-1,0),0,.8,.35,1.15,decor=True,sill=False)
    pwall(S,(s*WX0,MCY,0),(s,0,0),(RY-FY)/2-.5,EM,[],.5)
pwall(S,(0,RY,0),(0,1,0),6.0,EM,[(x,.9,1.5,3.0) for x in (-3.5,0,3.5)]+[(x,1.0,4.6,6.4) for x in (-3.5,0,3.5)],.5)
for x in (-3.5,0,3.5):lancet((0,RY,0),(0,1,0),x,.9,1.5,3.0);lancet((0,RY,0),(0,1,0),x,1.0,4.6,6.4)
hip(0,MCY,MEX,MEY,EM,EM+MEY*KM,[in_bay,in_ct,in_tu,in_ctr]+dorm,BIG)
# ---- entrance bay ------------------------------------------------------------------
DW,DS,WW,WB,WS=2.9,3.75,3.0,7.7,9.38
pwall(S,(0,BF,0),(0,-1,0),BW,BTOP,[(0,DW,0,DS),(0,WW,WB,WS)],.6)
P,R=frame((0,BF,0),(0,-1,0))
S.prism([P(-BW,-.3,BTOP),P(BW,-.3,BTOP),P(0,-.3,BR-.05)],(0,-1,0),.6,M['stone'])     # gable
for s in (-1,1):
    S.beam(P(s*(BW+.12),-.3,BTOP-.1),P(0,-.3,BR+.14),.3,.75,M['stone'],grain=False,rect=stone())   # raking coping
    pwall(S,(s*BW,BF+.6+1.2,0),(s,0,0),1.2,BTOP,[(0,.8,7.9,9.1)],.5)
    lancet((s*BW,BF+.6+1.2,0),(s,0,0),0,.8,7.9,9.1)
    yq=main_y(BTOP+.1);S.prism([(s*(BW-.25),FY+.5,main_z(Vector((BW,FY+.5,0)))-.1),(s*(BW-.25),FY+.5,BTOP),(s*(BW-.25),yq,BTOP)],(1,0,0),.5,M['stone'])
finial(0,BF-.3,BR+.25,BR+.95)
lancet((0,BF,0),(0,-1,0),0,WW,WB,WS,th=.6,lights=4)
roundel((0,BF,0),(0,-1,0),0,13.95,.42)
# portal: tympanum glass over a transom, plank leaves, three archivolt orders on jamb shafts
lancet((0,BF,0),(0,-1,0),0,DW,DS,DS,th=.6,lights=2,sill=False,hood=False)
S.box(P(0,-.3,DS),(DW,.3,.16),M['stone'],.012,R,rect=stone())
for s in (-1,1):                # one mesh per leaf so each opens; hinges on the outer jambs
    L=Geo(f'SM_SBTownHall_Door{len(kit.DOORS)+1}',asset);G[L.name]=L
    kit.DOORS.append(dict(geo=L,r=Vector((-s,0,0)),o=Vector((0,-1,0)),width=DW/2))   # r points from the hinge jamb to the meeting line
    L.box(P(s*DW/4,-.3,(.75+DS-.08)/2),(DW/2-.01,.1,DS-.08-.75),M['door'],.006,R)
    for z in (1.3,2.4,3.3):L.box(P(s*DW*.3,-.235,z),(DW*.36,.02,.07),M['iron'],.004,R)
    L.box(P(s*.22,-.22,2.0),(.05,.03,.18),M['iron'],.004,R)
for k,off in enumerate((.12,.36,.6)):ring(P,R,(0,-1,0),0,DW,DS,off,.22,.04+.07*k,.12,jamb=.75,n=11)
for i,(hw,fy) in enumerate(((2.7,-8.2),(2.65,-7.8),(2.6,-7.4))):S.box((0,(fy+BF+.6)/2,.125*(i+1)),(2*hw,BF+.6-fy,.25*(i+1)),M['stone'],.012)
for s in (-1,1):
    S.box((s*BW,BF-.3,5.5),(.6,.9,11.0),M['stone'],.015)            # corner buttresses
    pinnacle(s*BW,BF-.3,11.0,1.5,.24,1.0)
    S.box((s*3.15,-8.0,.6),(.6,.6,1.2),M['stone'],.015,rect=stone());S.box((s*3.15,-8.0,1.25),(.72,.72,.1),M['stone'],.012,rect=stone())
    lantern(G,M,(s*3.15,-8.0,1.71))
    lantern(G,M,(s*BW,BF-1.17,3.7),arm=((s*BW,BF-.75,3.7),(0,-1,0)))
banner((0,BF,0),(0,-1,0),0,9.6,5.95,1.3,d=.32)
LB=math.hypot(BW+.35,BR-bay_z(BW+.35))
plane((-(BW+.35),0,bay_z(BW+.35)),(0,-1,0),(BW+.35,0,BR-bay_z(BW+.35)),LB,lambda t:0,lambda t:-BF+.05,[under_main,in_ctr,in_tu],**BIG)
plane((BW+.35,0,bay_z(BW+.35)),(0,1,0),(-(BW+.35),0,BR-bay_z(BW+.35)),LB,lambda t:BF-.05,lambda t:0,[under_main,in_ctr,in_tu],**BIG)
capped((0,BF-.05,BR+.05),(0,0,BR+.05),16,[in_ctr])
# ---- central clock and bell tower ------------------------------------------------------
cx,cy=TC;S.box((cx,cy,(7.5+19.65)/2),(2*TH,2*TH,19.65-7.5),M['stone'],.02)
for sx in (-1,1):
    for sy in (-1,1):S.box((cx+sx*(TH-.05),cy+sy*(TH-.05),(12.5+19.45)/2),(.5,.5,19.45-12.5),M['stone'],.015)
S.box((cx,cy,19.55),(2*TH+.4,2*TH+.4,.22),M['stone'],.015)
roundel((cx,cy-TH,0),(0,-1,0),0,17.5,.8,clock=True)
for o,c,h in (((0,-1,0),(cx,cy-TH,19.66),TH),((0,1,0),(cx,cy+TH,19.66),TH),((1,0,0),(cx+TH,cy,19.66),TH-.5),((-1,0,0),(cx-TH,cy,19.66),TH-.5)):
    pwall(S,c,o,h,1.94,[(0,1.5,.25,.75)],.5)
    Pb,Rb=frame(c,o);ring(Pb,Rb,o,0,1.5,.75,.09,.15,.03,.1,jamb=.25)
S.box((cx,cy,21.75),(2*TH+.5,2*TH+.5,.3),M['stone'],.015)
G['timber'].beam((cx-TH+.4,cy,21.35),(cx+TH-.4,cy,21.35),.22,.22,M['timber'],band=band())
D.vessel((cx,cy,20.05),[(0,0),(.62,0),(.58,.12),(.46,.45),(.36,.9),(.34,1.05),(.2,1.15),(0,1.15)],M['bronze'],24)
D.box((cx,cy,21.2),(.12,.12,.22),M['iron'],.01)
for sx in (-1,1):
    for sy in (-1,1):pinnacle(cx+sx*(TH-.05),cy+sy*(TH-.05),21.9,.85,.26,1.15)
hip(cx,cy,1.65,1.65,21.9,26.65,capw=.2)
finial(cx,cy,26.55,27.95)
# ---- corner towers and roof turrets -------------------------------------------------------
for s in (-1,1):
    x,y,h,top=CT;x*=s
    S.box((x,y,top/2),(2*h,2*h,top),M['stone'],.02)
    S.box((x,y,top-.15),(2*h+.3,2*h+.3,.32),M['stone'],.015)
    for lz in (8.6,1.5):lancet((x,y-h,0),(0,-1,0),0,.5,lz,lz+.75,decor=True,lights=1)
    lancet((x+s*h,y,0),(s,0,0),0,.5,8.6,9.35,decor=True,lights=1)
    banner((x,y-h,0),(0,-1,0),0,7.9,4.45,1.15)
    hip(x,y,h+.25,h+.25,top+.01,14.4,capw=.2);finial(x,y,14.3,15.6)
    x,y,h,top=TU;x*=s
    S.box((x,y,(7.9+top)/2),(2*h,2*h,top-7.9),M['stone'],.02)
    S.box((x,y,top+.12),(2*h+.3,2*h+.3,.25),M['stone'],.015)
    for o,c in (((0,-1,0),(x,y-h,0)),((s,0,0),(x+s*h,y,0))):lancet(c,o,0,.42,14.8,15.55,decor=True,lights=1)
    hip(x,y,h+.15,h+.15,top+.25,18.3,capw=.18);finial(x,y,18.2,19.3)
# ---- wings ------------------------------------------------------------------------------
wing_y=lambda z:WFY-.3+(z-WTOP)/KW
for s in (-1,1):
    xc=s*(WX0+WX1)/2;xo=s*WX1;xf=s*(WX1+CT[0]+CT[2])/2;hf=(WX1-CT[0]-CT[2])/2
    pwall(S,(xf,WFY,0),(0,-1,0),hf,WTOP,[(0,.9,1.4,3.7)],.45);lancet((xf,WFY,0),(0,-1,0),0,.9,1.4,3.7)
    pwall(S,(xc,WRY,0),(0,1,0),(WX1-WX0)/2,WTOP,[(0,.9,1.4,3.7)],.45);lancet((xc,WRY,0),(0,1,0),0,.9,1.4,3.7)
    so=(WRY-WFY)/2-.45;yc=(WRY+WFY)/2
    pwall(S,(xo,yc,0),(s,0,0),so,WTOP,[(y,.9,1.4,3.7) for y in (-2.2,2.2)],.45)
    for y in (-2.2,2.2):lancet((xo,yc,0),(s,0,0),y,.9,1.4,3.7)
    G['timber'].box((xo,WFY,WTOP/2),(.26,.26,WTOP),M['timber'],.015,grain=True,band=band())
    G['timber'].box((xf,WFY-.08,WTOP-.12),(2*hf,.16,.22),M['timber'],.015,grain=True,band=band())
    gab=dormer(s*9.3,WFY,WTOP,.7,1.5,7.35,wing_y,M['plaster'],timbered=True)
    hip(s*(WX1+WX0)/2,(WRY+WFY)/2,(WX1-WX0+.6)/2,(WRY-WFY)/2+.3,WTOP,WTOP+(WX1-WX0+.6)/2*KW,[in_hall,in_ct,gab],BIG)
    rl=(WRY-WFY)/2+.3-(WX1-WX0+.6)/2
    for sy in (-1,1):finial(s*(WX1+WX0)/2,(WRY+WFY)/2+sy*rl,WTOP+(WX1-WX0+.6)/2*KW,WTOP+(WX1-WX0+.6)/2*KW+1.6)
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'ground not at z=0: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookTownHall'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookTownHall')  # door leaves: own meshes, hinged; deselected for the main FBX
# Door leaves are their own meshes hinged at the jamb; check each opens inward (+Y, away from the street) above its sill.
for d,row in zip(kit.DOORS,DOORS):
    ob_=bpy.data.objects[d['geo'].name];vs=[ob_.matrix_world@v.co for v in ob_.data.vertices]
    pv=Vector(row['pivot_m']);rot=Matrix.Rotation(math.radians(row['travel']),3,'Z');op=[pv+rot@(v-pv) for v in vs]
    dy=sum(v.y for v in op)/len(op)-sum(v.y for v in vs)/len(vs);print('DOOR_SWING',row['name'],row['pivot_m'],row['travel'],round(dy,3))
    assert dy>.3 and min(v.z for v in op)>pv.z-1e-3,f"{row['name']} does not swing inward"
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-60,-60,0),(60,-60,0),(60,60,0),(-60,60,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,target,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=(Vector(target)-Vector(loc)).to_track_quat('-Z','Y').to_euler();pre.objects.link(o);c.clip_end=500
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-27,-40,11),(0,0,11.5),lens=38);cam('Front',(0,-80,14.2),(0,0,14.2),29.5);cam('Side',(-80,0,14.2),(0,0,14.2),29.5);cam('Rear',(0,80,14.2),(0,0,14.2),29.5)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'TownHall-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=400_000,f'landmark triangle budget: {tris}'
manifest=dict(asset='SilverbrookTownHall',reference=dict(file='TownHall-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_townhall.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookTownHall',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookTownHall.blend','SilverbrookTownHall.glb','SilverbrookTownHall.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('TOWNHALL_BUILT',manifest['bounds_m'],tris)
