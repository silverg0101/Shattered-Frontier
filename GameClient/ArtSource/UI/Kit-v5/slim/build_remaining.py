"""Build the remaining slim v5 chrome from approved concept crops.

Run from anywhere: py -3 GameClient/ArtSource/UI/Kit-v5/slim/build_remaining.py
The deliberately simple geometry keeps nine-slice edges straight and repeatable.
"""
import json
from pathlib import Path
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
KIT = HERE.parent
V5 = HERE.parents[4] / 'docs/Game-Design/concept-art/UI/v5'
OUT = HERE.parents[4] / 'Artifacts/slim2-vs-concept.png'

# Boxes are in the approved concepts' 1672 x 941 coordinate space.
CROPS = {
    'modal_frame': ('market-board', (213, 92, 1462, 795)),
    'detail_card_frame': ('market-board', (1094, 251, 1435, 771)),
    'button_primary_brass': ('trade', (840, 730, 1207, 792)),
    'hold_confirm_ring': ('trade', (858, 737, 904, 784)),
    'minimap_ring': ('gameplay-hud', (1399, 17, 1652, 253)),
    'minimap_footer': ('gameplay-hud', (1436, 246, 1617, 277)),
}
MARGINS = {
    'modal_frame': [28, 28, 28, 28],
    'detail_card_frame': [24, 24, 24, 24],
    'button_primary_brass': [28, 20, 28, 20],
    'minimap_footer': [20, 18, 20, 18],
}
BRASS = (177, 143, 82, 245)
HIGHLIGHT = (218, 184, 115, 205)
LOWLIGHT = (70, 57, 37, 210)
NAVY = (13, 22, 27, 210)


def polygon(w, h, inset, chamfer):
    l, t, r, b = inset, inset, w - 1 - inset, h - 1 - inset
    c = chamfer
    return [(l+c,t), (r-c,t), (r,t+c), (r,b-c), (r-c,b),
            (l+c,b), (l,b-c), (l,t+c)]


def panel(size, margin, detail=False):
    w, h = size
    im = Image.new('RGBA', size)
    d = ImageDraw.Draw(im)
    p = polygon(w, h, 5, 13 if detail else 16)
    d.polygon(p, fill=NAVY)
    d.line(p+[p[0]], fill=LOWLIGHT, width=6, joint='curve')
    d.line(p+[p[0]], fill=BRASS, width=3, joint='curve')
    q = polygon(w, h, 11, 10 if detail else 13)
    d.line(q+[q[0]], fill=(209, 177, 109, 64), width=1)
    # Restrained corner ticks from the market board concept; fit inside slice caps.
    tick = min(margin[0]-12, 13)
    for x, sx in ((19,1),(w-20,-1)):
        for y, sy in ((19,1),(h-20,-1)):
            d.line([(x,y+sy*tick),(x,y),(x+sx*tick,y)], fill=HIGHLIGHT, width=2)
    if not detail:
        cx = w//2
        d.line([(cx-8,6),(cx,1),(cx+8,6),(cx,11),(cx-8,6)], fill=HIGHLIGHT, width=2)
    return im


def button(size):
    w, h = size
    im = Image.new('RGBA', size)
    d = ImageDraw.Draw(im)
    p = polygon(w,h,4,18)
    d.polygon(p, fill=(73, 58, 37, 240))
    # A darker, flat label field gives ivory text clear contrast at runtime.
    inner = polygon(w,h,11,13)
    d.polygon(inner, fill=(82, 65, 39, 247))
    d.line(p+[p[0]], fill=LOWLIGHT, width=7, joint='curve')
    d.line(p+[p[0]], fill=BRASS, width=4, joint='curve')
    d.line(inner+[inner[0]], fill=(222, 188, 117, 130), width=2)
    d.line([(35,15),(w-36,15)], fill=(239, 204, 137, 43), width=3)
    return im


