"""Shared Blender authoring kit for Silverbrook buildings.

Metres, Z up, front faces -Y, origin at ground centre. Materials are CC0 Poly
Haven scans from ./Textures (materials-provenance.json) mapped at their measured
physical size. Small parts (one slate, one dressed stone, one beam) sample the
interior of a single element in the scan instead of shrinking the whole texture.
Vertex colour 'Weathering': R darkens (damp, soot), G grows lichen on slate.
"""
import bpy,math,random,json,numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
KIT=Path(__file__).resolve().parent;TEX=KIT/'Textures'
SIZE={r['asset']:r['physical_size_metres'][0] for r in json.loads((KIT/'materials-provenance.json').read_text())}
UP=Vector((0,0,1));rng=random.Random(0);DOORS=[];MATS={};SCALE={};DAMP=set();AGED={};ROWS=[]  # ROWS: engine-neutral material table for the UE library

def reset(seed):
    bpy.ops.wm.read_factory_settings(use_empty=True);rng.seed(seed);DOORS.clear();MATS.clear();SCALE.clear();DAMP.clear();AGED.clear();ROWS.clear()
    scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1;return scene

def _sock(node,ident):return next(s for s in [*node.inputs,*node.outputs] if s.identifier==ident)
def _mix(nt,fac,a,b,blend='MIX'):
    m=nt.nodes.new('ShaderNodeMix');m.data_type='RGBA';m.blend_type=blend
    for ident,value in [('Factor_Float',fac),('A_Color',a),('B_Color',b)]:
        s=_sock(m,ident)
        if isinstance(value,bpy.types.NodeSocket):nt.links.new(value,s)
        else:s.default_value=value
    return _sock(m,'Result_Color')
def _image(asset,channel):
    image=bpy.data.images.load(str(TEX/f'{asset}_{channel}.jpg'),check_existing=True)
    if channel!='Diffuse':image.colorspace_settings.name='Non-Color'
    return image

def scan(name,asset,dark=(.42,.36,.28),lichen=None,normal=1.0,damp=True,aged=0,metal=False):
    """aged: baseline R darkening (weathered timber, sun-faded paint) on every face.
    metal: the Rough map carries metalness in G (Poly Haven model sets from acquire_models.py)."""
    if name in MATS:return MATS[name]
    m=bpy.data.materials.new(name);m.use_nodes=True;m.use_backface_culling=True;nt=m.node_tree;n=nt.nodes;link=nt.links.new;p=n.get('Principled BSDF')
    uv=n.new('ShaderNodeUVMap');uv.uv_map='UVMap'
    def tex(channel):
        t=n.new('ShaderNodeTexImage');t.image=_image(asset,channel);link(uv.outputs['UV'],t.inputs['Vector']);return t
    diffuse,rough,nor=tex('Diffuse'),tex('Rough'),tex('nor_gl')
    w=n.new('ShaderNodeVertexColor');w.layer_name='Weathering';sep=n.new('ShaderNodeSeparateColor');link(w.outputs['Color'],sep.inputs['Color'])
    colour=diffuse.outputs['Color'];colour=_mix(nt,sep.outputs['Red'],colour,_mix(nt,1,colour,(*dark,1),'MULTIPLY'))
    if lichen:
        # Crustose lichen: the scanned lichen pattern, warmed to the ochre
        # Xanthoria tone common on old slate, masked per element by G.
        uv2=n.new('ShaderNodeUVMap');uv2.uv_map='WorldUV';lt=n.new('ShaderNodeTexImage');lt.image=_image(lichen,'Diffuse');link(uv2.outputs['UV'],lt.inputs['Vector'])
        bw=n.new('ShaderNodeRGBToBW');link(lt.outputs['Color'],bw.inputs['Color'])
        mask=n.new('ShaderNodeMapRange');mask.inputs['From Min'].default_value=.03;mask.inputs['From Max'].default_value=.08;link(bw.outputs['Val'],mask.inputs['Value'])
        mul=n.new('ShaderNodeMath');mul.operation='MULTIPLY';link(mask.outputs['Result'],mul.inputs[0]);link(sep.outputs['Green'],mul.inputs[1])
        colour=_mix(nt,mul.outputs['Value'],colour,_mix(nt,1,lt.outputs['Color'],(1.0,.6,.2,1),'MULTIPLY'))
    link(colour,p.inputs['Base Color'])
    rs=n.new('ShaderNodeSeparateColor');link(rough.outputs['Color'],rs.inputs['Color']);link(rs.outputs['Red'],p.inputs['Roughness'])
    if metal:link(rs.outputs['Green'],p.inputs['Metallic'])
    nm=n.new('ShaderNodeNormalMap');nm.uv_map='UVMap';nm.inputs['Strength'].default_value=normal;link(nor.outputs['Color'],nm.inputs['Color']);link(nm.outputs['Normal'],p.inputs['Normal'])
    MATS[name]=m;SCALE[name]=SIZE[asset];AGED[name]=aged;ROWS.append(dict(name=name,scan=asset,dark=list(dark),lichen=lichen,normal=normal,metal=metal))
    if damp:DAMP.add(name)
    return m

