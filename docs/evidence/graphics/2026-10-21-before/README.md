# GFX-01 Silverbrook reference: BEFORE (2026-10-21)

Captured in the built UE 5.8 editor target with `-game -RenderOffscreen`, before any GFX-01 renderer change. All four views are 2560 × 1440 `HighResShot` frames. PNGs above 3 MB were converted to JPEG with `ffmpeg -q:v 2`. The saved map has a sparse, graybox approach and no forest floor detail; the first and fourth images record those gaps at the fixed reference positions. The player is still the gray cube.

| Capture | Camera XY (m) | Look XY (m) | Height / look height (m above collision) | FOV |
|---|---:|---:|---:|---:|
| [01 forest path toward bridge, town and mountains](01-forest-path-town.jpg) | 154, 632 | 18, 190 | 1.7 / 10 | 70° |
| [02 main street](02-main-street.jpg) | 22, 330 | 0, -30 | 1.7 / 4 | 70° |
| [03 river and bridge](03-river-bridge.jpg) | 55, 182 | 20, 190 | 3 / 2 | 70° |
| [04 forest floor](04-forest-floor.jpg) | 520, 120 | 520.7, 119.4 | 0.55 / 0.05 | 65° |

The first view places the pawn 5 m ahead and 1.5 m right of the camera. The script fixes the directional light at pitch −40°, yaw −60°, disables eye adaptation and auto exposure, sets exposure offset to zero, disables motion blur, sets screen percentage to 100, and hides debug text labels. Terrain height is read from map collision at each XY.

From the repository root, with no editor open:

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' 'C:/Projects/Shattered-Frontier/GameClient/ShatteredFrontier.uproject' '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook' -game -RenderOffscreen -unattended -nosound -NoSplash -GfxReferenceCapture -ResX=2560 -ResY=1440 '-ExecCmds=py C:/Projects/Shattered-Frontier/GameClient/ArtSource/Greenlands/review_silverbrook_in_game.py'
```

If the editor is open in a Silverbrook play session, run `C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe -I tools/Run-InEditor.py GameClient/ArtSource/Greenlands/review_silverbrook_in_game.py -GfxReferenceCapture`. The script's normal mode still performs the route/swim review. Capture logs showed four `GFX01_CAPTURE` entries and four 2560 × 1440 screenshots.
