# market-board: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| modal frame | [246, 109, 1430, 803] | Border:Border_1 | +4 | -1 | -9 | +7 | 0.99 | modal fill 3.7, modal fill 3.9 | FAIL |
| emblem and title Market Board | [292, 132, 392, 76] | HorizontalBox:HorizontalBox_0 | +12 | +0 | -35 | -6 | 0.84 | - | FAIL |
| tab Browse (selected) | [286, 221, 207, 52] | SizeBox:SizeBox_2 | +0 | +12 | +0 | +0 | 0.62 | selected tab fill 5.2 | FAIL |
| tab My Listings | [502, 221, 208, 52] | SizeBox:SizeBox_3 | +3 | +12 | -1 | +0 | 0.61 | tab fill 1.6 | FAIL |
| tab Sell | [719, 221, 208, 52] | SizeBox:SizeBox_4 | +5 | +12 | -1 | +0 | 0.61 | tab fill 3.1 | FAIL |
| rail All Items (selected) | [279, 294, 222, 58] | SizeBox:SizeBox_7 | +0 | +12 | +0 | +0 | 0.66 | selected rail fill 5.4 | FAIL |
| rail Weapons | [279, 353, 222, 58] | SizeBox:SizeBox_9 | +0 | +11 | +0 | +0 | 0.68 | rail fill 5.2 | FAIL |
| rail Materials | [279, 412, 222, 58] | SizeBox:SizeBox_11 | +0 | +10 | +0 | +0 | 0.70 | rail fill 5.0 | FAIL |
| rail Consumables | [279, 471, 222, 58] | SizeBox:SizeBox_13 | +0 | +9 | +0 | +0 | 0.73 | - | FAIL |
| rail Tools | [279, 531, 222, 58] | SizeBox:SizeBox_15 | +0 | +7 | +0 | +0 | 0.78 | - | FAIL |
| search field | [522, 295, 720, 48] | SizeBox:SizeBox_18 | +0 | +11 | +8 | +0 | 0.62 | field fill 2.8 | FAIL |
| table header row | [522, 360, 720, 56] | SizeBox:SizeBox_30 | +8 | +4 | +0 | +0 | 0.85 | header fill 4.9 | FAIL |
| table row Iron Axe (selected) | [522, 418, 720, 70] | SizeBox:SizeBox_60 | +0 | +6 | +8 | +0 | 0.83 | selected row fill 5.9, selected row fill 5.4 | FAIL |
| table row Oak Planks | [522, 489, 720, 70] | SizeBox:SizeBox_73 | +0 | +6 | +8 | +0 | 0.83 | row fill 5.0 | FAIL |
| table row River Herbs | [522, 780, 720, 70] | HorizontalBox:HorizontalBox_63 | +8 | -1 | -8 | +0 | 0.96 | row fill 4.9 | FAIL |
| detail card frame | [1257, 290, 388, 592] | SizeBox:SizeBox_44 | +7 | +45 | +0 | +0 | 0.83 | card fill 4.9 | FAIL |
| detail item image well | [1283, 314, 338, 198] | SizeBox:SizeBox_32 | +6 | +45 | +0 | +0 | 0.61 | well fill 2.3 | FAIL |
| detail name Iron Axe | [1282, 526, 132, 36] | - | - | - | - | - | 0.00 | - | REVIEW |
| detail stat rows | [1283, 598, 340, 112] | - | - | - | - | - | 0.00 | - | REVIEW |
| Hold to Buy button | [1281, 780, 342, 78] | SizeBox:SizeBox_43 | +8 | +11 | -4 | -38 | 0.50 | brass fill 9.7 | FAIL |
| hotbar slot 1 (sword) | [716, 962, 77, 81] | - | - | - | - | - | 0.00 | slot fill 7.6 | FAIL |
| hotbar slot 6 (herbs) | [1127, 962, 76, 81] | - | - | - | - | - | 0.00 | slot fill 1.2 | REVIEW |
| vitals HP bar | [135, 970, 392, 32] | Overlay:Overlay_4 | +28 | +13 | -12 | +0 | 0.39 | hp red fill 26.0 | FAIL |
| vitals level badge | [29, 930, 106, 114] | Overlay:Overlay_0 | +9 | -36 | +9 | +44 | 0.57 | badge fill 4.5 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