_MODELS={}
def model(col,mid,matrix,aged=0,name=None):
    """Place a Poly Haven model (acquire_models.py) into col with matrix (location/rotation/scale).
    Keeps the model's UVs and smooth normals; each texture set becomes scan material SB_PH_<set> (metal-aware)."""
    if mid not in _MODELS:
        before=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(KIT/'Models'/mid/f'{mid}.gltf'))
        new=[o for o in bpy.data.objects if o not in before];meshes=[o for o in new if o.type=='MESH'];data=[]
        for o in meshes:
            me=o.data.copy();me.transform(o.matrix_world)
            for i,m in enumerate(me.materials):
                img=next((nd.image for nd in m.node_tree.nodes if nd.type=='TEX_IMAGE' and nd.image and '_diff' in nd.image.name),None) if m and m.use_nodes else None
                tset=Path(img.filepath or img.name).stem.split('_diff')[0] if img else None
                me.materials[i]=scan('SB_PH_'+tset,'ph_'+tset,damp=False,metal=True) if tset else solid('SB_PH_Plain',(.3,.3,.3),.6)
            if 'WorldUV' not in me.uv_layers:me.uv_layers.new(name='WorldUV')
            me.uv_layers[0].name='UVMap'
            data.append(me)
        for o in new:bpy.data.objects.remove(o,do_unlink=True)
        _MODELS[mid]=data
    objs=[]
    for k,me in enumerate(_MODELS[mid]):
        m=me.copy()
        if 'Weathering' not in m.color_attributes:m.color_attributes.new('Weathering','FLOAT_COLOR','CORNER')
        m.color_attributes['Weathering'].data.foreach_set('color',[aged,0,0,1]*len(m.loops))
        o=bpy.data.objects.new(f'{name or mid}_{k}',m);o.matrix_world=Matrix(matrix);col.objects.link(o);objs.append(o)
    return objs

