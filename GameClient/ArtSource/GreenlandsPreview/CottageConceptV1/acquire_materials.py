"""Finite, hash-verified CC0 material palette selected against the generated concept."""
from pathlib import Path
import urllib.request,json,hashlib,concurrent.futures
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'Textures';OUT.mkdir(exist_ok=True)
ASSETS=['red_slate_roof_tiles_01','roof_tiles_14','clay_roof_tiles_02','roof_3','plastered_wall_02','medieval_wall_02','fine_grained_wood']
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'GreenlandsCottage/1.0'}),timeout=120) as r:return r.read()
jobs=[];manifest=[]
for asset in ASSETS:
    meta=json.loads(get('https://api.polyhaven.com/info/'+asset));files=json.loads(get('https://api.polyhaven.com/files/'+asset))
    size=meta.get('scale',[2,2]);rows=[]
    for channel in ['Diffuse','nor_gl','nor_dx','Rough','Displacement']:
        if channel not in files:continue
        item=files[channel]['2k']['jpg'];name=asset+'_'+channel+'.jpg';jobs.append((OUT/name,item));rows.append(dict(file='Textures/'+name,url=item['url'],md5=item['md5']))
    manifest.append(dict(asset=asset,page='https://polyhaven.com/a/'+asset,license='CC0-1.0',authors=meta.get('authors',{}),physical_scale_metres=size,files=rows))
def download(pair):
    path,item=pair
    if path.exists() and hashlib.md5(path.read_bytes()).hexdigest()==item['md5']:return
    data=get(item['url']);assert hashlib.md5(data).hexdigest()==item['md5'],path;path.write_bytes(data)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(download,jobs))
(ROOT/'materials-provenance.json').write_text(json.dumps(manifest,indent=2))
print('Verified',len(jobs),'CC0 maps across',len(manifest),'concept material families')
