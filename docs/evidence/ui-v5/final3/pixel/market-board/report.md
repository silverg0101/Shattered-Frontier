# market-board: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| modal frame | [246, 109, 1430, 803] | Border:Border_1 | +4 | -1 | -9 | +7 | 0.99 | modal fill 1.8, modal fill 3.0 | FAIL |
| emblem and title Market Board | [292, 132, 392, 76] | HorizontalBox:HorizontalBox_0 | +12 | -6 | -82 | -18 | 0.51 | - | FAIL |
| tab Browse (selected) | [286, 221, 207, 52] | SizeBox:SizeBox_2 | +18 | -27 | +0 | +0 | 0.28 | selected tab fill 5.2 | FAIL |
| tab My Listings | [502, 221, 208, 52] | SizeBox:SizeBox_3 | +21 | -27 | -1 | +0 | 0.28 | tab fill 1.3 | FAIL |
| tab Sell | [719, 221, 208, 52] | SizeBox:SizeBox_4 | +23 | -27 | -1 | +0 | 0.27 | tab fill 3.1 | FAIL |
| rail All Items (selected) | [279, 294, 222, 58] | SizeBox:SizeBox_9 | +8 | +25 | +0 | +0 | 0.38 | selected rail fill 10.2 | FAIL |
| rail Weapons | [279, 353, 222, 58] | SizeBox:SizeBox_11 | +8 | +24 | +0 | +0 | 0.39 | rail fill 3.8 | FAIL |
| rail Materials | [279, 412, 222, 58] | SizeBox:SizeBox_13 | +8 | +23 | +0 | +0 | 0.41 | rail fill 3.1 | FAIL |
| rail Consumables | [279, 471, 222, 58] | SizeBox:SizeBox_15 | +8 | +22 | +0 | +0 | 0.43 | - | FAIL |
| rail Tools | [279, 531, 222, 58] | - | - | - | - | - | 0.00 | - | REVIEW |
| search field | [522, 295, 720, 48] | HorizontalBox:HorizontalBox_8 | +16 | +24 | +0 | +8 | 0.29 | field fill 3.8 | FAIL |
| table header row | [522, 360, 720, 56] | HorizontalBox:HorizontalBox_23 | +16 | +19 | -8 | +14 | 0.41 | header fill 16.3 | FAIL |
| table row Iron Axe (selected) | [522, 418, 720, 70] | HorizontalBox:HorizontalBox_31 | +16 | +32 | -8 | +0 | 0.36 | selected row fill 10.2, selected row fill 11.0 | FAIL |
| table row Oak Planks | [522, 489, 720, 70] | HorizontalBox:HorizontalBox_39 | +16 | +32 | -8 | +0 | 0.36 | row fill 3.8 | FAIL |
| table row River Herbs | [522, 780, 720, 70] | - | - | - | - | - | 0.00 | row fill 2.0 | REVIEW |
| detail card frame | [1257, 290, 388, 592] | SizeBox:SizeBox_42 | +15 | -29 | +0 | +16 | 0.87 | card fill 2.9 | FAIL |
| detail item image well | [1283, 314, 338, 198] | SizeBox:SizeBox_31 | +14 | -33 | +0 | +0 | 0.67 | well fill 2.4 | FAIL |
| detail name Iron Axe | [1282, 526, 132, 36] | - | - | - | - | - | 0.00 | - | REVIEW |
| detail stat rows | [1283, 598, 340, 112] | HorizontalBox:HorizontalBox_20 | +14 | +1 | -2 | -81 | 0.26 | - | FAIL |
| Hold to Buy button | [1281, 780, 342, 78] | Overlay:Overlay_2 | +16 | -37 | -4 | -6 | 0.29 | brass fill 18.0 | FAIL |
| hotbar slot 1 (sword) | [716, 962, 77, 81] | - | - | - | - | - | 0.00 | slot fill 14.6 | FAIL |
| hotbar slot 6 (herbs) | [1127, 962, 76, 81] | - | - | - | - | - | 0.00 | slot fill 1.2 | REVIEW |
| vitals HP bar | [135, 970, 392, 32] | Overlay:Overlay_4 | +28 | -2 | -12 | +0 | 0.78 | hp red fill 26.0 | FAIL |
| vitals level badge | [29, 930, 106, 114] | Overlay:Overlay_0 | +9 | -51 | +9 | +44 | 0.52 | badge fill 6.3 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
