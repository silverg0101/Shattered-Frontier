"""Build an isolated, baked two-clump secondary-motion proof in Blender.

Distance/bending constraints and capsule/ground contacts operate in world space.
The horse uses a rigid motion harness, not a completed locomotion skeleton.
"""
import argparse,json,math,sys,hashlib,time
from pathlib import Path
from dataclasses import dataclass
import bpy,numpy as np
from mathutils import Vector,Matrix

@dataclass(frozen=True)
class Settings:
    fps:int=30
    duration_seconds:int=11
    substeps:int=6
    constraint_iterations:int=8
    gravity_m_s2:float=9.81
    drag_per_second:float=2.8
    shape_spring_per_second_squared:float=32.
    bend_stiffness:float=.35
    collision_radius_m:float=.025
    wind_acceleration_m_s2:float=.8
    render_stride_frames:int=8

BOUNDS={'fps':(24,60),'duration_seconds':(11,11),'substeps':(4,16),'constraint_iterations':(4,24),
    'gravity_m_s2':(0,20),'drag_per_second':(0,15),'shape_spring_per_second_squared':(0,100),
    'bend_stiffness':(0,1),'collision_radius_m':(.005,.06),'wind_acceleration_m_s2':(0,5),'render_stride_frames':(1,30)}
p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--config',required=True);p.add_argument('--output',required=True)
p.add_argument('--actual-mesh',action='store_true',help='Skin selected regions of the original Pixal mesh instead of adding test clumps')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);out=Path(a.output);out.mkdir(parents=True,exist_ok=False);(out/'previews').mkdir()
raw=json.loads(Path(a.config).read_text());cfg=Settings(**{k:raw[k] for k in BOUNDS})
for key,(low,high) in BOUNDS.items():
    value=getattr(cfg,key);assert type(value) is type(getattr(Settings(),key)) and low<=value<=high,(key,value)
bpy.ops.wm.open_mainfile(filepath=str(Path(a.source).resolve()));scene=bpy.context.scene
scene.render.fps=cfg.fps;scene.frame_start=1;scene.frame_end=cfg.fps*cfg.duration_seconds
asset=bpy.data.collections['Voidreign | Generated asset'];asset.name='Horse | original static context'
demo=bpy.data.collections.new('Prototype | dynamic mane and tail');scene.collection.children.link(demo)
rider_collection=bpy.data.collections.new('Rider | collision proxy');scene.collection.children.link(rider_collection)
def move_to(obj,collection):
    for old in list(obj.users_collection):old.objects.unlink(obj)
    collection.objects.link(obj)
def material(name,color,metal=0):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True;n=m.node_tree.nodes.get('Principled BSDF');n.inputs['Base Color'].default_value=(*color,1);n.inputs['Roughness'].default_value=.7;n.inputs['Metallic'].default_value=metal;return m
black=material('Prototype hair charcoal',(.025,.018,.02));red=material('Prototype hair crimson',(.16,.012,.019));gold=material('Dynamic root markers',(.5,.24,.03),.5)
grey=material('Rider collision proxy',(.16,.28,.31));obstacle_mat=material('Environment contact post',(.35,.075,.035))
root=bpy.data.objects.new('MOTION HARNESS | rigid horse, not gait animation',None);demo.objects.link(root)
for obj in list(asset.objects):
    if obj.parent is None:obj.parent=root
def capsule(name,a,b,radius,mat,collection,parent=None):
    a,b=Vector(a),Vector(b);length=(b-a).length;verts=[];faces=[]
    rings=[(-length/2+radius*math.sin(theta),radius*math.cos(theta)) for theta in [-math.pi/2,-math.pi/3,-math.pi/6,0]]
    rings += [(length/2+radius*math.sin(theta),radius*math.cos(theta)) for theta in [0,math.pi/6,math.pi/3,math.pi/2]]
    for z,r in rings:
        for k in range(12):verts.append((r*math.cos(k*math.pi/6),r*math.sin(k*math.pi/6),z))
    for ring in range(len(rings)-1):
        for k in range(12):faces.append((ring*12+k,ring*12+(k+1)%12,(ring+1)*12+(k+1)%12,(ring+1)*12+k))
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update();obj=bpy.data.objects.new(name,data);collection.objects.link(obj);obj.location=(a+b)/2
    obj.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();obj.data.materials.append(mat);obj.parent=parent;move_to(obj,collection)
    for face in obj.data.polygons:face.use_smooth=True
    return obj
