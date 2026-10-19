"""Download hash-verified CC0 Poly Haven scans for Silverbrook buildings.

py -3 acquire_materials.py <asset> [<asset> ...]   (adds to the shared palette)
"""
from pathlib import Path
import urllib.request,json,hashlib,sys,concurrent.futures
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'Textures';OUT.mkdir(exist_ok=True);PROV=ROOT/'materials-provenance.json'
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'SilverbrookArt/1.0'}),timeout=120) as r:return r.read()
manifest={row['asset']:row for row in json.loads(PROV.read_text())} if PROV.exists() else {}
jobs=[]
for asset in sys.argv[1:]:
    meta=json.loads(get('https://api.polyhaven.com/info/'+asset));files=json.loads(get('https://api.polyhaven.com/files/'+asset));rows=[]
    for channel in ['Diffuse','nor_gl','Rough','Displacement']:
        item=files[channel]['2k']['jpg'];name=asset+'_'+channel+'.jpg';jobs.append((OUT/name,item));rows.append(dict(file='Textures/'+name,url=item['url'],md5=item['md5']))
    # Poly Haven reports physical size in millimetres.
    manifest[asset]=dict(asset=asset,page='https://polyhaven.com/a/'+asset,license='CC0-1.0',authors=meta.get('authors',{}),physical_size_metres=[d/1000 for d in meta.get('dimensions',[2000,2000])],files=rows)
def download(pair):
    path,item=pair
    if path.exists() and hashlib.md5(path.read_bytes()).hexdigest()==item['md5']:return
    data=get(item['url']);assert hashlib.md5(data).hexdigest()==item['md5'],path;path.write_bytes(data)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(download,jobs))
PROV.write_text(json.dumps(sorted(manifest.values(),key=lambda r:r['asset']),indent=2)+'\n')
print('Verified',len(jobs),'maps;',len(manifest),'materials in palette')
