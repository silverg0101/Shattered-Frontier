"""Rebuild the v5 contact sheet and a 1920x1080 kit-only HUD study."""
from pathlib import Path
import json
import math
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).parent
CONCEPT=ROOT.parents[3]/'docs'/'Game-Design'/'concept-art'/'UI'/'v5'
PIECES=json.loads((ROOT/'kit.json').read_text())['pieces']
FONT=ROOT.parent.parent/'Fonts'/'EBGaramond'/'EBGaramond-Regular.ttf'

def asset(name,size=None):
    im=Image.open(ROOT/PIECES[name]['file']).convert('RGBA')
    return im.resize(size,Image.Resampling.LANCZOS) if size else im

names=list(PIECES)
cw,ch,cols=156,132,10
contact=Image.new('RGB',(cw*cols,ch*math.ceil(len(names)/cols)),(43,50,53))
draw=ImageDraw.Draw(contact)
font=ImageFont.truetype(str(FONT),14)
for i,name in enumerate(names):
    x,y=(i%cols)*cw,(i//cols)*ch
    im=asset(name)
    im.thumbnail((cw-10,ch-28),Image.Resampling.LANCZOS)
    contact.paste(im,(x+(cw-im.width)//2,y+(ch-28-im.height)//2),im)
    draw.text((x+4,y+ch-22),name,font=font,fill=(229,222,208))
contact.save(CONCEPT/'kit-v5-contact.png')
if '--contact-only' in sys.argv:
    raise SystemExit(0)

master=Image.open(CONCEPT/'gameplay-hud.png').convert('RGB')
# Scene-only center crop removes the master HUD; it is the sole background layer.
scene=master.crop((225,105,1390,790)).resize((1920,1080),Image.Resampling.LANCZOS)
hud=scene.convert('RGBA')
draw=ImageDraw.Draw(hud)
big=ImageFont.truetype(str(FONT),29)
small=ImageFont.truetype(str(FONT),23)
tiny=ImageFont.truetype(str(FONT),19)

def put(name,x,y,w,h):
    im=asset(name,(w,h)); hud.alpha_composite(im,(x,y))

put('target_frame',735,30,440,77)
put('bar_hp',760,75,402,31)
draw.text((955,48),'Wild Boar',font=big,anchor='mm',fill='#eee4d2')
draw.text((955,90),'320 / 320',font=small,anchor='mm',fill='#eee4d2')
put('minimap_ring',1603,18,278,278)
put('minimap_footer',1645,282,197,37)
draw.text((1740,301),'Silverbrook · 227 m',font=tiny,anchor='mm',fill='#eee4d2')
put('quest_tracker_frame',1600,340,284,161)
draw.text((1622,355),'Silverbrook',font=big,fill='#eee4d2')
for i,s in enumerate(['Speak to the Millwarden','Gather 3 Iron Ore   0/3','Clear the Boar Den  0/1']):
    put('checkbox',1625,392+i*30,20,20)
    draw.text((1652,392+i*30),s,font=tiny,fill='#eee4d2')
for i,(icon,top,bottom) in enumerate([('icon_oak_log','Item gained','Oak Logs ×4'),('icon_woodcutting_axes','Skill gain','Woodcutting +1'),('icon_leather_bag','Warning','Inventory nearly full')]):
    y=512+i*79
    put('toast_frame',1603,y,280,70)
    put(icon,1615,y+9,49,49)
    draw.text((1673,y+9),top,font=tiny,fill='#dac48e')
    draw.text((1673,y+31),bottom,font=small,fill='#eee4d2')
put('hex_level_badge',37,890,152,160)
draw.text((114,1023),'18',font=big,anchor='mm',fill='#eee4d2')
for i,(name,value) in enumerate([('bar_hp','424 / 424'),('bar_stamina','150 / 150')]):
    y=930+i*45
    put(name,156,y,395,42)
    draw.text((356,y+20),value,font=small,anchor='mm',fill='#eee4d2')
put('keycap_f',851,843,43,42)
put('button_normal',835,834,238,61)
put('keycap_f',850,843,43,42)
draw.text((953,862),'Interact',font=big,anchor='mm',fill='#eee4d2')
put('action_bar',648,913,618,128)
for i,(icon,qty) in enumerate([('icon_woodcutting_axes',''),('icon_iron_ore',''),('icon_sword',''),('icon_bandage','5'),('icon_healing_potion','2'),('icon_campfire','3')]):
    x=654+i*101
    put('slot_hotbar_selected' if i==0 else 'slot_hotbar_normal',x,923,96,98)
    put(icon,x+15,938,64,64)
    draw.text((x+13,933),str(i+1),font=tiny,fill='#eee4d2')
    if qty: draw.text((x+79,996),qty,font=tiny,fill='#eee4d2')
hud.convert('RGB').save(CONCEPT/'kit-v5-hud-mock.png')
