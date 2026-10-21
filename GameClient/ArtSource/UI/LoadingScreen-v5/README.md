# Loading screen v5 art

Target: `docs/Game-Design/concept-art/UI/v5/9-loading-screen.png` (`concept.png` here is a copy). `generate.sh` makes `source/` with the Codex image tool (magenta key for the UI pieces; `ls_background` is scene-only, no UI). `tools/Process-HudKit.ps1 -Kit GameClient/ArtSource/UI/LoadingScreen-v5` writes `processed/`; `Import-LoadingScreen-v5.py` imports everything to `/Game/Frontier/UI/LoadingScreen/T_LS_*`. Widget: `Framework/SFLoadingScreen.cpp`.

Evidence: `docs/evidence/ui-v5/9-loading-screen-live.png` (-game, 1920x1080), `9-loading-screen-compare.png` (concept left, live right) and `9-loading-screen-live-ultrawide.png` (2580x1080: background crops, nothing squashes).

Remaining differences: the concept has a darker local plaque behind the emblem/title and a thicker bar; the live bar starts nearly empty (time ease). Contrast layer: `Make-Vignette.ps1` writes `source/ls_vignette.png` (edge vignette, dark ellipse behind emblem/title, bottom band), imported as `T_LS_Vignette`. Title is EB Garamond (mixed case). Known differences: the bar fill is a time-based ease (MoviePlayer gives no progress), not real load progress.
