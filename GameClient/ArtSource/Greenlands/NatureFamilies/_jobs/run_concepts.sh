#!/bin/bash
# usage: run_concepts.sh Family "sheet prompt" "view prompt"
F="$1"; D="/c/Projects/Shattered-Frontier/GameClient/ArtSource/Greenlands/NatureFamilies/$F/Concept"
mkdir -p "$D" && cd "$D" || exit 1
STYLE="Photoreal CC0-scan-like realism, cool overcast temperate-forest light, grey-blue granite, pine and birch forest floor, no stylization, no text labels except size annotations."
timeout 600 codex exec --skip-git-repo-check -s workspace-write "Generate one image with your image generation tool: A concept sheet for a game environment asset. $2 $STYLE Include a few dimension callouts in metres or centimetres. Save it as concept-sheet.png in the current directory." > sheet.log 2>&1
[ -f concept-sheet.png ] || exit 2
timeout 600 codex exec --skip-git-repo-check -s workspace-write "Generate one image with your image generation tool, using the attached concept sheet as the design master: $3 Neutral light, plain light-grey background, no labels, no scene, no ruler, one subject only. $STYLE Save it as asset-view.png in the current directory." -i concept-sheet.png > view.log 2>&1