def ring(size, minimap=False):
    w,h=size
    im=Image.new('RGBA',size)
    d=ImageDraw.Draw(im)
    pad=8 if minimap else 5
    box=(pad,pad,w-pad-1,h-pad-1)
    if minimap:
        # A clear aperture lets the map render target show through.
        d.ellipse(box, outline=LOWLIGHT, width=9)
        d.ellipse((pad+3,pad+3,w-pad-4,h-pad-4), outline=BRASS, width=3)
        d.ellipse((pad+9,pad+9,w-pad-10,h-pad-10), outline=(223,190,120,75), width=2)
        cx=w//2
        for y,sy in ((pad,-1),(h-pad-1,1)):
            d.polygon([(cx-7,y),(cx,y+sy*7),(cx+7,y),(cx,y-sy*7)], fill=(166,131,73,225))
        for x,sx in ((pad,-1),(w-pad-1,1)):
            d.polygon([(x,h//2-6),(x+sx*6,h//2),(x,h//2+6),(x-sx*6,h//2)], fill=(144,111,64,210))
    else:
        d.ellipse(box, fill=(14,22,26,225), outline=BRASS, width=3)
        d.ellipse((pad+6,pad+6,w-pad-7,h-pad-7), outline=(202,170,105,115), width=2)
        # Base track only: runtime draws the actual hold progress.
    return im


def footer(size):
    w,h=size
    im=Image.new('RGBA',size)
    d=ImageDraw.Draw(im)
    p=polygon(w,h,3,12)
    d.polygon(p,fill=(12,20,25,226))
    d.line(p+[p[0]],fill=BRASS,width=3)
    q=polygon(w,h,8,8)
    d.line(q+[q[0]],fill=(211,179,111,65),width=1)
    return im


def preview(crops, built):
    cell_w, cell_h = 570, 280
    sheet=Image.new('RGB',(cell_w*2,cell_h*len(built)),(31,37,40))
    d=ImageDraw.Draw(sheet)
    for row,(name,im) in enumerate(built.items()):
        for col,source in enumerate((crops[name],im)):
            src=source.convert('RGBA')
            src.thumbnail((cell_w-28,cell_h-46),Image.Resampling.LANCZOS)
            x=col*cell_w+(cell_w-src.width)//2
            y=row*cell_h+33+(cell_h-46-src.height)//2
            sheet.paste(src,(x,y),src)
        d.text((12,row*cell_h+8),name+' — concept crop',fill=(235,231,217))
        d.text((cell_w+12,row*cell_h+8),name+' — slim 2x PNG',fill=(235,231,217))
    OUT.parent.mkdir(exist_ok=True)
    sheet.save(OUT)


def main():
    data=json.loads((KIT/'kit.json').read_text(encoding='utf-8'))
    crops={}
    built={}
    for name,(concept,box) in CROPS.items():
        source=Image.open(V5/(concept+'.png')).convert('RGB')
        scale=source.width/1672
        crop=source.crop(tuple(round(v*scale) for v in box))
        crops[name]=crop
        path=KIT/data['pieces'][name]['file']
        size=Image.open(path).size
        if name in ('modal_frame','detail_card_frame'):
            im=panel(size,MARGINS[name],name=='detail_card_frame')
        elif name=='button_primary_brass':
            im=button(size)
        elif name=='hold_confirm_ring':
            im=ring(size)
        elif name=='minimap_ring':
            im=ring(size,True)
        else:
            im=footer(size)
        im.save(path)
        built[name]=im
        if name in MARGINS:
            data['pieces'][name]['nine_slice_px']=MARGINS[name]
        data['pieces'][name]['intended_1080p_px']=[size[0]//2,size[1]//2]
    (KIT/'kit.json').write_bytes((json.dumps(data,indent=2,ensure_ascii=False)+'\n').replace('\n','\r\n').encode('utf-8'))
    preview(crops,built)
    print('Built',', '.join(built),'and',OUT)


if __name__=='__main__':
    main()
