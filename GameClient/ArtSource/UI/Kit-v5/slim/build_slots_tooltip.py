"""Rebuild the remaining slim tooltip and slots from v5 concept samples.

Run: py -3 GameClient/ArtSource/UI/Kit-v5/slim/build_slots_tooltip.py
"""
import json
from pathlib import Path
from statistics import median

from PIL import Image, ImageDraw


KIT = Path(__file__).resolve().parent.parent
ROOT = KIT.parents[3]
CONCEPT = ROOT / 'docs/Game-Design/concept-art/UI/v5'
PREVIEW = ROOT / 'Artifacts/slim3-vs-concept.png'

# Approved concept images use a 1672 x 941 coordinate grid.
CROPS = {
    'tooltip_frame': ('inventory', (510, 102, 870, 390)),
    'slot_bag_normal': ('trade', (465, 392, 549, 474)),
    'slot_bag_selected': ('inventory', (427, 111, 496, 181)),
    'slot_equipment_normal': ('inventory', (115, 111, 185, 182)),
    'slot_equipment_selected': ('inventory', (427, 111, 496, 181)),
    'slot_hotbar_normal': ('gameplay-hud', (659, 807, 744, 895)),
    'slot_hotbar_selected': ('inventory', (578, 816, 663, 902)),
}


def sample(im, box):
    pixels = list(im.crop(box).convert('RGB').get_flattened_data())
    return tuple(round(median(p[i] for p in pixels)) for i in range(3))


def rgba(rgb, alpha=255):
    return tuple(rgb) + (alpha,)


def slot(size, dark, brass, selected, hotbar=False):
    w, h = size
    im = Image.new('RGBA', size)
    d = ImageDraw.Draw(im)
    pad = 4
    end = (w - pad - 1, h - pad - 1)
    # The plain square and recessed inner shade follow inventory/trade slots.
    if hotbar:
        c = 10
        outline = [(pad+c,pad), (end[0]-c,pad), (end[0],pad+c),
                   (end[0],end[1]-c), (end[0]-c,end[1]),
                   (pad+c,end[1]), (pad,end[1]-c), (pad,pad+c)]
        d.polygon(outline, fill=rgba(dark, 227))
    else:
        d.rectangle((pad, pad, *end), fill=rgba(dark, 227))
    d.rectangle((pad + 5, pad + 5, end[0] - 5, end[1] - 5),
                fill=(12, 20, 24, 219))
    d.line((pad + 7, end[1] - 6, end[0] - 7, end[1] - 6),
           fill=(6, 12, 15, 110), width=2)
    edge = tuple(min(255, c + (43 if selected else 0)) for c in brass)
    if hotbar:
        d.line(outline + [outline[0]], fill=rgba(edge, 215 if selected else 160), width=2)
    else:
        d.rectangle((pad + 1, pad + 1, end[0] - 1, end[1] - 1),
                    outline=rgba(edge, 215 if selected else 125), width=2)
    d.rectangle((pad + 4, pad + 4, end[0] - 4, end[1] - 4),
                outline=rgba(edge, 125 if selected else 52), width=1)
    if selected:
        d.rectangle((pad + 7, pad + 7, end[0] - 7, end[1] - 7),
                    outline=(226, 215, 186, 65), width=2)
    return im


def tooltip(size, dark, brass):
    w, h = size
    im = Image.new('RGBA', size)
    d = ImageDraw.Draw(im)
    p = 5
    # Small corner steps match the inventory tooltip without thick ornaments.
    shape = [(p+13,p), (w-p-14,p), (w-p,p+14), (w-p,h-p-15),
             (w-p-15,h-p), (p+14,h-p), (p,h-p-14), (p,p+13)]
    d.polygon(shape, fill=rgba(dark, 230))
    d.line(shape + [shape[0]], fill=rgba(brass, 194), width=3, joint='curve')
    inner = [(x + (5 if x < w//2 else -5), y + (5 if y < h//2 else -5))
             for x, y in shape]
    d.line(inner + [inner[0]], fill=rgba(brass, 67), width=1)
    for x, sx in ((p+3, 1), (w-p-4, -1)):
        for y, sy in ((p+3, 1), (h-p-4, -1)):
            d.line((x, y+sy*16, x, y, x+sx*16, y),
                   fill=rgba(brass, 215), width=2)
    return im


def main():
    data = json.loads((KIT / 'kit.json').read_text(encoding='utf-8'))
    sources = {name: Image.open(CONCEPT / (stem + '.png')).convert('RGB')
               for name, (stem, _) in CROPS.items()}
    inventory = sources['tooltip_frame']
    # Pixel samples are taken from unlettered, unoccupied parts of the concepts.
    dark = sample(inventory, (693, 353, 733, 373))
    brass = inventory.getpixel((515, 105))[:3]  # clean tooltip top rule
    crops, built = {}, {}
    for name, (stem, box) in CROPS.items():
        src = sources[name]
        scale = src.width / 1672
        crops[name] = src.crop(tuple(round(v * scale) for v in box))
        path = KIT / data['pieces'][name]['file']
        size = Image.open(path).size
        built[name] = (tooltip(size, dark, brass) if name == 'tooltip_frame'
                       else slot(size, dark, brass, name.endswith('selected'),
                                 name.startswith('slot_hotbar')))
        built[name].save(path)
        data['pieces'][name]['nine_slice_px'] = ([20, 20, 20, 20] if name == 'tooltip_frame'
                                                  else [12, 12, 12, 12])
        data['pieces'][name]['intended_1080p_px'] = [size[0]//2, size[1]//2]
    (KIT / 'kit.json').write_bytes((json.dumps(data, indent=2, ensure_ascii=False) + '\n')
                                   .replace('\n', '\r\n').encode('utf-8'))

    cell_w, cell_h = 450, 240
    sheet = Image.new('RGB', (cell_w*2, cell_h*len(built)), (37, 43, 46))
    d = ImageDraw.Draw(sheet)
    for row, (name, art) in enumerate(built.items()):
        for col, src in enumerate((crops[name], art)):
            pic = src.copy().convert('RGBA')
            pic.thumbnail((cell_w-24, cell_h-43), Image.Resampling.LANCZOS)
            if name != 'tooltip_frame':
                pic = pic.resize((pic.width*2, pic.height*2), Image.Resampling.NEAREST)
                pic.thumbnail((190, 190), Image.Resampling.NEAREST)
            x = col*cell_w + (cell_w-pic.width)//2
            y = row*cell_h + 30 + (cell_h-43-pic.height)//2
            sheet.paste(pic, (x, y), pic)
        d.text((12, row*cell_h+8), name + ' - concept crop', fill=(235,231,217))
        d.text((cell_w+12, row*cell_h+8), name + ' - slim 2x PNG', fill=(235,231,217))
    PREVIEW.parent.mkdir(exist_ok=True)
    sheet.save(PREVIEW)
    print('Built', ', '.join(built), 'and', PREVIEW)


if __name__ == '__main__':
    main()
