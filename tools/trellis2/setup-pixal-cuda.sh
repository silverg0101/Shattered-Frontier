#!/usr/bin/env bash
# Unmodified upstream Pixal3D on the RTX 3060 (CUDA, sm_86) in WSL. No sudo: micromamba
# supplies Python 3.12 and the CUDA 12.8 compiler; the ROCm env-pixal is not touched.
# Upstream pins: TRELLIS.2 basics, torch 2.7.0+cu128, natten 0.21.0 (Pixal3D README).
set -euo pipefail
lab=/home/grego/sf-asset-lab; c=$lab/cuda; env=$lab/env-pixal-cuda
mkdir -p "$c/src" "$c/bin"
[ -x "$c/bin/micromamba" ] || curl -Ls https://micro.mamba.pm/api/micromamba/linux-64/2.3.2 | tar -xj -C "$c" bin/micromamba
export MAMBA_ROOT_PREFIX=$c/mamba
[ -d "$env" ] || "$c/bin/micromamba" create -y -p "$env" -c conda-forge python=3.12 cuda-nvcc=12.8 cuda-cudart-dev=12.8 \
  cuda-cccl=12.8 cuda-libraries-dev=12.8 cuda-profiler-api=12.8 gxx_linux-64=13 git
export PATH=$env/bin:$PATH CUDA_HOME=$env TORCH_CUDA_ARCH_LIST=8.6 MAX_JOBS=8
# conda-forge keeps CUDA headers/libs under targets/, which torch's extension builder does not search.
export CPATH=$env/targets/x86_64-linux/include LIBRARY_PATH=$env/targets/x86_64-linux/lib:$env/lib
pip() { "$env/bin/python" -m pip "$@"; }
pip install -q torch==2.7.0 torchvision==0.22.0 --index-url https://download.pytorch.org/whl/cu128
pip install -q imageio imageio-ffmpeg tqdm easydict opencv-python-headless ninja trimesh transformers==4.57.3 pandas lpips \
  zstandard kornia timm einops diffusers==0.37.1 accelerate==1.13.0 plyfile pillow==12.0.0 'git+https://github.com/microsoft/MoGe.git'
pip install -q 'git+https://github.com/EasternJournalist/utils3d.git@9a4eb15e4021b67b12c460c7057d642626897ec8'
pip install -q https://github.com/LDYang694/Storages/releases/download/20260430/utils3d-0.0.2-py3-none-any.whl
pip install -q 'natten==0.21.0+torch270cu128' -f https://whl.natten.org/
cd "$c/src"
[ -d nvdiffrast ] || git clone -q -b v0.4.0 https://github.com/NVlabs/nvdiffrast.git
[ -d CuMesh ] || git clone -q --recursive https://github.com/JeffreyXiang/CuMesh.git
[ -d FlexGEMM ] || git clone -q --recursive https://github.com/JeffreyXiang/FlexGEMM.git
[ -d TRELLIS.2 ] || git clone -q --recursive https://github.com/microsoft/TRELLIS.2.git
git -C TRELLIS.2 submodule update -q --init --recursive
for d in nvdiffrast CuMesh FlexGEMM TRELLIS.2/o-voxel; do
  echo "building $d"; pip install --no-build-isolation --no-deps "$c/src/$d" > "$c/build-${d//\//-}.log" 2>&1
done
for d in nvdiffrast CuMesh FlexGEMM TRELLIS.2; do echo "$d $(git -C "$c/src/$d" rev-parse HEAD)"; done > "$c/source-revisions.txt"
"$env/bin/python" -c "import torch,natten,o_voxel,cumesh,flex_gemm,nvdiffrast.torch;print(torch.__version__,torch.cuda.get_device_name(0))"
echo PIXAL_CUDA_READY
