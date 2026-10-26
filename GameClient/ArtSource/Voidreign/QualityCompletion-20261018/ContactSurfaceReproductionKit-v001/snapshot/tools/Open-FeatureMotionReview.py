"""Unsaved Blender GUI review layout for an already checked motion candidate.

No source save, simulation, render, export or cache mutation is requested.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--object', required=True)
p.add_argument('--report', required=True)
p.add_argument('--play', action='store_true')
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
assert not bpy.app.background, 'This utility is an explicitly requested visible review'
obj = bpy.data.objects[a.object]
scene = bpy.context.scene
colors = np.tile(np.array([.38, .40, .43, 1.], np.float32), (len(obj.data.vertices), 1))
for name, color in [('Mane', [.025, .65, .60]), ('Tail', [.9, .25, .025])]:
    attribute = obj.data.color_attributes.get('Motion | ' + name)
    if attribute:
        values = np.empty(len(colors) * 4, np.float32)
        attribute.data.foreach_get('color', values)
        mask = values.reshape(-1, 4)[:, :1]
        colors[:, :3] = colors[:, :3] * (1 - mask) + np.array(color) * mask
overlay = obj.data.color_attributes.new(name='Review overlay | unsaved weighted regions', type='FLOAT_COLOR', domain='POINT')
overlay.data.foreach_set('color', colors.ravel())
obj.data.color_attributes.active_color = overlay
for other in scene.objects:
    if other.type == 'MESH' and other != obj:
        other.color = (.42, .60, .68, 1.) if 'Rider' in other.name else (.65, .32, .14, 1.)
scene.frame_set(1)
scene.sync_mode = 'FRAME_DROP'
scene['Review notice'] = 'Tail contact checks pass; Mane/rider contacts still under repair. Unsaved review colors only.'
for window in bpy.context.window_manager.windows:
    for area in window.screen.areas:
        if area.type == 'VIEW_3D':
            space = area.spaces.active
            space.shading.type = 'SOLID'
            space.shading.color_type = 'VERTEX'
            space.overlay.show_extras = False
            space.overlay.show_floor = False
            space.region_3d.view_perspective = 'CAMERA'
report_path = Path(a.report)
assert not report_path.exists()
report_path.parent.mkdir(parents=True, exist_ok=True)
report_path.write_text(json.dumps(dict(status='visible-unsaved-review-requested', source=bpy.data.filepath,
    source_sha256=hashlib.sha256(Path(bpy.data.filepath).read_bytes()).hexdigest(),
    autoplay_requested=a.play, limitations=['Acceptance is recorded in source-bound check reports; this review grants no acceptance.',
    'Original source was not saved; review overlay affects this GUI session only.']), indent=2))
if a.play:
    def play():
        window = bpy.context.window_manager.windows[0]
        with bpy.context.temp_override(window=window, screen=window.screen):
            if not window.screen.is_animation_playing:
                bpy.ops.screen.animation_play()
        return None
    bpy.app.timers.register(play, first_interval=1.0)
