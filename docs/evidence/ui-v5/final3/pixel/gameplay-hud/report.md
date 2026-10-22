# gameplay-hud: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| target frame | [735, 32, 433, 74] | SizeBox:SizeBox_17 | +8 | +0 | +0 | +0 | 0.96 | fill dark 7.6, fill dark 4.6 | FAIL |
| target level cap diamond | [735, 32, 57, 55] | Overlay:Overlay_10 | +22 | +10 | +0 | +0 | 0.33 | fill dark 5.0 | FAIL |
| target HP bar | [762, 76, 403, 29] | Overlay:Overlay_9 | +10 | -2 | -28 | +5 | 0.80 | hp red fill 11.4 | FAIL |
| level badge hex | [37, 894, 115, 158] | Overlay:Overlay_0 | +1 | -15 | +0 | +0 | 0.81 | badge fill dark 4.1, badge fill dark 23.2 | FAIL |
| HP bar | [157, 925, 396, 44] | Overlay:Overlay_1 | +6 | +1 | -16 | -12 | 0.70 | hp red fill 8.0 | FAIL |
| stamina bar | [157, 972, 396, 42] | Overlay:Overlay_4 | +6 | -4 | -16 | -10 | 0.58 | stamina amber fill 14.1 | FAIL |
| prompt bar | [848, 838, 239, 60] | SizeBox:SizeBox_12 | +22 | +0 | -60 | +0 | 0.74 | fill dark 64.1 | FAIL |
| keycap F | [885, 846, 43, 43] | SizeBox:SizeBox_22 | +10 | +4 | -3 | -7 | 0.58 | fill dark 3.8 | FAIL |
| hotbar slot 1 (axe) | [657, 925, 95, 99] | SizeBox:SizeBox_1 | +9 | +13 | -7 | -11 | 0.76 | fill dark 6.0, fill dark 38.0 | FAIL |
| hotbar slot 3 (sword) | [857, 925, 95, 99] | SizeBox:SizeBox_5 | +9 | +13 | -7 | -11 | 0.76 | fill dark 32.8 | FAIL |
| hotbar slot 6 (campfire) | [1157, 925, 98, 99] | SizeBox:SizeBox_11 | +9 | +13 | -10 | -11 | 0.77 | fill dark 7.7 | FAIL |
| minimap ring | [1609, 17, 286, 287] | VerticalBox:VerticalBox_1 | +42 | +5 | -36 | -1 | 0.81 | player arrow white 37.0 | FAIL |
| minimap footer | [1650, 284, 203, 33] | SizeBox:SizeBox_20 | +36 | -8 | -23 | -1 | 0.46 | fill dark 5.5 | FAIL |
| quest tracker frame | [1612, 338, 286, 160] | SizeBox:SizeBox_0 | +1 | +0 | +0 | -43 | 0.73 | fill dark 9.6, fill dark 56.8 | FAIL |
| quest title Silverbrook | [1632, 350, 148, 29] | TextBlock:TextBlock_0 | +3 | +6 | +94 | +3 | 0.38 | - | FAIL |
| quest checkbox row 1 | [1631, 396, 22, 22] | - | - | - | - | - | 0.00 | fill dark 12.6 | FAIL |
| toast Item gained | [1612, 508, 286, 76] | VerticalBox:VerticalBox_0 | +1 | +0 | -6 | +2 | 0.95 | fill dark 8.5, fill dark 7.2 | FAIL |
| toast Skill gain | [1612, 589, 286, 76] | - | - | - | - | - | 0.00 | fill dark 66.5 | FAIL |
| toast Warning | [1612, 670, 286, 76] | - | - | - | - | - | 0.00 | fill dark 62.1 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