rider_caps=[('Rider torso',(0,.03,1.96),(0,-.02,2.43),.16),('Rider head',(0,-.04,2.67),(0,-.04,2.73),.115),
    ('Rider thigh near',(.10,.08,1.92),(.35,-.25,1.55),.105),('Rider shin near',(.35,-.25,1.55),(.37,-.10,1.03),.075),
    ('Rider thigh far',(-.10,.08,1.92),(-.35,-.25,1.55),.105),('Rider shin far',(-.35,-.25,1.55),(-.37,-.10,1.03),.075),
    ('Rider upper arm near',(.18,-.02,2.37),(.38,-.33,2.14),.065),('Rider forearm near',(.38,-.33,2.14),(.46,-.65,1.94),.06),
    ('Rider arm far',(-.18,-.02,2.37),(-.32,-.48,2.05),.065)]
rider_objects=[]
for name,start,end,radius in rider_caps:rider_objects.append(capsule(name,start,end,radius,grey,rider_collection,root))
reach=capsule('Rider hand | contact demonstration',(.85,-.67,1.82),(.85,-.82,1.73),.085,grey,rider_collection,root)
environment=('Environment',np.array([.43,1.50,.22]),np.array([.43,1.50,1.40]),.12)
capsule('STATIC environment test post',environment[1],environment[2],environment[3],obstacle_mat,demo)
horse_caps=[('Horse',np.array([0,-.35,1.36]),np.array([0,.62,1.36]),.34),
    ('Horse',np.array([0,-.78,1.52]),np.array([0,-1.05,2.03]),.22)]
def ease(x):x=max(0,min(1,x));return x*x*(3-2*x)
def motion(t):
    if t<=1:return np.array([0.,0.,0.]),0.,'SETTLE'
    if t<=3:
        d=t-1;return np.array([0.,-.22*d,.025*math.sin(2*math.pi*1.4*d)]),0.,'WALK RESPONSE'
    if t<=5:
        d=t-3;return np.array([0.,-.44-.65*d,.055*math.sin(2*math.pi*2.4*d)]),0.,'RUN RESPONSE'
    if t<=7:
        d=(t-5)/2;return np.array([.7*ease(d),-1.74-.35*ease(d),.025*math.sin(2*math.pi*2*d)]),-math.pi/3*ease(d),'TURN RESPONSE'
    if t<=9:return np.array([.7,-2.09,0.]),-math.pi/3,'STOP / SETTLE'
    return np.array([.7,-2.09,0.]),-math.pi/3,'RIDER CONTACT'
def transform(t):
    pos,angle,label=motion(t);c,s=math.cos(angle),math.sin(angle)
    return pos,np.array([[c,-s,0],[s,c,0],[0,0,1.]]),label
def reach_offset(t):return -.34*math.sin(math.pi*max(0,min(1,(t-9)/2)))
def colliders(t,pos,rot):
    result=[]
    for label,start,end,radius in horse_caps:result.append((label,rot@start+pos,rot@end+pos,radius))
    for _,start,end,radius in rider_caps:result.append(('Rider',rot@np.array(start)+pos,rot@np.array(end)+pos,radius))
    off=np.array([reach_offset(t),0.,0.]);result.append(('Rider',rot@(np.array([.85,-.67,1.82])+off)+pos,rot@(np.array([.85,-.82,1.73])+off)+pos,.085))
    result.append(environment);return result
