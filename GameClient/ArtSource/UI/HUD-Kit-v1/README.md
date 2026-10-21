# HUD kit v1

Shared art for every HUD screen, generated with the Codex CLI image tool to match `docs/Game-Design/concept-art/UI/HUD-v1` (hud-gameplay.png, hud-inventory.png): dark slate (#17232A) panels, aged-brass (#A98A55) trim, ivory (#E8E1CE) accents, pale-stag crest.

## Provenance

- `prompts.tsv`: element name and its element prompt. `generate.sh` prepends the shared style text (see `BASE` in that file) and runs one `codex exec` per element; `generate.sh <name>...` regenerates only those.
- `source/<name>.png`: raw Codex output, on pure magenta (#FF00FF) or already transparent. Kept unmodified.
- `kit.json`: processed size, 9-slice margins (processed px) and content inset per element.
- `processed/<name>.png`: output of `tools/Process-HudKit.ps1` (magenta keyed to alpha with despill, crop to artwork, resize to the kit.json size).
- Import: `tools/Import-HudKit.py` (run in the editor) creates `/Game/Frontier/UI/Kit/T_Kit_*`, the Cinzel font and `DA_HudKit`, which `Framework/SFHudStyle.h` reads. Without the asset the HUD falls back to the procedural frames.

Elements: panel_frame, title_bar, tab_normal, tab_selected, button_normal, button_hover, button_pressed, slot_empty, slot_selected, bar_frame, bar_fill_health, bar_fill_stamina, bar_fill_moss, crest_plate, ring_frame, tooltip_frame, divider.

## HUD-v1 v4 additions (source only)

Existing elements above remain unchanged. `source/status_glyphs.png`, `source/gamepad_glyphs.png`, and `source/input_glyphs.png` are the generated transparent sheets; `source/hold_confirm_ring.png` and `source/toast_card.png` are individual generated masters. `processed/` holds alpha-clean, cropped PNGs. Generation prompts and sheet order are in `prompts.md`.

Import these processed PNGs as UI textures under `/Game/Frontier/UI/Kit/` when the editor is available: `status_tick`, `status_cross`, `warning_triangle`; `gamepad_a`, `gamepad_b`, `gamepad_x`, `gamepad_y`, `gamepad_lb`, `gamepad_rb`, `gamepad_lt`, `gamepad_rt`, `gamepad_dpad`, `gamepad_l3`, `gamepad_r3`; `keycap_blank`, `mouse_lmb`, `mouse_rmb`, `mouse_wheel`; `hold_confirm_ring`; `toast_card`. Name textures `T_Kit_<name>`. All glyphs are 64×64 RGBA; the ring is 128×128 RGBA; the toast card is 512×104 RGBA. Keep UI texture compression, alpha, and sRGB enabled.

Render the toast at 270×54 px at 1080p (5% screen height), with 9-slice margins **[112, 20, 20, 20]** in source pixels; the left slice preserves the icon well. Place an icon in the left well and two text lines in the open right area. Use `hold_confirm_ring` as the visible frame with a UI material angular alpha mask controlled by a bounded `Progress` scalar (0–1, clockwise from 12 o'clock); the action's hold duration belongs to typed gameplay configuration, not the texture. `keycap_blank` receives its binding label from UI text. Tick/cross must accompany have/need and placement text; warning is amber. These assets are not imported or wired to widgets yet.

Fonts: titles use Cinzel (SIL OFL 1.1, `../../Fonts/Cinzel`); numbers and body keep the engine sans.
