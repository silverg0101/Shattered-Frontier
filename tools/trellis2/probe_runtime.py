import json, time
from pathlib import Path
import torch

assert torch.version.hip is not None, 'ROCm build required'
assert torch.cuda.is_available(), 'Radeon compute unavailable'
name=torch.cuda.get_device_name(0)
assert '7900 XTX' in name, f'Wrong GPU: {name}'
started=time.perf_counter()
x=torch.ones((512,512),device='cuda',dtype=torch.float16)
y=x@x
torch.cuda.synchronize()
assert y[0,0].item()==512.0
del x,y
allocation=torch.empty(5_000_000_000,device='cuda',dtype=torch.float16)
allocation.fill_(1)
torch.cuda.synchronize()
assert allocation[0].item()==1 and allocation[-1].item()==1
report=dict(torch=torch.__version__,hip=torch.version.hip,gpu=name,
    architecture=torch.cuda.get_device_properties(0).gcnArchName,
    total_vram_bytes=torch.cuda.get_device_properties(0).total_memory,
    verified_allocation_bytes=allocation.numel()*allocation.element_size(),
    checks=['FP16 matmul result checked','10 GB allocated, written and read on GPU'],
    seconds=round(time.perf_counter()-started,3))
del allocation
torch.cuda.empty_cache()
output=Path('/mnt/c/Projects/Shattered-Frontier/GameClient/ArtSource/Voidreign/Trellis2-20261018/runtime-report.json')
output.write_text(json.dumps(report,indent=2))
print(json.dumps(report),flush=True)
