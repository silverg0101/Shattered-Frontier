"""Fetch a finite CC0 surface set from Poly Haven's documented API."""
from pathlib import Path
import urllib.request, json, hashlib
ROOT=Path(__file__).resolve().parent/'Surfaces';ROOT.mkdir(exist_ok=True)
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Codex-GreenlandsPreview/1.0'}),timeout=60) as r:return r.read()
manifest=[]
for asset in ['grass_ground','brown_mud_dry','rock_face_03']:
    files=json.loads(get('https://api.polyhaven.com/files/'+asset))
    for channel in ['Diffuse','nor_dx','Rough']:
        item=files[channel]['1k']['jpg'];data=get(item['url'])
        if hashlib.md5(data).hexdigest()!=item['md5']:raise RuntimeError('Hash mismatch')
        path=ROOT/(asset+'_'+channel+'.jpg');path.write_bytes(data)
        manifest.append(dict(asset=asset,channel=channel,file=path.name,url=item['url'],md5=item['md5'],page='https://polyhaven.com/a/'+asset,license='CC0',license_url='https://polyhaven.com/license'))
(ROOT/'provenance.json').write_text(json.dumps(manifest,indent=2))
