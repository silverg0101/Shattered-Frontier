# gameplay-hud: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| target frame | [735, 32, 433, 74] | SizeBox:SizeBox_16 | +20 | +11 | -23 | -4 | 0.74 | fill dark 7.7, fill dark 43.4 | FAIL |
| target level cap diamond | [735, 32, 57, 55] | - | - | - | - | - | 0.00 | fill dark 25.9 | FAIL |
| target HP bar | [762, 76, 403, 29] | VerticalBox:VerticalBox_0 | +15 | -27 | -37 | +29 | 0.48 | hp red fill 32.2 | FAIL |
| level badge hex | [37, 894, 115, 158] | Overlay:Overlay_0 | +1 | +27 | +1 | -42 | 0.71 | badge fill dark 2.8, badge fill dark 25.5 | FAIL |
| HP bar | [157, 925, 396, 44] | SizeBox:SizeBox_1 | +7 | +17 | -16 | -12 | 0.54 | hp red fill 8.0 | FAIL |
| stamina bar | [157, 972, 396, 42] | SizeBox:SizeBox_4 | +7 | +17 | -16 | -10 | 0.50 | stamina amber fill 14.1 | FAIL |
| prompt bar | [848, 838, 239, 60] | SizeBox:SizeBox_12 | +20 | +28 | -54 | -8 | 0.33 | fill dark 5.1 | FAIL |
| keycap F | [885, 846, 43, 43] | - | - | - | - | - | 0.00 | fill dark 4.7 | FAIL |
| hotbar slot 1 (axe) | [657, 925, 95, 99] | SizeBox:SizeBox_1 | +9 | +24 | -7 | -11 | 0.61 | fill dark 5.8, fill dark 11.4 | FAIL |
| hotbar slot 3 (sword) | [857, 925, 95, 99] | SizeBox:SizeBox_5 | +9 | +24 | -7 | -11 | 0.61 | fill dark 2.3 | FAIL |
| hotbar slot 6 (campfire) | [1157, 925, 98, 99] | SizeBox:SizeBox_11 | +9 | +24 | -10 | -11 | 0.61 | fill dark 6.4 | FAIL |
| minimap ring | [1609, 17, 286, 287] | VerticalBox:VerticalBox_1 | +42 | +5 | -36 | -1 | 0.81 | player arrow white 37.0 | FAIL |
| minimap footer | [1650, 284, 203, 33] | SizeBox:SizeBox_19 | +36 | -8 | -23 | -1 | 0.46 | fill dark 5.3 | FAIL |
| quest tracker frame | [1612, 338, 286, 160] | SizeBox:SizeBox_0 | +1 | +0 | -6 | -30 | 0.80 | fill dark 9.2, fill dark 32.1 | FAIL |
| quest title Silverbrook | [1632, 350, 148, 29] | TextBlock:TextBlock_0 | -3 | +4 | +100 | +14 | 0.33 | - | FAIL |
| quest checkbox row 1 | [1631, 396, 22, 22] | - | - | - | - | - | 0.00 | fill dark 10.2 | FAIL |
| toast Item gained | [1612, 508, 286, 76] | VerticalBox:VerticalBox_0 | +1 | -36 | -6 | +2 | 0.37 | fill dark 6.0, fill dark 6.1 | FAIL |
| toast Skill gain | [1612, 589, 286, 76] | - | - | - | - | - | 0.00 | fill dark 28.4 | FAIL |
| toast Warning | [1612, 670, 286, 76] | - | - | - | - | - | 0.00 | fill dark 29.4 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