specs=[('Mane',np.array([[.32,-.82,2.18],[.37,-.70,2.17],[.42,-.58,2.11],[.46,-.46,2.02],[.49,-.36,1.90],[.51,-.28,1.76],[.53,-.22,1.61],[.54,-.14,1.47],[.55,-.04,1.34]])),
    ('Tail',np.array([[.30,.90,1.64],[.36,1.08,1.64],[.41,1.25,1.58],[.45,1.42,1.48],[.48,1.58,1.34],[.50,1.73,1.19],[.52,1.88,1.03],[.54,2.01,.85],[.56,2.11,.66],[.58,2.16,.47],[.60,2.18,.28],[.61,2.14,.13]]))]
if a.actual_mesh:
    specs=[('Mane',np.array([[-.17,-.94,2.34],[-.17,-.82,2.38],[-.16,-.70,2.35],[-.15,-.58,2.30],[-.14,-.46,2.24],[-.13,-.34,2.18],[-.12,-.22,2.12],[-.11,-.10,2.06],[-.10,.02,1.98]])),
        ('Tail',np.array([[0,1.00,1.48],[.04,1.08,1.43],[.09,1.16,1.35],[.14,1.24,1.24],[.19,1.32,1.12],[.24,1.40,.99],[.29,1.48,.85],[.33,1.56,.71],[.37,1.64,.57],[.40,1.72,.43],[.43,1.80,.30],[.45,1.87,.17]]))]
arm_data=bpy.data.armatures.new('Voidreign prototype hair chains');arm=bpy.data.objects.new('RIG | mane + tail only',arm_data);demo.objects.link(arm);arm.parent=root;arm.show_in_front=True
bpy.context.view_layer.objects.active=arm;arm.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
if a.actual_mesh:
    anchor=arm_data.edit_bones.new('StaticBody');anchor.head=(0,0,0);anchor.tail=(0,0,.3)
for name,points in specs:
    previous=None
    for index in range(len(points)-1):
        bone=arm_data.edit_bones.new(f'{name}_{index:02}');bone.head=points[index];bone.tail=points[index+1]
        if previous:bone.parent=previous;bone.use_connect=False
        previous=bone
bpy.ops.object.mode_set(mode='OBJECT')
for bone in arm.pose.bones:bone.rotation_mode='QUATERNION'
hair_objects=[]
for name,points in specs:
    if a.actual_mesh:continue
    verts=[];faces=[];bindings=[]
    for strand in range(5):
        centers=points.copy();centers[:,0]+=(strand-2)*.022
        centers[:,1]+=.02*np.sin(np.linspace(0,math.pi,len(points))+strand)
        start=len(verts)
        for index,center in enumerate(centers):
            tangent=centers[min(index+1,len(points)-1)]-centers[max(0,index-1)];tangent/=np.linalg.norm(tangent)
            side=np.cross(tangent,[1,0,0]);side/=np.linalg.norm(side);up=np.cross(tangent,side)
            radius=.018*(1-index/(len(points)-.5))+.003
            for k in range(6):verts.append(center+radius*(math.cos(k*math.pi/3)*side+math.sin(k*math.pi/3)*up));bindings.append(min(index,len(points)-2))
            if index:
                for k in range(6):
                    v=start+index*6+k;w=start+index*6+(k+1)%6;faces.append((v-6,w-6,w,v))
        faces.append(tuple(start+k for k in reversed(range(6))));faces.append(tuple(start+(len(points)-1)*6+k for k in range(6)))
    data=bpy.data.meshes.new(name+' deformable clump');data.from_pydata(verts,[],faces);data.update()
    obj=bpy.data.objects.new('DYNAMIC '+name+' | prototype clump',data);demo.objects.link(obj);obj.parent=root;obj.data.materials.append(black);obj.data.materials.append(red)
    for index,face in enumerate(data.polygons):face.use_smooth=True;face.material_index=1 if index%11==0 else 0
    groups=[obj.vertex_groups.new(name=f'{name}_{i:02}') for i in range(len(points)-1)]
    for index,binding in enumerate(bindings):
        if binding>0:groups[binding-1].add([index],.5,'REPLACE');groups[binding].add([index],.5,'REPLACE')
        else:groups[0].add([index],1.,'REPLACE')
    modifier=obj.modifiers.new('Bone-chain secondary motion','ARMATURE');modifier.object=arm;hair_objects.append(obj)
    capsule('ROOT | '+name,points[0],points[0]+[0,0,.025],.03,gold,demo,root)
