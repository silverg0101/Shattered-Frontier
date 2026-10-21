#!/bin/bash
# Generates the v5 loading-screen art with the Codex CLI image tool, matching concept.png (= docs/Game-Design/concept-art/UI/v5/9-loading-screen.png).
# Usage: generate.sh [name ...]. Outputs land in source/.
cd "$(dirname "$0")"; HERE="$(pwd)"; mkdir -p source
UI="Game UI asset matching the approved concept image concept.png in the current directory exactly (same brass, ivory, stag and engraving style). Flat straight-on view, perfectly symmetric, no text, no letters, no watermark, no drop shadow or glow outside the asset. The asset sits on a solid pure magenta (#FF00FF) background with a clear margin, nothing else in frame."
gen() {
  name="$1"; prompt="$2"; d="$(mktemp -d)"; cp "$HERE/concept.png" "$d/"
  (cd "$d" && timeout 900 codex exec --skip-git-repo-check -s workspace-write "Generate one image with your image generation tool: $prompt Save it as $name.png in the current directory." >/dev/null 2>&1)
  mv "$d/$name.png" "$HERE/source/$name.png" 2>/dev/null; rm -rf "$d"
}
export -f gen; export HERE
declare -A P
P[ls_background]="Look at concept.png. Recreate exactly that 16:9 full-bleed scene (Silverbrook stone bridge, church tower, mill with waterwheel, river, mossy foreground rocks, tree at left, snowy mountains) as pure environment art with NO emblem, NO stag, NO title, NO text, NO progress bar, NO spinner and no UI of any kind; keep the scene composition, lighting and colours the same, and fill the area where the emblem and title were with the natural scene (river and bridge)."
P[ls_emblem]="$UI Element: the white stag head with antlers inside the thin brass-and-dark diamond frame with engraved diagonal lines, exactly as in concept.png, centred, on solid magenta."
P[ls_rule]="$UI Element: the thin engraved brass horizontal rule with a small centre diamond ornament under the title 'Silverbrook' in concept.png, wide 8:1 aspect, centred, on solid magenta."
P[ls_bar_frame]="$UI Element: the empty progress bar frame from concept.png: slim brass outline bar with diamond end ornaments, a dark slate empty interior, about 44:1 aspect, horizontal, 3-slice friendly (plain interior), on solid magenta."
P[ls_bar_fill]="$UI Element: the filled brass-gold progress fill strip from concept.png, plain horizontal gradient brass bar with subtle highlight, about 40:1 aspect, on solid magenta."
P[ls_spinner]="$UI Element: the small thin ring spinner from concept.png: a circular brass ring that is a three-quarter arc with a gap, square 1:1, on solid magenta."
for n in "${!P[@]}"; do
  if [ $# -gt 0 ] && [[ ! " $* " =~ " $n " ]]; then continue; fi
  printf '%s\0%s\0' "$n" "${P[$n]}"
done | xargs -0 -n2 -P 6 bash -c 'gen "$0" "$1"'
