"""Silverbrook street prop: FLOWER TUB. 0.5 m oak half-barrel, iron hoops, damp soil, red geraniums 0.3 m above the rim, trailing thyme.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propflowertub.py
Concept: concept/flower-tub-sheet.png. Origin = bottom centre; round, front -Y. Flowers are the SB_GeraniumBloom slot (shared with
PropSillPlanter): red in the mesh, pink via MI_SB_GeraniumBloom_Pink.
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
from prop_common_wns import revolve
import prop_common_sheet as sheet
import bpy
scene=kit.reset(5306);asset=bpy.data.collections.new('SilverbrookPropFlowerTub');scene.collection.children.link(asset)
M=dict(timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    soil=kit.solid('SB_DampSoil',(.03,.021,.015),.95),leaf=kit.solid('SB_GeraniumLeaf',(.075,.19,.04),.6),stem=kit.solid('SB_GeraniumStem',(.07,.12,.035),.7),
    bloom=kit.solid('SB_GeraniumBloom',(.5,.02,.02),.55),thyme=kit.solid('SB_ThymeSprig',(.06,.12,.05),.7))
sheet.add_variants('SB_GeraniumBloom',dict(Pink=[.62,.13,.2]))
BANDS=kit.bands('weathered_brown_planks');band=lambda:kit.rng.choice(BANDS)
G=dict(timber=Geo('SM_SBFlowerTub_Staves',asset),iron=Geo('SM_SBFlowerTub_Hoops',asset),soil=Geo('SM_SBFlowerTub_Soil',asset),leaf=Geo('SM_SBFlowerTub_Leaves',asset),
    stem=Geo('SM_SBFlowerTub_Stems',asset),bloom=Geo('SM_SBFlowerTub_Blooms',asset),thyme=Geo('SM_SBFlowerTub_Thyme',asset))
rng=kit.rng;V=Vector
H=.45;N=20
def radius(z):return .215+.035*math.sin(math.pi*.5*min(z/.3,1))-.006*max(0,(z-.3)/.15)   # bellies out, slight tuck at the rim
def frame(a,p,q):
    """Box rotation: columns radial, tangent, along the stave (right-handed)."""
    ax=(q-p).normalized();t=V((-math.sin(a),math.cos(a),0));n=t.cross(ax).normalized();t=ax.cross(n)
    return Matrix((n,t,ax)).transposed()
# ---- staves: 20 boards in two facets each, outer face on the bulge ----------------------------------------
ZS=(0,.26,H)
for i in range(N):
    a=math.tau*i/N+rng.uniform(-.01,.01);top=H+rng.uniform(-.008,.006);bd=band();zs=ZS[:-1]+(top,)
    for z0,z1 in zip(zs,zs[1:]):
        r0,r1=radius(z0)-.011,radius(z1)-.011;p=V((r0*math.cos(a),r0*math.sin(a),z0));q=V((r1*math.cos(a),r1*math.sin(a),z1))
        G['timber'].box((p+q)/2,(.022,math.tau*radius((z0+z1)/2)/N*.97,(q-p).length),M['timber'],.004,frame(a,p,q),grain=True,band=bd)
G['timber'].poly([V((.2*math.cos(math.tau*k/N),.2*math.sin(math.tau*k/N),.02)) for k in range(N)],M['timber'])   # floor (closes the bottom from above)
# ---- hoops: three iron bands just proud of the staves ---------------------------------------------------------
for zc,hh in ((.06,.045),(.27,.04),(.40,.04)):
    for k in range(24):
        a=math.tau*(k+.5)/24;r=radius(zc)+.004;w=math.tau*r/24*1.04
        p=V((r*math.cos(a),r*math.sin(a),zc-hh/2));q=V((r*math.cos(a),r*math.sin(a),zc+hh/2))
        G['iron'].box((p+q)/2,(.007,w,hh),M['iron'],0,frame(a,p,q))
# ---- soil, 5 cm below the rim ---------------------------------------------------------------------------------
SZ=H-.05;G['soil'].poly([V(((radius(SZ)-.02)*math.cos(math.tau*k/N),(radius(SZ)-.02)*math.sin(math.tau*k/N),SZ)) for k in range(N)],M['soil'])
# ---- geraniums: a ring of five plants and one in the middle ---------------------------------------------------
def leaf(c,a,tilt,rad,mat=M['leaf'],geo='leaf'):
    r=V((math.cos(a),math.sin(a),0));e1=V((-math.sin(a),math.cos(a),0));e2=r*math.cos(tilt)+V((0,0,math.sin(tilt)))
    pts=[c+e1*rad*(1 if k%2==0 else .82)*math.cos(math.tau*k/8)+e2*rad*(1 if k%2==0 else .82)*math.sin(math.tau*k/8) for k in range(8)]
    G[geo].poly(pts,mat,w=(.1,0));G[geo].poly(pts[::-1],mat,w=(.1,0))
plants=[(0,0)]+[(.12*math.cos(math.tau*k/5+.3),.12*math.sin(math.tau*k/5+.3)) for k in range(5)]
for i,(px,py) in enumerate(plants):
    base=V((px,py,SZ))
    for k in range(6):
        a=math.tau*k/6+rng.uniform(-.3,.3);d=.04+.03*(k%3)/2;leaf(base+V((d*math.cos(a),d*math.sin(a),.07+.03*(k%3))),a,rng.uniform(.4,.8),rng.uniform(.09,.11))
    for k in range(2):
        top=base+V((rng.uniform(-.05,.05),rng.uniform(-.05,.05),rng.uniform(.27,.33)));G['stem'].beam(base+V((0,0,.01)),top,.008,.008,M['stem'],grain=False)
        for f in range(5):                                                                                     # umbel: five florets in a flat head
            o=V((0,0,0)) if f==0 else V((.04*math.cos(math.tau*(f-1)/4),.04*math.sin(math.tau*(f-1)/4),-.008*(f%2)))
            revolve(G['bloom'],top+o,(0,0,1),[(0,0),(.032,.005),(.024,.018),(0,.023)],M['bloom'],5,phase=f)
# ---- trailing thyme over the rim ------------------------------------------------------------------------------
for i in range(10):
    a=math.tau*i/10+rng.uniform(-.15,.15);c,s=math.cos(a),math.sin(a);rr=radius(H)
    pts=[V(((rr-.05)*c,(rr-.05)*s,SZ+.005)),V(((rr+.012)*c,(rr+.012)*s,H+.01))]
    n=rng.randint(3,4);drop=rng.uniform(.10,.20)
    for k in range(1,n+1):e=rr+.02+.003*k;pts.append(V((e*c+rng.uniform(-.012,.012)*s,e*s-rng.uniform(-.012,.012)*c,H-drop*k/n)))
    for p,q in zip(pts,pts[1:]):G['thyme'].beam(p,q,.006,.006,M['thyme'],grain=False)
    for k,p in enumerate(pts[2:]):sd=1 if k%2 else -1;leaf(p+V((-s*sd*.008,c*sd*.008,0)),a,rng.uniform(.8,1.3),.012,M['thyme'],'thyme')
for g in G.values():g.object()
sheet.finish(HERE,'PropFlowerTub',scene,asset,dict(footprint_m=None,front='-Y',ground_contact=True,near=['cottage','townhouse','inn','market'],clearance_m=.2),[(.48,.62),(.48,.62),(.7,.9)],'flower-tub-sheet.png',
    'bottom centre (round tub, front -Y)',extra=dict(variants=dict(note='flowers are slot SB_GeraniumBloom (red); the Pink MI is MI_SB_GeraniumBloom_Pink')))