def export_parts(parts,out,mesh_prefix):
    """Moving parts (wheels, gate leaves, doors) as their own FBX, origin at the pivot, for separate Unreal meshes.
    parts: [(obj, name, pivot, axis, motion, travel)] in final model space; motion 'rotate' (continuous), 'swing'
    (travel = degrees about axis) or 'slide' (travel = metres along axis); the optional 7th item names the modelled
    state ('open' or 'closed') and travel leads to the other one.
    Returns manifest rows; the caller leaves these objects out of the main FBX."""
    rows=[];keep=[o for o in bpy.data.objects if o.select_get()];active=bpy.context.view_layer.objects.active
    for obj,name,pivot,axis,motion,travel,*state in parts:
        tmp=bpy.data.objects.new(obj.name+'_Part',obj.data.copy());tmp.data.transform(Matrix.Translation(-Vector(pivot)));obj.users_collection[0].objects.link(tmp)
        for o in bpy.data.objects:o.select_set(o is tmp)
        bpy.context.view_layer.objects.active=tmp;path=f'{out}_{name}.fbx'
        bpy.ops.export_scene.fbx(filepath=path,use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
        me=tmp.data;bpy.data.objects.remove(tmp);bpy.data.meshes.remove(me)
        rows.append(dict(name=name,fbx=Path(path).name,unreal_mesh=f'{mesh_prefix}_{name}',pivot_m=[round(v,4) for v in pivot],axis=list(axis),motion=motion,travel=travel,modelled=(state or [None])[0]))
    room=_vestibules(parts,mesh_prefix)
    for o in bpy.data.objects:o.select_set(o in keep or o is room)
    bpy.context.view_layer.objects.active=active
    return rows

def _slab_axis(xy):
    """Long horizontal axis of a thin slab (door leaf): the edge direction of its minimum-width bounding rectangle.
    Straps and rings sticking out of one face can't tilt it the way a principal axis or farthest vertex does."""
    pts=sorted(set((round(x,5),round(y,5)) for x,y in xy));hull=[]
    for seq in (pts,pts[::-1]):  # monotone chain
        part=[]
        for p in seq:
            while len(part)>1 and (part[-1][0]-part[-2][0])*(p[1]-part[-2][1])-(part[-1][1]-part[-2][1])*(p[0]-part[-2][0])<=0:part.pop()
            part.append(p)
        hull+=part[:-1]
    best=None
    for (ax,ay),(bx,by) in zip(hull,hull[1:]+hull[:1]):
        d=Vector((bx-ax,by-ay,0))
        if d.length<1e-6:continue
        d.normalize();nrm=Vector((-d.y,d.x,0));w=max(nrm.x*x+nrm.y*y for x,y in hull)-min(nrm.x*x+nrm.y*y for x,y in hull)
        if best is None or w<best[0]:best=(w,d)
    return best[1]

def _vestibules(parts,mesh_prefix,depth=1.4):
    """A dark entry room behind every closed door, so an opened door shows an interior instead of the hollow shell.
    Coplanar neighbouring leaves (double doors) share one room. Returns the object (exported with the main mesh) or None."""
    groups=[]
    for obj,name,pivot,axis,motion,travel,*state in parts:
        if motion!='swing' or (state or [None])[0]!='closed':continue
        pivot=Vector(pivot);vs=[obj.matrix_world@v.co for v in obj.data.vertices]
        u=_slab_axis([(v.x,v.y) for v in vs]);u=u if sum(v.x-pivot.x for v in vs)*u.x+sum(v.y-pivot.y for v in vs)*u.y>0 else -u  # hinge -> free edge
        n=(Matrix.Rotation(math.radians(travel),3,'Z')@u).normalized();t=n.cross(UP)   # n: inward (where the leaf swings)
        g=next((g for g in groups if g['n'].dot(n)>.99 and abs(g['d']-pivot.dot(n))<.15 and abs(g['z0']-pivot.z)<.15),None)  # same doorway
        if not g:g=dict(n=n,t=t,d=pivot.dot(n),z0=pivot.z,z1=pivot.z,s=[]);groups.append(g)
        g['z1']=max(g['z1'],max(v.z for v in vs));g['s'].append((min(v.dot(t) for v in vs),max(v.dot(t) for v in vs)))
    if not groups:return None
    G=Geo(mesh_prefix.replace('SM_Silverbrook','SM_SB')+'_EntryRooms',parts[0][0].users_collection[0]);mat=solid('SB_EntryRoom',(.05,.045,.04),.95)
    def quad(pts,want):
        pts=[Vector(p) for p in pts]
        if (pts[1]-pts[0]).cross(pts[2]-pts[0]).dot(want)<0:pts.reverse()
        G.poly(pts,mat)
    for g in groups:
        spans=sorted(g['s']);merged=[list(spans[0])]
        for a,b in spans[1:]:
            if a<=merged[-1][1]+.1:merged[-1][1]=max(merged[-1][1],b)
            else:merged.append([a,b])
        n,t=g['n'],g['t']
        for a,b in merged:
            a,b=a+.01,b-.01;d0,d1=g['d']+.01,g['d']+max(depth,b-a+.1);z0,z1=g['z0']+.002,g['z1']
            P=lambda s_,d_,z_,t=t,n=n:Vector((t.x*s_+n.x*d_,t.y*s_+n.y*d_,z_))
            quad([P(a,d1,z0),P(b,d1,z0),P(b,d1,z1),P(a,d1,z1)],-n)        # back wall faces the door
            quad([P(a,d0,z0),P(a,d1,z0),P(a,d1,z1),P(a,d0,z1)],t)         # side walls face inward
            quad([P(b,d0,z0),P(b,d1,z0),P(b,d1,z1),P(b,d0,z1)],-t)
            quad([P(a,d0,z0),P(b,d0,z0),P(b,d1,z0),P(a,d1,z0)],UP)        # floor
            quad([P(a,d0,z1),P(b,d0,z1),P(b,d1,z1),P(a,d1,z1)],-UP)       # ceiling
    return G.object()

def split_doors(out,mesh_prefix):
    """Export every kit.door leaf as a part (modelled closed; opens 90 deg inward about its left hinge) and deselect it.
    Call right before the main FBX export; returns the manifest rows."""
    parts=[]
    for k,d in enumerate(DOORS,1):
        obj=bpy.data.objects[d['geo'].name];vs=[obj.matrix_world@v.co for v in obj.data.vertices];r,o=d['r'],d['o']
        # Hinge: the leaf's bottom corner at the -r jamb on its inner (-o) face; frame axes are orthonormal.
        pivot=r*min(v.dot(r) for v in vs)+o*min(v.dot(o) for v in vs)+UP*min(v.z for v in vs)
        parts.append((obj,f'Door{k}',pivot,(0,0,1),'swing',90 if UP.cross(r).dot(o)<0 else -90,'closed'))  # opens inward (-o)
    rows=export_parts(parts,out,mesh_prefix)
    for obj,*_ in parts:obj.select_set(False)
    return rows

def solid(name,colour,rough=.6,metal=0,emission=None,strength=0):
    if name in MATS:return MATS[name]
    m=bpy.data.materials.new(name);m.use_nodes=True;m.use_backface_culling=True;p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*colour,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
    if emission:p.inputs['Emission Color'].default_value=(*emission,1);p.inputs['Emission Strength'].default_value=strength
    MATS[name]=m;SCALE[name]=1;ROWS.append(dict(name=name,colour=list(colour),roughness=rough,metallic=metal,emission=list(emission) if emission else None,strength=strength));return m

def _pixels(asset):
    image=_image(asset,'Diffuse');px=np.empty(len(image.pixels),np.float32);image.pixels.foreach_get(px)
    return px.reshape(image.size[1],image.size[0],4)[:,:,:3].mean(axis=2)
def patches(asset,size,dark=.6,frac=.015,std=.25,bright=0,count=80,seed=7):
    """UV rects (u,v,du,dv) holding the interior of one scanned element: no dark joints, even tone."""
    lum=_pixels(asset);H,W=lum.shape;r=random.Random(seed);out=[];du,dv=size;median=np.median(lum)
    for _ in range(20000):
        u,v=r.uniform(0,1-du),r.uniform(0,1-dv);b=lum[int(v*H):int((v+dv)*H),int(u*W):int((u+du)*W)];m=b.mean()
        if (b<m*dark).mean()<frac and b.std()<m*std and m>median*bright:out.append((u,v,du,dv))
        if len(out)>=count:break
    assert len(out)>8,f'{asset}: only {len(out)} clean element interiors';return out
def bands(asset,inset=.18,minimum=.035):
    """V ranges of single planks in a scan of horizontal boards (seams = dark rows)."""
    profile=np.convolve(_pixels(asset).mean(axis=1),np.ones(7)/7,'same');seam=profile<np.percentile(profile,10);N=len(profile);out=[];start=None
    for i,s in enumerate(list(seam)+[True]):
        if not s and start is None:start=i
        elif s and start is not None:
            if i-start>minimum*N:pad=(i-start)*inset;out.append(((start+pad)/N,(i-pad)/N))
            start=None
    assert out,f'{asset}: no plank bands';return out

def _project(points,scale):
    normal=(points[1]-points[0]).cross(points[2]-points[0]);dom=max(range(3),key=lambda k:abs(normal[k]));a,b=[k for k in range(3) if k!=dom]
    return [(p[a]/scale,p[b]/scale) for p in points]

class Geo:
    """One exported mesh object; every face carries its own material, UVs and weathering."""
    def __init__(self,name,collection):self.name=name;self.col=collection;self.v=[];self.f=[];self.uv=[];self.mat=[];self.w=[];self.smooth=[]
    def poly(self,points,mat,uv=None,w=(0,0),smooth=False,swap=False):
        points=[Vector(p) for p in points]
        if uv is None:
            uv=_project(points,SCALE[mat.name])
            if swap:uv=[(b,a) for a,b in uv]
        start=len(self.v);self.v+=points;self.f.append(tuple(range(start,start+len(points))));self.uv.append(uv);self.mat.append(mat);self.w.append(w);self.smooth.append(smooth)
    def box(self,centre,size,mat,bevel=.008,rot=None,rect=None,band=None,grain=False,swap=False,w=(0,0)):
        """Chamfered prism. UVs: world projection (continuous walls), `grain` along the long
        axis (timber, optionally confined to one plank `band`), or a single-element `rect`."""
        sx,sy,sz=size;cut=min(bevel,sx*.2,sy*.2)
        corners=[(-sx/2+cut,-sy/2),(sx/2-cut,-sy/2),(sx/2,-sy/2+cut),(sx/2,sy/2-cut),(sx/2-cut,sy/2),(-sx/2+cut,sy/2),(-sx/2,sy/2-cut),(-sx/2,-sy/2+cut)] if cut else [(-sx/2,-sy/2),(sx/2,-sy/2),(sx/2,sy/2),(-sx/2,sy/2)]
        local=[Vector((x,y,z)) for z in (-sz/2,sz/2) for x,y in corners];world=[(rot@p if rot is not None else p)+Vector(centre) for p in local]
        k=len(corners);faces=[list(reversed(range(k))),list(range(k,2*k))]+[[i,(i+1)%k,(i+1)%k+k,i+k] for i in range(k)]
        off=(rng.random()*5,rng.random()*5);mirrored=rot is not None and rot.determinant()<0  # e.g. frame() bases
        vol=0;c=Vector(centre)  # volume about the centre: absolute coordinates lose precision far from the origin
        for idx in faces:
            normal=(local[idx[1]]-local[idx[0]]).cross(local[idx[2]]-local[idx[0]]);dom=max(range(3),key=lambda a:abs(normal[a]));axes=[a for a in range(3) if a!=dom];uv=None
            if rect:
                u0,v0,du,dv=rect;uv=[(u0+(local[i][axes[0]]/size[axes[0]]+.5)*du,v0+(local[i][axes[1]]/size[axes[1]]+.5)*dv) for i in idx]
            elif grain:
                s=SCALE[mat.name];long=max(axes,key=lambda a:size[a]);other=next(a for a in axes if a!=long)
                uv=[(local[i][long]/s+off[0],band[0]+(local[i][other]/size[other]+.5)*(band[1]-band[0]) if band else local[i][other]/s+off[1]) for i in idx]
            if mirrored:idx,uv=idx[::-1],(uv[::-1] if uv else uv)   # keep outward winding under a reflection
            self.poly([world[i] for i in idx],mat,uv,w,swap=swap)
            vol+=sum((world[idx[0]]-c).dot((world[idx[j]]-c).cross(world[idx[j+1]]-c)) for j in range(1,len(idx)-1))
        assert vol>0,'inside-out box'  # Unreal culls back faces; Blender would hide this
    def beam(self,a,b,width,depth=None,mat=None,**kw):
        a,b=Vector(a),Vector(b);rot=(b-a).to_track_quat('Z','Y').to_matrix();kw.setdefault('grain',True);self.box((a+b)/2,(width,depth or width,(b-a).length),mat,.012,rot,**kw)
    def prism(self,points,axis,thickness,mat,**kw):
        points=[Vector(p) for p in points];axis=Vector(axis).normalized()
        # Newell normal: the first three points of a concave outline (arch spandrels) can point either way.
        if sum((a.cross(points[(i+1)%len(points)]) for i,a in enumerate(points)),Vector()).dot(axis)<0:points.reverse()
        o=axis*thickness/2;self.poly([p+o for p in points],mat,**kw);self.poly([p-o for p in reversed(points)],mat,**kw)
        for i,a in enumerate(points):b=points[(i+1)%len(points)];self.poly([a-o,b-o,b+o,a+o],mat,**kw)
    def vessel(self,centre,profile,mat,segments=24):
        cx,cy,cz=centre
        for (ra,za),(rb,zb) in zip(profile,profile[1:]):
            for j in range(segments):
                a,b=math.tau*j/segments,math.tau*(j+1)/segments
                self.poly([(cx+ra*math.cos(a),cy+ra*math.sin(a),cz+za),(cx+ra*math.cos(b),cy+ra*math.sin(b),cz+za),(cx+rb*math.cos(b),cy+rb*math.sin(b),cz+zb),(cx+rb*math.cos(a),cy+rb*math.sin(a),cz+zb)],mat,[(j/segments,za),((j+1)/segments,za),((j+1)/segments,zb),(j/segments,zb)],smooth=True)
    def object(self):
        mats=list(dict.fromkeys(self.mat));mesh=bpy.data.meshes.new(self.name);mesh.from_pydata([tuple(v) for v in self.v],[],self.f)
        for m in mats:mesh.materials.append(m)
        uv=mesh.uv_layers.new(name='UVMap');uv2=mesh.uv_layers.new(name='WorldUV');col=mesh.color_attributes.new('Weathering','FLOAT_COLOR','CORNER')
        for poly,coords,mat,w,smooth in zip(mesh.polygons,self.uv,self.mat,self.w,self.smooth):
            poly.material_index=mats.index(mat);poly.use_smooth=smooth
            for li,c in zip(poly.loop_indices,coords):
                uv.data[li].uv=c;p=mesh.vertices[mesh.loops[li].vertex_index].co;uv2.data[li].uv=(p.x/2+p.y/2,p.z/2) if abs(poly.normal.z)<.5 else (p.x/2,p.y/2)
                damp=max(0,1-p.z/.65)*(.30+.15*math.sin(p.x*7.3+p.y*5.1)) if mat.name in DAMP else 0
                r=min(.8,max(w[0],damp,AGED.get(mat.name,0)));col.data[li].color=(r,w[1],0,1)
        mesh.update();obj=bpy.data.objects.new(self.name,mesh);self.col.objects.link(obj);return obj

def frame(c,o):
    """Local wall frame: a along the wall (viewer's right), d outward, z up."""
    c,o=Vector(c),Vector(o).normalized();r=UP.cross(o);rot=Matrix((r,o,UP)).transposed()
    return (lambda a,d,z:c+r*a+o*d+UP*z),rot

def wall(G,mat,c,o,half,height,openings=(),th=.3,arch=None,**kw):
    """Solid wall, outer face through c (bottom centre). openings: (a, width, bottom, top).
    arch=(a,width,spring) rounds the head of that opening with stone spandrels."""
    P,R=frame(c,o);cuts=sorted([-half,half]+[x for a,w,_,_ in openings for x in (a-w/2,a+w/2)])
    for a0,a1 in zip(cuts,cuts[1:]):
        hole=next((h for h in openings if abs(h[0]-(a0+a1)/2)<h[1]/2),None);spans=[(0,height)] if not hole else [(0,hole[2]),(hole[3],height)]
        for z0,z1 in spans:
            if z1-z0>.005:G.box(P((a0+a1)/2,-th/2,(z0+z1)/2),(a1-a0,th,z1-z0),mat,0,R,**kw)
    if arch:
        a,w,spring=arch;rad=w/2
        for side in (-1,1):
            arc=[P(a+side*rad*math.cos(t),0,spring+rad*math.sin(t)) for t in np.linspace(0,math.pi/2,9)]
            outline=arc+[P(a+side*rad,0,spring+rad)];G.prism([p-Vector(o)*th/2 for p in outline],o,th,mat,**kw)

def window(G,M,c,o,w,h,shutters=True,planter=False,wall_th=.3,band=None):
    """Leaded casement pair with oak frame, sill, optional board shutters and planter.
    c: bottom centre of the glazed area on the outer wall face."""
    P,R=frame(c,o);t=.09;fd=wall_th+.04
    def B(geo,a,d,z,size,mat,bevel=.008,**kw):geo.box(P(a,d,z),size,mat,bevel,R,**kw)
    for a in (-(w/2+t/2),w/2+t/2):B(G['timber'],a,-wall_th/2+.02,h/2,(t,fd,h+2*t),M['timber'],grain=True,band=band)
    for z in (h+t/2,-t/2):B(G['timber'],0,-wall_th/2+.02,z,(w,fd,t),M['timber'],grain=True,band=band)
    B(G['timber'],0,-.09,h/2,(.06,.12,h),M['timber'],grain=True,band=band)
    pw=(w-.06)/2
    for ac in (-(pw/2+.03),pw/2+.03):
        B(G['detail'],ac,-.14,h/2,(pw,.01,h),M['glass'],0)
        # Diamond quarries: two families of lead cames clipped to the pane.
        lo,hi=ac-pw/2,ac+pw/2;slope=1.7;step=.10*slope
        for sign in (1,-1):
            for k in range(-30,31):
                b=k*step;pts=[]
                for a in (lo,hi):
                    z=sign*slope*(a-ac)+b
                    if 0<=z<=h:pts.append((a,z))
                for z in (0,h):
                    a=ac+(z-b)/(sign*slope)
                    if lo<a<hi:pts.append((a,z))
                if len(pts)>=2:
                    (a0,z0),(a1,z1)=pts[0],pts[1]
                    if math.hypot(a1-a0,z1-z0)>.02:G['detail'].beam(P(a0,-.128,z0),P(a1,-.128,z1),.009,.012,M['lead'],grain=False)
    B(G['timber'],0,.05,-t-.025,(w+2*t+.12,.14,.05),M['timber'],grain=True,band=band)
    if shutters:
        sw=w/2+t
        for side in (-1,1):
            a=side*(w/2+t+sw/2+.015);B(G['detail'],a,.03,h/2,(sw,.04,h+2*t),M['shutter'],.004,swap=True)
            for z in (.18*h,.82*h):B(G['detail'],a,.06,z,(sw-.05,.025,.08),M['shutter'],.004)
            G['detail'].beam(P(a-side*(sw/2-.06),.062,.22*h),P(a+side*(sw/2-.06),.062,.78*h),.07,.022,M['shutter'],grain=False)
            for z in (.18*h,.82*h):B(G['detail'],a-side*sw*.15,.078,z,(sw*.72,.008,.035),M['iron'],.002)
    if planter:
        B(G['timber'],0,.15,-t-.2,(w+2*t+.1,.24,.2),M['planks'],.006)
        B(G['detail'],0,.15,-t-.095,(w+2*t+.04,.19,.02),M['soil'],0)
        for a in (-w/2,w/2):G['timber'].beam(P(a,.03,-t-.55),P(a,.25,-t-.3),.06,.06,M['timber'],band=band)

def door(G,M,c,o,w,spring,stone_rects,wall_th=.3,band=None):
    """Round-headed ledged plank door in a dressed-stone surround (jamb quoins + voussoirs)."""
    P,R=frame(c,o);rad=w/2
    # The leaf (boards, straps, ring) is its own mesh so it can open: hinge on the left jamb seen from outside.
    D=Geo(f"{G['detail'].name}_Door{len(DOORS)+1}",G['detail'].col);G[D.name]=D
    outline=[P(-rad,-.2,0),P(rad,-.2,0)]+[P(rad*math.cos(t),-.2,spring+rad*math.sin(t)) for t in np.linspace(0,math.pi,17)]
    D.prism(outline,o,.07,M['door'])
    for z in (.35,spring-.1):
        D.box(P(-rad*.15,-.15,z),(w*.8,.012,.05),M['iron'],.004,R)
    for k in range(10):
        t=math.tau*k/10;D.box(P(rad*.55+.06*math.cos(t),-.13,1.0+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
    D.box(P(rad*.55,-.155,1.06),(.07,.01,.07),M['iron'],.004,R)
    DOORS.append(dict(geo=D,r=(R@Vector((1,0,0))).normalized(),o=Vector(o).normalized(),width=w))
    z=0;i=0
    while z<spring-.05:
        hgt=min(.31,spring-z);qw=.30 if i%2 else .44
        for side in (-1,1):G['stone'].box(P(side*(rad-.012+qw/2),-wall_th/2+.025,z+hgt/2),(qw,wall_th+.05,hgt-.012),M['ashlar'],.012,R,rect=rng.choice(stone_rects))
        z+=hgt;i+=1
    n=9;depth=.30
    for k in range(n):
        t=math.pi*(k+.5)/n;rm=rad-.012+depth/2;radial=R@Vector((math.cos(t),0,math.sin(t)));tang=R@Vector((-math.sin(t),0,math.cos(t)));o_=Vector(o).normalized()
        G['stone'].box(P(rm*math.cos(t),-wall_th/2+.025,spring+rm*math.sin(t)),(math.pi*rm/n-.014,wall_th+.05,depth),M['ashlar'],.012,Matrix((tang,o_,radial)).transposed(),rect=rng.choice(stone_rects))

def lantern(G,M,top,arm=None):
    """Forged-iron lantern hanging from `top`; with arm=(wall point, outward vector) a wall bracket."""
    top=Vector(top)
    if arm:
        base,out=Vector(arm[0]),Vector(arm[1]).normalized();tip=top+UP*.08
        G['detail'].beam(base,tip,.025,.025,M['iron'],grain=False);G['detail'].beam(base-UP*.3,base+(tip-base)*.55,.02,.02,M['iron'],grain=False)
        G['detail'].box(base-UP*.12,(.08,.08,.36),M['iron'],.004)
    G['detail'].beam(top+UP*.08,top-UP*.06,.012,.012,M['iron'],grain=False)
    body=top-UP*.27
    G['detail'].box(body,(.15,.15,.24),M['lamp'],0)
    for x in (-1,1):
        for y in (-1,1):G['detail'].box(body+Vector((x*.075,y*.075,0)),(.016,.016,.25),M['iron'],0)
    for dz,s in ((-.13,.19),(.13,.2)):G['detail'].box(body+UP*dz,(s,s,.025),M['iron'],.004)
    G['detail'].prism([body+Vector((-.1,0,.14)),body+Vector((.1,0,.14)),body+Vector((0,0,.24))],(0,1,0),.2,M['iron'])

def tile_plane(geo,O,e,u,L,lo,hi,mat,rects,covered=(),size=(.3,.42,.022),exposure=.2,lichen=lambda p:0,tone=(.1,.45)):
    """Lay staggered, individually tilted slates on a roof plane.
    O on the eave line, e along the eave, u up-slope; tiles span lo(t)..hi(t) along e at slope distance t.
    A slate is omitted when every corner lies inside a `covered` volume (valleys, dormers, stacks)."""
    O,e,u=Vector(O),Vector(e).normalized(),Vector(u).normalized();n=e.cross(u);tw,tl,th=size
    tilt=math.atan2(th,tl*.55);uu=u*math.cos(tilt)-n*math.sin(tilt);nn=n*math.cos(tilt)+u*math.sin(tilt);k=0
    while k*exposure<L-.03:
        t0=k*exposure;t1=min(t0+tl,L+.02);l0,h0=max(lo(t0),lo(t1)),min(hi(t0),hi(t1));s=l0-(k%2)*tw/2-rng.uniform(0,.04)
        while s<h0-.03:
            a0,a1=max(s,l0),min(s+tw,h0);s+=tw
            if a1-a0<.05:continue
            if all(any(c(O+e*x+u*y) for c in covered) for x in (a0,a1) for y in (t0,t1)):continue
            rot=Matrix.Rotation(rng.uniform(-.025,.025),3,n)@Matrix((e,uu,nn)).transposed();centre=O+e*(a0+a1)/2+u*(t0+t1)/2+n*th*1.2
            geo.box(centre,(a1-a0-.008,t1-t0,th*rng.uniform(.8,1.2)),mat,.004,rot,rect=rng.choice(rects),w=(rng.uniform(*tone),lichen(centre)))
        k+=1

def deck(geo,O,e,u,L,lo,hi,mat,covered=(),cell=.4,th=.035):
    """Roof boarding under the slates, cut to the plane outline; omitted where fully covered."""
    O,e,u=Vector(O),Vector(e).normalized(),Vector(u).normalized();n=e.cross(u);nt=max(1,round(L/cell))
    for i in range(nt):
        t0,t1=i*L/nt,(i+1)*L/nt;ns=max(1,round(max(hi(t0)-lo(t0),.01)/cell))
        for j in range(ns):
            f0,f1=j/ns,(j+1)/ns;quad=[O+e*(lo(t)+(hi(t)-lo(t))*f)+u*t for t,f in ((t0,f0),(t0,f1),(t1,f1),(t1,f0))]
            if all(any(c(p) for c in covered) for p in quad):continue
            geo.poly(quad,mat);geo.poly([p-n*th for p in reversed(quad)],mat)
