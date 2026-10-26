"""CPU-only audit of a locally generated Pixal material-volume cache.

Does not import the renderer, run inference, bake, or create a GPU context.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path

os.environ['HIP_VISIBLE_DEVICES'] = '-1'
os.environ['ROCR_VISIBLE_DEVICES'] = '-1'
import numpy as np
import torch


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--volume', required=True)
p.add_argument('--output', required=True)
p.add_argument('--source-root', required=True)
a = p.parse_args()
out = Path(a.output).resolve()
assert not out.exists() and not torch.cuda.is_initialized()
torch.set_num_threads(4)
with torch.serialization.safe_globals([slice]):
    data = torch.load(a.volume, map_location='cpu', weights_only=True, mmap=True)
attrs = data['attrs']
assert attrs.device.type == 'cpu' and attrs.ndim == 2 and 1 <= attrs.shape[1] <= 32
layout = data['layout']
assert all(k in layout for k in ['base_color', 'metallic', 'roughness', 'alpha'])
count = len(attrs)
total = np.zeros(attrs.shape[1]); squares = total.copy()
low = np.full(attrs.shape[1], np.inf); high = -low
below = total.copy(); above = total.copy()
for offset in range(0, count, 131072):
    block = attrs[offset:offset + 131072].float().numpy()
    assert np.isfinite(block).all()
    total += block.sum(axis=0, dtype=np.float64)
    squares += (block.astype(np.float64) ** 2).sum(axis=0)
    low = np.minimum(low, block.min(axis=0)); high = np.maximum(high, block.max(axis=0))
    below += (block < .1).sum(axis=0); above += (block > .9).sum(axis=0)
sample = attrs[torch.linspace(0, count - 1, min(count, 65536)).long()].float().numpy()
mean = total / count
channels = [dict(channel=i, minimum=float(low[i]), maximum=float(high[i]),
    mean=float(mean[i]), std=float(np.sqrt(max(squares[i] / count - mean[i] ** 2, 0))),
    fraction_below_point1=float(below[i] / count), fraction_above_point9=float(above[i] / count),
    sampled_quantiles=np.quantile(sample[:, i], [0, .01, .5, .99, 1]).tolist())
    for i in range(attrs.shape[1])]
sources = []
root = Path(a.source_root)
for relative, intervals in [('pixal3d/representations/mesh/base.py', [(167, 180)]),
                            ('pixal3d/pipelines/pixal3d_image_to_3d.py', [(80, 95), (575, 610)])]:
    path = root / relative
    lines = path.read_text().splitlines()
    sources.append(dict(path=str(path), sha256=sha(path), excerpts=[dict(first_line=lo,
        text='\n'.join(lines[lo-1:hi])) for lo, hi in intervals]))
assert not torch.cuda.is_initialized()
out.mkdir(parents=True)
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
report = dict(schema_version=1, status='cpu-volume-channel-diagnostic-only', cpu_only=True,
    gpu_context_initialized=False, torch=torch.__version__, volume=str(Path(a.volume).resolve()),
    volume_sha256=sha(a.volume), attrs_shape=list(attrs.shape), attrs_dtype=str(attrs.dtype),
    layout={k:dict(start=v.start,stop=v.stop,step=v.step) for k,v in layout.items()},
    channels=channels, source_evidence=sources,
    limitations=['Raw volume statistics, not semantic UV coverage or approved PBR values.',
                 'No inference, raster/bake, image/material edit, environment install or shared GPU access.'])
(out / 'audit.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(dict(status=report['status'],shape=list(attrs.shape),layout=report['layout'],
    channels=channels, gpu_context_initialized=False)), flush=True)
