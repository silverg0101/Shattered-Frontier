"""Turn the slim Codex sources (source/<piece>.png, magenta background) into the Kit-v5 PNGs and update kit.json margins.

Each piece keeps its Kit-v5 file name and 2x size. Frames get the concepts' translucent fill (dark pixels at FILL_ALPHA).
The bar frame is fitted so its hollow lands on fill_inset_rect_px [42, 13, 718, 51] (the rect MakeFramedBar uses); every
bar's _frame and _back are written from it, the _back being the frame's silhouette in trough slate.
Usage: py -3 process.py [piece ...]
"""
import json
import sys
from pathlib import Path
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
KIT = HERE.parent
FILL_ALPHA = 214  # ~84%: the concepts show the world faintly through panels
MARGINS = {  # 9-slice margins in 2x px for the slim art (the old values covered the heavy ornament)
    'panel_frame': [32, 32, 32, 32], 'window_frame': [40, 40, 40, 40], 'quest_tracker_frame': [24, 24, 24, 24],
    'toast_frame': [24, 24, 24, 24], 'target_frame': [28, 24, 28, 24], 'slot_hotbar_normal': [24] * 4,
    'slot_hotbar_selected': [24] * 4, 'slot_catalogue_normal': [24] * 4, 'slot_catalogue_selected': [24] * 4,
    'button_normal': [24, 24, 24, 24], 'tab_normal': [22, 20, 22, 20], 'tab_selected': [22, 20, 22, 20],
    'search_field': [22, 20, 22, 20], 'dropdown_field': [22, 20, 22, 20], 'recipe_row': [22, 20, 22, 20],
    'queue_row': [22, 20, 22, 20], 'minimap_footer': [20, 18, 20, 18],
    'modal_frame': [40, 40, 40, 40], 'detail_card_frame': [28, 28, 28, 28], 'button_primary_brass': [32, 24, 32, 24],
}
OPAQUE = {'bar_hp_fill', 'bar_stamina_fill', 'bar_xp_fill', 'keycap_f', 'button_primary_brass', 'hold_confirm_ring', 'minimap_ring'}
BAR_RECT = (42, 13, 718, 51)  # left, top, right (exclusive), bottom (exclusive) in the 760x64 frame
TROUGH = (14, 22, 27)


def keyed(path):
    image = Image.open(path).convert('RGBA')
    px = image.load()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = px[x, y]
            m = min(r, b) - g
            alpha = 0 if m > 90 else int(255 * (90 - m) / 50) if m > 40 else 255
            if 0 < alpha < 255:
                r, b = min(r, g + 20), min(b, g + 20)
            px[x, y] = (r, g, b, min(a, alpha))
    box = image.getchannel('A').point(lambda v: 255 if v > 8 else 0).getbbox()
    return image.crop(box) if box else image


def translucent(image):
    px = image.load()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = px[x, y]
            if a > 200 and max(r, g, b) < 48 and max(r, g, b) - min(r, g, b) < 22:  # the dark navy fill, not the brass
                px[x, y] = (r, g, b, FILL_ALPHA)
    return image


def clean_edges(image, band=6):
    """Drop chroma red/green keying fringe from the outer band (the rule scan_borders.py checks)."""
    px = image.load()
    for y in range(image.height):
        for x in range(image.width):
            if band <= x < image.width - band and band <= y < image.height - band:
                continue
            r, g, b, a = px[x, y]
            if a < 8:
                px[x, y] = (0, 0, 0, 0)  # near-transparent resample fringe carries keyed colour
            elif ((r >= 80 and g * 10 <= r and b * 10 <= r) or (g >= 80 and r * 10 <= g and b * 10 <= g) or min(r, b) - g > 40):
                px[x, y] = (r, g, b, 0)
    return image


