#!/bin/bash
# Generates every HUD kit element listed in prompts.tsv with the Codex CLI image tool. Usage: generate.sh [name ...]
BASE="Game UI asset for a shipped-quality dark fantasy survival MMO HUD in a restrained Greenlands style: deep slate-navy (#17232A) surfaces, fine aged-brass (#A98A55) trim, slightly chamfered corners, subtle engraved linework and fine grain, warm ivory (#E8E1CE) accents. No text, no letters, no numbers, no watermark. Flat orthographic straight-on view, perfectly symmetric and centred, designed for 9-slice scaling with a uniform border and a plain flat interior. The whole asset sits on a solid pure magenta (#FF00FF) background with a clear margin, nothing else in frame, no drop shadow, no glow outside the asset."
cd "$(dirname "$0")"; mkdir -p source
gen() {
  name="$1"; prompt="$2"; d="$(mktemp -d)"
  (cd "$d" && timeout 900 codex exec --skip-git-repo-check -s workspace-write "Generate one image with your image generation tool: $BASE Element: $prompt Save it as $name.png in the current directory." >/dev/null 2>&1)
  mv "$d/$name.png" "source/$name.png" 2>/dev/null; rm -rf "$d"
}
export -f gen; export BASE
while IFS=$'\t' read -r name prompt; do
  if [ $# -gt 0 ] && [[ ! " $* " =~ " $name " ]]; then continue; fi
  printf '%s\0%s\0' "$name" "$prompt"
done < prompts.tsv | xargs -0 -n2 -P 6 bash -c 'gen "$0" "$1"'
