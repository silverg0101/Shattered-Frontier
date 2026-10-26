"""CPU saved-source pose previews with an unsaved diagnostic ground plane."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
from mathutils import Vector

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile,out=Path(a.profile).resolve(),Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and not out.exists() and 1<=cfg['samples']<=64
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));scene=bpy.context.scene
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=cfg['samples'];scene.cycles.use_denoising=False
bpy.ops.mesh.primitive_plane_add(size=cfg['ground_size_m'],location=(0,0,cfg['ground_height_m']))
ground=bpy.context.object;ground.name='DIAGNOSTIC | ground plane, not asset'
mat=bpy.data.materials.new('Ground review gray');mat.diffuse_color=(.3,.3,.3,1);mat.use_nodes=True
mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.3,.3,.3,1);ground.data.materials.append(mat)
bpy.ops.object.light_add(type='AREA',location=cfg['fill_light_position_m']);light=bpy.context.object;light.data.energy=cfg['fill_light_watts'];light.data.size=cfg['fill_light_size_m']
light.rotation_euler=(Vector(cfg['target_m'])-light.location).to_track_quat('-Z','Y').to_euler()
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
for frame in cfg['frames']:
    assert scene.frame_start<=frame<=scene.frame_end
    scene.frame_set(frame);scene.render.image_settings.file_format='PNG';scene.render.filepath=str(out/('frame-'+str(frame)+'.png'));bpy.ops.render.render(write_still=True)
report=dict(status='rendered-source-ground-review-only',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),frames=cfg['frames'],limitations=['Diagnostic plane/light were not saved to source. Images do not accept contacts or materials.'])
(out/'render-report.json').write_text(json.dumps(report,indent=2))
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
