#!/bin/bash
# Generates the construction catalogue icons with the Codex CLI image tool (HUD-v1 icon style).
STYLE="Square 1:1 construction catalogue thumbnail for a realistic fantasy survival MMO HUD. Single naturalistic hand-rendered architectural piece in rough timber and weathered grey stone, three-quarter view, centred with margin, muted material colours, crisp readable silhouette at 64px, soft top-left light, flat deep slate-navy (#17232A) background with no vignette, no text, no border, no frame, no watermark."
cd "$(dirname "$0")"
gen() {
  d="work-$1"; mkdir -p "$d"
  (cd "$d" && timeout 900 codex exec --skip-git-repo-check -s workspace-write "Generate one image with your image generation tool: $STYLE Subject: $2. Save it as $1.png in the current directory." >/dev/null 2>&1)
  mv "$d/$1.png" "$1.png" 2>/dev/null
}
gen build_foundation "a low rough-timber and stone foundation platform slab" &
gen build_wall "a rough-timber plank wall section on a stone footing" &
gen build_door "a sturdy rough-timber plank door in a timber frame with iron hinges" &
gen build_roof "a sloped timber-framed roof section with dark wooden shingles" &
wait
rmdir work-* 2>/dev/null
