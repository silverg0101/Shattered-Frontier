#!/bin/bash
# usage: run_views.sh <dir containing concept-sheet.png> "view prompt"   (second step of run_concepts.sh, for reruns)
cd "$1" || exit 1
STYLE="Photoreal CC0-scan-like realism, cool overcast temperate-forest light, grey-blue granite, pine and birch forest floor, no stylization, no text labels."
timeout 600 codex exec --skip-git-repo-check -s workspace-write "Generate one image with your image generation tool, using the attached concept sheet as the design master: $2 Neutral light, plain light-grey background, no labels, no scene, no ruler, one subject only. $STYLE Save it as asset-view.png in the current directory." -i concept-sheet.png > view.log 2>&1
