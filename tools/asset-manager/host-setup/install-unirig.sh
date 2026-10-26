#!/bin/bash
# Isolated learned auto-rigger for the Video2Rig fork; does not modify ComfyUI.
set -euo pipefail
COMFY=${COMFY:-$HOME/comfy-h3/ComfyUI}
UV=${UV:-$HOME/.local/bin/uv}
TARGET=$(dirname "$COMFY")/unirig
REV=6793c6640ff01c8fb389f3993434124bb43d2933
if [ ! -d "$TARGET/.git" ]; then
  test ! -e "$TARGET" || { echo 'UniRig target exists without Git' >&2; exit 1; }
  git clone https://github.com/VAST-AI-Research/UniRig.git "$TARGET"
fi
test -z "$(git -C "$TARGET" status --porcelain --untracked-files=no)"
git -C "$TARGET" fetch --quiet origin "$REV"
git -C "$TARGET" checkout --quiet --detach "$REV"
test -x "$TARGET/.venv/bin/python" || "$UV" venv --python 3.11 "$TARGET/.venv"
PY="$TARGET/.venv/bin/python"
"$UV" pip install --python "$PY" torch==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cu124
"$UV" pip install --python "$PY" -r "$TARGET/requirements.txt" torch==2.5.1 numpy==1.26.4 \
  spconv-cu120 torch-scatter torch-cluster --find-links https://data.pyg.org/whl/torch-2.5.0+cu124.html \
  --only-binary torch-scatter,torch-cluster \
  'flash-attn @ https://github.com/Dao-AILab/flash-attention/releases/download/v2.7.4.post1/flash_attn-2.7.4.post1%2Bcu12torch2.5cxx11abiFALSE-cp311-cp311-linux_x86_64.whl'
"$UV" pip freeze --python "$PY" > "$TARGET/forge-environment.txt"
cd "$TARGET"
CUDA_VISIBLE_DEVICES='' "$PY" -W ignore -c 'import flash_attn, spconv.pytorch, torch_scatter, torch_cluster, bpy; import src.model.unirig_ar; print("UniRig imports ready")'
"$PY" -c 'from pathlib import Path; from src.inference.download import download; p=Path(download("experiments/skeleton/articulation-xl_quantization_256/model.ckpt")); assert p.is_file(), "UniRig checkpoint missing"; print("UniRig checkpoint ready", p.stat().st_size)'
