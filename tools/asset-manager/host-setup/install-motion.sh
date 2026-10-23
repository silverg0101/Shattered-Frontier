#!/bin/bash
# Readies a ComfyUI GPU host for motion jobs, without sudo: headless Blender 4.2.3 (render and bake) and
# CoTracker3 (tracking, through torch.hub at the commit pinned in motion/track_solve.py, so nothing is
# pip-installed into ComfyUI's venv). The video nodes themselves are ComfyUI's built-in API nodes.
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

# 2. CoTracker3 code and weights into ~/.cache/torch/hub, so the first job doesn't download them.
cd "$HERE/motion"
"$COMFY/.venv/bin/python" -c "import torch, track_solve as t; torch.hub.load(t.COTRACKER, 'cotracker3_offline', trust_repo=True)"
"$COMFY/.venv/bin/python" track_solve.py --check
echo "motion ready"
