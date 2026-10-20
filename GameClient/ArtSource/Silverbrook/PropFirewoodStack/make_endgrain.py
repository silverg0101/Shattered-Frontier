"""Self-made end-grain texture `sb_endgrain` (no Poly Haven log-end scan exists): weathered pale grey-tan oak, concentric growth
rings round a dark pith, radial checks, darker toward the bark. One tile = 0.25 m, rings centred on the tile centre so each
log end maps its own pith. Writes ../Textures/sb_endgrain_{Diffuse,nor_gl,Rough,Displacement}.jpg and its materials-provenance.json
row (under the same lock file acquire_materials.py uses).

py -3 make_endgrain.py
"""
import numpy as np,json,os,time
from pathlib import Path
from PIL import Image,ImageFilter
ROOT=Path(__file__).resolve().parents[1];TEX=ROOT/'Textures';N=1024;SIZE=.25
rng=np.random.default_rng(5201)
def noise(cells,seed):
    g=np.random.default_rng(seed).random((cells,cells)).astype(np.float32)
    return np.asarray(Image.fromarray((g*255).astype(np.uint8)).resize((N,N),Image.BICUBIC),np.float32)/255
y,x=np.mgrid[0:N,0:N].astype(np.float32);x=(x/N-.5)*SIZE;y=(y/N-.5)*SIZE
r=np.hypot(x,y);a=np.arctan2(y,x)
warp=(noise(6,1)-.5)*.012+(noise(24,2)-.5)*.003+np.sin(a*3+noise(4,3)*6)*.002   # lopsided, wobbling rings
phase=(r+warp)/.0075;f=phase%1
late=np.clip((f-.55)/.35,0,1)*np.clip((1-f)/.1,0,1)+np.clip((f-.55)/.35,0,1)*.0   # dark late-wood band per ring
late=np.clip(late,0,1)
h=1-late*.8
base=np.array([.64,.57,.47],np.float32)                       # desaturated grey-tan
dark=np.array([.40,.35,.29],np.float32)
col=base*(1-late[...,None]*.5)+dark*late[...,None]*.5
col*=(.9+.2*noise(48,4))[...,None]
col*=(1-.45*np.clip((r-.045)/.08,0,1))[...,None]               # darker toward the bark
col*=(1-.5*np.exp(-(r/.006)**2))[...,None]                      # pith
check=np.zeros((N,N),np.float32)
for ang in rng.uniform(0,np.pi*2,5):                           # radial drying checks
    d=np.abs(np.sin(a-ang))*r;along=np.cos(a-ang)*r;ln=rng.uniform(.015,.045)
    check=np.maximum(check,((d<.0005+.00002*along*40)&(along>.01)&(along<ln)).astype(np.float32))
check=np.asarray(Image.fromarray((check*255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.1)),np.float32)/255
col*=(1-.45*check)[...,None];h=h-.6*check
gx,gy=np.gradient(h*.003)
nz=np.ones_like(h)*(SIZE/N);nx,ny=-gx,gy;ln=np.sqrt(nx**2+ny**2+nz**2)
normal=np.stack([nx/ln,ny/ln,nz/ln],-1)*.5+.5
rough=np.clip(.86+(noise(32,5)-.5)*.1+.05*check,0,1)
def save(name,arr):Image.fromarray((np.clip(arr,0,1)*255).astype(np.uint8)).save(TEX/f'sb_endgrain_{name}.jpg',quality=92)
save('Diffuse',col);save('nor_gl',normal);save('Rough',np.stack([rough]*3,-1));save('Displacement',np.stack([h*.5+.25]*3,-1))
PROV=ROOT/'materials-provenance.json';LOCK=PROV.with_suffix('.lock')
for _ in range(600):
    try:fd=os.open(LOCK,os.O_CREAT|os.O_EXCL);break
    except FileExistsError:time.sleep(.1)
else:raise SystemExit(f'{LOCK} held for 60 s')
try:
    rows=[row for row in json.loads(PROV.read_text()) if row['asset']!='sb_endgrain']
    rows.append(dict(asset='sb_endgrain',license='self-made (procedural, PropFirewoodStack/make_endgrain.py)',physical_size_metres=[SIZE,SIZE],
        note='procedural weathered oak end grain, rings centred on the tile; Rough is roughness only',files=[dict(file=f'Textures/sb_endgrain_{c}.jpg') for c in ('Diffuse','nor_gl','Rough','Displacement')]))
    PROV.write_text(json.dumps(rows,indent=2)+'\n')
finally:os.close(fd);os.unlink(LOCK)
print('sb_endgrain written')
