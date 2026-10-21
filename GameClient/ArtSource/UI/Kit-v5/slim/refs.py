"""Crop each structural piece out of its approved v5 concept (1672x941 coordinates) into refs/<piece>.png, the reference
generate.sh hands Codex. Usage: py -3 refs.py"""
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
V5 = HERE.parents[4] / 'docs/Game-Design/concept-art/UI/v5'
CROPS = {  # piece: (concept, (left, top, right, bottom))
    'quest_tracker_frame': ('gameplay-hud', (1405, 295, 1652, 436)),
    'toast_frame': ('gameplay-hud', (1405, 445, 1652, 509)),
    'target_frame': ('gameplay-hud', (640, 30, 1018, 94)),
    'bar_hp_frame': ('gameplay-hud', (135, 808, 482, 884)),
    'slot_hotbar_selected': ('gameplay-hud', (572, 806, 656, 890)),
    'slot_hotbar_normal': ('gameplay-hud', (660, 806, 742, 890)),
    'button_normal': ('gameplay-hud', (738, 729, 946, 781)),
    'keycap_f': ('gameplay-hud', (766, 738, 810, 774)),
    'minimap_footer': ('gameplay-hud', (1436, 247, 1617, 276)),
    'panel_frame': ('build-mode', (28, 35, 484, 768)),
    'search_field': ('build-mode', (46, 95, 465, 132)),
    'tab_selected': ('build-mode', (45, 139, 271, 175)),
    'tab_normal': ('build-mode', (279, 139, 467, 175)),
    'slot_catalogue_normal': ('build-mode', (248, 185, 354, 279)),
    'slot_catalogue_selected': ('build-mode', (135, 285, 242, 378)),
    'dropdown_field': ('build-mode', (38, 587, 470, 629)),
    'recipe_row': ('crafting-station', (571, 320, 832, 375)),
    'queue_row': ('crafting-station', (575, 667, 1278, 712)),
    'window_frame': ('defeated', (467, 167, 1198, 713)),
}
(HERE / 'refs').mkdir(exist_ok=True)
for piece, (concept, box) in CROPS.items():
    image = Image.open(V5 / (concept + '.png')).convert('RGB')
    scale = image.width / 1672
    image.crop(tuple(round(v * scale) for v in box)).save(HERE / 'refs' / (piece + '.png'))
    print(piece, concept, box)
