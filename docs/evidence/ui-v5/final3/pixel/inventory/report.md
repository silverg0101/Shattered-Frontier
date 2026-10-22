# inventory: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| inventory panel frame | [85, 31, 575, 887] | Border:Border_0 | +0 | +0 | +0 | +0 | 1.00 | panel fill 5.5, panel fill 5.6 | FAIL |
| title Inventory | [103, 50, 204, 48] | TextBlock:TextBlock_0 | +16 | +15 | -85 | -7 | 0.37 | - | FAIL |
| equip slot head | [134, 127, 78, 78] | SizeBox:SizeBox_2 | +16 | +8 | +0 | +0 | 0.55 | slot fill 3.0 | FAIL |
| equip slot chest | [134, 215, 78, 78] | SizeBox:SizeBox_3 | +16 | +8 | +0 | +0 | 0.55 | slot fill 2.9 | FAIL |
| equip slot boots | [134, 463, 78, 56] | - | - | - | - | - | 0.00 | slot fill 5.6 | FAIL |
| equip slot main hand (selected) | [490, 128, 80, 80] | SizeBox:SizeBox_8 | +16 | +7 | -2 | -2 | 0.60 | selected slot fill 3.2 | FAIL |
| equip slot shield | [490, 217, 80, 80] | SizeBox:SizeBox_9 | +16 | +6 | -2 | -2 | 0.61 | slot fill 12.6 | FAIL |
| equip slot ring | [490, 397, 80, 80] | SizeBox:SizeBox_11 | +16 | +2 | -2 | -2 | 0.67 | slot fill 12.8 | FAIL |
| paperdoll adventurer | [272, 118, 158, 390] | SizeBox:SizeBox_7 | +16 | +8 | +0 | +0 | 0.79 | backdrop fill 4.4 | FAIL |
| bag slot 1 Oak Log | [108, 527, 62, 61] | SizeBox:SizeBox_12 | +16 | +8 | +0 | +1 | 0.47 | slot fill 7.0 | FAIL |
| bag slot 2 Stone | [174, 527, 62, 61] | SizeBox:SizeBox_13 | +16 | +8 | +0 | +1 | 0.47 | slot fill 7.5 | FAIL |
| bag grid 8x5 | [108, 527, 527, 327] | Overlay:Overlay_49 | +14 | +6 | +1 | +3 | 0.91 | - | FAIL |
| bag slot last (stick) | [573, 793, 62, 61] | SizeBox:SizeBox_51 | +13 | +6 | +0 | +1 | 0.55 | slot fill 7.2 | FAIL |
| weight footer text | [121, 876, 156, 20] | TextBlock:TextBlock_82 | +16 | +8 | -12 | +7 | 0.32 | - | FAIL |
| gold footer | [503, 873, 116, 22] | - | - | - | - | - | 0.00 | - | REVIEW |
| tooltip frame Iron Axe | [587, 120, 410, 326] | - | - | - | - | - | 0.00 | tooltip fill 36.5, tooltip fill 48.7 | FAIL |
| tooltip title Iron Axe | [742, 141, 103, 27] | - | - | - | - | - | 0.00 | - | REVIEW |
| tooltip item art | [602, 140, 120, 178] | - | - | - | - | - | 0.00 | - | REVIEW |
| tooltip stat rows | [742, 213, 220, 107] | - | - | - | - | - | 0.00 | - | REVIEW |
| tooltip description | [614, 353, 256, 67] | - | - | - | - | - | 0.00 | - | REVIEW |
| minimap ring | [1651, 22, 243, 248] | SizeBox:SizeBox_18 | +0 | +0 | +7 | +2 | 0.96 | - | FAIL |
| quest tracker frame | [1626, 315, 275, 273] | - | - | - | - | - | 0.00 | fill dark 22.1 | FAIL |
| toast River Herbs | [1627, 603, 274, 67] | - | - | - | - | - | 0.00 | fill dark 35.5 | FAIL |
| hotbar slot 1 (selected axe) | [670, 946, 92, 89] | SizeBox:SizeBox_1 | -4 | -8 | -4 | -1 | 0.73 | slot fill 4.7 | FAIL |
| hotbar slot 6 (campfire) | [1159, 946, 89, 89] | SizeBox:SizeBox_11 | +7 | -8 | -1 | -1 | 0.72 | slot fill 3.9 | FAIL |
| vitals HP bar | [134, 950, 392, 36] | VerticalBox:VerticalBox_0 | +29 | -34 | -12 | +48 | 0.40 | hp red fill 14.9 | FAIL |
| vitals level badge | [32, 940, 116, 110] | Overlay:Overlay_0 | +6 | -61 | -1 | +48 | 0.52 | badge fill 6.3 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
