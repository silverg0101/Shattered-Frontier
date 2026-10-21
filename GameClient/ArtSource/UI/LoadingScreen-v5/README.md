# Loading screen v5 art

Target: `docs/Game-Design/concept-art/UI/v5/9-loading-screen.png` (`concept.png` here is a copy). `generate.sh` makes `source/` with the Codex image tool (magenta key for the UI pieces; `ls_background` is scene-only, no UI). `tools/Process-HudKit.ps1 -Kit GameClient/ArtSource/UI/LoadingScreen-v5` writes `processed/`; `Import-LoadingScreen-v5.py` imports everything to `/Game/Frontier/UI/LoadingScreen/T_LS_*`. Widget: `Framework/SFLoadingScreen.cpp`.

Evidence: `docs/evidence/ui-v5/9-loading-screen-live.png` (-game, 1920x1080) and `9-loading-screen-compare.png` (concept left, live right).

Known differences from the concept: the regenerated background has no snow peaks or open sky and lacks the dark vignette under the title and tip, so the tip line is low contrast (add a dark gradient strip); the title is Cinzel (all small caps) where the concept shows a mixed-case serif; the bar fill is a time-based ease (MoviePlayer gives no progress), not real load progress.
