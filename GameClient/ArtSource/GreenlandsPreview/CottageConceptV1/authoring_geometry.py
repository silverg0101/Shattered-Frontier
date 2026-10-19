"""Author the generated Greenlands cottage as original, editable Blender geometry.

Metres, Z up, front -Y. Reference interpretation, not image reconstruction.
"""
import bpy,bmesh,math,random,json,hashlib,numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parent;TEX=ROOT/'Textures';rng=random.Random(90317)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
asset=bpy.data.collections.new('Greenlands Cottage — original mesh');scene.collection.children.link(asset)
MATERIALS={};MATERIAL_ROWS=[]
def pbr(name,source,tile):
    m=bpy.data.materials.new(name);m.use_nodes=True;n=m.node_tree.nodes;l=m.node_tree.links;p=n.get('Principled BSDF')
    remap=None
    if source=='plastered_wall_02':
        uv=n.new('ShaderNodeTexCoord');scale=n.new('ShaderNodeVectorMath');scale.operation='MULTIPLY';scale.inputs[1].default_value=(4.8,1,1);l.new(uv.outputs['UV'],scale.inputs[0])
        wrap=n.new('ShaderNodeVectorMath');wrap.operation='FRACTION';l.new(scale.outputs[0],wrap.inputs[0])
        crop=n.new('ShaderNodeVectorMath');crop.operation='MULTIPLY';crop.inputs[1].default_value=(.16,1,1);l.new(wrap.outputs[0],crop.inputs[0])
        remap=n.new('ShaderNodeVectorMath');remap.operation='ADD';remap.inputs[1].default_value=(.30,0,0);l.new(crop.outputs[0],remap.inputs[0])
    for channel,socket in [('Diffuse','Base Color'),('Rough','Roughness'),('nor_gl','Normal')]:
        file=TEX/(source+'_'+channel+'.jpg');image=bpy.data.images.load(str(file),check_existing=True)
        if channel!='Diffuse':image.colorspace_settings.name='Non-Color'
        tex=n.new('ShaderNodeTexImage');tex.image=image
        if remap:l.new(remap.outputs[0],tex.inputs['Vector'])
        if channel=='nor_gl':normal=n.new('ShaderNodeNormalMap');normal.inputs['Strength'].default_value=.8;l.new(tex.outputs['Color'],normal.inputs['Color']);l.new(normal.outputs['Normal'],p.inputs[socket])
        elif channel=='Diffuse':
            # Exported vertex mask keeps patina local and reproducible in UE.
            attr=n.new('ShaderNodeVertexColor');attr.layer_name='CottageWeathering'
            mix=n.new('ShaderNodeMixRGB');mix.blend_type='MULTIPLY';mix.inputs[2].default_value=(.40,.34,.24,1)
            l.new(attr.outputs['Color'],mix.inputs[0]);l.new(tex.outputs['Color'],mix.inputs[1]);l.new(mix.outputs['Color'],p.inputs[socket])
        else:l.new(tex.outputs['Color'],p.inputs[socket])
    MATERIALS[name]=m;MATERIAL_ROWS.append(dict(name=name,source=source,tile_metres=tile,uv_interior_crop=source=='plastered_wall_02'));return m
wood=pbr('Cottage_Timber','fine_grained_wood',.6);plaster=pbr('Cottage_LimePlaster','plastered_wall_02',2.2);roof=pbr('Cottage_ClayTiles','clay_roof_tiles_02',1.5);stone=pbr('Cottage_Fieldstone','medieval_wall_02',2)
def solid(name,colour,rough=.6,metal=0,emission=None):
    m=bpy.data.materials.new(name);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*colour,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
    if emission:p.inputs['Emission Color'].default_value=(*emission,1);p.inputs['Emission Strength'].default_value=1
    MATERIALS[name]=m;MATERIAL_ROWS.append(dict(name=name,colour_linear=colour,roughness=rough,metallic=metal,emission=emission));return m
iron=solid('Cottage_ForgedIron',(.035,.025,.016),.65,.85);glass=solid('Cottage_OldGlass',(.025,.045,.055),.2,0,(.005,.003,.001));soil=solid('Cottage_PlanterSoil',(.07,.045,.022),.95);warm=solid('Cottage_LanternGlow',(.7,.3,.055),.3,0,(2.5,1.1,.22))
glass.node_tree.nodes.get('Principled BSDF').inputs['Transmission Weight'].default_value=.8
clay=solid('Cottage_Earthenware',(.19,.075,.035),.65);linen=solid('Cottage_Linen',(.34,.29,.19),.96)
char=solid('Cottage_Charcoal',(.012,.009,.007),.98);ember=solid('Cottage_HearthEmber',(.14,.025,.004),.8,0,(2.8,.22,.012))

