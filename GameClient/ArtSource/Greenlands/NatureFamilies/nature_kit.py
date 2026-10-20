"""Shared Blender helpers for the Greenlands nature families (pine, broadleaf, boulder, bedrock slab).

A family builder runs as
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P <Family>/build_<family>.py
Units: metres, Z up, origin at the base centre of each variant, no ground in any mesh.
Materials are described once as plain dicts (SPECS in each builder). The same dicts are written to the family
manifest, drive the Blender look here, and are rebuilt as Unreal materials by ../import_nature.py.
"""
import bpy,json,math,hashlib
import numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parent;SRC=ROOT/'Sources'


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def load_gltf(asset):
    """Import a Poly Haven glTF; returns {name: object} of the NEW mesh objects. The stray 'Cube' every Poly Haven file carries is dropped."""
    before=set(bpy.data.objects.keys());bpy.ops.import_scene.gltf(filepath=str(SRC/asset/(asset+'.gltf')))
    new=[o for o in bpy.data.objects if o.name not in before]
    for o in new:
        if o.name.split('.')[0]=='Cube' or o.type!='MESH':bpy.data.objects.remove(o,do_unlink=True)
    return {o.name:o for o in new if o.name in bpy.data.objects}


def arrays(o):
    """(verts (n,3), tris (m,3), uv (m,3,2), material index (m,)) of an object, object space, triangulated."""
    me=o.data;me.calc_loop_triangles();m=len(me.loop_triangles)
    v=np.empty(len(me.vertices)*3,np.float32);me.vertices.foreach_get('co',v);v=v.reshape(-1,3)
    t=np.empty(m*3,np.int32);me.loop_triangles.foreach_get('vertices',t);t=t.reshape(-1,3)
    mi=np.empty(m,np.int32);me.loop_triangles.foreach_get('material_index',mi)
    lt=np.empty(m*3,np.int32);me.loop_triangles.foreach_get('loops',lt)
    uvd=np.empty(len(me.loops)*2,np.float32);me.uv_layers[0].uv.foreach_get('vector',uvd)
    return v,t,uvd.reshape(-1,2)[lt].reshape(m,3,2),mi


def make(name,v,t,uv,mi,mats,vcol=None):
    """Object from arrays. vcol: optional (n,4) per-vertex colour, stored as colour attribute 'Col'. UVs are per triangle corner."""
    me=bpy.data.meshes.new(name);me.from_pydata(np.asarray(v,np.float32),[],np.asarray(t,np.int32))
    me.uv_layers.new(name='UVMap').uv.foreach_set('vector',np.asarray(uv,np.float32).reshape(-1,2).ravel())
    me.polygons.foreach_set('material_index',np.asarray(mi,np.int32))
    for m in mats:me.materials.append(m)
    if vcol is not None:vcol_set_mesh(me,vcol)
    me.shade_smooth();me.update();o=bpy.data.objects.new(name,me);bpy.context.scene.collection.objects.link(o);return o


def vcol_set_mesh(me,rgba):
    if 'Col' in me.color_attributes:me.color_attributes.remove(me.color_attributes['Col'])
    a=me.color_attributes.new('Col','FLOAT_COLOR','POINT');a.data.foreach_set('color',np.asarray(rgba,np.float32).ravel())


def tricount(o):
    o.data.calc_loop_triangles();return len(o.data.loop_triangles)


def decimate(o,target):
    n=tricount(o)
    if n<=target:return o
    mod=o.modifiers.new('dec','DECIMATE');mod.ratio=target/n;mod.use_collapse_triangulate=True
    bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name);return o


def join(objs,name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0]
    if len(objs)>1:bpy.ops.object.join()
    o=bpy.context.object;o.name=name;o.data.name=name;return o


def verts(o):
    v=np.empty(len(o.data.vertices)*3,np.float32);o.data.vertices.foreach_get('co',v);return v.reshape(-1,3)


def to_base_centre(o,centre_xy=None,extras=()):
    """Apply transforms; lowest point -> z=0, footprint centre (or centre_xy) -> origin."""
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    v=verts(o);lo,hi=v.min(0),v.max(0);c=((lo[0]+hi[0])/2,(lo[1]+hi[1])/2) if centre_xy is None else centre_xy
    M=Matrix.Translation((-c[0],-c[1],-lo[2]));o.data.transform(M);o.data.update()
    for x in extras:x.data.transform(M);x.data.update()
    return o


