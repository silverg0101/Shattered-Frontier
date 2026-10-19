"""Restore the finite CC0 road scan; verify each downloaded channel."""
from pathlib import Path
import urllib.request,json,hashlib
ROOT=Path(__file__).resolve().parent/'RoadSurfaces';ROOT.mkdir(exist_ok=True)
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Greenlands-Art/1.0'}),timeout=60) as r:return r.read()
files=json.loads(get('https://api.polyhaven.com/files/gravel_road'));manifest=[]
for channel in ['Diffuse','nor_dx','Rough','Displacement']:
    item=files[channel]['2k']['jpg'];path=ROOT/('gravel_road_'+channel+'.jpg')
    if not path.exists() or hashlib.md5(path.read_bytes()).hexdigest()!=item['md5']:path.write_bytes(get(item['url']))
    assert hashlib.md5(path.read_bytes()).hexdigest()==item['md5']
    manifest.append(dict(file=path.name,url=item['url'],md5=item['md5'],license='CC0',source='https://polyhaven.com/a/gravel_road',license_url='https://polyhaven.com/license',physical_tile_metres=2))
(ROOT/'provenance.json').write_text(json.dumps(manifest,indent=2))
