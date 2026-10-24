#!/usr/bin/env bash
# UltraTex texturing for the asset manager's 3D graph (ultratex_nodes in app.py). Run on each SSH GPU host after
# ComfyUI-UltraTex (https://github.com/pixelartistry/PixelArtistry-UltraTex) is in custom_nodes. Idempotent, no sudo.
# FLUX.2 Klein 9B is gated on Hugging Face and non-commercial: accepting its licence is the owner's step.
set -euo pipefail
COMFY=${COMFY:-$HOME/comfy-h3/ComfyUI}
cd "$COMFY"

missing=0
for f in models/diffusion_models/flux-2-klein-base-9b-fp8.safetensors models/text_encoders/qwen_3_8b_fp8mixed.safetensors \
         models/vae/flux2-vae.safetensors models/loras/ultratex_flux2.safetensors models/loras/ultratex_flux2_mr.safetensors \
         models/ultratex/ultratex_flux2_decoder.pt custom_nodes/ComfyUI-UltraTex/nodes.py; do
    [ -e "$f" ] || { echo "missing: $COMFY/$f"; missing=1; }
done

# moderngl's standalone context defaults to X11, and the hosts are headless (XOpenDisplay fails in UltraTex Prep).
R=custom_nodes/ComfyUI-UltraTex/ultratex_core/render.py
grep -q 'sfam patch: EGL' "$R" || sed -i \
    's/^        _CTX = moderngl.create_standalone_context(require=330)$/        import os  # sfam patch: EGL when headless\n        _CTX = moderngl.create_standalone_context(require=330, **({} if os.environ.get("DISPLAY") else {"backend": "egl"}))/' \
    "$R"
grep -q 'sfam patch: EGL' "$R" || { echo "EGL patch did not apply to $R"; exit 1; }
.venv/bin/python -c 'import moderngl; print("EGL:", moderngl.create_standalone_context(require=330, backend="egl").info["GL_RENDERER"])'
[ "$missing" = 0 ] && echo "UltraTex ready; restart ComfyUI (systemctl --user restart comfy-h3@1 comfy-h3@2) to load the patch"
exit "$missing"
