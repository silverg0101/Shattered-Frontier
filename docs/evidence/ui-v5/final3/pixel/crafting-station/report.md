# crafting-station: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| station window frame | [631, 97, 866, 797] | SFStationPanel:SFStationPanel_0 | +0 | +0 | +0 | +0 | 1.00 | window fill 4.0, window fill 1.1 | FAIL |
| title Sawmill | [663, 117, 131, 33] | - | - | - | - | - | 0.00 | - | REVIEW |
| search recipes field | [655, 183, 301, 44] | - | - | - | - | - | 0.00 | field fill 3.2 | FAIL |
| filter tab All (selected) | [655, 243, 100, 38] | - | - | - | - | - | 0.00 | selected tab fill 12.2 | FAIL |
| filter tab Lumber | [760, 243, 96, 38] | - | - | - | - | - | 0.00 | tab fill 9.7 | FAIL |
| filter tab Parts | [862, 243, 94, 38] | - | - | - | - | - | 0.00 | tab fill 12.5 | FAIL |
| recipe row Oak Planks (selected) | [655, 297, 300, 64] | - | - | - | - | - | 0.00 | selected row fill 4.9 | FAIL |
| recipe row Timber Beam | [655, 367, 300, 64] | - | - | - | - | - | 0.00 | row fill 4.5 | FAIL |
| recipe row Wooden Pegs | [655, 437, 300, 64] | - | - | - | - | - | 0.00 | row fill 3.7 | FAIL |
| recipe row Treated Board | [655, 507, 300, 64] | - | - | - | - | - | 0.00 | row fill 6.0 | FAIL |
| detail title Oak Planks | [998, 187, 165, 33] | - | - | - | - | - | 0.00 | - | REVIEW |
| detail description | [1000, 228, 438, 45] | - | - | - | - | - | 0.00 | - | REVIEW |
| detail item image | [1100, 279, 264, 129] | - | - | - | - | - | 0.00 | image well fill 3.6 | FAIL |
| ingredients heading | [1000, 419, 98, 19] | - | - | - | - | - | 0.00 | - | REVIEW |
| ingredient row Oak Logs | [1000, 446, 470, 52] | - | - | - | - | - | 0.00 | row fill 4.7 | FAIL |
| ingredient row Iron Nails (short) | [1000, 504, 470, 52] | - | - | - | - | - | 0.00 | row fill 13.4 | FAIL |
| quantity stepper | [1131, 572, 184, 46] | - | - | - | - | - | 0.00 | field fill 4.4 | FAIL |
| craft time label | [1156, 626, 130, 22] | - | - | - | - | - | 0.00 | - | REVIEW |
| Hold Craft button | [1041, 655, 371, 60] | - | - | - | - | - | 0.00 | brass fill 25.0 | FAIL |
| crafting queue heading | [660, 737, 175, 22] | - | - | - | - | - | 0.00 | - | REVIEW |
| queue row Timber Beam 60% | [660, 765, 808, 54] | - | - | - | - | - | 0.00 | row fill 3.2 | FAIL |
| queue row Wooden Pegs 25% | [660, 824, 808, 55] | - | - | - | - | - | 0.00 | row fill 3.6 | FAIL |
| queue progress bar Timber Beam | [944, 787, 408, 12] | - | - | - | - | - | 0.00 | - | REVIEW |
| hotbar slot 1 (sword) | [677, 929, 89, 89] | SizeBox:SizeBox_1 | -11 | +9 | -1 | -1 | 0.65 | - | FAIL |
| hotbar slot 6 (bandage) | [1161, 929, 88, 89] | SizeBox:SizeBox_11 | +5 | +9 | +0 | -1 | 0.74 | slot fill 4.0 | FAIL |
| vitals HP bar | [136, 938, 365, 38] | Overlay:Overlay_1 | +27 | +3 | +15 | -6 | 0.71 | hp red fill 10.9 | FAIL |
| vitals level badge | [27, 903, 100, 130] | Overlay:Overlay_0 | +11 | -9 | +15 | +28 | 0.59 | badge fill 4.1 | FAIL |
| minimap ring | [1674, 12, 217, 220] | Image:Image_10 | -13 | +20 | +13 | +10 | 0.76 | - | FAIL |
| minimap footer text | [1732, 237, 100, 43] | - | - | - | - | - | 0.00 | - | REVIEW |
| quest tracker frame | [1636, 308, 264, 198] | SizeBox:SizeBox_0 | -23 | +30 | +22 | -81 | 0.56 | - | FAIL |
| toast Oak Logs | [1646, 541, 254, 62] | - | - | - | - | - | 0.00 | fill dark 39.6 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
