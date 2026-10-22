# gathering: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| skills panel frame | [50, 64, 527, 672] | Overlay:Overlay_0 | -12 | +12 | -47 | +84 | 0.76 | panel fill 78.7, panel fill 7.4 | FAIL |
| title Gathering Skills | [87, 90, 273, 38] | TextBlock:TextBlock_0 | -13 | -6 | +149 | +26 | 0.38 | - | FAIL |
| skill card Woodcutting | [66, 151, 494, 143] | SizeBox:SizeBox_1 | -6 | +5 | -58 | +1 | 0.81 | card fill 11.3 | FAIL |
| skill card Foraging | [66, 438, 494, 132] | SizeBox:SizeBox_17 | -6 | +22 | -58 | +12 | 0.59 | card fill 7.5 | FAIL |
| skill card Mining | [66, 295, 494, 133] | SizeBox:SizeBox_9 | -6 | +13 | -58 | +11 | 0.68 | card fill 23.2 | FAIL |
| skill card Fishing | [66, 581, 494, 140] | SizeBox:SizeBox_25 | -6 | +31 | -58 | +4 | 0.55 | card fill 7.7 | FAIL |
| Woodcutting level well | [77, 165, 100, 110] | VerticalBox:VerticalBox_1 | +3 | +0 | -28 | +15 | 0.65 | well fill 36.4 | FAIL |
| Woodcutting XP bar | [198, 208, 336, 18] | - | - | - | - | - | 0.00 | xp green fill 39.1 | FAIL |
| Mining XP bar | [198, 351, 336, 18] | - | - | - | - | - | 0.00 | xp green fill 40.7 | FAIL |
| target frame Old Oak | [790, 32, 340, 92] | - | - | - | - | - | 0.00 | frame fill 6.2, frame fill 9.4 | FAIL |
| target name Old Oak | [915, 40, 89, 23] | - | - | - | - | - | 0.00 | - | REVIEW |
| target HP bar | [790, 71, 340, 18] | - | - | - | - | - | 0.00 | hp red fill 34.1 | FAIL |
| target HP value | [930, 99, 61, 17] | - | - | - | - | - | 0.00 | - | REVIEW |
| progress ring 64% | [989, 366, 110, 110] | - | - | - | - | - | 0.00 | ring centre fill 4.8 | FAIL |
| action label Woodcutting | [969, 484, 148, 33] | - | - | - | - | - | 0.00 | label plate fill 3.6 | FAIL |
| prompt bar Chop | [869, 843, 183, 51] | - | - | - | - | - | 0.00 | fill dark 8.8 | FAIL |
| keycap E | [911, 850, 35, 34] | - | - | - | - | - | 0.00 | - | REVIEW |
| hotbar frame | [649, 905, 621, 115] | HorizontalBox:HorizontalBox_0 | +11 | +44 | -21 | -27 | 0.52 | - | FAIL |
| hotbar slot 1 (selected axe) | [660, 913, 93, 101] | SizeBox:SizeBox_1 | +6 | +36 | -5 | -13 | 0.49 | selected slot fill 9.7 | FAIL |
| hotbar slot 6 (bandage) | [1166, 913, 95, 101] | SizeBox:SizeBox_11 | +0 | +36 | -7 | -13 | 0.49 | slot fill 8.7 | FAIL |
| vitals HP bar | [151, 943, 393, 33] | SizeBox:SizeBox_1 | +13 | -1 | -13 | -1 | 0.87 | hp red fill 10.8 | FAIL |
| vitals level badge | [33, 902, 108, 141] | Overlay:Overlay_0 | +5 | +19 | +8 | -25 | 0.71 | badge fill 34.3 | FAIL |
| minimap ring | [1654, 17, 241, 240] | SizeBox:SizeBox_17 | -3 | +5 | +9 | +10 | 0.89 | - | FAIL |
| minimap footer text | [1697, 263, 158, 20] | - | - | - | - | - | 0.00 | - | REVIEW |
| quest tracker frame | [1618, 304, 286, 203] | SizeBox:SizeBox_0 | -5 | +34 | -6 | -73 | 0.61 | fill dark 4.9 | FAIL |
| toast Skill gain | [1618, 523, 286, 77] | - | - | - | - | - | 0.00 | fill dark 6.7 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
