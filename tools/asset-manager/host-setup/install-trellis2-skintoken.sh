#!/bin/bash
# Adds ComfyUI-Trellis2 (Pixal3D + TRELLIS.2-4B) and ComfyUI-SkinToken to an existing ComfyUI on a
# GPU host, without sudo. Recorded from the RA-SIEMS install (2026-10-23): Ubuntu 24.04, gcc 13,
# RTX 3090 (sm_86), ComfyUI venv with Python 3.12 and torch 2.9.1+cu130. Run as the ComfyUI user.
#
#   COMFY=~/comfy-h3/ComfyUI bash install-trellis2-skintoken.sh
#
# Rollback point: $COMFY/../venv-freeze-before-trellis2.txt (pip freeze taken before anything else).
# Existing packages are pinned with a constraints file, so other workloads in the venv keep their versions.
set -euo pipefail
COMFY=${COMFY:-$HOME/comfy-h3/ComfyUI}
PY=$COMFY/.venv/bin/python
UV=${UV:-$HOME/.local/bin/uv}
ARCH=${TORCH_CUDA_ARCH_LIST:-8.6}
SRC=$HOME/trellis2-src
CUDA_TK=$HOME/cuda13-build
STATE=$(dirname "$COMFY")

[ -x "$UV" ] || curl -LsSf https://astral.sh/uv/0.9.5/install.sh | env UV_NO_MODIFY_PATH=1 sh
[ -f "$STATE/venv-freeze-before-trellis2.txt" ] || "$UV" pip freeze --python "$PY" > "$STATE/venv-freeze-before-trellis2.txt"
grep -v -E "^-e|@ file|^#" "$STATE/venv-freeze-before-trellis2.txt" > "$STATE/trellis2-constraints.txt"
pip_c() { "$UV" pip install --python "$PY" -c "$STATE/trellis2-constraints.txt" "$@"; }

# 1. Custom nodes, pinned.
cd "$COMFY/custom_nodes"
[ -d ComfyUI-Trellis2 ] || git clone -q https://github.com/visualbruno/ComfyUI-Trellis2
git -C ComfyUI-Trellis2 checkout -q b0e84c0f29857a444e8bcb5b9be2ea218e96efd0
[ -d ComfyUI-SkinToken ] || git clone -q https://github.com/Rizzlord/ComfyUI-SkinToken
git -C ComfyUI-SkinToken checkout -q 4a83157
# The rig export welds vertices closer than 1e-4, which collapses Pixal3D meshes (89,886 -> 5,997 faces).
# Pass 0 so only exactly coincident vertices merge.
grep -q '"merge_distance": 0.0' ComfyUI-SkinToken/sktn_nodes.py || sed -i \
    's/        "export_yup": use_transfer,/        "export_yup": use_transfer,\n        "merge_distance": 0.0,  # sfam patch: the 1e-4 default welds Pixal3D meshes down to a few thousand faces/' \
    ComfyUI-SkinToken/sktn_nodes.py
# Generation is capped at 2048 tokens; a large predicted skeleton runs out before every bone gets its skin
# tokens and the node fails with "assert res.skin_pred is not None".
sed -i 's/^        "max_length": 2048,$/        "max_length": 4096,  # sfam patch: 2048 runs out before every bone gets skin tokens on big skeletons/' \
    ComfyUI-SkinToken/sktn_nodes.py
# After an FBX rig export the node reloads its output with trimesh, which cannot read FBX; return the input mesh.
sed -i 's/^        output_mesh = _load_output_trimesh(output_path)$/        output_mesh = _as_single_trimesh(trimesh) if output_path.suffix.lower() == ".fbx" else _load_output_trimesh(output_path)  # sfam patch: trimesh cannot read FBX/' \
    ComfyUI-SkinToken/sktn_nodes.py
