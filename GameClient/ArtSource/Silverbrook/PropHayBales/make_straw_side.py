"""Kit scan `sb_straw_side`: the long faces of a small square bale, straw lying along the bale (u axis) in layered flakes.
Writes ../Textures/sb_straw_side_{Diffuse,nor_gl,Rough,Displacement}.jpg (one tile = 0.6 m, tileable) and its
materials-provenance.json row (same lock file as make_straw_scan.py). Procedural: no source image.

py -3 make_straw_side.py
"""
import numpy as np,json,os,time,random
from pathlib import Path
from PIL import Image,ImageDraw
HERE=Path(__file__).resolve().parent;ROOT=HERE.parent;TEX=ROOT/'Textures';SIZE=.6;N=1024
rnd=random.Random(41)
gold=np.array([.50,.39,.22]);pale=np.array([.62,.52,.34]);dark=np.array([.34,.22,.09])
alb=Image.new('RGB',(N,N),tuple(int(c*255) for c in dark*.8));hgt=Image.new('L',(N,N),30)
da,dh=ImageDraw.Draw(alb),ImageDraw.Draw(hgt)
def strand(v0,u0,length,width,tone,lift):
    pts=[];ph=rnd.uniform(0,6.28);amp=rnd.uniform(1,5);slope=rnd.uniform(-.16,.16)
    for k in range(7):pts.append((u0+length*k/6,v0+slope*length*k/6+amp*np.sin(ph+k*.9)))
    col=tuple(int(np.clip(c,0,1)*255) for c in tone)
    for off in (-N,0,N):  # wrap in u so the tile repeats
        p=[(x+off,y) for x,y in pts]
        for yy in (-N,0,N):
            q=[(x,y+yy) for x,y in p]
            da.line(q,fill=col,width=width);dh.line(q,fill=lift,width=width)
for _ in range(2600):   # long lower layer, dimmer, then the bright top layer
    strand(rnd.uniform(0,N),rnd.uniform(0,N),rnd.uniform(160,560),rnd.choice((3,4,5,6)),(gold*rnd.uniform(.5,.95)+dark*.25),rnd.randint(70,150))
for _ in range(2200):
    t=rnd.random();tone=gold*(1-t)+pale*t;tone=tone*rnd.uniform(.7,1.15)
    strand(rnd.uniform(0,N),rnd.uniform(0,N),rnd.uniform(100,420),rnd.choice((2,3,3,4,5)),tone,rnd.randint(150,255))
a=np.asarray(alb,np.float32)/255;h=np.asarray(hgt,np.float32)/255
def lowfreq(cells,seed):  # tileable value noise, bilinear up-sampled to N x N
    g=np.random.default_rng(seed).random((cells,cells)).astype(np.float32);ix=np.linspace(0,cells,N,endpoint=False);i0=ix.astype(int);f=ix-i0;i1=(i0+1)%cells
    f=f*f*(3-2*f);r=g[i0][:,i0]*(1-f)[None,:]*(1-f)[:,None]+g[i0][:,i1]*f[None,:]*(1-f)[:,None]+g[i1][:,i0]*(1-f)[None,:]*f[:,None]+g[i1][:,i1]*f[None,:]*f[:,None];return r
a*=(.74+.2*lowfreq(7,1)+.1*lowfreq(19,2))[:,:,None]   # clumps of paler and duller straw
# flake seams: a darker, lower band across the strands about every 0.1 m, jittered
for k in range(6):
    c=int((k+rnd.uniform(-.2,.2))/6*N)%N;w=int(rnd.uniform(5,9));prof=np.exp(-((np.arange(N)-c+N/2)%N-N/2)**2/(2*(w/2)**2));fade=np.clip(lowfreq(5,10+k)[:,0]*2.2-.4,0,1)  # seams fade in and out along their length
    a*=1-.2*prof[None,:,None]*fade[:,None,None];h*=1-.4*prof[None,:]*fade[:,None]
gx,gy=np.gradient(np.pad(h,((1,1),(1,1)),mode='wrap')[1:-1,1:-1]*.004)   # 4 mm relief over the full height range
px=SIZE/N;nz=np.ones_like(h)*px;nx,ny=-gx,gy;ln=np.sqrt(nx**2+ny**2+nz**2)
normal=np.stack([nx/ln,ny/ln,nz/ln],-1)*.5+.5
rough=np.clip(.92-.2*h,0,1)
def save(name,arr):Image.fromarray((np.clip(arr,0,1)*255).astype(np.uint8)).save(TEX/f'sb_straw_side_{name}.jpg',quality=92)
save('Diffuse',a);save('nor_gl',normal);save('Rough',np.stack([rough]*3,-1));save('Displacement',np.stack([h]*3,-1))
PROV=ROOT/'materials-provenance.json';LOCK=PROV.with_suffix('.lock')
for _ in range(600):
    try:fd=os.open(LOCK,os.O_CREAT|os.O_EXCL);break
    except FileExistsError:time.sleep(.1)
else:raise SystemExit(f'{LOCK} held for 60 s')
try:
    rows=[row for row in json.loads(PROV.read_text()) if row['asset']!='sb_straw_side']
    rows.append(dict(asset='sb_straw_side',license='self-made (procedural, PropHayBales/make_straw_side.py)',physical_size_metres=[SIZE,SIZE],
        note='straw lying along u in layered flakes, tileable; normal and roughness derived from the generated height map',files=[dict(file=f'Textures/sb_straw_side_{c}.jpg') for c in ('Diffuse','nor_gl','Rough','Displacement')]))
    PROV.write_text(json.dumps(rows,indent=2)+'\n')
finally:os.close(fd);os.unlink(LOCK)
print('sb_straw_side written',N)
