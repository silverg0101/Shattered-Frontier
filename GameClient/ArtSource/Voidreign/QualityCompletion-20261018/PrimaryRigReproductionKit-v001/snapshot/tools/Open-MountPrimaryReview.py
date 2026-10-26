"""Visible, unsaved Blender primary joint-pilot review; never saves the source."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--report',required=True)
p.add_argument('--play',action='store_true')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
report=Path(a.report);assert not report.exists()
source=Path(bpy.data.filepath);assert source.is_file()
rig=bpy.data.objects['PRIMARY | anatomical rig pilot']
rig.hide_set(False);rig.show_in_front=True
for window in bpy.context.window_manager.windows:
    for area in window.screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.region_3d.view_perspective='CAMERA'
report.parent.mkdir(parents=True,exist_ok=True)
report.write_text(json.dumps(dict(status='visible-unsaved-primary-joint-review-requested',
    source=str(source),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    autoplay_requested=a.play,limitations=['Joint-flex pilot, not locomotion or accepted stance.','No source save or acceptance.']),indent=2))
if a.play:
    def play():
        window=bpy.context.window_manager.windows[0]
        with bpy.context.temp_override(window=window,screen=window.screen):
            if not window.screen.is_animation_playing:bpy.ops.screen.animation_play()
        return None
    bpy.app.timers.register(play,first_interval=1.)
