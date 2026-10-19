"""Orthographic geometry inspection of native wall apertures before fitting shutters."""
import bpy,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'FabSource/FacadeInspection';OUT.mkdir(parents=True,exist_ok=True)
names=['SM_MedievalModularWall2x2MD','SM_MedievalModularWall2x2MG','SM_MedievalModularWall2x2MH'];size=192;pixels=[0.0]*(size*3*size*4);report=[]
for tile,name in enumerate(names):
    bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(ROOT/'PreparedArrival'/(name+'.fbx')))
    deps=bpy.context.evaluated_depsgraph_get();holes=[]
    for iz in range(size):
        for ix in range(size):
            x=2*(ix+.5)/size;z=2*(iz+.5)/size
            hit=bpy.context.scene.ray_cast(deps,Vector((x,-2,z)),Vector((0,1,0)),distance=4)[0]
            if not hit and .15<x<1.85 and .15<z<1.85:holes.append((x,z))
            offset=(iz*size*3+tile*size+ix)*4;pixels[offset:offset+4]=[.8 if hit else .04]*3+[1]
    report.append(dict(mesh=name,hole_min=[min(p[k] for p in holes) for k in range(2)] if holes else None,hole_max=[max(p[k] for p in holes) for k in range(2)] if holes else None))
image=bpy.data.images.new('Facade apertures D G H',width=size*3,height=size);image.pixels=pixels;image.filepath_raw=str(OUT/'wall-apertures.png');image.file_format='PNG';image.save()
(OUT/'apertures.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
