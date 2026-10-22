# gameplay-hud: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| target frame | [735, 32, 433, 74] | - | - | - | - | - | 0.00 | fill dark 30.1, fill dark 33.7 | FAIL |
| target level cap diamond | [735, 32, 57, 55] | - | - | - | - | - | 0.00 | fill dark 27.3 | FAIL |
| target HP bar | [762, 76, 403, 29] | - | - | - | - | - | 0.00 | hp red fill 33.9 | FAIL |
| level badge hex | [37, 894, 115, 158] | Overlay:Overlay_0 | +1 | +0 | +0 | +0 | 0.97 | badge fill dark 2.8, badge fill dark 41.2 | FAIL |
| HP bar | [157, 925, 396, 44] | Overlay:Overlay_1 | +6 | +16 | -16 | -12 | 0.56 | hp red fill 8.0 | FAIL |
| stamina bar | [157, 972, 396, 42] | Overlay:Overlay_4 | +6 | +11 | -16 | -10 | 0.70 | stamina amber fill 14.1 | FAIL |
| prompt bar | [848, 838, 239, 60] | SizeBox:SizeBox_12 | +22 | +0 | -60 | +0 | 0.74 | fill dark 5.6 | FAIL |
| keycap F | [885, 846, 43, 43] | SizeBox:SizeBox_21 | +10 | +4 | -3 | -7 | 0.58 | fill dark 3.8 | FAIL |
| hotbar slot 1 (axe) | [657, 925, 95, 99] | SizeBox:SizeBox_1 | +9 | +13 | -7 | -11 | 0.76 | fill dark 6.0, fill dark 38.0 | FAIL |
| hotbar slot 3 (sword) | [857, 925, 95, 99] | SizeBox:SizeBox_5 | +9 | +13 | -7 | -11 | 0.76 | fill dark 32.8 | FAIL |
| hotbar slot 6 (campfire) | [1157, 925, 98, 99] | SizeBox:SizeBox_11 | +9 | +13 | -10 | -11 | 0.77 | fill dark 7.7 | FAIL |
| minimap ring | [1609, 17, 286, 287] | VerticalBox:VerticalBox_1 | +42 | +5 | -36 | -1 | 0.81 | player arrow white 37.0 | FAIL |
| minimap footer | [1650, 284, 203, 33] | SizeBox:SizeBox_20 | +36 | -8 | -23 | -1 | 0.46 | fill dark 5.4 | FAIL |
| quest tracker frame | [1612, 338, 286, 160] | SizeBox:SizeBox_0 | +1 | +0 | +0 | -43 | 0.73 | fill dark 9.7, fill dark 56.6 | FAIL |
| quest title Silverbrook | [1632, 350, 148, 29] | TextBlock:TextBlock_0 | +3 | +12 | +28 | -11 | 0.38 | - | FAIL |
| quest checkbox row 1 | [1631, 396, 22, 22] | - | - | - | - | - | 0.00 | fill dark 12.9 | FAIL |
| toast Item gained | [1612, 508, 286, 76] | VerticalBox:VerticalBox_0 | +1 | +0 | -6 | +2 | 0.95 | fill dark 6.8, fill dark 7.4 | FAIL |
| toast Skill gain | [1612, 589, 286, 76] | - | - | - | - | - | 0.00 | fill dark 8.3 | FAIL |
| toast Warning | [1612, 670, 286, 76] | - | - | - | - | - | 0.00 | fill dark 38.8 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
