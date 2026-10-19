"""Render the saved source in a fresh Blender process; preserve the asset file."""
import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;scene=bpy.context.scene
for label,name in [('Beauty','Beauty — front left'),('Front','Front'),('Side','Side'),('Rear','Rear')]:
    scene.camera=bpy.data.objects[name];scene.render.filepath=str(ROOT/('Cottage-'+label+'.png'));bpy.ops.render.render(write_still=True)
    print('COTTAGE_RENDER',label,flush=True)
