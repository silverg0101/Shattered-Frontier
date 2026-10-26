#!/bin/bash
# Separate fork checkout/environment; reuses host torch without changing ComfyUI.
# COMFY=~/comfy-h3/ComfyUI bash host-setup/install-video2rig.sh
set -euo pipefail
COMFY=${COMFY:-$HOME/comfy-h3/ComfyUI}
HERE=$(cd "$(dirname "$0")/.." && pwd)
TARGET=$(dirname "$COMFY")/video2rig
UV=${UV:-$HOME/.local/bin/uv}
readarray -t SOURCE < <("$COMFY/.venv/bin/python" -c 'import json,sys; d=json.load(open(sys.argv[1])); print(d["repository"]); print(d["revision"])' "$HERE/video2rig.lock.json")
if [ ! -d "$TARGET/.git" ]; then
  test ! -e "$TARGET" || { echo 'video2rig target exists without Git; refusing to overwrite' >&2; exit 1; }
  git clone "${SOURCE[0]}" "$TARGET"
fi
test "$(git -C "$TARGET" remote get-url origin)" = "${SOURCE[0]}" || { echo 'unexpected fork remote' >&2; exit 1; }
test -z "$(git -C "$TARGET" status --porcelain --untracked-files=no)" || { echo 'commit local fork edits first' >&2; exit 1; }
git -C "$TARGET" fetch --quiet origin "${SOURCE[1]}"
git -C "$TARGET" checkout --quiet --detach "${SOURCE[1]}"
test -x "$TARGET/.venv/bin/python" || "$UV" venv --python "$COMFY/.venv/bin/python" "$TARGET/.venv"
# A .pth exposes the existing CUDA torch build. Packages installed in this venv
# take precedence, especially transformers 5; ComfyUI retains transformers 4.
TORCH_SITE=$("$COMFY/.venv/bin/python" -c 'import sysconfig; print(sysconfig.get_paths()["purelib"])')
TARGET_SITE=$("$TARGET/.venv/bin/python" -c 'import sysconfig; print(sysconfig.get_paths()["purelib"])')
printf '%s\n' "$TORCH_SITE" > "$TARGET_SITE/forge-host-torch.pth"
"$UV" pip install --python "$TARGET/.venv/bin/python" 'transformers==5.20.2' 'threadpoolctl==3.6.0' -r "$TARGET/requirements.txt"
"$UV" pip freeze --python "$TARGET/.venv/bin/python" > "$TARGET/forge-environment.txt"
cd "$TARGET"
"$TARGET/.venv/bin/python" -m unittest discover -s tests -p test_forge.py
"$TARGET/.venv/bin/python" -c 'from transformers import Dinov2Model; import torch; print("video2rig ready; torch", torch.__version__)'