# ---------- materials ----------
def image(path,srgb):
    im=bpy.data.images.load(str(path),check_existing=True);im.colorspace_settings.name='sRGB' if srgb else 'Non-Color';return im


def build_material(spec):
    """Blender look for a material spec dict.
    keys: name; base, normal (GL, path under NatureFamilies/), arm (R AO, G roughness, B metal) or rough (R); uv_scale, uv_offset;
    tint (rgb multiply), sat; normal_strength; blend:{same keys + amount, contrast} layered by vertex colour 'Col'.R;
    alpha (path, R channel) for masked cards. Spec is the contract with import_nature.py."""
    m=bpy.data.materials.new(spec['name']);m.use_nodes=True;nt=m.node_tree;nt.nodes.clear();L=nt.links.new

    def N(kind,x,y):
        n=nt.nodes.new(kind);n.location=(x,y);return n
    out=N('ShaderNodeOutputMaterial',1500,0);bsdf=N('ShaderNodeBsdfPrincipled',1200,0);L(bsdf.outputs[0],out.inputs[0])
    uv=N('ShaderNodeTexCoord',-1100,0)

    def layer(s,y):
        mp=N('ShaderNodeMapping',-900,y);sc=s.get('uv_scale',[1,1]);of=s.get('uv_offset',[0,0])
        mp.inputs['Scale'].default_value=(sc[0],sc[1],1);mp.inputs['Location'].default_value=(of[0],of[1],0);L(uv.outputs['UV'],mp.inputs['Vector'])

        def tex(path,srgb,yy):
            t=N('ShaderNodeTexImage',-650,yy);t.image=image(ROOT/path,srgb);L(mp.outputs['Vector'],t.inputs['Vector']);return t
        col=tex(s['base'],True,y+250).outputs['Color']
        if s.get('tint'):
            mx=N('ShaderNodeMix',-300,y+250);mx.data_type='RGBA';mx.blend_type='MULTIPLY';mx.inputs['Factor'].default_value=1;L(col,mx.inputs['A']);mx.inputs['B'].default_value=(*s['tint'],1);col=mx.outputs['Result']
        if s.get('sat',1)!=1:
            hs=N('ShaderNodeHueSaturation',-100,y+250);hs.inputs['Saturation'].default_value=s['sat'];L(col,hs.inputs['Color']);col=hs.outputs['Color']
        nm=N('ShaderNodeNormalMap',-300,y);nm.inputs['Strength'].default_value=s.get('normal_strength',1);L(tex(s['normal'],False,y).outputs['Color'],nm.inputs['Color'])
        sp=N('ShaderNodeSeparateColor',-300,y-250);L(tex(s['arm'] if 'arm' in s else s['rough'],False,y-250).outputs['Color'],sp.inputs['Color'])
        return col,nm.outputs['Normal'],sp.outputs['Green' if 'arm' in s else 'Red']
    col,nor,rough=layer(spec,500)
    if 'blend' in spec:
        bs=spec['blend'];c2,n2,r2=layer(bs,-800)
        vc=N('ShaderNodeVertexColor',200,-300);vc.layer_name='Col';sep=N('ShaderNodeSeparateColor',350,-300);L(vc.outputs['Color'],sep.inputs['Color'])
        mul=N('ShaderNodeMath',500,-300);mul.operation='MULTIPLY';L(sep.outputs['Red'],mul.inputs[0]);mul.inputs[1].default_value=bs.get('amount',1)
        ss=N('ShaderNodeMapRange',650,-300);ss.interpolation_type='SMOOTHSTEP';c=bs.get('contrast',.25)
        ss.inputs['From Min'].default_value=.5-c;ss.inputs['From Max'].default_value=.5+c;L(mul.outputs[0],ss.inputs['Value']);w=ss.outputs['Result']
        mc=N('ShaderNodeMix',900,400);mc.data_type='RGBA';L(w,mc.inputs['Factor']);L(col,mc.inputs['A']);L(c2,mc.inputs['B']);col=mc.outputs['Result']
        mn=N('ShaderNodeMix',900,150);mn.data_type='VECTOR';L(w,mn.inputs['Factor']);L(nor,mn.inputs['A']);L(n2,mn.inputs['B']);nor=mn.outputs['Result']
        mr=N('ShaderNodeMix',900,-100);mr.data_type='FLOAT';L(w,mr.inputs['Factor']);L(rough,mr.inputs['A']);L(r2,mr.inputs['B']);rough=mr.outputs['Result']
    L(col,bsdf.inputs['Base Color']);L(nor,bsdf.inputs['Normal']);L(rough,bsdf.inputs['Roughness'])
    if spec.get('alpha'):
        a=N('ShaderNodeTexImage',-650,900);a.image=image(ROOT/spec['alpha'],False);L(uv.outputs['UV'],a.inputs['Vector'])
        sp=N('ShaderNodeSeparateColor',-300,900);L(a.outputs['Color'],sp.inputs['Color']);L(sp.outputs['Red'],bsdf.inputs['Alpha'])
        m.surface_render_method='DITHERED'
    m.use_backface_culling=False
    return m


