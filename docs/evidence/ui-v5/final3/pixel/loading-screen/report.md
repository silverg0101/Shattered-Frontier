# loading-screen: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| stag emblem with diamond frame | [834, 322, 247, 267] | - | +0 | +5 | +1 | +10 | 0.07 | - | FAIL |
| stag head | [860, 322, 200, 241] | - | -1 | -5 | -10 | +30 | 0.05 | stag ivory 11.4 | FAIL |
| zone title Silverbrook | [712, 582, 508, 89] | - | - | - | - | - | 0.04 | - | REVIEW |
| title divider with centre ornament | [712, 679, 493, 29] | - | +0 | -9 | +4 | -3 | 0.12 | - | FAIL |
| divider centre diamond | [943, 679, 35, 29] | - | +11 | +19 | -19 | -32 | 0.21 | - | FAIL |
| tip text | [719, 870, 485, 30] | - | +2 | -1 | +19 | -26 | 0.79 | - | FAIL |
| progress bar frame | [609, 915, 701, 35] | - | -1 | +6 | +19 | -10 | 0.08 | - | FAIL |
| progress bar left cap | [609, 915, 36, 35] | - | -1 | -3 | +16 | +1 | 0.08 | - | FAIL |
| progress bar right cap | [1274, 915, 36, 35] | - | -20 | +26 | +38 | -48 | 0.08 | - | FAIL |
| progress bar fill | [644, 925, 321, 16] | - | - | - | - | - | 0.00 | brass fill 12.4 | FAIL |
| progress bar empty track | [966, 925, 316, 16] | - | - | - | - | - | 0.00 | track fill 35.4 | FAIL |
| spinner ring | [939, 972, 42, 43] | - | - | - | - | - | 0.03 | - | REVIEW |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
