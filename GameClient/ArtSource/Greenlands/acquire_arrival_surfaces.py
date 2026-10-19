from pathlib import Path
import json,urllib.request,hashlib
ROOT=Path(__file__).resolve().parent
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'GreenlandsEnvironment/1.0'}),timeout=90) as r:return r.read()
records=[]
def fetch(path,item,asset,channel):
    path.parent.mkdir(exist_ok=True,parents=True)
    data=path.read_bytes() if path.exists() else get(item['url'])
    assert hashlib.md5(data).hexdigest()==item['md5'];path.write_bytes(data)
    records.append(dict(asset=asset,channel=channel,file=str(path.relative_to(ROOT)),url=item['url'],md5=item['md5'],license='CC0-1.0',page='https://polyhaven.com/a/'+asset))
for asset,channel in [('tree_small_02','leaves_alpha'),('grass_medium_01','Alpha'),('fern_02','Alpha')]:
    files=json.loads((ROOT/'ArrivalAssets'/asset/'files.json').read_text())
    if channel not in files:channel=next(k for k in files if k.lower()=='alpha')
    item=files[channel]['1k'].get('png',files[channel]['1k'].get('jpg'))
    fetch(ROOT/'ArrivalAssets'/asset/'textures'/(asset+'_opacity.png'),item,asset,channel)
for asset in ['leafy_grass','grass_path_3','brown_planks_03','grey_roof_tiles_02','rough_plaster_03']:
    files=json.loads(get('https://api.polyhaven.com/files/'+asset))
    for channel in ['Diffuse','nor_dx','Rough']:
        fetch(ROOT/'ArrivalSurfaces'/(asset+'_'+channel+'.jpg'),files[channel]['2k']['jpg'],asset,channel)
(ROOT/'arrival-surface-provenance.json').write_text(json.dumps(records,indent=2))
print('Acquired',len(records),'CC0 opacity/surface maps',flush=True)
