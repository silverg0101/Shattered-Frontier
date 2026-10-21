#!/bin/bash
# Generates the pilot item icons with the Codex CLI image tool (HUD-v1 icon style). Usage: generate.sh
STYLE="Square 1:1 inventory item icon for a realistic fantasy survival MMO HUD. Single naturalistic hand-rendered object, three-quarter view, centred with margin, muted material colours, crisp readable silhouette at 64px, soft top-left light, flat deep slate-navy (#17232A) background with no vignette, no text, no border, no frame, no watermark."
cd "$(dirname "$0")"
gen() {
  d="work-$1"; mkdir -p "$d"
  (cd "$d" && timeout 900 codex exec --skip-git-repo-check -s workspace-write "Generate one image with your image generation tool: $STYLE Subject: $2. Save it as $1.png in the current directory." >/dev/null 2>&1)
}
gen oak_log "a cut oak log segment with bark and pale ring end" &
gen iron_ore "a chunk of raw iron ore, dark grey rock with rust-orange veins" &
gen rough_timber "a rough-sawn timber plank board, weathered pale wood" &
gen iron_ingot "a single forged iron ingot bar, dull steel grey" &
gen boar_hide "a folded raw boar hide, coarse brown fur and leather" &
gen iron_axe "a woodcutting axe with iron head and wooden haft" &
gen iron_pickaxe "a mining pickaxe with iron head and wooden haft" &
wait
