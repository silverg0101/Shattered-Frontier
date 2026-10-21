# UI v5 Fable re-review recapture — 2026-10-21

This folder pairs the approved concept (left) with a fresh 1920×1080 E1 game viewport frame (right) for each of the nine requested screens. The comparison header identifies the actual live state. These are capture records for independent review, not acceptance judgements. The *-live.png files are unscaled engine captures; *-compare.png files use 960×540 per side.

## Import and capture

- Opened GameClient/ShatteredFrontier.uproject once in UnrealEditor on /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook. Ran py -3 tools/Process-KitV5.py (188 pieces), then py -3 tools/Run-InEditor.py tools/Import-HudKit.py --save. Ran tools/Process-HudKit.ps1 -Kit GameClient/ArtSource/UI/LoadingScreen-v5, then py -3 tools/Run-InEditor.py GameClient/ArtSource/UI/LoadingScreen-v5/Import-LoadingScreen-v5.py --save. Both editor imports reported success. Closed editor PID 61824 before every game launch. No compile or UI source edit.
- Game command pattern: UnrealEditor.exe GameClient/ShatteredFrontier.uproject /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -windowed -ResX=1920 -ResY=1080 -ForceRes -nosound -nop4 <pilot flags> -UserDir=<isolated artifact dir> -ExecCmds="py <capture script> <artifact frames dir> ...". Captures used the engine console line Shot showui. tools/Capture-TimedShots.py records that HighResShot omits UMG in this build; Shot showui produced 1920×1080 viewport PNGs with UI. Each attempt ran on this E1 map, not the construction sandbox.
- The default E1 spawn was the Silverbrook town square. The boar attempt used an in-game Python capture hook to position the E1 pawn near a spawned boar; the station attempt positioned it near a sawmill. Neither produced the prescribed river-trail/bridge WP2 composition. The game shows daylight, but the exact WP2 camera and lighting parameters were not verified.

| Screen | Fresh live state in the pair | Staging limit |
| --- | --- | --- |
| gameplay-hud | E1 pawn near a boar in the Silverbrook field | No target frame or harvest toast; river trail not staged. |
| build-mode | B opened Build Catalogue on the E1 controller | It shows unlimited development stock and no valid placement preview; production catalogue state remains blocked. |
| inventory | I opened inventory with starter axe visible | Pointer hover over bag axe and main-hand slot did not show the tooltip. |
| market-board | F7 opened board; Iron Axe row selected | Requested selection captured. |
| trade | F6 opened trade; my offer was locked by clicking Lock Offer | Partner remained “Offer locked,” not “Reviewing offer”; the demo initializes partner as locked. |
| gathering | E1 Silverbrook gathering scenario attempt | Mid-chop and open skills panel were not reached. |
| crafting-station | E1 pawn positioned next to a spawned sawmill | Station was nearby, but aim prompt and station UI did not appear. |
| defeated | E1 Silverbrook combat scenario attempt | Pawn remained alive; defeated modal not reached. |
| loading-screen | Fresh post-spawn E1 frame, labelled as an unsuccessful loading attempt | Existing timed collector starts after pawn creation. A PreLoadMap screenshot hook was not available in the no-source-edit capture path, so no fresh loading frame was obtained. |

The right-hand frames for the incomplete rows are attempt evidence, not substitutes for the named states. The loading *-live.png likewise contains post-spawn gameplay, not a loading screen. No independent visual review has been performed here.
