"""Finite CC0 asset acquisition through Poly Haven's documented public API."""
from pathlib import Path
import urllib.request,json,hashlib,concurrent.futures
ROOT=Path(__file__).resolve().parent/'ArrivalAssets';ROOT.mkdir(exist_ok=True)
ASSETS=['tree_small_02','grass_medium_01','fern_02','rock_moss_set_01','tree_stump_02','wine_barrel_01','wooden_crate_01']
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'GreenlandsEnvironment/1.0'}),timeout=120) as r:return r.read()
def download(pair):
    path,item=pair;path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists() and hashlib.md5(path.read_bytes()).hexdigest()==item['md5']:return
    data=get(item['url'])
    if hashlib.md5(data).hexdigest()!=item['md5']:raise RuntimeError('Hash mismatch: '+str(path))
    path.write_bytes(data)
manifest=[];jobs=[]
for asset in ASSETS:
    meta=json.loads(get('https://api.polyhaven.com/info/'+asset))
    files=json.loads(get('https://api.polyhaven.com/files/'+asset));entry=files['gltf']['1k']['gltf'];folder=ROOT/asset;folder.mkdir(exist_ok=True)
    (folder/'info.json').write_text(json.dumps(meta,indent=2));(folder/'files.json').write_text(json.dumps(files,indent=2))
    items=[(folder/(asset+'.gltf'),entry)]+[(folder/name,item) for name,item in entry.get('include',{}).items()]
    jobs+=items
    manifest.append(dict(asset=asset,page='https://polyhaven.com/a/'+asset,license='CC0-1.0',license_url='https://polyhaven.com/license',author=meta.get('authors',{}),files=[dict(file=str(path.relative_to(ROOT)),url=item['url'],md5=item['md5'],bytes=item['size']) for path,item in items]))
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(download,jobs))
(ROOT/'provenance.json').write_text(json.dumps(manifest,indent=2))
print('Acquired',len(ASSETS),'CC0 models;',sum(x[1]['size'] for x in jobs),'bytes',flush=True)
