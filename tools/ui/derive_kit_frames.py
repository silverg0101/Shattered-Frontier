"""Derive the kit v7 select/hover frames from the generated stepper buttons.

The generated select_frame/hover_frame only drew top and bottom lines. The stepper buttons carry the same gold frame on all four
sides, so their dark interior is cut out (alpha cleanup only; no drawing) to give 9-slice frames with a clear centre.
Run: py -3 tools/ui/derive_kit_frames.py   (writes GameClient/ArtSource/UI/Kit-v7/select_frame.png and hover_frame.png)
"""
from pathlib import Path
from PIL import Image

KIT = Path(__file__).resolve().parents[2] / 'GameClient/ArtSource/UI/Kit-v7'
INSET = 6        # border pixels always kept
DARK = 75        # interior pixels darker than this become transparent


def derive(source: str, target: str) -> None:
    im = Image.open(KIT / f'{source}.png').convert('RGBA')
    px = im.load()
    w, h = im.size
    for y in range(INSET, h - INSET):
        for x in range(INSET, w - INSET):
            r, g, b, a = px[x, y]
            if 0.299 * r + 0.587 * g + 0.114 * b < DARK:
                px[x, y] = (r, g, b, 0)
    im.save(KIT / f'{target}.png')
    assert im.getpixel((w // 2, h // 2))[3] == 0 and im.getpixel((2, h // 2))[3] > 200, target


derive('stepper_button_hover', 'select_frame')
derive('stepper_button_normal', 'hover_frame')
print('select_frame, hover_frame: 84x84, 9-slice margins 14 px each side')
