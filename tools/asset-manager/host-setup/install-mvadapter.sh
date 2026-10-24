#!/usr/bin/env bash
# MV-Adapter for the asset manager's multi-view image jobs (mvadapter_graph in app.py). Run on each SSH GPU host.
# Idempotent, no sudo. Licences: node and adapter Apache-2.0, SDXL base OpenRAIL++, sdxl-vae-fp16-fix MIT.
# The node pins diffusers 0.31 / transformers 4.46; the host's newer ones work (tested with diffusers 0.39), so only
# peft is added and every installed package is held at its version. SDXL (~17 GB) downloads to models/diffusers on
# the first job.
set -euo pipefail
COMFY=${COMFY:-$HOME/comfy-h3/ComfyUI}
NODE=$COMFY/custom_nodes/ComfyUI-MVAdapter
REV=9d6d8afeff135d81a31f16dde5291d8b40dbb0a4
UV=${UV:-$HOME/.local/bin/uv}
cd "$COMFY"

[ -d "$NODE/.git" ] || git clone -q https://github.com/huanngzh/ComfyUI-MVAdapter "$NODE"
git -C "$NODE" fetch -q origin "$REV" 2>/dev/null || true
git -C "$NODE" checkout -q "$REV"

HOLD=$(mktemp)
"$UV" pip freeze --python .venv/bin/python > "$HOLD"
"$UV" pip install -q --python .venv/bin/python -c "$HOLD" peft
rm -f "$HOLD"
.venv/bin/python -c 'import peft, diffusers; print("peft", peft.__version__, "diffusers", diffusers.__version__)'
echo "MV-Adapter ready; restart ComfyUI (systemctl --user restart comfy-h3@1 comfy-h3@2) to load the node"