skin_report={}
if a.actual_mesh:
    obj=next(o for o in asset.all_objects if o.type=='MESH');points=np.array([obj.matrix_world@v.co for v in obj.data.vertices])
    y,z=points[:,1],points[:,2]
    smooth_array=lambda x:np.clip(x,0,1)**2*(3-2*np.clip(x,0,1))
    mane_mask=smooth_array((y+.97)/.18)*smooth_array((.06-y)/.18)*smooth_array((z-2.075)/.13)
    tail_boundary=1.01+.30*np.clip((1.4-z)/1.0,0,1)
    tail_mask=smooth_array((y-tail_boundary)/.18)*smooth_array((1.84-z)/.15)
    masks={'Mane':mane_mask,'Tail':tail_mask}
    static=obj.vertex_groups.new(name='StaticBody')
    static_weights=1-np.clip(mane_mask+tail_mask,0,1)
    for index,weight in enumerate(static_weights):
        if weight>0:static.add([index],float(weight),'REPLACE')
    for name,rest in specs:
        groups=[obj.vertex_groups.new(name=f'{name}_{i:02}') for i in range(len(rest)-1)]
        mask=masks[name];indices=np.flatnonzero(mask>1e-5);selected=points[indices]
        starts=rest[:-1];segments=np.diff(rest,axis=0)
        parameter=np.clip(np.einsum('nki,ki->nk',selected[:,None,:]-starts[None],segments)/(segments*segments).sum(1),0,1)
        projected=starts[None]+parameter[:,:,None]*segments[None]
        nearest=np.argmin(((selected[:,None]-projected)**2).sum(2),axis=1)
        for vertex,index in enumerate(indices):
            group=int(nearest[vertex]);blend=float(parameter[vertex,group]);next_group=min(group+1,len(groups)-1)
            if next_group==group:groups[group].add([int(index)],float(mask[index]),'REPLACE')
            else:
                groups[group].add([int(index)],float(mask[index]*(1-blend)),'REPLACE');groups[next_group].add([int(index)],float(mask[index]*blend),'REPLACE')
        attribute=obj.data.color_attributes.new(name='Motion | '+name,type='FLOAT_COLOR',domain='POINT')
        values=np.column_stack([mask,mask,mask,np.ones(len(mask))]).astype(np.float32).ravel();attribute.data.foreach_set('color',values)
        skin_report[name]=dict(weighted_vertices=int((mask>.01).sum()),fully_weighted_vertices=int((mask>.99).sum()))
    modifier=obj.modifiers.new('ACTUAL Pixal mane and tail | prototype skin','ARMATURE');modifier.object=arm
    skin_report.update(original_vertices=len(obj.data.vertices),original_triangles=sum(len(f.vertices)-2 for f in obj.data.polygons),method='Original mesh retained intact; spatially feathered weights; all remaining vertices use StaticBody')
# Verlet/PBD integration. Local rest shape provides weak styling, not keyframed tips.
states=[]
for name,points in specs:states.append(dict(name=name,rest=points,p=points.copy(),old=points.copy(),length=np.linalg.norm(np.diff(points,axis=0),axis=1)))
contacts={key:0 for key in ['Horse','Rider','Environment','Ground']};metrics=[];trajectory=[]
dt=1/(cfg.fps*cfg.substeps);radius=cfg.collision_radius_m
def project(points,caps,record=False):
    correction=np.zeros_like(points)
    for label,start,end,collider_radius in caps:
        segment=end-start;u=np.clip(((points-start)@segment)/(segment@segment),0,1);closest=start+u[:,None]*segment
        delta=points-closest;distance=np.linalg.norm(delta,axis=1);penetration=collider_radius+radius-distance
        hit=(penetration>0);hit[0]=False
        if record:contacts[label]+=int(hit.sum())
        normal=delta/np.maximum(distance[:,None],1e-9);normal[distance<1e-9]=[1,0,0]
        points[hit]+=normal[hit]*penetration[hit,None]
    hit=points[:,2]<radius;hit[0]=False
    if record:contacts['Ground']+=int(hit.sum())
    points[hit,2]=radius
