# market-board: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| modal frame | [246, 109, 1430, 803] | Border:Border_1 | +4 | -1 | -9 | +7 | 0.99 | modal fill 1.8, modal fill 2.9 | FAIL |
| emblem and title Market Board | [292, 132, 392, 76] | HorizontalBox:HorizontalBox_0 | +12 | -6 | +58 | +9 | 0.74 | - | FAIL |
| tab Browse (selected) | [286, 221, 207, 52] | SizeBox:SizeBox_2 | +18 | +0 | -17 | -8 | 0.77 | selected tab fill 8.9 | FAIL |
| tab My Listings | [502, 221, 208, 52] | SizeBox:SizeBox_3 | +4 | +0 | -18 | -8 | 0.77 | tab fill 1.3 | FAIL |
| tab Sell | [719, 221, 208, 52] | SizeBox:SizeBox_4 | -11 | +0 | -18 | -8 | 0.69 | tab fill 3.1 | FAIL |
| rail All Items (selected) | [279, 294, 222, 58] | SizeBox:SizeBox_7 | +25 | -14 | -32 | -10 | 0.42 | selected rail fill 7.4 | FAIL |
| rail Weapons | [279, 353, 222, 58] | SizeBox:SizeBox_9 | +25 | -17 | -32 | -10 | 0.37 | rail fill 3.6 | FAIL |
| rail Materials | [279, 412, 222, 58] | SizeBox:SizeBox_11 | +25 | -20 | -32 | -10 | 0.32 | rail fill 3.5 | FAIL |
| rail Consumables | [279, 471, 222, 58] | SizeBox:SizeBox_13 | +25 | -23 | -32 | -10 | 0.28 | - | FAIL |
| rail Tools | [279, 531, 222, 58] | - | - | - | - | - | 0.00 | - | REVIEW |
| search field | [522, 295, 720, 48] | SizeBox:SizeBox_17 | -8 | -15 | +8 | -4 | 0.46 | field fill 2.9 | FAIL |
| table header row | [522, 360, 720, 56] | SizeBox:SizeBox_41 | -8 | +10 | +8 | +0 | 0.69 | header fill 23.5 | FAIL |
| table row Iron Axe (selected) | [522, 418, 720, 70] | SizeBox:SizeBox_49 | -8 | +9 | +8 | -14 | 0.79 | selected row fill 8.3, selected row fill 10.5 | FAIL |
| table row Oak Planks | [522, 489, 720, 70] | SizeBox:SizeBox_57 | -8 | -5 | +8 | -14 | 0.67 | row fill 10.4 | FAIL |
| table row River Herbs | [522, 780, 720, 70] | - | - | - | - | - | 0.00 | row fill 4.5 | FAIL |
| detail card frame | [1257, 290, 388, 592] | SizeBox:SizeBox_30 | +9 | -10 | -48 | -84 | 0.73 | card fill 2.4 | FAIL |
| detail item image well | [1283, 314, 338, 198] | SizeBox:SizeBox_25 | +7 | -21 | -46 | -48 | 0.52 | well fill 2.4 | FAIL |
| detail name Iron Axe | [1282, 526, 132, 36] | - | - | - | - | - | 0.00 | - | REVIEW |
| detail stat rows | [1283, 598, 340, 112] | HorizontalBox:HorizontalBox_17 | +7 | +35 | -48 | -79 | 0.25 | - | FAIL |
| Hold to Buy button | [1281, 780, 342, 78] | - | - | - | - | - | 0.00 | brass fill 21.4 | FAIL |
| hotbar slot 1 (sword) | [716, 962, 77, 81] | SizeBox:SizeBox_1 | -50 | -13 | +11 | +7 | 0.26 | slot fill 1.4 | FAIL |
| hotbar slot 6 (herbs) | [1127, 962, 76, 81] | - | - | - | - | - | 0.00 | slot fill 1.4 | REVIEW |
| vitals HP bar | [135, 970, 392, 32] | VerticalBox:VerticalBox_0 | +29 | -38 | -12 | +62 | 0.32 | hp red fill 5.2 | FAIL |
| vitals level badge | [29, 930, 106, 114] | Overlay:Overlay_0 | +9 | -9 | +10 | +2 | 0.68 | badge fill 5.7 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
