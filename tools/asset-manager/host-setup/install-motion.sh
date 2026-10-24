#!/bin/bash
# Readies a ComfyUI GPU host for motion jobs, without sudo: headless Blender 4.2.3 (render, training images
# and bake), DINOv2 ViT-S/14 (each model's joint detector) and CoTracker3 (the fallback tracker), both through
# torch.hub so nothing is pip-installed into ComfyUI's venv. The video model is the host's local MiniMax H3.
# Run as the ComfyUI user from a copy of tools/asset-manager:
#
#   COMFY=~/comfy-h3/ComfyUI bash host-setup/install-motion.sh
set -euo pipefail
COMFY=${COMFY:-$HOME/comfy-h3/ComfyUI}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BLENDER=$HOME/blender/blender-4.2.3-linux-x64/blender

# 1. Blender: the asset manager calls this path unless the host's settings name another ("blender").
if [ ! -x "$BLENDER" ]; then
  mkdir -p "$HOME/blender"
  curl -L https://download.blender.org/release/Blender4.2/blender-4.2.3-linux-x64.tar.xz | tar -xJ -C "$HOME/blender"
fi
"$BLENDER" -b --version | head -1

# 2. DINOv2 and CoTracker3 code and weights into ~/.cache/torch/hub, so the first job doesn't download them.
cd "$HERE/motion"
"$COMFY/.venv/bin/python" -c "import torch, detector as d; torch.hub.load(d.DINO, 'dinov2_vits14', trust_repo=True)"
"$COMFY/.venv/bin/python" -c "import torch, track_solve as t; torch.hub.load(t.COTRACKER, 'cotracker3_offline', trust_repo=True)"
CUDA_VISIBLE_DEVICES= "$COMFY/.venv/bin/python" detector.py --check
"$COMFY/.venv/bin/python" track_solve.py --check
echo "motion ready"
