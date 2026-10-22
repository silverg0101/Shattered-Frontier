# settings-keep-revert: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| modal frame | [548, 379, 824, 341] | SizeBox:SizeBox_149 | +0 | +0 | +0 | +0 | 1.00 | panelfill 3.0, panelfill 3.0 | PASS |
| modal title | [701, 444, 524, 46] | TextBlock:TextBlock_63 | -2 | -2 | -3 | +0 | 0.74 | - | FAIL |
| modal detail | [764, 506, 393, 24] | TextBlock:TextBlock_64 | +2 | -2 | -5 | +1 | 0.33 | - | FAIL |
| modal countdown | [838, 549, 245, 25] | - | - | - | - | - | 0.00 | - | REVIEW |
| modal progress bar | [630, 586, 659, 18] | SizeBox:SizeBox_146 | +0 | +2 | +0 | +0 | 0.85 | - | PASS |
| button Keep Changes | [597, 629, 350, 67] | SizeBox:SizeBox_147 | +0 | -2 | +0 | +0 | 0.95 | - | PASS |
| button Revert | [976, 629, 347, 67] | SizeBox:SizeBox_148 | +0 | -2 | +0 | +0 | 0.95 | - | PASS |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