# ---------- render ----------
def render_setup(res=(1800,900),samples=64):
    sc=bpy.context.scene;sc.render.engine='BLENDER_EEVEE';sc.render.resolution_x,sc.render.resolution_y=res
    sc.eevee.taa_render_samples=samples;sc.view_settings.view_transform='AgX';sc.view_settings.exposure=-.3
    w=bpy.data.worlds.new('w');sc.world=w;w.use_nodes=True;bg=w.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.56,.58,.6,1);bg.inputs['Strength'].default_value=.85
    sun=bpy.data.lights.new('sun','SUN');sun.energy=3.2;sun.color=(1,.95,.88);sun.angle=math.radians(1.5);so=bpy.data.objects.new('sun',sun);sc.collection.objects.link(so)
    so.rotation_euler=(math.radians(50),0,math.radians(-30));return sc


def add_ground(size=300,colour=(.2,.19,.17)):
    bpy.ops.mesh.primitive_plane_add(size=size);g=bpy.context.object;g.name='RENDER_ONLY_ground'
    m=bpy.data.materials.new('ground');m.use_nodes=True;b=m.node_tree.nodes['Principled BSDF'];b.inputs['Base Color'].default_value=(*colour,1);b.inputs['Roughness'].default_value=.95
    g.data.materials.append(m);return g


def add_human(x,y):
    """1.8 m scale figure, render only."""
    m=bpy.data.materials.new('human');m.use_nodes=True;b=m.node_tree.nodes['Principled BSDF'];b.inputs['Base Color'].default_value=(.75,.12,.08,1)
    bpy.ops.mesh.primitive_cylinder_add(vertices=24,radius=.2,depth=1.45,location=(x,y,.725));a=bpy.context.object;a.data.materials.append(m)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=.115,location=(x,y,1.68));h=bpy.context.object;h.data.materials.append(m);a.name=h.name='RENDER_ONLY_scale_1.8m'


def camera(loc,target,ortho=None,lens=60):
    cd=bpy.data.cameras.new('cam');co=bpy.data.objects.new('cam',cd);bpy.context.scene.collection.objects.link(co);bpy.context.scene.camera=co
    if ortho:cd.type='ORTHO';cd.ortho_scale=ortho
    else:cd.lens=lens
    co.location=loc;co.rotation_euler=(Vector(target)-Vector(loc)).to_track_quat('-Z','Y').to_euler();return co


def render(path):
    bpy.context.scene.render.filepath=str(path);bpy.ops.render.render(write_still=True)


def frustum(name,cx,cy,r0,r1,h,sides=12):
    """Convex trunk collision prism (Unreal UCX_ convention: name it UCX_<mesh>_NN). Not renderable."""
    v=[];f=[]
    for k in range(sides):
        a=2*math.pi*k/sides;v+=[(cx+r0*math.cos(a),cy+r0*math.sin(a),0),(cx+r1*math.cos(a),cy+r1*math.sin(a),h)]
    for k in range(sides):
        k2=(k+1)%sides;f.append((2*k,2*k2,2*k2+1,2*k+1))
    f.append(tuple(2*k for k in reversed(range(sides))));f.append(tuple(2*k+1 for k in range(sides)))
    me=bpy.data.meshes.new(name);me.from_pydata(v,[],f);me.update();o=bpy.data.objects.new(name,me);bpy.context.scene.collection.objects.link(o);o.hide_render=True;return o


