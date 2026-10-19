"""Render matching neutral geometry views from reviewed asset sources."""
import argparse,json,sys
from pathlib import Path
import bpy
p=argparse.ArgumentParser()
p.add_argument('--folder',action='append',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
folders=[Path(folder).resolve() for folder in a.folder]
size=max(max(json.loads((folder/'asset-review.json').read_text())['dimensions_m']) for folder in folders)*1.35
for folder in folders:
    bpy.ops.wm.open_mainfile(filepath=str(folder/'Voidreign.blend'))
    scene=bpy.context.scene
    material=bpy.data.materials.new('Matched comparison clay');material.use_nodes=True
    bsdf=material.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value=(.16,.17,.18,1)
    bsdf.inputs['Roughness'].default_value=.85
    scene.view_layers[0].material_override=material
    scene.view_settings.exposure=-1.0
    for name in ['Beauty','Front','Side','Rear']:
        camera=bpy.data.objects[name];camera.data.ortho_scale=size;scene.camera=camera
        scene.render.filepath=str(folder/'previews'/('Clay-'+name+'.png'))
        bpy.ops.render.render(write_still=True)
print(json.dumps({'folders':[str(folder) for folder in folders],'common_ortho_scale_m':size,'exposure':-1.0}))