# A modeled tile receives an interior of a scanned tile, rather than a second
# miniature roof pattern. Select naturally russet patches without mortar seams.
roof_image=bpy.data.images.load(str(TEX/'clay_roof_tiles_02_Diffuse.jpg'),check_existing=True)
pixels=np.empty(len(roof_image.pixels),dtype=np.float32);roof_image.pixels.foreach_get(pixels);pixels=pixels.reshape(roof_image.size[1],roof_image.size[0],4)
tile_patches=[];moss_patches=[];patch_rng=random.Random(118)
for attempt in range(5000):
    u,v=patch_rng.uniform(.03,.93),patch_rng.uniform(.03,.90);uw,vh=.028,.045
    block=pixels[int(v*pixels.shape[0]):int((v+vh)*pixels.shape[0]),int(u*pixels.shape[1]):int((u+uw)*pixels.shape[1]),:3]
    mean=block.mean(axis=(0,1));lum=block.mean(axis=2)
    if mean[0]>.07 and mean[0]>mean[1]*1.12 and mean[1]>mean[2]*1.12 and (lum<lum.mean()*.45).mean()<.025 and lum.std()<lum.mean()*.30:tile_patches.append((u,v,uw,vh))
    elif mean[1]>mean[0]*1.02 and mean[1]>mean[2]*1.1 and lum.mean()>.06:moss_patches.append((u,v,uw,vh))
assert len(tile_patches)>10,'No suitable scanned tile interiors found'
def tile_patch(moss_probability=.05):return rng.choice(moss_patches if moss_patches and rng.random()<moss_probability else tile_patches)

