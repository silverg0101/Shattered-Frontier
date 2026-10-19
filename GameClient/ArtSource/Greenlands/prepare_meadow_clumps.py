"""Retain the scan's dense clumps, instead of using isolated stems as a meadow."""
import bpy,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'PreparedArrival'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'ArrivalAssets/grass_medium_01/grass_medium_01.gltf'))
objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
for o in objects:o.data.calc_loop_triangles()
chosen=sorted(objects,key=lambda o:len(o.data.loop_triangles),reverse=True)[:4];report=[]
for i,o in enumerate(chosen):
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    lo=Vector(tuple(min(v.co[k] for v in o.data.vertices) for k in range(3)));hi=Vector(tuple(max(v.co[k] for v in o.data.vertices) for k in range(3)))
    centre=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
    for vertex in o.data.vertices:vertex.co-=centre
    name='SM_PH_GrassClump_%d'%i;o.name=name
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
    report.append(dict(mesh=name,dimensions_metres=list(hi-lo),triangles=len(o.data.loop_triangles),source='grass_medium_01',license='CC0'))
# Original scanned clump cards retain density at distance without thousands of blades.
card=max((o for o in objects if len(o.data.loop_triangles)<100),key=lambda o:len(o.data.loop_triangles))
bpy.ops.object.select_all(action='DESELECT');card.select_set(True);bpy.context.view_layer.objects.active=card
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
lo=Vector(tuple(min(v.co[k] for v in card.data.vertices) for k in range(3)));hi=Vector(tuple(max(v.co[k] for v in card.data.vertices) for k in range(3)));centre=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
for vertex in card.data.vertices:vertex.co-=centre
bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');card.name='SM_PH_MeadowDistanceClump'
bpy.ops.export_scene.fbx(filepath=str(OUT/'SM_PH_MeadowDistanceClump.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
(OUT/'meadow-clumps.json').write_text(json.dumps(report,indent=2))
for image in bpy.data.images:
    if image.source=='FILE':
        try:image.pack()
        except RuntimeError:pass
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MeadowClumps.blend'))