def export_fbx(o,path,extra=()):
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    for x in extra:x.select_set(True)
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},colors_type='LINEAR',axis_forward='-Y',axis_up='Z',mesh_smooth_type='OFF',path_mode='STRIP',add_leaf_bones=False)


def sha256(p):
    h=hashlib.sha256()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1<<20),b''):h.update(b)
    return h.hexdigest()


# ---------- shared bookkeeping ----------
def provenance(ids):
    """Rows of Sources/provenance.json (page, licence, authors, file urls and md5) for the given Poly Haven ids."""
    rows={r['asset']:r for r in json.loads((SRC/'provenance.json').read_text())};return [rows[i] for i in ids]


def moss_mask(o,seed=1,low=.45,high=.85,breakup=.55):
    """Per-vertex moss weight in colour R: upward-facing normals (z) with two octaves of positional noise to break the edge. Needs >~5k verts for detail."""
    v=verts(o);o.data.calc_normals_split() if hasattr(o.data,'calc_normals_split') else None
    nz=np.empty(len(v)*3,np.float32);o.data.vertex_normals.foreach_get('vector',nz);nz=nz.reshape(-1,3)[:,2]
    rng=np.random.default_rng(seed);ph=rng.uniform(0,6.28,(4,3))
    def n(freq,k):return .5+.5*np.sin(v[:,0]*freq*1.0+ph[k,0])*np.sin(v[:,1]*freq*1.3+ph[k,1])*np.sin(v[:,2]*freq*.9+ph[k,2])
    noise=.6*n(2.1,0)+.4*n(7.3,1)
    up=np.clip((nz-low)/(high-low),0,1);up=up*up*(3-2*up)
    mask=np.clip(up*(1-breakup+breakup*2*noise),0,1)
    rgba=np.zeros((len(v),4),np.float32);rgba[:,0]=mask;rgba[:,3]=1;vcol_set_mesh(o.data,rgba);return float(mask.mean())


def frame(cx,width,look_z=1.0,lens=60,elev_deg=14,height_extra=0.0,side=-1):
    """Perspective camera on the -Y (side=-1) or +Y side that fits `width` metres at the target distance."""
    d=(width*lens/36.0);a=math.radians(elev_deg)
    return camera((cx,side*d*math.cos(a),d*math.sin(a)+look_z+height_extra),(cx,0,look_z),lens=lens)


def gltf_material_specs(asset):
    """{material name: spec dict} from a Poly Haven glTF: texture files (relative to NatureFamilies/), KHR_texture_transform as uv_scale/uv_offset,
    alpha mode. Matches the colour/normal/ARM packing of the model downloads."""
    g=json.loads((SRC/asset/(asset+'.gltf')).read_text());rel=lambda i:'Sources/'+asset+'/'+g['images'][g['textures'][i['index']]['source']]['uri'];out={}
    for m in g['materials']:
        pbr=m.get('pbrMetallicRoughness',{});base=pbr.get('baseColorTexture');arm=pbr.get('metallicRoughnessTexture');nor=m.get('normalTexture')
        if not(base and arm and nor):continue
        s=dict(name=m['name'],base=rel(base),normal=rel(nor),arm=rel(arm),two_sided=m.get('doubleSided',False),alpha_mode=m.get('alphaMode','OPAQUE'))
        tr=base.get('extensions',{}).get('KHR_texture_transform')
        if tr:s['uv_scale']=tr.get('scale',[1,1]);s['uv_offset']=tr.get('offset',[0,0])
        out[m['name']]=s
    return out


def components(tris,nverts):
    """Connected-component label per triangle (vectorised union-find with pointer jumping); tris (m,3) vertex indices."""
    L=np.arange(nverts);a=np.r_[tris[:,0],tris[:,1],tris[:,2]];b=np.r_[tris[:,1],tris[:,2],tris[:,0]]
    while True:
        la=L[a];lb=L[b];m=np.minimum(la,lb);N=L.copy();np.minimum.at(N,la,m);np.minimum.at(N,lb,m);N=N[N]
        if (N==L).all():break
        L=N
    return L[tris[:,0]]