for frame in range(1,scene.frame_end+1):
    for substep in range(cfg.substeps):
        t=((frame-1)+(substep+1)/cfg.substeps)/cfg.fps;pos,rot,label=transform(t);caps=colliders(t,pos,rot)
        for state in states:
            points=state['p'];old=state['old'];target=state['rest']@rot.T+pos
            speed=(points-old)*math.exp(-cfg.drag_per_second*dt);old[:]=points
            spring=cfg.shape_spring_per_second_squared*np.linspace(1,.22,len(points))[:,None]*(target-points)
            acceleration=spring+np.array([cfg.wind_acceleration_m_s2*math.sin(t*2),0,-cfg.gravity_m_s2])
            points+=speed+acceleration*dt*dt;points[0]=target[0]
            for iteration in range(cfg.constraint_iterations):
                for index,length in enumerate(state['length']):
                    delta=points[index+1]-points[index];dist=np.linalg.norm(delta);change=delta*(1-length/max(dist,1e-9))
                    if index:points[index]+=.5*change;points[index+1]-=.5*change
                    else:points[index+1]-=change
                for index in range(len(points)-2):
                    delta=points[index+2]-points[index];rest=np.linalg.norm(state['rest'][index+2]-state['rest'][index]);distance=np.linalg.norm(delta)
                    change=cfg.bend_stiffness*delta*(1-rest/max(distance,1e-9))
                    if index:points[index]+=.5*change;points[index+2]-=.5*change
                    else:points[index+2]-=change
                project(points,caps,record=iteration==0);points[0]=target[0]
    t=frame/cfg.fps;pos,rot,label=transform(t);root.location=pos;root.rotation_mode='QUATERNION';root.rotation_quaternion=Matrix(rot.tolist()).to_quaternion();root.keyframe_insert('location',frame=frame);root.keyframe_insert('rotation_quaternion',frame=frame)
    reach.location.x=.85+reach_offset(t);reach.keyframe_insert('location',frame=frame)
    bpy.context.view_layer.update()
    for state in states:
        local=(state['p']-pos)@rot
        previous_matrix=None
        for index in range(len(local)-1):
            bone=arm.pose.bones[f'{state["name"]}_{index:02}'];direction=Vector(local[index+1]-local[index]);length=direction.length
            matrix=Matrix.Translation(Vector(local[index]))@direction.to_track_quat('Y','Z').to_matrix().to_4x4()
            rest_bone=arm_data.bones[bone.name]
            basis=rest_bone.matrix_local.inverted()@matrix if previous_matrix is None else rest_bone.matrix_local.inverted()@rest_bone.parent.matrix_local@previous_matrix.inverted()@matrix
            bone.matrix_basis=basis;previous_matrix=matrix
            for path in ['location','rotation_quaternion','scale']:bone.keyframe_insert(path,frame=frame)
        stretch=np.max(np.abs(np.linalg.norm(np.diff(state['p'],axis=0),axis=1)/state['length']-1))
        trajectory.append(dict(frame=frame,section=state['name'],points=state['p'].round(6).tolist()))
        metrics.append(dict(frame=frame,section=state['name'],root_error_m=float(np.linalg.norm(state['p'][0]-(state['rest'][0]@rot.T+pos))),max_relative_segment_error=float(stretch)))
    if frame%60==0:print('Simulated',frame,'/',scene.frame_end,contacts,flush=True)
