"""Kit scan `sb_straw_face` from the Codex straw sheet (WP6d review fix 5): textures/straw-face.png (albedo) and
textures/straw-face-height.png -> ../Textures/sb_straw_face_{Diffuse,nor_gl,Rough,Displacement}.jpg, plus its
materials-provenance.json row (same lock file as make_endgrain.py). Normal and roughness are derived from the height.

py -3 make_straw_scan.py
"""
import numpy as np,json,os,time
from pathlib import Path
from PIL import Image
HERE=Path(__file__).resolve().parent;ROOT=HERE.parent;TEX=ROOT/'Textures';SIZE=.6   # one tile = 0.6 m
alb=Image.open(HERE/'textures/straw-face.png').convert('RGB')
h=np.asarray(Image.open(HERE/'textures/straw-face-height.png').convert('L'),np.float32)/255
N=h.shape[0]
gx,gy=np.gradient(h*.004)                                      # 4 mm relief over the full height range
nz=np.ones_like(h)*(SIZE/N);nx,ny=-gx,gy;ln=np.sqrt(nx**2+ny**2+nz**2)
normal=np.stack([nx/ln,ny/ln,nz/ln],-1)*.5+.5
rough=np.clip(.62+.33*(1-h),0,1)                               # glossy stalk ends, rough shadowed gaps
def save(name,arr):Image.fromarray((np.clip(arr,0,1)*255).astype(np.uint8)).save(TEX/f'sb_straw_face_{name}.jpg',quality=92)
alb.save(TEX/'sb_straw_face_Diffuse.jpg',quality=92)
save('nor_gl',normal);save('Rough',np.stack([rough]*3,-1));save('Displacement',np.stack([h]*3,-1))
PROV=ROOT/'materials-provenance.json';LOCK=PROV.with_suffix('.lock')
for _ in range(600):
    try:fd=os.open(LOCK,os.O_CREAT|os.O_EXCL);break
    except FileExistsError:time.sleep(.1)
else:raise SystemExit(f'{LOCK} held for 60 s')
try:
    rows=[row for row in json.loads(PROV.read_text()) if row['asset']!='sb_straw_face']
    rows.append(dict(asset='sb_straw_face',license='self-made (Codex image generation, PropHayBales/textures; make_straw_scan.py)',physical_size_metres=[SIZE,SIZE],
        note='packed golden straw cut face; normal and roughness derived from the generated height map',files=[dict(file=f'Textures/sb_straw_face_{c}.jpg') for c in ('Diffuse','nor_gl','Rough','Displacement')]))
    PROV.write_text(json.dumps(rows,indent=2)+'\n')
finally:os.close(fd);os.unlink(LOCK)
print('sb_straw_face written',N)
