"""Fresh saved-source views, then independent exchange reimports."""
import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent
for label in ['Beauty','Front','Side','Rear']:
    bpy.context.scene.camera=bpy.data.objects[label];bpy.context.scene.render.filepath=str(ROOT/('Workshop-'+label+'.png'));bpy.ops.render.render(write_still=True)
report=[]
for ext in ['fbx','glb']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if ext=='fbx':bpy.ops.import_scene.fbx(filepath=str(ROOT/'GreenlandsWorkshop.fbx'))
    else:bpy.ops.import_scene.gltf(filepath=str(ROOT/'GreenlandsWorkshop.glb'))
    meshes=[o for o in bpy.data.objects if o.type=='MESH'];assert len(meshes)==5
    for obj in meshes:assert len(obj.data.polygons)>0 and len(obj.data.uv_layers)>0 and len(obj.data.materials)>0 and min(obj.dimensions)>.1
    report.append(dict(format=ext,meshes=[dict(name=o.name,dimensions_metres=list(o.dimensions),vertex_colour_layers=len(o.data.color_attributes)) for o in meshes]))
(ROOT/'workshop-exchange-review.json').write_text(json.dumps(report,indent=2));print('WORKSHOP_SOURCE_REVIEWED',flush=True)