class Geometry:
    def __init__(self,name):self.name=name;self.v=[];self.faces=[];self.uv=[];self.mi=[]
    def poly(self,points,material,uv=None):
        start=len(self.v);self.v.extend(tuple(p) for p in points);self.faces.append(tuple(range(start,start+len(points))));self.mi.append(list(MATERIALS).index(material.name))
        if uv is None:
            normal=(Vector(points[1])-Vector(points[0])).cross(Vector(points[2])-Vector(points[0]));axis=max(range(3),key=lambda k:abs(normal[k]));axes=[k for k in range(3) if k!=axis];tile=next((r.get('tile_metres',1) for r in MATERIAL_ROWS if r['name']==material.name),1)
            uv=[(p[axes[0]]/tile,p[axes[1]]/tile) for p in points]
        self.uv.append(uv)
    def box(self,centre,size,material,bevel=.006,rotation=None,texture_rect=None):
        # Chamfered prism: real rounded edges without thousands of modifier objects.
        sx,sy,sz=size;cut=min(bevel,sx*.2,sy*.2);corners=[(-sx/2+cut,-sy/2), (sx/2-cut,-sy/2),(sx/2,-sy/2+cut),(sx/2,sy/2-cut),(sx/2-cut,sy/2),(-sx/2+cut,sy/2),(-sx/2,sy/2-cut),(-sx/2,-sy/2+cut)] if cut else [(-sx/2,-sy/2),(sx/2,-sy/2),(sx/2,sy/2),(-sx/2,sy/2)]
        points=[];local=[]
        for z in [-sz/2,sz/2]:
            for x,y in corners:
                p=Vector((x,y,z));local.append(p.copy());p=rotation@p if rotation is not None else p;points.append(p+Vector(centre))
        count=len(corners);faces=[list(reversed(range(count))),list(range(count,count*2))]+[[i,(i+1)%count,(i+1)%count+count,i+count] for i in range(count)]
        offset=rng.random()*7 if material==wood else 0
        for indices in faces:
            coords=None;normal=(local[indices[1]]-local[indices[0]]).cross(local[indices[2]]-local[indices[0]]);dominant=max(range(3),key=lambda k:abs(normal[k]))
            if texture_rect:
                u0,v0,uw,vh=texture_rect;axes=[k for k in range(3) if k!=dominant];coords=[(u0+(local[i][axes[0]]/size[axes[0]]+.5)*uw,v0+(local[i][axes[1]]/size[axes[1]]+.5)*vh) for i in indices]
            elif material==wood:
                longest=max((k for k in range(3) if k!=dominant),key=lambda k:size[k]);other=next(k for k in range(3) if k!=longest and k!=dominant)
                coords=[(local[i][other]/.6+offset,local[i][longest]/.6+offset) for i in indices]
            self.poly([points[i] for i in indices],material,coords)
    def beam(self,a,b,width,depth=None,material=wood):
        a,b=Vector(a),Vector(b);rotation=(b-a).to_track_quat('Z','Y').to_matrix();self.box((a+b)/2,(width,depth or width,(b-a).length),material,.012,rotation)
    def vessel(self,centre,profile,material):
        # Revolved cross-section includes the inside wall and a thick rolled rim.
        cx,cy,cz=centre;segments=32
        for (ra,za),(rb,zb) in zip(profile,profile[1:]):
            for j in range(segments):
                a=math.tau*j/segments;b=math.tau*(j+1)/segments
                self.poly([(cx+ra*math.cos(a),cy+ra*math.sin(a),cz+za),(cx+ra*math.cos(b),cy+ra*math.sin(b),cz+za),(cx+rb*math.cos(b),cy+rb*math.sin(b),cz+zb),(cx+rb*math.cos(a),cy+rb*math.sin(a),cz+zb)],material,[(j/segments,za),((j+1)/segments,za),((j+1)/segments,zb),(j/segments,zb)])
    def prism(self,points,axis,thickness,material):
        points=[Vector(p) for p in points];axis=Vector(axis);normal=(points[1]-points[0]).cross(points[2]-points[0])
        if normal.dot(axis)<0:points.reverse()
        offset=axis.normalized()*thickness/2;self.poly([p+offset for p in points],material);self.poly([p-offset for p in reversed(points)],material)
        for i,a in enumerate(points):b=points[(i+1)%len(points)];self.poly([a-offset,b-offset,b+offset,a+offset],material)
    def object(self):
        mesh=bpy.data.meshes.new(self.name);mesh.from_pydata(self.v,[],self.faces);mesh.materials.clear()
        for m in MATERIALS.values():mesh.materials.append(m)
        uv=mesh.uv_layers.new(name='PhysicalScaleUV')
        for poly,coords,index in zip(mesh.polygons,self.uv,self.mi):
            poly.material_index=index
            for li,value in zip(poly.loop_indices,coords):uv.data[li].uv=value
        colour=mesh.color_attributes.new(name='CottageWeathering',type='FLOAT_COLOR',domain='CORNER')
        for poly in mesh.polygons:
            mat=list(MATERIALS.values())[poly.material_index]
            for li in poly.loop_indices:
                x,y,z=mesh.vertices[mesh.loops[li].vertex_index].co
                noise=.5+.25*math.sin(x*9.7+y*7.1)+.25*math.sin(x*3.2-y*11.3)
                damp=max(0,1-(z-.16)/.70)*(.18+.20*noise)
                soot=max(0,min(1,(z-7.2)/.6))*.55 if mat==stone and x<-2.5 else 0
                drips=0
                if mat==plaster and abs(y+3)<.22:
                    for wx in [-2.6,2.6]:
                        drips=max(drips,max(0,1-abs(x-wx)/.65)*max(0,1-abs(z-.73)/.4)*(.10+.12*noise))
                strength=min(.65,max(damp,soot,drips)) if mat in [wood,plaster,stone] else 0
                colour.data[li].color=(strength,strength,strength,1)
        mesh.update()
        # Smooth only the cloth and pottery surfaces, retaining hard timber and
        # stone edges. Weld their coincident authoring vertices before normals.
        smooth_slots={list(MATERIALS).index(name) for name in ['Cottage_Linen','Cottage_Earthenware']}
        bm=bmesh.new();bm.from_mesh(mesh);verts={v for face in bm.faces if face.material_index in smooth_slots for v in face.verts}
        if verts:
            bmesh.ops.remove_doubles(bm,verts=list(verts),dist=.000001)
            for face in bm.faces:
                if face.material_index in smooth_slots:face.smooth=True
            bm.to_mesh(mesh)
        bm.free();mesh.update();obj=bpy.data.objects.new(self.name,mesh);asset.objects.link(obj);return obj
