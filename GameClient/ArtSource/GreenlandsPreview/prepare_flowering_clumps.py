"""Preserve three full-bloom scan variants, centred at their natural ground line."""
import bpy,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'PreparedArrival'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'ArrivalAssets/flower_heliophila/flower_heliophila.gltf'))
objects=sorted((o for o in bpy.context.scene.objects if o.type=='MESH'),key=lambda o:o.name)
report=[]
for i,o in enumerate(objects):
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    lo=Vector(tuple(min(v.co[k] for v in o.data.vertices) for k in range(3)));hi=Vector(tuple(max(v.co[k] for v in o.data.vertices) for k in range(3)))
    centre=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
    for vertex in o.data.vertices:vertex.co-=centre
    o.name='SM_PH_FloweringClump_'+str(i);bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    o.data.calc_loop_triangles()
    row=dict(mesh=o.name,source_object=o.data.name,dimensions_metres=list(hi-lo),triangles=len(o.data.loop_triangles));report.append(row);print(json.dumps(row),flush=True)
    bpy.ops.export_scene.fbx(filepath=str(OUT/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
for image in bpy.data.images:
    if image.source=='FILE':image.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'FloweringClumps.blend'))
(OUT/'flowering-clumps.json').write_text(json.dumps(report,indent=2))
