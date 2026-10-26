#!/usr/bin/env bash
set -euo pipefail
source /home/grego/sf-asset-lab/venv-current/bin/activate
export ROCM_HOME=$(rocm-sdk path --root)
export ROCM_PATH=$ROCM_HOME HIP_PATH=$ROCM_HOME
export PATH=$ROCM_HOME/bin:$PATH
export GPU_ARCHS=gfx1100 PYTORCH_ROCM_ARCH=gfx1100 BUILD_TARGET=rocm MAX_JOBS=4
python - <<'PY'
from pathlib import Path
root=Path('/home/grego/sf-asset-lab/sources/trellis2-amd/forks')
for part in ['FlexGEMM-rocm','nvdiffrast-rocm','CuMesh-rocm','TRELLIS2-rocm/o-voxel']:
    for path in (root/part).rglob('setup.py'):
        source=path.read_text()
        changed=source.replace('c++17','c++20')
        if changed!=source:
            path.write_text(changed)
            print('PyTorch 2.14 requires C++20:',path)
PY
echo "Build SDK: $ROCM_HOME"
for item in FlexGEMM-rocm nvdiffrast-rocm CuMesh-rocm TRELLIS2-rocm/o-voxel; do
  logname=${item//\//-}
  echo "Building $item"
  python -m pip install --no-build-isolation --no-deps -v "/home/grego/sf-asset-lab/sources/trellis2-amd/forks/$item" > "/home/grego/sf-asset-lab/logs/build-$logname.log" 2>&1
done
python -m pip install --no-deps 'git+https://github.com/EasternJournalist/utils3d.git@9a4eb15e4021b67b12c460c7057d642626897ec8' > /home/grego/sf-asset-lab/logs/utils3d-install.log 2>&1
echo NATIVE_BUILDS_COMPLETE
