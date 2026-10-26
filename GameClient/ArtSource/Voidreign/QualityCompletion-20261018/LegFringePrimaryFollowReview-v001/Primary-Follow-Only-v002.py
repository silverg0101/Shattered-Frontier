"""Compose a sampled source-frame review GIF with recorded timing/provenance."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--report',required=True);p.add_argument('--output',required=True);p.add_argument('--fps',type=int,default=30)
a=p.parse_args();source=Path(a.report).resolve();out=Path(a.output).resolve();assert not out.exists() and 1<=a.fps<=120
r=json.loads(source.read_text());rows=r['images'];assert 2<=len(rows)<=128 and all(rows[i]['frame']<rows[i+1]['frame'] for i in range(len(rows)-1))
images=[];inputs=[]
for row in rows:
    path=(source.parent/row['image']).resolve();assert path.is_relative_to(source.parent) and path.is_file()
    im=Image.open(path).convert('RGB');assert max(im.size)<=2048
    if images:assert im.size==images[0].size
    images.append(im);inputs.append(dict(frame=row['frame'],path=str(path),sha256=sha(path)))
palette=images[0].quantize(colors=256);frames=[palette]+[im.quantize(palette=palette) for im in images[1:]]
duration=[max(10,round((rows[i+1]['frame']-rows[i]['frame'])*100/a.fps)*10) for i in range(len(rows)-1)]+[max(10,round(100/a.fps)*10)]
expected_ms=(rows[-1]['frame']-rows[0]['frame']+1)*1000/a.fps
assert abs(sum(duration)-expected_ms)<=5*len(rows),'Preview timing disagrees with source frames'
out.parent.mkdir(parents=True,exist_ok=True);frames[0].save(out,save_all=True,append_images=frames[1:],duration=duration,loop=0,disposal=2,optimize=False)
out.with_suffix('.json').write_text(json.dumps(dict(status='sampled-review-gif-not-motion-acceptance',report=str(source),report_sha256=sha(source),tool_sha256=sha(__file__),fps=a.fps,inputs=inputs,durations_ms=duration,gif_sha256=sha(out),limitations=['Sampled display preview with GIF10ms timing quantization, not an all-frame physics/clip check.','Scope and acceptance limits are inherited from the capture report.']),indent=2));out.with_suffix('.py').write_bytes(Path(__file__).read_bytes());print(json.dumps(dict(status='written',frames=len(frames),duration_ms=sum(duration),bytes=out.stat().st_size)))