# Stage markers and a tracking camera. Existing static hair remains explicit context.
for frame,label in [(1,'SETTLE'),(31,'WALK RESPONSE'),(91,'RUN RESPONSE'),(151,'TURN RESPONSE'),(211,'STOP / SETTLE'),(271,'RIDER CONTACT')]:scene.timeline_markers.new(label,frame=frame)
camera=bpy.data.objects['Beauty'];camera.data.type='ORTHO';camera.data.ortho_scale=6.0
for frame in range(1,scene.frame_end+1):
    pos,rot,_=transform(frame/cfg.fps);target=Vector(pos)+Vector((0,.1,1.4));camera.location=target+Vector((7,-4,2.7));camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.keyframe_insert('location',frame=frame);camera.keyframe_insert('rotation_euler',frame=frame)
scene.camera=camera;scene.render.resolution_x=960;scene.render.resolution_y=720;scene.render.resolution_percentage=100
scene.view_settings.exposure=0;scene.render.image_settings.file_format='PNG'
scene['Prototype scope']=('Actual Pixal mesh; selected mane/tail regions skinned in place. Rigid horse motion harness, not a gait rig.' if a.actual_mesh else 'Two additional dynamic clumps; original generated hair remains static. Rigid horse motion harness, not a gait rig.')
scene['Tuning']='Edit shipped JSON and rebuild into a new folder. Baked animation is not a live simulation.'
for key,(low,high) in BOUNDS.items():
    scene['Hair '+key]=getattr(cfg,key)
    scene.id_properties_ui('Hair '+key).update(min=low,max=high,description='Author setting; effective after offline rebuild with the JSON configuration')
scene.frame_set(125);bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(out/'Voidreign-HairPrototype.blend'))
# Export only horse, hair rig and motion root. Rider/post are diagnostic scene context.
bpy.ops.object.select_all(action='DESELECT');root.select_set(True);arm.select_set(True)
for obj in list(asset.objects)+hair_objects:obj.select_set(True)
bpy.context.view_layer.objects.active=arm
bpy.ops.export_scene.gltf(filepath=str(out/'Voidreign-HairPrototype.glb'),export_format='GLB',use_selection=True,export_animations=True,export_animation_mode='SCENE',export_anim_scene_split_object=False)
bpy.ops.export_scene.fbx(filepath=str(out/'Voidreign-HairPrototype.fbx'),use_selection=True,object_types={'MESH','ARMATURE','EMPTY'},path_mode='COPY',embed_textures=True,bake_anim=True,bake_anim_simplify_factor=0.,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,add_leaf_bones=False)
(out/'simulation-trajectory.json').write_text(json.dumps(trajectory))
report=dict(status='baked prototype',blender=bpy.app.version_string,source_sha256=hashlib.sha256(Path(a.source).read_bytes()).hexdigest(),config=raw,frames=scene.frame_end,bones=len(arm_data.bones),contacts=contacts,
    max_root_error_m=max(row['root_error_m'] for row in metrics),max_relative_segment_error=max(row['max_relative_segment_error'] for row in metrics),metrics=metrics,
    limitations=['Rigid motion harness does not animate horse legs.','Two new prototype clumps; existing generated mane/tail remain static.','Point/capsule and ground collision, no self-collision or triangle-mesh environment collision.','Collision radius approximates clump thickness; segment interiors and rendered strands need visual review.','Rider is a simplified seated proxy; no saddle/reins rig.','Baked offline animation, not Unreal runtime simulation.'])
if a.actual_mesh:
    report['skin']=skin_report
    report['limitations'][1]='Selected actual mane/tail regions move; fused geometry and feathered boundaries require cleanup. Remaining hair stays static.'
(out/'simulation-report.json').write_text(json.dumps(report,indent=2));(out/'config.json').write_text(json.dumps(raw,indent=2))
for frame in [50,120,180,230,300]:
    scene.frame_set(frame);scene.render.filepath=str(out/'previews'/f'Frame-{frame:03}.png');bpy.ops.render.render(write_still=True)
print(json.dumps({key:report[key] for key in ['bones','contacts','max_root_error_m','max_relative_segment_error']}),flush=True)
