"""Hash-verified CC0 Poly Haven downloads for the four Greenlands nature families.

py -3 acquire_sources.py            (models: glTF 2k; scans: 2k Diffuse/nor_gl/Rough/Displacement)
Files land in Sources/<id>/; Sources/provenance.json lists page, licence, authors, url, md5 and bytes per file.
Large glTF .bin files are git-ignored (see .gitignore); provenance lets anyone restore them.
"""
from pathlib import Path
import urllib.request,json,hashlib,concurrent.futures
ROOT=Path(__file__).resolve().parent/'Sources';ROOT.mkdir(exist_ok=True)
MODELS=['pine_tree_01','pine_sapling_medium','tree_small_02','boulder_01','namaqualand_boulder_02','namaqualand_boulder_03','namaqualand_boulder_04',
        'coast_land_rocks_02','coast_land_rocks_03','coast_land_rocks_04','rock_face_01','rock_face_02','moss_01','namaqualand_boulder_05','namaqualand_boulder_06','coast_rocks_05']
SCANS=['pine_bark','rock_08','rock_3','rock_surface','marble_rock_01','rock_boulder_dry','mossy_rock','forest_ground_04','japanese_hackberry_bark','brown_mud_02','rocky_trail']
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'GreenlandsEnvironment/1.0'}),timeout=300) as r:return r.read()
def download(pair):
    path,item=pair;path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists() and hashlib.md5(path.read_bytes()).hexdigest()==item['md5']:return
    data=get(item['url'])
    if hashlib.md5(data).hexdigest()!=item['md5']:raise RuntimeError('Hash mismatch: '+str(path))
    path.write_bytes(data)
manifest=[];jobs=[]
for asset in MODELS+SCANS:
    meta=json.loads(get('https://api.polyhaven.com/info/'+asset));files=json.loads(get('https://api.polyhaven.com/files/'+asset));folder=ROOT/asset;folder.mkdir(exist_ok=True)
    (folder/'info.json').write_text(json.dumps(meta,indent=2));(folder/'files.json').write_text(json.dumps(files,indent=2))
    if asset in MODELS:
        entry=files['gltf']['2k']['gltf'];items=[(folder/(asset+'.gltf'),entry)]+[(folder/n,i) for n,i in entry.get('include',{}).items()]
    else:
        items=[(folder/(asset+'_'+c+'.jpg'),files[c]['2k']['jpg']) for c in ('Diffuse','nor_gl','Rough','Displacement') if c in files]
    jobs+=items
    manifest.append(dict(asset=asset,kind='model' if asset in MODELS else 'scan',page='https://polyhaven.com/a/'+asset,license='CC0-1.0',license_url='https://polyhaven.com/license',author=meta.get('authors',{}),
        physical_size_mm=meta.get('dimensions'),files=[dict(file=p.relative_to(ROOT).as_posix(),url=i['url'],md5=i['md5'],bytes=i['size']) for p,i in items]))
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(download,jobs))
(ROOT/'provenance.json').write_text(json.dumps(manifest,indent=2))
print('Acquired',len(MODELS),'models,',len(SCANS),'scans;',sum(j[1]['size'] for j in jobs)//1000000,'MB',flush=True)
