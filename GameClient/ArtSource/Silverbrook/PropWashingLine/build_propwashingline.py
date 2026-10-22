"""Silverbrook street prop: WASHING LINE. Two 2.4 m log posts 3.5 m apart with leaning props, a sagging rope and six hanging cloth pieces (linen, undyed wool, one indigo).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propwashingline.py
Concept: concept/washing-line-sheet.png. Origin = ground centre between the posts; the line runs along X; props lean towards +Y (back), cloth faces +-Y.
"""
import sys,math
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix
from prop_common_wns import revolve
from prop_common_street import torus
import prop_common_sheet as sheet
import bpy
scene=kit.reset(5302);asset=bpy.data.collections.new('SilverbrookPropWashingLine');scene.collection.children.link(asset)
M=dict(post=kit.scan('SB_PostBark','bark_brown_02',aged=.3),timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),rope=kit.solid('SB_HempRope',(.3,.21,.11),.95),
    linen=kit.scan('SB_Linen','hessian_380',damp=False),wool=kit.scan('SB_UndyedWool','curly_teddy_natural',damp=False),indigo=kit.scan('SB_IndigoCloth','denim_fabric_03',damp=False))
BANDS=kit.bands('weathered_brown_planks');band=lambda:kit.rng.choice(BANDS)
G=dict(posts=Geo('SM_SBWashingLine_Posts',asset),timber=Geo('SM_SBWashingLine_Props',asset),rope=Geo('SM_SBWashingLine_Rope',asset),cloth=Geo('SM_SBWashingLine_Cloth',asset))
PX,PH,SEG=1.75,2.4,10
# ---- posts, props, rope wraps --------------------------------------------------------------------------
for s in (-1,1):
    x=s*PX
    revolve(G['posts'],(x,0,0),(0,0,1),[(.075,0),(.08,.04),(.07,1.0),(.068,2.0),(.064,PH)],M['post'],SEG)
    G['timber'].beam((x,0,2.2),(x-s*.3,0,2.2),.06,.06,M['timber'],band=band())                                  # cross stub the rope is wrapped round
    G['timber'].beam((x,.58,0),(x,.07,1.55),.07,.065,M['timber'],band=band())                                  # leaning prop
    torus(G['rope'],(x,0,2.27),(1,0,0),(0,1,0),.085,.016,M['rope'],14,6);torus(G['rope'],(x,0,2.31),(1,0,0),(0,1,0),.082,.016,M['rope'],14,6)
# ---- rope with sag ------------------------------------------------------------------------------------
L,Z0,SAG=PX-.07,2.27,.2
rz=lambda x:Z0-SAG*(1-(x/L)**2)
pts=[Vector((L*(-1+2*i/28),0,rz(L*(-1+2*i/28)))) for i in range(29)]
for a,b in zip(pts,pts[1:]):G['rope'].beam(a,b,.024,.024,M['rope'],grain=False)
# ---- cloth ---------------------------------------------------------------------------------------------
def panel(x0,x1,_,h,y,mat,phase,amp=.03,nx=5,nz=6):
    """Hanging rectangle x0..x1 whose top edge follows the rope; fold depth grows towards the hem. Front and back surfaces."""
    def pt(i,j):
        u,v=i/nx,j/nz;x=x0+(x1-x0)*u;top=rz(x)-.02
        return Vector((x,y+amp*v*math.sin(phase+u*7.2)+.012*v*v,top-h*v-.02*v*math.sin(phase*2+u*11)))
    for i in range(nx):
        for j in range(nz):
            q=[pt(i,j),pt(i+1,j),pt(i+1,j+1),pt(i,j+1)];G['cloth'].poly(q,mat,w=(.15+.1*j/nz,0));G['cloth'].poly(q[::-1],mat,w=(.15+.1*j/nz,0))
def tunic(cx,body,sleeve,h,sh,mat,y,phase):
    panel(cx-body/2,cx+body/2,0,h,y,mat,phase)
    for s in (-1,1):                                           # short sleeves hanging from the shoulders
        a,b=(cx+s*body/2),(cx+s*(body/2+sleeve));panel(min(a,b),max(a,b),0,sh,y+.01,mat,phase+1.3,nx=2,nz=3,amp=.015)
tunic(-.12,.38,.12,.66,.22,M['indigo'],.02,.5);tunic(-.78,.36,.12,.62,.2,M['wool'],-.02,1.7)
panel(-1.62,-1.1,0,.72,.03,M['linen'],.2,.045);panel(.28,.7,0,.58,-.02,M['linen'],2.1)
panel(.78,.94,0,.5,.0,M['wool'],.9,.02,2,5);panel(.97,1.13,0,.5,.015,M['wool'],1.4,.02,2,5);panel(.78,1.13,0,.14,.0,M['wool'],1.1,.02,5,2)   # trousers: two legs and a waist
panel(1.2,1.52,0,.46,-.03,M['linen'],3.0,.03)
for x in (-1.6,-1.12,-1.0,-.57,-.28,.0,.3,.68,.8,1.1,1.22,1.5):G['timber'].box((x,0,rz(x)-.012),(.016,.022,.075),M['timber'],0,grain=True,band=band())   # pegs
for g in G.values():g.object()
sheet.finish(HERE,'PropWashingLine',scene,asset,dict(footprint_m=None,front='-Y',ground_contact=True,near=['cottage','farmhouse','yard'],clearance_m=.6),[(3.3,4.0),(.5,1.1),(2.3,2.6)],'washing-line-sheet.png',
    'ground centre between the posts (line along X, props lean towards +Y)')
