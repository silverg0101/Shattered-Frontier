"""Fresh-process review: render the saved source, then reimport GLB/FBX into empty scenes.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropHayBales.blend -P review_prophaybales.py
"""
import bpy,json
from pathlib import Path
from mathutils import Vector
HERE=Path(__file__).resolve().parent;scene=bpy.context.scene
for name in ['Beauty','Front','Side','Rear']:
    scene.camera=bpy.data.objects[name];scene.render.filepath=str(HERE/f'PropHayBales-{name}.png');bpy.ops.render.render(write_still=True)
def summary(col=None):
    objs=[o for o in (col.objects if col else bpy.data.objects) if o.type=='MESH'];pts=[o.matrix_world@Vector(c) for o in objs for c in o.bound_box]
    return dict(meshes=len(objs),triangles=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs),
        size_m=[round(max(p[k] for p in pts)-min(p[k] for p in pts),3) for k in range(3)],
        materials=sorted({m.name for o in objs for m in o.data.materials if m}),colour_attributes=sorted({a.name for o in objs for a in o.data.color_attributes}),
        images_missing=sorted(i.name for i in bpy.data.images if i.source=='FILE' and not Path(bpy.path.abspath(i.filepath)).exists()),images=len([i for i in bpy.data.images if i.source=='FILE']))
report={'source':summary(bpy.data.collections['SilverbrookPropHayBales'])}
for ext,op in [('glb',lambda f:bpy.ops.import_scene.gltf(filepath=f)),('fbx',lambda f:bpy.ops.import_scene.fbx(filepath=f))]:
    bpy.ops.wm.read_factory_settings(use_empty=True);op(str(HERE/f'SilverbrookPropHayBales.{ext}'));report[ext]=summary()
(HERE/'exchange-review.json').write_text(json.dumps(report,indent=2)+'\n');assert not report['fbx']['images_missing'] and report['fbx']['images'],'FBX texture references do not resolve';print('REVIEW',json.dumps({k:(v['meshes'],v['triangles'],v['size_m']) for k,v in report.items()}))
