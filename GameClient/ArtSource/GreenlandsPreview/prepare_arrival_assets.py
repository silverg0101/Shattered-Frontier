"""Prepare curated CC0 models for realtime use, retaining source scale and UVs."""
import bpy,json,math,bmesh,sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'PreparedArrival';OUT.mkdir(exist_ok=True)
only=sys.argv[sys.argv.index('--only')+1] if '--only' in sys.argv else None
manifest=json.loads((OUT/'meshes.json').read_text()) if only and (OUT/'meshes.json').exists() else []
materials=json.loads((OUT/'materials.json').read_text()) if only and (OUT/'materials.json').exists() else []
if only:
    manifest=[m for m in manifest if m['asset'] not in (only,'Barrel_01')];materials=[m for m in materials if m['asset'] not in (only,'Barrel_01')]
def export(name,objs,asset,collision=False):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0]
    if len(objs)>1:bpy.ops.object.join()
    o=bpy.context.object;o.name=name
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    # Keep the geometry's native dimensions, put each independent prop on the ground.
    lo=Vector(tuple(min(v.co[i] for v in o.data.vertices) for i in range(3)));hi=Vector(tuple(max(v.co[i] for v in o.data.vertices) for i in range(3)))
    center=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
    for v in o.data.vertices:v.co-=center
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    o.data.calc_loop_triangles();tris=len(o.data.loop_triangles)
    if collision:
        # Trunk-only simple collision. Foliage never blocks the road/camera.
        bpy.ops.mesh.primitive_cylinder_add(vertices=10,radius=.16,depth=1.7,location=(0,0,.85));c=bpy.context.object;c.name='UCX_'+name+'_00';c.select_set(True);o.select_set(True)
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',path_mode='RELATIVE')
    for image in bpy.data.images:
        if image.source=='FILE':
            try:image.pack()
            except RuntimeError:pass
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(name+'.blend')))
    manifest.append(dict(mesh=name,asset=asset,triangles=tris,dimensions_metres=list(hi-lo),materials=[m.name for m in o.data.materials if m],simple_trunk_collision=collision))
    if collision:bpy.data.objects.remove(c,do_unlink=True)
    bpy.data.objects.remove(o,do_unlink=True)
for folder in sorted((ROOT/'ArrivalAssets').iterdir()):
    if not folder.is_dir() or folder.name=='Barrel_01' or (only and folder.name!=only):continue
    asset=folder.name;bpy.ops.wm.read_factory_settings(use_empty=True)
    source=json.loads((folder/(asset+'.gltf')).read_text())
    for m in source['materials']:
        record=dict(name=m['name'],asset=asset,alpha=m.get('alphaMode','OPAQUE'),two_sided=m.get('doubleSided',False))
        for key,slot in [('base',m.get('pbrMetallicRoughness',{}).get('baseColorTexture')),('arm',m.get('pbrMetallicRoughness',{}).get('metallicRoughnessTexture')),('normal',m.get('normalTexture'))]:
            if slot:
                uri=source['images'][source['textures'][slot['index']]['source']]['uri'];record[key]=str((folder/uri).relative_to(ROOT));record['uv']=slot.get('texCoord',0);record['transform']=slot.get('extensions',{}).get('KHR_texture_transform',{})
        if record['alpha']!='OPAQUE':record['opacity']=str((folder/'textures'/(asset+'_opacity.png')).relative_to(ROOT))
        materials.append(record)
    bpy.ops.import_scene.gltf(filepath=str(folder/(asset+'.gltf')))
    objs=[o for o in bpy.context.scene.objects if o.type=='MESH']
    if asset=='tree_small_02':
        o=objs[0];bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.mesh.separate(type='MATERIAL');parts=[a for a in bpy.context.scene.objects if a.type=='MESH']
        for p in parts:
            bpy.context.view_layer.objects.active=p;p.data.calc_loop_triangles();count=len(p.data.loop_triangles)
            name=p.data.materials[0].name
            target=220000 if 'leaves' in name else (20000 if 'branches' in name else 9000)
            if count>target:
                mod=p.modifiers.new('Realtime reduction','DECIMATE');mod.ratio=target/count;bpy.ops.object.modifier_apply(modifier=mod.name)
        export('SM_PH_Broadleaf',parts,asset,True)
    else:
        # Grass/fern sources contain multiple discrete clumps/LODs; use compact variants.
        for o in objs:o.data.calc_loop_triangles()
        if asset=='grass_medium_01':chosen=sorted([o for o in objs if 300<=len(o.data.loop_triangles)<=1600],key=lambda o:len(o.data.loop_triangles))[:4]
        elif asset=='wooden_crate_01':chosen=[max(objs,key=lambda o:len(o.data.loop_triangles))]
        else:chosen=objs
        for i,o in enumerate(chosen):
            bpy.context.view_layer.objects.active=o
            count=len(o.data.loop_triangles)
            if count>18000:
                mod=o.modifiers.new('Realtime reduction','DECIMATE');mod.ratio=18000/count;bpy.ops.object.modifier_apply(modifier=mod.name)
            export('SM_PH_'+asset+'_%02d'%i,[o],asset)
(OUT/'meshes.json').write_text(json.dumps(manifest,indent=2));(OUT/'materials.json').write_text(json.dumps(materials,indent=2))
print('Prepared',len(manifest),'meshes',flush=True)
