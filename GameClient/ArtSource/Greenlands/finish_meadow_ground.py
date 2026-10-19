"""World-scale, non-repeating meadow ground; preserve close-up scan detail."""
import unreal as u,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;P='/Game/Frontier/Environment/Greenlands'
m=u.load_asset(P+'/Arrival/Materials/M_LivingMeadow');assert m
u.MaterialEditingLibrary.delete_all_material_expressions(m)
def ex(cls,**props):
    node=u.MaterialEditingLibrary.create_material_expression(m,cls)
    for k,v in props.items():node.set_editor_property(k,v)
    return node
def link(a,b,pin,output=''):assert u.MaterialEditingLibrary.connect_material_expressions(a,output,b,pin)
def scalar(value):return ex(u.MaterialExpressionConstant,r=value)
def multiply(a,b):
    n=ex(u.MaterialExpressionMultiply);link(a,n,'A');link(b,n,'B');return n
def lerp(a,b,weight):
    n=ex(u.MaterialExpressionLinearInterpolate);link(a,n,'A');link(b,n,'B');link(weight,n,'Alpha');return n
world=ex(u.MaterialExpressionWorldPosition)
xy=ex(u.MaterialExpressionComponentMask,r=True,g=True,b=False,a=False);link(world,xy,'')
def uv(scale,angle,offset):
    n=multiply(xy,scalar(scale))
    if angle:
        import math
        x=ex(u.MaterialExpressionComponentMask,r=True,g=False,b=False,a=False);link(n,x,'')
        y=ex(u.MaterialExpressionComponentMask,r=False,g=True,b=False,a=False);link(n,y,'')
        c=math.cos(angle*math.tau);s=math.sin(angle*math.tau)
        rx=ex(u.MaterialExpressionAdd);link(multiply(x,scalar(c)),rx,'A');link(multiply(y,scalar(-s)),rx,'B')
        ry=ex(u.MaterialExpressionAdd);link(multiply(x,scalar(s)),ry,'A');link(multiply(y,scalar(c)),ry,'B')
        n=ex(u.MaterialExpressionAppendVector);link(rx,n,'A');link(ry,n,'B')
    add=ex(u.MaterialExpressionAdd);link(n,add,'A');link(ex(u.MaterialExpressionConstant2Vector,r=offset[0],g=offset[1]),add,'B');return add
coords=[uv(1/250,0,(.17,.41)),uv(1/293.25,.25,(7.13,3.74)),uv(1/343.5,.125,(13.33,-6.2))]
def sample(path,coord,normal=False):
    t=u.load_asset(path);assert t,path
    n=ex(u.MaterialExpressionTextureSample,texture=t)
    if normal:n.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    link(coord,n,'UVs');return n
def noise(metres):
    position=multiply(world,scalar(1/(metres*100)))
    n=ex(u.MaterialExpressionNoise,scale=1,levels=2,quality=1,output_min=0,output_max=1,turbulence=False)
    link(position,n,'');return n
blend_a=noise(13);blend_b=noise(31);macro=noise(85)
base_path=P+'/Arrival/Textures/leafy_grass_Diffuse'
colors=[sample(base_path,c) for c in coords]
color=lerp(lerp(colors[0],colors[1],blend_a),colors[2],blend_b)
# Moss/soil variation is metres-wide, while leaf detail keeps its physical scale.
moss_path=P+'/Village/Megascans/Surfaces/MossyGround/T_MossyGround_A'
if u.EditorAssetLibrary.does_asset_exist(moss_path):
    moss=sample(moss_path,coords[1]);weight=multiply(noise(22),scalar(.4));color=lerp(color,moss,weight)
tint_a=ex(u.MaterialExpressionConstant3Vector,constant=u.LinearColor(.45,.80,.32,1))
tint_b=ex(u.MaterialExpressionConstant3Vector,constant=u.LinearColor(.70,.95,.48,1))
color=multiply(color,lerp(tint_a,tint_b,macro))
assert u.MaterialEditingLibrary.connect_material_property(color,'',u.MaterialProperty.MP_BASE_COLOR)
normal=sample(P+'/Arrival/Textures/leafy_grass_nor_dx',coords[0],True)
assert u.MaterialEditingLibrary.connect_material_property(normal,'RGB',u.MaterialProperty.MP_NORMAL)
assert u.MaterialEditingLibrary.connect_material_property(scalar(.94),'',u.MaterialProperty.MP_ROUGHNESS)
u.MaterialEditingLibrary.recompile_material(m);assert u.EditorAssetLibrary.save_loaded_asset(m)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);levels.load_level('/Game/Frontier/Maps/Test/Greenlands_ArtReview')
for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor,u.DirectionalLight):
        actor.light_component.set_editor_property('intensity',6);actor.light_component.set_editor_property('use_temperature',True);actor.light_component.set_editor_property('temperature',6500)
    elif isinstance(actor,u.SkyLight):actor.light_component.set_editor_property('intensity',1.4)
assert levels.save_current_level()
(ROOT/'meadow-ground.json').write_text(json.dumps(dict(detail_tile_metres=[2.5,2.9325,3.435],rotations_degrees=[0,90,45],blend_noise_metres=[13,31],macro_variation_metres=85,close_detail='Original scanned leaf/soil detail retained; world-space multi-scale blending breaks repetition',reference='https://www.fab.com/listings/6a5ae8db-d80f-4b23-b276-87da390cfe56'),indent=2))

