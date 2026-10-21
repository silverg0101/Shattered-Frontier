#!/bin/bash
# Regenerates the Kit-v5 structural pieces in the slim style of the approved v5 concepts (thin brass hairline, small
# chamfers, dark navy fill) with the Codex CLI image tool. Each run gets its concept crop (refs/<piece>.png, from
# refs.py) as ref.png. Raw outputs land in source/; process.py turns them into the Kit-v5 PNGs.
# Usage: generate.sh [piece ...]
cd "$(dirname "$0")"; HERE="$(pwd)"; mkdir -p source
STYLE="Recreate the UI element shown in ref.png as an EMPTY reusable game UI asset that matches it exactly: the same thin aged-brass hairline border (about 2 px at this size, subtle warm highlight, no thick bevel, no rivets, no ornate corner brackets), the same small chamfered corners, the same faint inner line if ref.png has one, and a flat solid very dark navy-slate fill #111A20 with only the faintest texture. Remove every word, number, letter, icon, item picture, checkbox and bar fill from the inside: the interior is plain fill. Straight-on flat view, crisp edges, no perspective, no drop shadow, no glow, nothing outside the element. Place it centred on a solid pure magenta (#FF00FF) background with a clear margin. Render the image large: at least 1600 pixels on its long side, keeping the aspect ratio given below."
gen() {
  name="$1"; prompt="$2"; d="$(mktemp -d)"; cp "$HERE/refs/$name.png" "$d/ref.png" 2>/dev/null || cp "$HERE/refs/bar_hp_frame.png" "$d/ref.png"
  (cd "$d" && printf '%s' "Generate one image with your image generation tool. $prompt Save it as $name.png in the current directory." | timeout 900 codex exec --dangerously-bypass-approvals-and-sandbox --skip-git-repo-check -C . - >/dev/null 2>&1)
  mv "$d/$name.png" "$HERE/source/$name.png" 2>/dev/null || echo "FAILED $name"; rm -rf "$d"
}
export -f gen; export HERE
declare -A P
P[panel_frame]="$STYLE Element: the large side panel frame of the Build Catalogue (outer frame only, none of its inner boxes, tabs or tiles), drawn as a 1024x640 (1.6:1 landscape) rectangle frame with the same corner treatment."
P[window_frame]="$STYLE Element: the dialog window frame (outer frame only: thin brass border, small corner flourish notches as in ref.png, a tiny brass point at the top centre), 512x420 aspect."
P[quest_tracker_frame]="$STYLE Element: the quest tracker box frame (outer frame only, no title rule, no rows), 280x154 aspect."
P[toast_frame]="$STYLE Element: the notification toast card frame, 280x66 aspect (wide and short)."
P[target_frame]="$STYLE Element: the target name plate frame at the top of the screen, without the hexagonal level cap and without the red health bar, 410x70 aspect."
P[bar_hp_frame]="$STYLE Element: ONLY the thin brass outline of one horizontal resource bar from ref.png, with its slightly pointed rounded ends; the inside of the outline must be pure magenta (hollow, see-through), 380x32 aspect (very wide and thin)."
P[bar_hp_back]="Recreate, from ref.png, the empty trough of one horizontal resource bar: a plain very dark navy-slate #0E161B rounded strip with slightly pointed ends, no border, no fill colour, 380x32 aspect, centred on a solid pure magenta (#FF00FF) background."
P[bar_hp_fill]="Recreate, from ref.png, the red health fill of the top bar as a seamless horizontal strip: deep crimson #A34842 with a soft lighter band along the top and a darker lower edge, exactly as in ref.png, 64x38 aspect, filling the whole image edge to edge, no border, no ends."
P[bar_stamina_fill]="Recreate, from ref.png, the gold stamina fill of the lower bar as a seamless horizontal strip: muted amber gold #B69A55 with a soft lighter band along the top and a darker lower edge, exactly as in ref.png, 64x38 aspect, filling the whole image edge to edge, no border, no ends."
P[bar_xp_fill]="Recreate, from ref.png, the style of the bar fills as a seamless horizontal strip in muted moss green #688267 with a soft lighter band along the top and a darker lower edge, 64x38 aspect, filling the whole image edge to edge, no border, no ends."
P[slot_hotbar_normal]="$STYLE Element: one empty hotbar slot (square, no number, no item), 1:1."
P[slot_hotbar_selected]="$STYLE Element: the selected hotbar slot with its brighter brass border as in ref.png (square, no number, no item), 1:1."
P[slot_catalogue_normal]="$STYLE Element: one empty catalogue tile (square, no picture, no caption), 1:1."
P[slot_catalogue_selected]="$STYLE Element: the selected catalogue tile with its bright brass border and faint warm inner glow line as in ref.png (square, no picture, no caption), 1:1."
P[button_normal]="$STYLE Element: the action prompt plate behind 'F Interact' (no key cap, no text), 300x56 aspect."
P[tab_normal]="$STYLE Element: the unselected tab 'Blueprints' (no text), 190x44 aspect."
P[tab_selected]="$STYLE Element: the selected tab 'Voxel Objects' with its muted moss green tinted fill #3E5543 and brass border as in ref.png (no text), 190x44 aspect."
P[search_field]="$STYLE Element: the search field (no magnifier, no placeholder text), 260x36 aspect."
P[dropdown_field]="$STYLE Element: the material dropdown field (no swatch, no text, no arrow), 220x40 aspect."
P[recipe_row]="$STYLE Element: one list row frame like the 'Timber Beam' row (no icon, no text), 320x56 aspect."
P[queue_row]="$STYLE Element: one queue row frame (no icon, no text, no progress bar, no percent), 320x48 aspect."
P[minimap_footer]="$STYLE Element: the small location plate under the minimap (no text), 180x32 aspect."
P[keycap_f]="Recreate, from ref.png, the square key cap with the letter F exactly: dark navy-slate square with thin brass border and small chamfered corners and an ivory serif capital F centred, 40x36 aspect, straight-on, centred on a solid pure magenta (#FF00FF) background."
P[modal_frame]="$STYLE Element: the large Market Board modal frame (outer frame only: thin brass border, small corner flourishes and the tiny brass point at the top centre exactly as in ref.png; no stag, no title, no tabs, no table, no inner panels), drawn as a 1.28:1 landscape rectangle (wider than tall)."
P[detail_card_frame]="$STYLE Element: the item detail card frame on the right of the Market Board (outer frame only, no picture, no text, no button). It MUST keep the clearly visible thin aged-brass #A98A55 border line around all four sides with the small chamfered corners and tiny corner flourishes, exactly as in ref.png. Portrait 0.88:1 aspect."
P[button_primary_brass]="Recreate, from ref.png, the primary button as an EMPTY asset in exactly its muted aged-brass colour: a soft bronze-gold face (top about #B8975C, bottom about #8A6C3E, a gentle vertical gradient with faint brushed texture; NOT bright yellow, NOT saturated gold), a darker bronze #5E4A28 hairline border with small chamfered corners and a fine lighter inner line, flat and elegant, no heavy bevel, no rivets. Remove the text and the ring: the face is plain. Straight-on, 300x56 aspect, centred on a solid pure magenta (#FF00FF) background with a clear margin. Render it large: at least 1600 pixels wide."
P[hold_confirm_ring]="Recreate, from ref.png, the hold-to-confirm ring at the left end of the 'Hold to Buy' button as an EMPTY ring: a dark navy-slate #111A20 disc with a thin brass outer rim and an empty dark circular track (no progress arc, no fill), perfectly circular, 1:1, straight-on, centred on a solid pure magenta (#FF00FF) background. Render it large: at least 1024 pixels wide."
P[minimap_ring]="Recreate, from ref.png, ONLY the frame of the round minimap: the slim aged-brass circular ring with its fine inner line and the small north marker plate at the top with the letter N, exactly as thin and elegant as in ref.png; the inside of the ring must be pure magenta (hollow, no map, no icons), no footer plate below it. Perfectly circular, 1:1, straight-on, centred on a solid pure magenta (#FF00FF) background. Render it large: at least 1024 pixels wide."
for n in "${!P[@]}"; do
  if [ $# -gt 0 ] && [[ ! " $* " =~ " $n " ]]; then continue; fi
  printf '%s\0%s\0' "$n" "${P[$n]}"
done | xargs -0 -n2 -P 8 bash -c 'gen "$0" "$1"'
