# loading-screen: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| stag emblem with diamond frame | [834, 322, 247, 267] | - | +0 | +3 | +1 | +16 | 0.06 | - | FAIL |
| stag head | [860, 322, 200, 241] | - | - | - | - | - | 0.03 | stag ivory 71.1 | FAIL |
| zone title Silverbrook | [712, 582, 508, 89] | - | - | - | - | - | 0.02 | - | REVIEW |
| title divider with centre ornament | [712, 679, 493, 29] | - | +0 | -18 | +4 | +38 | 0.05 | - | FAIL |
| divider centre diamond | [943, 679, 35, 29] | - | +14 | +9 | +2 | -5 | 0.11 | - | FAIL |
| tip text | [719, 870, 485, 30] | - | -5 | +1 | +34 | -25 | 0.75 | - | FAIL |
| progress bar frame | [609, 915, 701, 35] | - | +16 | +7 | -14 | -10 | 0.15 | - | FAIL |
| progress bar left cap | [609, 915, 36, 35] | - | +16 | -28 | -5 | +26 | 0.15 | - | FAIL |
| progress bar right cap | [1274, 915, 36, 35] | - | -7 | -2 | +9 | -8 | 0.06 | - | FAIL |
| progress bar fill | [644, 925, 321, 16] | - | - | - | - | - | 0.00 | brass fill 19.9 | FAIL |
| progress bar empty track | [966, 925, 316, 16] | - | - | - | - | - | 0.00 | track fill 25.2 | FAIL |
| spinner ring | [939, 972, 42, 43] | - | +1 | -1 | -3 | -3 | 0.17 | - | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
