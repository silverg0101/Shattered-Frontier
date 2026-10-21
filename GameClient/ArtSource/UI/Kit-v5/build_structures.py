"""Cut the approved-style generated atlas into the fixed UI Kit v5 contract."""
from pathlib import Path
import colorsys
import json
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = Path(__file__).parent
ATLAS = Image.open(ROOT / "structural-source.png").convert("RGBA")
MANIFEST = json.loads((ROOT / "kit.json").read_text())
BOX = {
    "panel": (40, 90, 607, 254),
    "bar": (645, 121, 1194, 211),
    "slot": (1236, 55, 1462, 275),
    "button": (53, 422, 595, 582),
    "key": (709, 412, 885, 586),
    "map": (1082, 310, 1452, 661),
    "badge": (105, 660, 369, 951),
    "ring": (503, 657, 786, 945),
    "action": (864, 757, 1498, 920),
}


def nine(source, size, margin):
    w, h = source.size
    W, H = size
    l, t, r, b = margin
    # Source corners follow the same proportions as the output corner margins.
    sl, sr = min(55, w // 4), min(55, w // 4)
    st, sb = min(48, h // 4), min(48, h // 4)
    if w < 300:
        sl = sr = min(45, w // 4)
        st = sb = min(45, h // 4)
    xs, ys = [0, sl, w - sr, w], [0, st, h - sb, h]
    xd, yd = [0, l, W - r, W], [0, t, H - b, H]
    out = Image.new("RGBA", size)
    for j in range(3):
        for i in range(3):
            piece = source.crop((xs[i], ys[j], xs[i+1], ys[j+1]))
            dw, dh = xd[i+1]-xd[i], yd[j+1]-yd[j]
            if dw and dh:
                out.paste(piece.resize((dw, dh), Image.Resampling.BICUBIC), (xd[i], yd[j]))
    return out


def recolor(im, hue, saturation=1.0, value=1.0):
    out = im.copy()
    px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r,g,b,a = px[x,y]
            h,s,v = colorsys.rgb_to_hsv(r/255,g/255,b/255)
            if s > .18 and r > g * 1.12:  # crimson fill, not brass
                nr,ng,nb = colorsys.hsv_to_rgb(hue,min(1,s*saturation),min(1,v*value))
                px[x,y] = (round(nr*255),round(ng*255),round(nb*255),a)
    return out


def alpha_from_old(name, im, *, interior=225):
    old = Image.open(ROOT / f"{name}.png").convert("RGBA")
    alpha = old.getchannel("A")
    if interior is not None:
        # Preserve the original transparent silhouette and translucent slate body.
        alpha = alpha.point(lambda a: min(a, interior) if a < 250 else a)
    im.putalpha(alpha)
    return im


def save(name, source, margin=None, *, tint=None, interior=225):
    size = Image.open(ROOT / f"{name}.png").size
    im = nine(source, size, margin) if margin else source.resize(size, Image.Resampling.LANCZOS)
    if tint:
        im = recolor(im, *tint)
    im = alpha_from_old(name, im, interior=interior)
    im.save(ROOT / f"{name}.png")


panel = ATLAS.crop(BOX["panel"])
bar = ATLAS.crop(BOX["bar"])
slot = ATLAS.crop(BOX["slot"])
button = ATLAS.crop(BOX["button"])
key = ATLAS.crop(BOX["key"])
badge = ATLAS.crop(BOX["badge"])
ring = ATLAS.crop(BOX["ring"])
action = ATLAS.crop(BOX["action"])

panel_names = ["window_frame", "panel_frame", "tooltip_frame", "modal_frame", "target_frame", "quest_tracker_frame", "toast_frame", "news_frame", "detail_card_frame", "search_field", "dropdown_field", "realm_plate", "title_banner", "legend_frame", "journal_book_frame", "character_row", "recipe_row", "quest_row", "tab_normal", "tab_selected", "portrait_well", "respawn_image_well", "map_label_plate", "queue_row", "preset_card", "minimap_footer", "toggle_segment", "stepper_field", "progress_plate", "slider_track"]
for name in panel_names:
    size = Image.open(ROOT / f"{name}.png").size
    m = MANIFEST["pieces"][name]["nine_slice_px"]
    # Keep a meaningful corner area in large frames without overrunning small rows.
    margin = [min(max(m[0], 36), size[0]//4), min(max(m[1], 28), size[1]//4), min(max(m[2], 36), size[0]//4), min(max(m[3], 28), size[1]//4)]
    MANIFEST["pieces"][name]["nine_slice_px"] = margin
    save(name, panel, margin)

for name in ["button_normal", "button_hover", "button_pressed", "button_disabled", "button_primary_brass", "button_destructive"]:
    source = button if name == "button_primary_brass" else panel
    if name == "button_destructive":
        source = bar
    margin = [40, 28, 40, 28]
    MANIFEST["pieces"][name]["nine_slice_px"] = margin
    save(name, source, margin, interior=190 if name == "button_disabled" else 225)

for name in ["slot_bag_normal", "slot_bag_selected", "slot_equipment_normal", "slot_equipment_selected", "slot_hotbar_normal", "slot_hotbar_selected", "slot_catalogue_normal", "slot_catalogue_selected"]:
    size = Image.open(ROOT / f"{name}.png").size
    m = max(25, min(36, size[0]//5))
    MANIFEST["pieces"][name]["nine_slice_px"] = [m,m,m,m]
    save(name, slot, [m,m,m,m])

for name, tint in [("bar_hp",None),("bar_stamina",(.12,.72,1)),("bar_xp",(.19,.75,.9)),("bar_channel",(.09,.35,.9))]:
    margin=[54,12,54,12]
    MANIFEST["pieces"][name]["nine_slice_px"] = margin
    save(name, bar, margin, tint=tint, interior=None)

save("hex_level_badge", badge, interior=None)
save("action_bar", action, [48,32,48,32])
MANIFEST["pieces"]["action_bar"]["nine_slice_px"]=[48,32,48,32]

for name in ["gathering_channel_ring", "hold_confirm_ring", "loading_spinner"]:
    save(name, ring, interior=None)

# The bezel and ring have open centers; use their radial shape, retaining the generated N.
for name, source, inner in [("minimap_ring", ATLAS.crop(BOX["map"]), .78)]:
    size=Image.open(ROOT/f"{name}.png").size
    im=source.resize(size,Image.Resampling.LANCZOS)
    # Key the generated brass rather than its dark studio backdrop.
    a=Image.new("L",size,0)
    src=im.load(); mask=a.load()
    for yy in range(size[1]):
        for xx in range(size[0]):
            r,g,b,_=src[xx,yy]
            if r>105 and g>75 and r>b*1.28:
                mask[xx,yy]=255
    a=a.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(.7))
    im.putalpha(a)
    im.save(ROOT/f"{name}.png")

font_path=ROOT.parent.parent/"Fonts"/"EBGaramond"/"EBGaramond-Regular.ttf"
for name in [n for n in MANIFEST["pieces"] if n.startswith("keycap_")]:
    label=name[7:].upper()
    size=Image.open(ROOT/f"{name}.png").size
    im=key.resize(size,Image.Resampling.LANCZOS)
    im=alpha_from_old(name,im,interior=None)
    # Erase the source F with a slate patch so runtime labels remain legible.
    draw=ImageDraw.Draw(im)
    w,h=size
    draw.rounded_rectangle((w*.19,h*.18,w*.81,h*.84),radius=3,fill=(18,27,33,240))
    font=ImageFont.truetype(str(font_path),round(h*(.43 if len(label)>2 else .58)))
    draw.text((w/2,h*.49),label,font=font,anchor="mm",fill=(237,225,199,255),stroke_width=1,stroke_fill=(74,55,34,255))
    im.save(ROOT/f"{name}.png")

# Small trim uses the generated brass edge, not a flat line.
for name in ["divider","section_rule","cost_short_underline","journal_hinge"]:
    size=Image.open(ROOT/f"{name}.png").size
    trim=panel.crop((75,3,520,14))
    im=trim.resize(size,Image.Resampling.BICUBIC)
    im=alpha_from_old(name,im,interior=None)
    im.save(ROOT/f"{name}.png")

(ROOT/"kit.json").write_text(json.dumps(MANIFEST,indent=2)+"\n")
