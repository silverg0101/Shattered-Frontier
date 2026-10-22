# inventory: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| inventory panel frame | [85, 31, 575, 887] | Border:Border_0 | -8 | +1 | +1 | -1 | 0.97 | panel fill 7.5, panel fill 2.3 | FAIL |
| title Inventory | [103, 50, 204, 48] | TextBlock:TextBlock_0 | +52 | -2 | -4 | +16 | 0.48 | - | FAIL |
| equip slot head | [134, 127, 78, 78] | SizeBox:SizeBox_2 | -27 | -15 | +22 | -34 | 0.26 | slot fill 24.3 | FAIL |
| equip slot chest | [134, 215, 78, 78] | SizeBox:SizeBox_7 | -27 | +25 | -6 | -6 | 0.26 | slot fill 2.7 | FAIL |
| equip slot boots | [134, 463, 78, 56] | SizeBox:SizeBox_10 | -27 | +11 | -6 | +16 | 0.26 | slot fill 2.5 | FAIL |
| equip slot main hand (selected) | [490, 128, 80, 80] | - | - | - | - | - | 0.00 | selected slot fill 2.1 | REVIEW |
| equip slot shield | [490, 217, 80, 80] | - | - | - | - | - | 0.00 | slot fill 4.0 | FAIL |
| equip slot ring | [490, 397, 80, 80] | - | - | - | - | - | 0.00 | slot fill 11.1 | FAIL |
| paperdoll adventurer | [272, 118, 158, 390] | SizeBox:SizeBox_11 | -17 | +49 | +62 | -10 | 0.59 | backdrop fill 8.2 | FAIL |
| bag slot 1 Oak Log | [108, 527, 62, 61] | SizeBox:SizeBox_16 | +0 | +30 | +0 | +3 | 0.32 | slot fill 3.2 | FAIL |
| bag slot 2 Stone | [174, 527, 62, 61] | SizeBox:SizeBox_17 | -2 | +30 | +0 | +3 | 0.31 | slot fill 2.8 | FAIL |
| bag grid 8x5 | [108, 527, 527, 327] | Overlay:Overlay_49 | -1 | +29 | -11 | +3 | 0.81 | - | FAIL |
| bag slot last (stick) | [573, 793, 62, 61] | SizeBox:SizeBox_55 | -14 | +28 | +0 | +3 | 0.26 | slot fill 3.1 | FAIL |
| weight footer text | [121, 876, 156, 20] | - | - | - | - | - | 0.00 | - | REVIEW |
| gold footer | [503, 873, 116, 22] | - | - | - | - | - | 0.00 | - | REVIEW |
| tooltip frame Iron Axe | [587, 120, 410, 326] | - | - | - | - | - | 0.00 | tooltip fill 36.7, tooltip fill 51.6 | FAIL |
| tooltip title Iron Axe | [742, 141, 103, 27] | - | - | - | - | - | 0.00 | - | REVIEW |
| tooltip item art | [602, 140, 120, 178] | - | - | - | - | - | 0.00 | - | REVIEW |
| tooltip stat rows | [742, 213, 220, 107] | - | - | - | - | - | 0.00 | - | REVIEW |
| tooltip description | [614, 353, 256, 67] | - | - | - | - | - | 0.00 | - | REVIEW |
| minimap ring | [1651, 22, 243, 248] | SizeBox:SizeBox_17 | +0 | +0 | +7 | +2 | 0.96 | - | FAIL |
| quest tracker frame | [1626, 315, 275, 273] | SizeBox:SizeBox_0 | -13 | +23 | +5 | -143 | 0.45 | fill dark 4.4 | FAIL |
| toast River Herbs | [1627, 603, 274, 67] | - | - | - | - | - | 0.00 | fill dark 33.7 | FAIL |
| hotbar slot 1 (selected axe) | [670, 946, 92, 89] | SizeBox:SizeBox_1 | -4 | +3 | -4 | -1 | 0.83 | slot fill 4.7 | FAIL |
| hotbar slot 6 (campfire) | [1159, 946, 89, 89] | SizeBox:SizeBox_11 | +7 | +3 | -1 | -1 | 0.82 | slot fill 3.3 | FAIL |
| vitals HP bar | [134, 950, 392, 36] | Overlay:Overlay_1 | +30 | -8 | -12 | +1 | 0.59 | hp red fill 8.3 | FAIL |
| vitals level badge | [32, 940, 116, 110] | Overlay:Overlay_0 | +6 | -19 | +0 | +6 | 0.68 | badge fill 6.4 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