# Blender's remove_doubles clamps a 0 threshold up and still collapses dense meshes, so skip it at 0.
grep -q 'sfam patch: Blender clamps' ComfyUI-SkinToken/vendor/skintokens/rig_package/parser/bpy.py || sed -i \
    's/^        bpy.ops.mesh.remove_doubles(threshold=merge_distance)$/        if merge_distance > 0:  # sfam patch: Blender clamps 0 up, which still collapses dense meshes\n            bpy.ops.mesh.remove_doubles(threshold=merge_distance)/' \
    ComfyUI-SkinToken/vendor/skintokens/rig_package/parser/bpy.py

# 2. CUDA 13.0 compiler from pip, kept outside the venv. nvcc, nvvm and crt must share one
#    version, or ptxas rejects the PTX ("Unsupported .version").
"$UV" pip install -q --target "$CUDA_TK" --python "$PY" "nvidia-cuda-nvcc==13.0.88" "nvidia-nvvm==13.0.88" \
    "nvidia-cuda-crt==13.0.88" "nvidia-cuda-cccl==13.0.*" "nvidia-cuda-runtime==13.0.48" "nvidia-cublas==13.0.0.19" \
    "nvidia-curand==10.4.0.35" "nvidia-cusparse==12.6.2.49" "nvidia-cusolver==12.0.3.29" "nvidia-cufft==12.0.0.15" \
    "nvidia-nvtx==13.0.39" "nvidia-cuda-nvrtc==13.0.48" "nvidia-nvjitlink==13.0.39"
export CUDA_HOME=$CUDA_TK/nvidia/cu13
( cd "$CUDA_HOME" && { [ -e lib64 ] || ln -s lib lib64; } && cd lib &&
  for f in libcudart.so.13 libcublas.so.13 libcurand.so.10 libcusparse.so.12 libcusolver.so.12 libcufft.so.12 libnvrtc.so.13; do
    [ -e "$f" ] && ln -sf "$f" "${f%%.so*}.so"; done )

# 3. CUDA extensions built for this GPU. The prebuilt wheels in ComfyUI-Trellis2/wheels link CUDA 12
#    and lack sm_86 kernels (CUDA error 209), so they are not used.
mkdir -p "$SRC" && cd "$SRC"
pin() { [ -d "$1" ] || git clone -q --recursive "$2" "$1"; git -C "$1" checkout -q "$3"; git -C "$1" submodule update -q --init --recursive; }
pin nvdiffrast https://github.com/NVlabs/nvdiffrast 253ac4fcea7de5f396371124af597e6cc957bfae
pin CuMesh https://github.com/visualbruno/CuMesh d10e54c30ddd03d11472c1431693f985501c7966
pin FlexGEMM https://github.com/visualbruno/FlexGEMM db388bd17c34abc697792aa718dca780734c2783
pin TRELLIS.2 https://github.com/visualbruno/TRELLIS.2 65d1e13b4a92296036044df0633242bb9e95abf6
EIGEN=CuMesh/third_party/cubvh/third_party/eigen  # CuMesh expects Eigen here but has no submodule for it
[ -d "$EIGEN/Eigen" ] || git clone -q --depth 1 --branch 3.4.0 https://gitlab.com/libeigen/eigen.git "$EIGEN"
pip_c ninja setuptools wheel
PATH=$CUDA_HOME/bin:$PATH LD_LIBRARY_PATH=$CUDA_HOME/lib TORCH_CUDA_ARCH_LIST=$ARCH MAX_JOBS=${MAX_JOBS:-4} \
    "$UV" pip install --python "$PY" --no-deps --no-build-isolation --reinstall --no-cache \
    ./nvdiffrast ./CuMesh ./FlexGEMM ./TRELLIS.2/o-voxel

# 4. Python dependencies. flash-attn is not installed: the app runs the dense backend as sdpa and
#    sparse attention falls back to sdpa. NATTEN is needed by Pixal3D's NAF upsampler (torch.hub).
pip_c meshlib pymeshlab trimesh plotly rembg open3d easydict zstandard plyfile
pip_c "utils3d @ git+https://github.com/EasternJournalist/utils3d.git@35a17ba705d921c7a559ec6067c8eaa5c03ddb37"
"$UV" pip install --python "$PY" --no-deps \
    "https://github.com/SHI-Labs/NATTEN/releases/download/v0.21.5/natten-0.21.5%2Btorch290cu130-cp312-cp312-linux_x86_64.whl"
