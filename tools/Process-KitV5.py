"""Prepare Kit-v5 for import: optionally apply the G2 nine-slice margins to kit.json, then resample each 2x PNG to its intended
1080p size (Slate draws box corners at the texture's real pixels, so only a 1x texture draws at design size).
Run: py tools/Process-KitV5.py [--apply-g2-margins]   (then python tools/Run-InEditor.py tools/Import-HudKit.py)
Writes GameClient/ArtSource/UI/Kit-v5/processed/<file>; the 2x sources stay as the basis for a later 4K kit."""
import json
import os
import sys

from PIL import Image

KIT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'GameClient', 'ArtSource', 'UI', 'Kit-v5'))
# 2x px margins (left, top, right, bottom); a single number applies to all four sides. docs/Game-Design/concept-art/UI/v5/IMPLEMENTATION-FIX.md G2.
MARGINS = {
    'panel_frame': (36, 140), 'window_frame': (36, 176), 'modal_frame': (36, 206), 'tooltip_frame': (36, 86),
    'detail_card_frame': (36, 142), 'quest_tracker_frame': (36, 76), 'target_frame': (36, 44),
    'toast_frame': (68, 28, 36, 28), 'button_primary_brass': (56, 48), 'tab_normal': (36, 30), 'tab_selected': (36, 30),
    'search_field': (36, 26), 'dropdown_field': (36, 28), 'action_bar': (52, 44), 'minimap_footer': (36, 22),
}
for name in ('button_normal', 'button_hover', 'button_pressed', 'button_disabled', 'button_destructive'): MARGINS[name] = (40, 40)
for name in ('slot_hotbar_normal', 'slot_hotbar_selected'): MARGINS[name] = (44,)
for name in ('slot_catalogue_normal', 'slot_catalogue_selected'): MARGINS[name] = (50,)
for name in ('slot_bag_normal', 'slot_bag_selected'): MARGINS[name] = (32,)
for name in ('slot_equipment_normal', 'slot_equipment_selected'): MARGINS[name] = (36,)

path = os.path.join(KIT, 'kit.json')
doc = json.load(open(path))
# kit.json is the source of truth for margins (the art session owns it); the G2 table is applied only on request.
if '--apply-g2-margins' in sys.argv:
    for name, m in MARGINS.items():
        if len(m) == 1: m = (m[0],) * 4
        elif len(m) == 2: m = (m[0], m[1], m[0], m[1])
        doc['pieces'][name]['nine_slice_px'] = list(m)
    json.dump(doc, open(path, 'w', newline='\r\n'), indent=2)  # the committed kit.json is CRLF

count = 0
for name, piece in doc['pieces'].items():
    source = os.path.join(KIT, piece['file'])
    size = piece.get('intended_1080p_px')
    if not size or not os.path.isfile(source): continue
    target = os.path.join(KIT, 'processed', piece['file'])
    os.makedirs(os.path.dirname(target), exist_ok=True)
    Image.open(source).convert('RGBA').resize((int(size[0]), int(size[1])), Image.LANCZOS).save(target)
    count += 1
print('resampled', count, 'pieces')
