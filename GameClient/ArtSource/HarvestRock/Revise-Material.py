"""Bounded visual correction: remove regular crack grid and rebake base colour."""
import bpy
import importlib.util
from pathlib import Path
root = Path(bpy.data.filepath).parent
spec=importlib.util.spec_from_file_location('harvest_rock_authoring', root.parents[2]/'tools'/'Create-HarvestRock.py')
authoring=importlib.util.module_from_spec(spec)
spec.loader.exec_module(authoring)
for name, interior in (('M_RockWeathered_Authoring', False), ('M_RockFracture_Authoring', True)):
    mat = bpy.data.materials.get(name) or authoring.rock_material(name, interior)
    mat.use_fake_user=True
    nt = mat.node_tree
    ramp = next(n for n in nt.nodes if n.type=='VALTORGB')
    ramp.color_ramp.elements[0].color = (0.012,0.018,0.019,1) if not interior else (0.036,0.044,0.046,1)
    ramp.color_ramp.elements[1].color = (0.10,0.12,0.112,1) if not interior else (0.17,0.18,0.17,1)
    coarse = next(n for n in nt.nodes if n.type=='TEX_NOISE' and n.inputs['Scale'].default_value<10)
    threshold = next(n for n in nt.nodes if n.type=='MATH')
    threshold.operation='GREATER_THAN'
    threshold.inputs[1].default_value=0.70
    nt.links.new(coarse.outputs['Fac'],threshold.inputs[0])
    mix=next(n for n in nt.nodes if n.type=='MIX_RGB')
    mix.inputs[2].default_value=(0.105,0.039,0.012,1)
    source = bpy.data.images.load(str(root/'Textures'/'T_HarvestRock_InteriorBaseColor.png'), check_existing=True)
    photo = nt.nodes.new('ShaderNodeTexImage')
    photo.image = source
    photo.projection = 'BOX'
    photo.projection_blend = 0.3
    coords = next(n for n in nt.nodes if n.type=='TEX_COORD')
    nt.links.new(coords.outputs['Object'], photo.inputs['Vector'])
    bs = next(n for n in nt.nodes if n.type=='BSDF_PRINCIPLED')
    nt.links.new(photo.outputs['Color'], bs.inputs['Base Color'])
    target=next((n for n in nt.nodes if n.type=='TEX_IMAGE' and n.image and n.image.name=='T_HarvestRock_BaseColor'),None)
    if target is None:
        target=nt.nodes.new('ShaderNodeTexImage')
        target.image=bpy.data.images['T_HarvestRock_BaseColor']
    nt.nodes.active=target
bpy.ops.object.select_all(action='DESELECT')
assets=[o for o in bpy.data.objects if o.name.startswith('SM_HarvestRock_')]
indices={o.name:[p.material_index for p in o.data.polygons] for o in assets}
for obj in assets:
    obj.hide_set(False)
    obj.hide_render=False
    obj.select_set(True)
    obj.data.materials.clear()
    obj.data.materials.append(bpy.data.materials['M_RockWeathered_Authoring'])
    obj.data.materials.append(bpy.data.materials['M_RockFracture_Authoring'])
    for p,i in zip(obj.data.polygons,indices[obj.name]):
        p.material_index=i
bpy.context.view_layer.objects.active=assets[0]
bpy.ops.object.bake(type='DIFFUSE')
image=bpy.data.images['T_HarvestRock_BaseColor']
image.filepath_raw=str(root/'Textures'/'T_HarvestRock_BaseColor.png')
image.save()
image.filepath='//Textures/T_HarvestRock_BaseColor.png'
for obj in assets:
    obj.data.materials.clear()
    obj.data.materials.append(bpy.data.materials['M_HarvestRock_Exterior'])
    obj.data.materials.append(bpy.data.materials['M_HarvestRock_Fracture'])
    for p,i in zip(obj.data.polygons,indices[obj.name]):
        p.material_index=i
    obj.select_set(False)
    obj.hide_set(True)
    obj.hide_render=True
bpy.ops.wm.save_as_mainfile(filepath=str(root/'HarvestRock.blend'))
print('ROCK_MATERIAL_REVISED')
