"""Acquire the CC0 flowering scan and its explicit opacity, with source hashes."""
from pathlib import Path
import urllib.request,json,hashlib,concurrent.futures
ROOT=Path(__file__).resolve().parent/'ArrivalAssets'/'flower_heliophila'
ROOT.mkdir(parents=True,exist_ok=True)
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'GreenlandsEnvironment/1.0'}),timeout=120) as response:return response.read()
meta=json.loads(get('https://api.polyhaven.com/info/flower_heliophila'))
files=json.loads(get('https://api.polyhaven.com/files/flower_heliophila'))
entry=files['gltf']['1k']['gltf']
items=[('flower_heliophila.gltf',entry)]+list(entry['include'].items())
items.append(('textures/flower_heliophila_opacity.png',files['Alpha']['1k']['png']))
def download(pair):
    name,item=pair;path=ROOT/name;path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists() and hashlib.md5(path.read_bytes()).hexdigest()==item['md5']:return
    data=get(item['url']);assert hashlib.md5(data).hexdigest()==item['md5'],name;path.write_bytes(data)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(download,items))
(ROOT/'info.json').write_text(json.dumps(meta,indent=2))
(ROOT/'files.json').write_text(json.dumps(files,indent=2))
(ROOT/'provenance.json').write_text(json.dumps(dict(asset='flower_heliophila',page='https://polyhaven.com/a/flower_heliophila',license='CC0-1.0',author=meta.get('authors',{}),files=[dict(file=name,url=item['url'],md5=item['md5']) for name,item in items]),indent=2))
print('Verified flowering scan:',len(items),'files')