def hollow_span(alpha, fixed, along_x):
    """Transparent run through the centre along one axis: (first, last) index."""
    size = alpha.width if along_x else alpha.height
    get = (lambda i: alpha.getpixel((i, fixed))) if along_x else (lambda i: alpha.getpixel((fixed, i)))
    mid = size // 2
    lo = mid
    while lo > 0 and get(lo - 1) < 128:
        lo -= 1
    hi = mid
    while hi < size - 1 and get(hi + 1) < 128:
        hi += 1
    return lo, hi + 1


def bar_frame(image):
    alpha = image.getchannel('A')
    x0, x1 = hollow_span(alpha, alpha.height // 2, True)
    y0, y1 = hollow_span(alpha, alpha.width // 2, False)
    if x1 - x0 < image.width * 0.5 or y1 - y0 < 2:
        raise RuntimeError('bar frame has no hollow centre; regenerate bar_hp_frame')
    sx, sy = (BAR_RECT[2] - BAR_RECT[0]) / (x1 - x0), (BAR_RECT[3] - BAR_RECT[1]) / (y1 - y0)
    scaled = image.resize((round(image.width * sx), round(image.height * sy)), Image.LANCZOS)
    out = Image.new('RGBA', (760, 64), (0, 0, 0, 0))
    out.alpha_composite(scaled, (round(BAR_RECT[0] - x0 * sx), round(BAR_RECT[1] - y0 * sy)))
    # Silhouette: every row filled between its first and last frame pixel.
    back = Image.new('RGBA', out.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(back)
    a = out.getchannel('A')
    for y in range(out.height):
        xs = [x for x in range(out.width) if a.getpixel((x, y)) > 60]
        if xs:
            draw.line([(xs[0], y), (xs[-1], y)], fill=TROUGH + (235,))
    return out, back


def main(only):
    kit_json = KIT / 'kit.json'
    data = json.loads(kit_json.read_bytes().decode('utf-8').replace(chr(13), ''))
    for source in sorted((HERE / 'source').glob('*.png')):
        piece = source.stem
        if only and piece not in only or piece == 'bar_hp_back':
            continue
        image = keyed(source)
        if piece == 'bar_hp_frame':
            frame, back = bar_frame(image)
            frame, back = clean_edges(frame), clean_edges(back)
            for bar in ('bar_hp', 'bar_stamina', 'bar_xp', 'bar_channel'):
                frame.save(KIT / (bar + '_frame.png')); back.save(KIT / (bar + '_back.png'))
                data['pieces'][bar + '_frame']['nine_slice_px'] = [40, 0, 40, 0]
                data['pieces'][bar + '_frame']['three_slice_px'] = [40, 40]
                data['pieces'][bar + '_back']['nine_slice_px'] = [40, 0, 40, 0]
                data['pieces'][bar + '_back']['three_slice_px'] = [40, 40]
            print('bars: frame + back fitted to', BAR_RECT)
            continue
        size = Image.open(KIT / data['pieces'][piece]['file']).size
        out = image.resize(size, Image.LANCZOS)
        if piece not in OPAQUE:
            out = translucent(out)
        if not piece.endswith('_fill'):
            out = clean_edges(out)
        out.save(KIT / data['pieces'][piece]['file'])
        if piece == 'bar_xp_fill':
            out.save(KIT / data['pieces']['bar_channel_fill']['file'])
        if piece in MARGINS:
            data['pieces'][piece]['nine_slice_px'] = MARGINS[piece]
        print(piece, image.size, '->', size)
    for name, piece_data in data['pieces'].items():  # the importer resamples every PNG to half size (1080p design px)
        if (KIT / piece_data['file']).is_file():
            width, height = Image.open(KIT / piece_data['file']).size
            piece_data['intended_1080p_px'] = [width // 2, height // 2]
    kit_json.write_bytes((json.dumps(data, indent=2, ensure_ascii=False) + chr(10)).replace(chr(10), chr(13) + chr(10)).encode('utf-8'))  # the file's own CRLF format


if __name__ == '__main__':
    main(set(sys.argv[1:]))
