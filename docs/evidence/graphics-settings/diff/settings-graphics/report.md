# settings-graphics: pixel diff, FAIL

Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (match < 0.5).

| element | concept rect | dx | dy | dw | dh | match | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| window frame | [336, 115, 1249, 810] | - | - | - | - | 0.32 | panelfill 2.7, panelfill 7.1 | FAIL |
| title Settings | [884, 140, 161, 44] | - | - | - | - | 0.49 | - | FAIL |
| close x | [1515, 139, 46, 46] | -8 | -7 | +15 | +15 | 0.67 | - | FAIL |
| tab Graphics | [452, 207, 202, 55] | - | - | - | - | 0.46 | mossfill 3.2 | FAIL |
| tab Audio | [655, 207, 198, 55] | +0 | +17 | -23 | -17 | 0.83 | - | FAIL |
| tab Controls | [852, 207, 204, 55] | +11 | +2 | -18 | -2 | 0.73 | - | FAIL |
| tab Interface | [1056, 207, 196, 55] | +1 | +22 | -25 | -22 | 0.86 | - | FAIL |
| tab Gameplay | [1253, 207, 209, 55] | +1 | +23 | -7 | -23 | 0.74 | - | FAIL |
| label Graphics Preset | [385, 292, 201, 32] | +3 | +1 | +5 | +0 | 0.52 | - | FAIL |
| button Auto-Detect | [1277, 291, 126, 36] | -11 | +0 | +21 | +1 | 0.53 | - | FAIL |
| status Preset | [1435, 299, 103, 21] | - | - | - | - | 0.36 | - | FAIL |
| preset Low | [386, 340, 215, 47] | +0 | +3 | -1 | -3 | 0.86 | platefill 2.0 | FAIL |
| preset Medium | [610, 340, 222, 47] | +0 | +3 | -2 | -3 | 0.88 | - | FAIL |
| preset High selected | [842, 340, 231, 47] | +0 | +4 | -1 | -4 | 0.60 | mossfill 6.6 | FAIL |
| preset Epic | [1083, 340, 223, 47] | +0 | +3 | -2 | -3 | 0.85 | - | FAIL |
| preset Ultra | [1315, 340, 220, 47] | +0 | +3 | +0 | -3 | 0.85 | - | FAIL |
| heading Display | [387, 432, 94, 28] | - | - | - | - | 0.42 | - | FAIL |
| stepper Resolution | [669, 478, 272, 37] | -20 | -10 | +41 | +19 | 0.64 | - | FAIL |
| stepper Window Mode | [669, 531, 272, 37] | -20 | -11 | +41 | +19 | 0.64 | - | FAIL |
| stepper Frame Rate | [669, 583, 272, 37] | -20 | -11 | +41 | +19 | 0.63 | - | FAIL |
| vsync On | [669, 640, 132, 39] | +0 | +0 | -2 | -1 | 0.79 | - | PASS |
| vsync Off | [814, 640, 129, 39] | - | - | - | - | 0.39 | - | FAIL |
| render scale track | [440, 736, 342, 21] | - | - | - | - | 0.37 | - | FAIL |
| render scale value | [859, 726, 87, 40] | - | - | - | - | 0.43 | - | FAIL |
| motion blur On | [669, 792, 132, 39] | +1 | +1 | -2 | -3 | 0.80 | - | FAIL |
| motion blur Off | [814, 792, 129, 39] | - | - | - | - | 0.17 | - | FAIL |
| heading Quality | [1005, 429, 87, 28] | - | - | - | - | 0.46 | - | FAIL |
| quality row View Distance | [1284, 475, 243, 31] | - | - | - | - | 0.48 | - | FAIL |
| quality row Shadows | [1284, 511, 243, 31] | - | - | - | - | 0.49 | - | FAIL |
| quality row Nanite Tessellation | [1284, 796, 243, 31] | - | - | - | - | 0.50 | - | FAIL |
| button Apply | [683, 861, 250, 54] | +0 | +1 | -1 | +0 | 0.68 | - | PASS |
| button Revert | [985, 861, 248, 54] | +0 | +1 | -1 | +0 | 0.74 | platefill 8.8 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