pip_c -r "$COMFY/custom_nodes/ComfyUI-SkinToken/requirements.txt"

# 5. Headless Blender for SkinToken's export (download.blender.org sits behind a Cloudflare
#    challenge; the Berkeley mirror is an official one). The drop-in survives other installers
#    rewriting the main unit file.
B=$HOME/blender/blender-4.2.3-linux-x64
if [ ! -x "$B/blender" ]; then
  mkdir -p "$HOME/blender" && cd "$HOME/blender"
  M=https://mirrors.ocf.berkeley.edu/blender/release/Blender4.2
  curl -sfLO $M/blender-4.2.3-linux-x64.tar.xz
  curl -sfL $M/blender-4.2.3.sha256 | grep linux-x64.tar.xz | sha256sum -c -
  tar xf blender-4.2.3-linux-x64.tar.xz && rm blender-4.2.3-linux-x64.tar.xz
fi
# SkinToken hands Blender numpy-2 pickles; Blender 4.2 bundles numpy 1.x, which has no numpy._core.
printf "%s\n" 'import sys, numpy.core, numpy.core.multiarray, numpy.core.numeric; sys.modules.setdefault("numpy._core", numpy.core); sys.modules.setdefault("numpy._core.multiarray", numpy.core.multiarray); sys.modules.setdefault("numpy._core.numeric", numpy.core.numeric)' \
    > "$B/4.2/python/lib/python3.11/site-packages/sfam_numpy2_pickles.pth"
UNIT=${UNIT:-comfy-h3@}
mkdir -p "$HOME/.config/systemd/user/$UNIT.service.d"
printf "[Service]\nEnvironment=SKINTOKEN_BLENDER_BIN=%s\n" "$B/blender" > "$HOME/.config/systemd/user/$UNIT.service.d/sfam-skintoken.conf"
systemctl --user daemon-reload

# 6. Models (~61 GB: Pixal3D 43 GB, TRELLIS.2-4B 16 GB, SkinTokens 1.6 GB, DINOv3, TRELLIS ss decoder),
#    at the paths the nodes would otherwise download them to on first use.
COMFY=$COMFY "$PY" - <<'EOF'
import os
from huggingface_hub import hf_hub_download, snapshot_download
M = os.path.join(os.environ["COMFY"], "models")
for repo in ["TencentARC/Pixal3D", "microsoft/TRELLIS.2-4B"]:
    snapshot_download(repo_id=repo, local_dir=os.path.join(M, repo))
for f in ["model.safetensors", "config.json", "preprocessor_config.json"]:
    hf_hub_download("visualbruno/dinov3-vitl16-pretrain-lvd1689m", f, local_dir=os.path.join(M, "facebook", "dinov3-vitl16-pretrain-lvd1689m"))
for f in ["ckpts/ss_dec_conv3d_16l8_fp16.json", "ckpts/ss_dec_conv3d_16l8_fp16.safetensors"]:
    hf_hub_download("microsoft/TRELLIS-image-large", f, local_dir=os.path.join(M, "microsoft", "TRELLIS-image-large"))
S = os.path.join(M, "skintoken")
for f in ["experiments/articulation_xl_quantization_256_token_4/grpo_1400.ckpt", "experiments/skin_vae_2_10_32768/last.ckpt"]:
    hf_hub_download("VAST-AI/SkinTokens", f, local_dir=S)
snapshot_download("Qwen/Qwen3-0.6B", local_dir=os.path.join(S, "Qwen3-0.6B"), ignore_patterns=["*.bin", "*.safetensors"])
EOF

echo "Done. Restart idle ComfyUI servers (systemctl --user restart ${UNIT}1 ...) and check /object_info/Trellis2LoadModel."
