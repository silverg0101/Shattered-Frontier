"""Read the delivered exchange files in a clean scene, not the authoring objects."""
import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;report=[]
for extension in ['fbx','glb']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if extension=='fbx':bpy.ops.import_scene.fbx(filepath=str(ROOT/('GreenlandsCottage.'+extension)))
    else:bpy.ops.import_scene.gltf(filepath=str(ROOT/('GreenlandsCottage.'+extension)))
    meshes=[o for o in bpy.data.objects if o.type=='MESH'];assert len(meshes)==4,(extension,len(meshes))
    for obj in meshes:
        assert len(obj.data.polygons)>0 and min(obj.dimensions)>.1
        assert len(obj.data.uv_layers)>0 and len(obj.data.materials)>0
    report.append(dict(format=extension,meshes=[dict(name=o.name,dimensions_metres=list(o.dimensions),materials=len(o.data.materials),vertex_colour_layers=len(o.data.color_attributes)) for o in meshes]))
(ROOT/'exchange-review.json').write_text(json.dumps(report,indent=2));print('COTTAGE_EXCHANGE_REIMPORT_OK',flush=True)
