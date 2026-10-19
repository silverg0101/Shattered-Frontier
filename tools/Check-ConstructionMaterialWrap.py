"""Reproducible tile-boundary continuity check for construction PBR maps."""
import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser(); p.add_argument('--maps',required=True); p.add_argument('--out',required=True); a=p.parse_args()
root=Path(a.maps); results=[]
def stats(seam,inside):
 return {'seam_mean':float(np.mean(seam)),'seam_p99':float(np.percentile(seam,99)),'interior_mean':float(np.mean(inside)),'interior_p99':float(np.percentile(inside,99))}
for name in ('basecolor.png','roughness.png','normal.png','height.png'):
 arr=np.asarray(Image.open(root/name))
 if name=='height.png': arr=arr.astype(np.float32)/257.0
 elif name=='normal.png':
  n=arr.astype(np.float32)/127.5-1; n/=np.maximum(np.linalg.norm(n,axis=2,keepdims=True),1e-8)
 else: arr=arr.astype(np.float32)
 for axis,label in ((1,'x'),(0,'y')):
  if name=='normal.png':
   seam_a=np.take(arr,0,axis=axis); seam_b=np.take(arr,-1,axis=axis)
   interior_a=np.take(arr,1,axis=axis); interior_b=np.take(arr,-2,axis=axis)
   def angle(x,y): return np.degrees(np.arccos(np.clip(np.sum(x*y,axis=-1),-1,1)))
   seam=angle(seam_a,seam_b); inside=np.concatenate([angle(seam_a,interior_a).ravel(),angle(seam_b,interior_b).ravel()])
   mean_limit=max(2*float(np.mean(inside)),.75); p99_limit=max(2*float(np.percentile(inside,99)),3.0); units='degrees'
  else:
   seam=np.abs(np.take(arr,0,axis=axis)-np.take(arr,-1,axis=axis)).astype(np.float32).ravel()
   ia=np.abs(np.take(arr,1,axis=axis)-np.take(arr,0,axis=axis)).astype(np.float32).ravel(); ib=np.abs(np.take(arr,-1,axis=axis)-np.take(arr,-2,axis=axis)).astype(np.float32).ravel(); inside=np.concatenate([ia,ib])
   mean_limit=max(2*float(np.mean(inside)),1.0); p99_limit=max(2*float(np.percentile(inside,99)),4.0); units='8bit-equivalent levels'
  m=stats(seam,inside); m.update({'map':name,'axis':label,'units':units,'mean_limit':mean_limit,'p99_limit':p99_limit,'status':'passed' if m['seam_mean']<=mean_limit and m['seam_p99']<=p99_limit else 'failed'}); results.append(m)
report={'method':'Compare wrap-boundary pixel step against two adjacent interior steps. Height16 normalized to 8-bit-equivalent levels; normal uses angular distance in degrees. Limits are twice interior mean/p99 with explicit floors.','status':'passed' if all(x['status']=='passed' for x in results) else 'failed','results':results}
Path(a.out).write_text(json.dumps(report,indent=2)+'\n'); print(json.dumps({'status':report['status'],'failed':[f"{x['map']}:{x['axis']}" for x in results if x['status']=='failed']}))
