"""CPU multiview ID closeups from exact mesh component selections.

Candidate triangles are drawn last for identification, including occluded parts.
These diagrams locate shapes/endpoints; they do not prove surface visibility.
"""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--inspection', required=True)
p.add_argument('--component', type=int, action='append')
p.add_argument('--output', required=True)
a = p.parse_args()
path = Path(a.inspection)
report = json.loads(path.read_text())
entry = report['objects'][0]
data = np.load(path.parent / entry['selection_file'])
points, triangles, labels = data['vertices'], data['triangles'], data['component_per_vertex']
face_labels = labels[triangles[:, 0]]
vectors = points[triangles]
normal = np.cross(vectors[:, 1]-vectors[:, 0], vectors[:, 2]-vectors[:, 0])
normal /= np.maximum(np.linalg.norm(normal, axis=1)[:, None], 1e-12)
out = Path(a.output)
out.mkdir(parents=True, exist_ok=False)
font_file = next((f for f in ['C:/Windows/Fonts/arial.ttf',
    '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'] if Path(f).exists()), None)
font = lambda n: ImageFont.truetype(font_file, n) if font_file else ImageFont.load_default()
views = [('Positive X', 1, 2, 0, 1), ('Negative X', 1, 2, 0, -1),
         ('Front', 0, 2, 1, -1), ('Top', 0, 1, 2, 1)]
records = []
for cid in a.component or range(entry['component_count']):
    selected = points[labels == cid]
    assert len(selected)
    center = (selected.min(0)+selected.max(0))/2
    extent = max(float(np.ptp(selected, axis=0).max())+.20, .30)
    canvas = Image.new('RGB', (1100, 1180), 'white')
    for index, (name, horizontal, vertical, depth_axis, direction) in enumerate(views):
        panel = Image.new('RGB', (550, 550), (238, 241, 244))
        draw = ImageDraw.Draw(panel)
        scale = 470/extent
        xsign = -1 if name == 'Negative X' else 1
        xy = np.column_stack([(points[:, horizontal]-center[horizontal])*scale*xsign+275,
                              (center[vertical]-points[:, vertical])*scale+300])
        face_xy = xy[triangles]
        visible = ((face_xy[:, :, 0].max(1) >= 0) & (face_xy[:, :, 0].min(1) < 550)
                   & (face_xy[:, :, 1].max(1) >= 40) & (face_xy[:, :, 1].min(1) < 550))
        depth = vectors[:, :, depth_axis].mean(1)*direction
        shade = np.clip(.42+.42*np.abs(normal[:, depth_axis]), 0, 1)
        for face in np.flatnonzero(visible)[np.argsort(depth[visible])]:
            grey = int(255*shade[face])
            draw.polygon([tuple(q) for q in face_xy[face]], fill=(grey, grey, grey))
        for face in np.flatnonzero(visible & (face_labels == cid)):
            draw.polygon([tuple(q) for q in face_xy[face]], fill=(225, 57, 31))
        draw.rectangle((0, 0, 550, 44), fill=(28, 38, 52))
        draw.text((14, 10), f'C{cid:04} | {name} | span {extent:.2f}m', font=font(20), fill='white')
        canvas.paste(panel, ((index % 2)*550, (index // 2)*550))
    draw = ImageDraw.Draw(canvas)
    component = entry['components'][cid]
    draw.text((16, 1110), component['id'] + f" | {component['triangles']} triangles", font=font(24), fill='black')
    draw.text((16, 1146), 'Red drawn through context for ID. Not an occlusion/attachment test.', font=font(19), fill=(90, 35, 25))
    file = f'C{cid:04}.png'
    canvas.save(out/file)
    records.append(dict(id=component['id'], image=file, bounds_m=component['bounds_m']))
(out/'atlas.json').write_text(json.dumps(dict(source_sha256=report['source_sha256'],
    mesh_hash=entry['mesh_hash'], inspection=str(path), components=records,
    method='CPU triangle painter; selected components drawn last through context'), indent=2))
print(json.dumps(dict(output=str(out), components=len(records))))
