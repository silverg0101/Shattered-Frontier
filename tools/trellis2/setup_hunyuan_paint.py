"""Connect the community ROCm paint wrapper to verified local source/weights."""
import json
from pathlib import Path
lab=Path('/home/grego/sf-asset-lab')
wrapper=lab/'sources/hunyuan-rocm'
source=lab/'sources/hunyuan3d-2.1'
model=Path(json.loads((lab/'challengers/tencent--Hunyuan3D-2.1.json').read_text())['path'])
def link(path,target):
    path.parent.mkdir(parents=True,exist_ok=True)
    if path.is_symlink():assert path.resolve()==target.resolve()
    elif path.exists():raise RuntimeError('Refusing to replace '+str(path))
    else:path.symlink_to(target,target_is_directory=target.is_dir())
link(wrapper/'Hunyuan3D-2.1',source)
link(wrapper/'weights/Hunyuan3D-2.1',model)
link(source/'hy3dpaint/ckpt/RealESRGAN_x4plus.pth',lab/'challengers/RealESRGAN_x4plus.pth')
print('Paint source and weight links prepared')
