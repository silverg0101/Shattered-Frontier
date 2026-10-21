# HUD-v1 v4 addition prompts

Shared style: transparent, flat orthographic fantasy MMO UI art; slate-navy `#17232A`, aged brass `#A98A55`, ivory `#E8E1CE`, moss `#688267`, crimson `#A34842`, amber `#B69A55`; crisp at HUD scale, no background or extra labels.

- `status_glyphs.png`: three isolated symbols in one row: ivory/moss tick, ivory/crimson cross, amber warning triangle with exclamation mark.
- `gamepad_glyphs.png`: 5×2 grid, row-major A, B, X, Y, LB / RB, LT, RT, D-pad, L3; engraved brass-edged slate buttons, legible ivory marks.
- `input_glyphs.png`: five in one row: R3, empty keycap, left mouse button, right mouse button, wheel; ivory active parts and brass-edged slate bodies.
- `hold_confirm_ring.png`: centered empty brass/slate annulus, transparent center, no progress wedge; intended for material-controlled angular reveal.
- `toast_card.png`: one wide, blank slate/brass chamfered card with a square left icon well and quiet space for two lines; 9-slice-safe border.

Each generated sheet is retained unmodified in `source/`; processed crops are resized and alpha-thresholded for delivery.
