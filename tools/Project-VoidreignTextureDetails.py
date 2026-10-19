"""Bake fixed-view AI texture edits onto visible head/saddle surfaces of a review copy."""
import argparse,json,sys,math
from pathlib import Path
import bpy
from mathutils import Vector,Matrix
from mathutils.bvhtree import BVHTree
p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--guides',required=True)
p.add_argument('--head',required=True);p.add_argument('--saddle',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);out=Path(a.output);out.mkdir(parents=True,exist_ok=False)
(out/'textures').mkdir();(out/'previews').mkdir()
bpy.ops.wm.open_mainfile(filepath=str(Path(a.source).resolve()));s=bpy.context.scene
asset=bpy.data.collections['Voidreign | Generated asset'];objects=[o for o in asset.all_objects if o.type=='MESH']
assert len(objects)==1,'This prototype expects one reviewed Pixal mesh'
obj=objects[0];mesh=obj.data
world=[obj.matrix_world@v.co for v in mesh.vertices]
bvh=BVHTree.FromPolygons(world,[tuple(face.vertices) for face in mesh.polygons],all_triangles=True)
specs=json.loads(Path(a.guides).read_text());counts={}
mat=obj.data.materials[0];nodes=mat.node_tree.nodes;links=mat.node_tree.links
bsdf=nodes.get('Principled BSDF');base=bsdf.inputs['Base Color'].links[0].from_socket
def smooth(x):
    x=max(0,min(1,x));return x*x*(3-2*x)
for spec,image_path in zip(specs,[a.head,a.saddle]):
    name=spec['name'];camera=bpy.data.objects.new(name+' texture projection',bpy.data.cameras.new(name+' texture projection'))
    s.collection.objects.link(camera);camera.matrix_world=Matrix(spec['camera_matrix_world']);camera.data.type='ORTHO';camera.data.ortho_scale=spec['scale']
    inverse=camera.matrix_world.inverted();scale=spec['scale'];weights=[]
    normals=obj.matrix_world.to_3x3().inverted().transposed()
    for vertex,point in zip(mesh.vertices,world):
        local=inverse@point;u=.5+local.x/scale;v=.5+local.y/scale
        edge=smooth(min(u,v,1-u,1-v)/.08)
        if name=='Head':region=smooth((-.60-point.y)/.18)*smooth((point.z-1.45)/.2)
        else:region=smooth((point.y+.70)/.15)*smooth((.90-point.y)/.15)*smooth((point.z-.88)/.2)*smooth((2.18-point.z)/.12)
        facing=smooth(((normals@vertex.normal).normalized().x-.12)/.45)
        weight=edge*region*facing
        if weight>.001:
            hit,_,_,_=bvh.ray_cast(Vector((7,point.y,point.z)),Vector((-1,0,0)),15)
            if hit is None or (hit-point).length>.004:weight=0
        weights.append(weight)
    attribute=mesh.color_attributes.new(name=name+' projection weight',type='FLOAT_COLOR',domain='POINT')
    for color,weight in zip(attribute.data,weights):color.color=(weight,weight,weight,1)
    counts[name]=sum(weight>.01 for weight in weights)
    coords=nodes.new('ShaderNodeTexCoord');coords.object=camera
    multiply=nodes.new('ShaderNodeVectorMath');multiply.operation='MULTIPLY';multiply.inputs[1].default_value=(1/scale,1/scale,0)
    links.new(coords.outputs['Object'],multiply.inputs[0])
    offset=nodes.new('ShaderNodeVectorMath');offset.operation='ADD';offset.inputs[1].default_value=(.5,.5,0)
    links.new(multiply.outputs[0],offset.inputs[0])
    texture=nodes.new('ShaderNodeTexImage');texture.image=bpy.data.images.load(str(Path(image_path).resolve()));texture.extension='CLIP'
    links.new(offset.outputs[0],texture.inputs['Vector'])
    color=nodes.new('ShaderNodeVertexColor');color.layer_name=attribute.name
    mix=nodes.new('ShaderNodeMixRGB');mix.blend_type='MIX';links.new(color.outputs['Color'],mix.inputs[0]);links.new(base,mix.inputs[1]);links.new(texture.outputs['Color'],mix.inputs[2]);base=mix.outputs[0]
emission=nodes.new('ShaderNodeEmission');links.new(base,emission.inputs['Color']);output=next(n for n in nodes if n.type=='OUTPUT_MATERIAL');links.new(emission.outputs[0],output.inputs['Surface'])
image=bpy.data.images.new('Voidreign-HeadSaddle-BaseColor-4K',width=4096,height=4096,alpha=True)
target=nodes.new('ShaderNodeTexImage');target.image=image;nodes.active=target
bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=1;s.render.bake.margin=12
bpy.ops.object.bake(type='EMIT')
image.filepath_raw=str(out/'textures'/'BaseColor-4K.png');image.file_format='PNG';image.save()
links.new(bsdf.outputs[0],output.inputs['Surface']);links.new(target.outputs['Color'],bsdf.inputs['Base Color'])
# Keep roughness/metallic maps; this trial refines albedo only.
for index,im in enumerate(bpy.data.images):
    if im.has_data and im!=image and im.source=='FILE':
        im.filepath_raw=str(out/'textures'/f'Preserved-{index}.png');im.file_format='PNG';im.save()
s.render.engine='BLENDER_EEVEE'
bpy.ops.object.select_all(action='DESELECT')
for o in asset.objects:o.select_set(True)
bpy.context.view_layer.objects.active=obj
bpy.ops.export_scene.gltf(filepath=str(out/'Voidreign.glb'),export_format='GLB',use_selection=True)
bpy.ops.export_scene.fbx(filepath=str(out/'Voidreign.fbx'),use_selection=True,object_types={'MESH','EMPTY'},path_mode='COPY',embed_textures=True)
for name in ['Beauty','Front','Side','Rear']:
    s.camera=bpy.data.objects[name];s.render.filepath=str(out/'previews'/(name+'.png'));bpy.ops.render.render(write_still=True)
s.camera=bpy.data.objects['Beauty'];bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(out/'Voidreign.blend'))
review=json.loads((Path(a.source).parent/'asset-review.json').read_text());(out/'asset-review.json').write_text(json.dumps(review,indent=2))
(out/'projection-report.json').write_text(json.dumps(dict(status='complete',weighted_vertices=counts,method='Fixed camera projection, spatial/normal feather and BVH visibility; EMIT bake into existing UVs',scope='Visible +X side of head and saddle only; albedo only; opposite/hidden surfaces retained'),indent=2))
