#!/usr/bin/env bash
set -euo pipefail
lab=/home/grego/sf-asset-lab
base="$lab/venv-current/lib/python3.12/site-packages"
backend=${1:?hunyuan, tripo or pixal required}
case "$backend" in hunyuan|tripo|pixal) ;; *) exit 2;; esac
envdir="$lab/env-$backend"
if [ ! -d "$envdir" ]; then python3 -m venv "$envdir"; fi
# Reuse the already verified Radeon torch/native binaries read-only. All new
# packages install into the child environment; never pip-install into the base.
printf '%s\n' "$base" > "$envdir/lib/python3.12/site-packages/radeon-base.pth"
printf '%s\n' 'torch==2.14.0+rocm10.1.0' 'torchvision==0.29.0a0+rocm10.1.0' > "$envdir/radeon-constraints.txt"
common=(omegaconf scikit-image trimesh fast-simplification jaxtyping typeguard)
case "$backend" in
  hunyuan) packages=(diffusers==0.30.0 transformers==4.46.3 accelerate==1.1.1 pymeshlab pytorch-lightning torchmetrics torchdiffeq PyMCubes pygltflib open3d realesrgan basicsr xatlas);;
  tripo) packages=(diffusers==0.32.2 transformers==4.48.3 accelerate==1.3.0 peft==0.14.0);;
  pixal) packages=(diffusers==0.37.1 transformers==4.57.3 accelerate==1.13.0 kornia plyfile);;
esac
"$envdir/bin/python" -m pip install -q --no-build-isolation -c "$envdir/radeon-constraints.txt" "${common[@]}" "${packages[@]}"
if [ "$backend" = pixal ]; then
  "$envdir/bin/python" -m pip install -q --no-deps git+https://github.com/EasternJournalist/utils3d-moge.git@62f09d58509485564e24d5d9f6aac9ee9ebc0c37
fi
"$envdir/bin/python" -c "import torch; print(torch.__version__,torch.version.hip,torch.cuda.get_device_name(0))"
