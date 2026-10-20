"""Download hash-verified CC0 Poly Haven models for Silverbrook set dressing and turn their textures into palette scans.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P acquire_models.py -- <model id> ...

Each model lands in Models/<id>/ (glTF 2k). Each of its texture sets becomes a palette scan ph_<id>[_<set>] in
Textures/ (1k Diffuse, nor_gl, Rough with roughness in R and metalness in G, flat Displacement) and a row in
materials-provenance.json, so kit.scan(..., metal=True) and the Unreal importer treat it like any other scan.
"""
import bpy,sys,os,time,json,hashlib,shutil,urllib.request,numpy as np
from pathlib import Path
ROOT=Path(__file__).resolve().parent;TEX=ROOT/'Textures';MODELS=ROOT/'Models';PROV=ROOT/'materials-provenance.json';SIDE=1024
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'SilverbrookArt/1.0'}),timeout=120) as r:return r.read()
def fetch(path,item):
    if path.exists() and hashlib.md5(path.read_bytes()).hexdigest()==item['md5']:return
    data=get(item['url']);assert hashlib.md5(data).hexdigest()==item['md5'],path;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
def pixels(path):
    im=bpy.data.images.load(str(path));im.colorspace_settings.name='Non-Color';a=np.array(im.pixels[:],dtype=np.float32).reshape(im.size[1],im.size[0],4);bpy.data.images.remove(im);return a
def save(a,path):
    while a.shape[1]>SIDE:a=(a[0::2,0::2]+a[1::2,0::2]+a[0::2,1::2]+a[1::2,1::2])/4  # props are small: 1k is plenty and keeps git lean
    h,w=a.shape[:2];im=bpy.data.images.new(path.stem,w,h,alpha=False,float_buffer=False);im.colorspace_settings.name='Non-Color'
    im.pixels.foreach_set(a.ravel());im.filepath_raw=str(path);im.file_format='JPEG';bpy.context.scene.render.image_settings.quality=95;im.save();bpy.data.images.remove(im)

rows={}
for mid in sys.argv[sys.argv.index('--')+1:]:
    meta=json.loads(get('https://api.polyhaven.com/info/'+mid));files=json.loads(get('https://api.polyhaven.com/files/'+mid))['gltf']['2k']['gltf']
    out=MODELS/mid;fetch(out/f'{mid}.gltf',files)
    for rel,item in files['include'].items():fetch(out/rel,item)
    maps={}  # texture set -> {channel: file}
    for rel in files['include']:
        stem=Path(rel).stem
        if not stem.endswith('_2k'):continue
        stem=stem[:-3]
        for ch in ('nor_gl','diff','arm','rough','metal'):
            if stem.endswith('_'+ch):maps.setdefault(stem[:-len(ch)-1],{})[ch]=out/rel
    for tset,m in maps.items():
        asset='ph_'+tset
        for src,ch in ((m['diff'],'Diffuse'),(m['nor_gl'],'nor_gl')):save(pixels(src),TEX/f'{asset}_{ch}.jpg')
        if 'arm' in m:a=pixels(m['arm']);rough,metal=a[...,1],a[...,2]
        else:rough=pixels(m['rough'])[...,0];metal=pixels(m['metal'])[...,0] if 'metal' in m else np.zeros_like(rough)
        save(np.dstack([rough,metal,np.zeros_like(rough),np.ones_like(rough)]),TEX/f'{asset}_Rough.jpg')
        save(np.full((8,8,4),(.5,.5,.5,1),np.float32),TEX/f'{asset}_Displacement.jpg')  # props carry no height
        rows[asset]=dict(asset=asset,page='https://polyhaven.com/a/'+mid,license='CC0-1.0',authors=meta.get('authors',{}),model=f'Models/{mid}/{mid}.gltf',
            physical_size_metres=[1.0,1.0],note='model texture set: UVs come from the model; Rough packs roughness (R) and metalness (G)',
            files=[dict(file=f'Textures/{asset}_{c}.jpg') for c in ('Diffuse','nor_gl','Rough','Displacement')])
    shutil.rmtree(out/'textures')  # converted into Textures/; the glTF is kept for its geometry only
    print('MODEL',mid,sorted(maps))
LOCK=PROV.with_suffix('.lock')  # same lock as acquire_materials.py
for _ in range(600):
    try:fd=os.open(LOCK,os.O_CREAT|os.O_EXCL);break
    except FileExistsError:time.sleep(.1)
else:raise SystemExit(f'{LOCK} held for 60 s; delete it if no download is running')
try:
    current={r['asset']:r for r in json.loads(PROV.read_text())};current.update(rows)
    PROV.write_text(json.dumps(sorted(current.values(),key=lambda r:r['asset']),indent=2)+'\n')
finally:os.close(fd);os.remove(LOCK)
print('ACQUIRED',len(rows),'texture sets')
