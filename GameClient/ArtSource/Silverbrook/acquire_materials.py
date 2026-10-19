"""Download hash-verified CC0 Poly Haven scans for Silverbrook buildings.

py -3 acquire_materials.py <asset> [<asset> ...]   (adds to the shared palette)
"""
from pathlib import Path
import urllib.request,json,hashlib,sys,os,time,concurrent.futures
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
# Several builders may add scans at once: merge into the current file under an exclusive lock file.
LOCK=PROV.with_suffix('.lock')
for _ in range(600):
    try:fd=os.open(LOCK,os.O_CREAT|os.O_EXCL);break
    except FileExistsError:time.sleep(.1)
else:raise SystemExit(f'{LOCK} held for 60 s; delete it if no download is running')
try:
    current={row['asset']:row for row in json.loads(PROV.read_text())} if PROV.exists() else {}
    current.update({k:manifest[k] for k in sys.argv[1:]})
    PROV.write_text(json.dumps(sorted(current.values(),key=lambda r:r['asset']),indent=2)+'\n')
finally:os.close(fd);os.remove(LOCK)
print('Verified',len(jobs),'maps;',len(current),'materials in palette')
