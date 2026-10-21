# HUD kit v1

Shared art for every HUD screen, generated with the Codex CLI image tool to match `docs/Game-Design/concept-art/UI/HUD-v1` (hud-gameplay.png, hud-inventory.png): dark slate (#17232A) panels, aged-brass (#A98A55) trim, ivory (#E8E1CE) accents, pale-stag crest.

## Provenance

- `prompts.tsv`: element name and its element prompt. `generate.sh` prepends the shared style text (see `BASE` in that file) and runs one `codex exec` per element; `generate.sh <name>...` regenerates only those.
- `source/<name>.png`: raw Codex output, on pure magenta (#FF00FF) or already transparent. Kept unmodified.
- `kit.json`: processed size, 9-slice margins (processed px) and content inset per element.
- `processed/<name>.png`: output of `tools/Process-HudKit.ps1` (magenta keyed to alpha with despill, crop to artwork, resize to the kit.json size).
- Import: `tools/Import-HudKit.py` (run in the editor) creates `/Game/Frontier/UI/Kit/T_Kit_*`, the Cinzel font and `DA_HudKit`, which `Framework/SFHudStyle.h` reads. Without the asset the HUD falls back to the procedural frames.

Elements: panel_frame, title_bar, tab_normal, tab_selected, button_normal, button_hover, button_pressed, slot_empty, slot_selected, bar_frame, bar_fill_health, bar_fill_stamina, bar_fill_moss, crest_plate, ring_frame, tooltip_frame, divider.

Fonts: titles use Cinzel (SIL OFL 1.1, `../../Fonts/Cinzel`); numbers and body keep the engine sans.
