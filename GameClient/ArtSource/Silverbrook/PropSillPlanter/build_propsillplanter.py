"""Silverbrook street prop: SILL PLANTER. 0.9 m terracotta trough with chipped lip, damp soil, six geranium plants and trailing thyme over the front.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propsillplanter.py
Concept: concept/sill-planter-sheet.png (red and pink variants). Origin = bottom centre of the trough's back edge; back face flush with y=0 (the wall), front -Y; sits on a sill at z=0.
Flowers are the SB_GeraniumBloom slot: red in the mesh, pink via the `Pink` variant MI.
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
from prop_common_wns import revolve
import prop_common_sheet as sheet
import bpy
scene=kit.reset(5304);asset=bpy.data.collections.new('SilverbrookPropSillPlanter');scene.collection.children.link(asset)
M=dict(clay=kit.solid('SB_Terracotta',(.36,.14,.07),.8),soil=kit.solid('SB_DampSoil',(.03,.021,.015),.95),leaf=kit.solid('SB_GeraniumLeaf',(.075,.19,.04),.6),
    stem=kit.solid('SB_GeraniumStem',(.07,.12,.035),.7),bloom=kit.solid('SB_GeraniumBloom',(.5,.02,.02),.55),thyme=kit.solid('SB_ThymeSprig',(.06,.12,.05),.7))
sheet.add_variants('SB_GeraniumBloom',dict(Pink=[.62,.13,.2]))
G=dict(clay=Geo('SM_SBSillPlanter_Trough',asset),soil=Geo('SM_SBSillPlanter_Soil',asset),leaf=Geo('SM_SBSillPlanter_Leaves',asset),stem=Geo('SM_SBSillPlanter_Stems',asset),bloom=Geo('SM_SBSillPlanter_Blooms',asset),thyme=Geo('SM_SBSillPlanter_Thyme',asset))
rng=kit.rng
def face(pts,want,mat=M['clay'],geo='clay'):
    """Quad wound so its normal faces `want`."""
    pts=[Vector(p) for p in pts]
    if (pts[1]-pts[0]).cross(pts[2]-pts[0]).dot(want)<0:pts.reverse()
    G[geo].poly(pts,mat,w=(rng.uniform(.1,.35),0))
V=Vector
# ---- trough: tapered shell, vertical back against the wall -----------------------------------------------
XB,YB,XT,YT,Z,T,FL=.41,.19,.45,.24,.2,.022,.02
OB=[V((-XB,0,0)),V((XB,0,0)),V((XB,-YB,0)),V((-XB,-YB,0))];OT=[V((-XT,0,Z)),V((XT,0,Z)),V((XT,-YT,Z)),V((-XT,-YT,Z))]
IB=[V((-XB+T,-T,FL)),V((XB-T,-T,FL)),V((XB-T,-YB+T,FL)),V((-XB+T,-YB+T,FL))];IT=[V((-XT+T,-T,Z)),V((XT-T,-T,Z)),V((XT-T,-YT+T,Z)),V((-XT+T,-YT+T,Z))]
ctr=V((0,-.1,.1))
for i in range(4):
    j=(i+1)%4;q=[OB[i],OB[j],OT[j],OT[i]];face(q,sum(q,V())/4-ctr)                                         # outer walls
    q=[IB[i],IB[j],IT[j],IT[i]];face(q,ctr-sum(q,V())/4)                                                      # inner walls
face(OB,-V((0,0,1)));face(IB,V((0,0,1)))                                                                      # underside, floor
face([IT[0],IT[1],OT[1],OT[0]],V((0,0,1)));face([IT[1],IT[2],OT[2],OT[1]],V((0,0,1)));face([IT[2],IT[3],OT[3],OT[2]],V((0,0,1)));face([IT[3],IT[0],OT[0],OT[3]],V((0,0,1)))   # rim top
# chipped lip band on the outside of the rim: front lip has a notch and a nibbled corner
LZ=Z+.006                                                                                                    # lip top
def lip(x0,x1,hgt):G['clay'].box(((x0+x1)/2,-YT+.005,LZ-hgt/2),(x1-x0,.034,hgt),M['clay'],.006)
for x0,x1,hgt in ((-.462,-.17,.036),(-.17,-.115,.016),(-.115,.462,.036)):lip(x0,x1,hgt)
for s_ in (-1,1):G['clay'].box((s_*(XT-.005),-YT/2,LZ-.018),(.034,YT,.036),M['clay'],.006)
# soil, just below the lip
SZ=Z-.035;sx=XT-T-.003
G['soil'].poly([V((-sx,-T-.002,SZ)),V((sx,-T-.002,SZ)),V((sx,-YT+T+.002,SZ)),V((-sx,-YT+T+.002,SZ))],M['soil'],[(0,0),(1,0),(1,.3),(0,.3)])
# ---- plants ---------------------------------------------------------------------------------------------
def leaf(c,a,tilt,rad,mat=M['leaf']):
    r=V((math.cos(a),math.sin(a),0));e1=V((-math.sin(a),math.cos(a),0));e2=r*math.cos(tilt)+V((0,0,math.sin(tilt)))
    pts=[c+e1*rad*(1 if k%2==0 else .82)*math.cos(math.tau*k/10)+e2*rad*(1 if k%2==0 else .82)*math.sin(math.tau*k/10) for k in range(10)]
    G['leaf'].poly(pts,mat,w=(.1,0));G['leaf'].poly(pts[::-1],mat,w=(.1,0))
for i in range(6):
    px=-.36+i*.144+rng.uniform(-.015,.015);py=-.115+rng.uniform(-.02,.02);base=V((px,py,SZ))
    for k in range(7):
        a=math.tau*k/7+rng.uniform(-.3,.3);d=.04+.03*(k%3)/2;leaf(base+V((d*math.cos(a),d*math.sin(a),.04+.012*(k%3))),a,rng.uniform(.5,.9),rng.uniform(.07,.095))
    for k in range(2+i%2):
        top=base+V((rng.uniform(-.045,.045),rng.uniform(-.03,.03),rng.uniform(.17,.26)));G['stem'].beam(base+V((0,0,.01)),top,.008,.008,M['stem'],grain=False)
        for f in range(5):                                                                                     # umbel: five florets in a flat head
            o=V((0,0,0)) if f==0 else V((.03*math.cos(math.tau*(f-1)/4),.03*math.sin(math.tau*(f-1)/4),-.006*(f%2)));revolve(G['bloom'],top+o,(0,0,1),[(0,0),(.022,.004),(.017,.014),(0,.018)],M['bloom'],6,phase=f)
# trailing thyme over the front lip
for i in range(10):
    x=-.4+i*.089+rng.uniform(-.02,.02);y=-YT+T+.0;pts=[V((x,y+.01,SZ+.005)),V((x+rng.uniform(-.01,.01),-YT-.018,Z-.005))]
    n=rng.randint(3,5);drop=rng.uniform(.09,.17)
    for s in range(1,n+1):pts.append(V((x+rng.uniform(-.015,.015),-YT-.03-.004*s,Z-.005-drop*s/n)))
    for a,b in zip(pts,pts[1:]):G['thyme'].beam(a,b,.006,.006,M['thyme'],grain=False)
    for p in pts[2:]:
        for s in (-1,1):leaf(p+V((s*.008,-.004,0)),-math.pi/2,rng.uniform(.8,1.3),.011,M['thyme'])
for g in G.values():g.object()
sheet.finish(HERE,'PropSillPlanter',scene,asset,dict(footprint_m=None,front='-Y',ground_contact=False,mount='sill',near=['cottage','townhouse','inn'],clearance_m=.1),[(.85,1.0),(.2,.45),(.3,.65)],'sill-planter-sheet.png',
    "bottom centre of the trough's back edge (back face flush with y=0 on the wall, front -Y, sits on a sill)",extra=dict(variants=dict(note='flowers are slot SB_GeraniumBloom (red); the Pink MI is MI_SB_GeraniumBloom_Pink')),wall=True)
