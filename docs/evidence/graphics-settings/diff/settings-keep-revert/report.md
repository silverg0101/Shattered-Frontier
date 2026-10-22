# settings-keep-revert: pixel diff, FAIL

Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (match < 0.5).

| element | concept rect | dx | dy | dw | dh | match | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| modal frame | [548, 379, 824, 341] | +0 | -7 | -1 | -5 | 0.50 | panelfill 29.4, panelfill 6.4 | FAIL |
| modal title | [700, 440, 526, 48] | -13 | -2 | +19 | -2 | 0.81 | - | FAIL |
| modal detail | [762, 503, 395, 30] | +3 | +5 | -7 | +1 | 0.72 | - | FAIL |
| modal countdown | [837, 545, 246, 30] | +0 | +9 | +0 | +0 | 0.76 | - | FAIL |
| modal progress bar | [630, 586, 659, 18] | - | - | - | - | 0.47 | - | FAIL |
| button Keep Changes | [597, 629, 350, 67] | - | - | - | - | 0.49 | - | FAIL |
| button Revert | [976, 629, 347, 67] | +6 | +11 | +1 | +0 | 0.68 | - | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
