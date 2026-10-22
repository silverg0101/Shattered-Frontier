# crafting-station: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| station window frame | [631, 97, 866, 797] | CanvasPanel:VitalsCanvas | -631 | -97 | +1054 | +283 | 0.33 | window fill 39.4, window fill 35.8 | FAIL |
| title Sawmill | [663, 117, 131, 33] | - | - | - | - | - | 0.00 | - | REVIEW |
| search recipes field | [655, 183, 301, 44] | - | - | - | - | - | 0.00 | field fill 28.4 | FAIL |
| filter tab All (selected) | [655, 243, 100, 38] | - | - | - | - | - | 0.00 | selected tab fill 37.5 | FAIL |
| filter tab Lumber | [760, 243, 96, 38] | - | - | - | - | - | 0.00 | tab fill 7.5 | FAIL |
| filter tab Parts | [862, 243, 94, 38] | - | - | - | - | - | 0.00 | tab fill 7.6 | FAIL |
| recipe row Oak Planks (selected) | [655, 297, 300, 64] | - | - | - | - | - | 0.00 | selected row fill 16.0 | FAIL |
| recipe row Timber Beam | [655, 367, 300, 64] | - | - | - | - | - | 0.00 | row fill 49.2 | FAIL |
| recipe row Wooden Pegs | [655, 437, 300, 64] | - | - | - | - | - | 0.00 | row fill 42.2 | FAIL |
| recipe row Treated Board | [655, 507, 300, 64] | - | - | - | - | - | 0.00 | row fill 90.0 | FAIL |
| detail title Oak Planks | [998, 187, 165, 33] | - | - | - | - | - | 0.00 | - | REVIEW |
| detail description | [1000, 228, 438, 45] | - | - | - | - | - | 0.00 | - | REVIEW |
| detail item image | [1100, 279, 264, 129] | - | - | - | - | - | 0.00 | image well fill 43.5 | FAIL |
| ingredients heading | [1000, 419, 98, 19] | - | - | - | - | - | 0.00 | - | REVIEW |
| ingredient row Oak Logs | [1000, 446, 470, 52] | - | - | - | - | - | 0.00 | row fill 41.3 | FAIL |
| ingredient row Iron Nails (short) | [1000, 504, 470, 52] | - | - | - | - | - | 0.00 | row fill 48.6 | FAIL |
| quantity stepper | [1131, 572, 184, 46] | - | - | - | - | - | 0.00 | field fill 47.1 | FAIL |
| craft time label | [1156, 626, 130, 22] | - | - | - | - | - | 0.00 | - | REVIEW |
| Hold Craft button | [1041, 655, 371, 60] | - | - | - | - | - | 0.00 | brass fill 17.5 | FAIL |
| crafting queue heading | [660, 737, 175, 22] | - | - | - | - | - | 0.00 | - | REVIEW |
| queue row Timber Beam 60% | [660, 765, 808, 54] | - | - | - | - | - | 0.00 | row fill 34.4 | FAIL |
| queue row Wooden Pegs 25% | [660, 824, 808, 55] | - | - | - | - | - | 0.00 | row fill 13.3 | FAIL |
| queue progress bar Timber Beam | [944, 787, 408, 12] | - | - | - | - | - | 0.00 | - | REVIEW |
| hotbar slot 1 (sword) | [677, 929, 89, 89] | SizeBox:SizeBox_1 | -11 | +20 | -1 | -1 | 0.52 | - | FAIL |
| hotbar slot 6 (bandage) | [1161, 929, 88, 89] | SizeBox:SizeBox_11 | +5 | +20 | +0 | -1 | 0.58 | slot fill 5.1 | FAIL |
| vitals HP bar | [136, 938, 365, 38] | SizeBox:SizeBox_1 | +28 | +4 | +15 | -6 | 0.71 | hp red fill 10.9 | FAIL |
| vitals level badge | [27, 903, 100, 130] | Overlay:Overlay_0 | +11 | +18 | +16 | -14 | 0.60 | badge fill 20.3 | FAIL |
| minimap ring | [1674, 12, 217, 220] | Image:Image_9 | -13 | +20 | +13 | +10 | 0.76 | - | FAIL |
| minimap footer text | [1732, 237, 100, 43] | - | - | - | - | - | 0.00 | - | REVIEW |
| quest tracker frame | [1636, 308, 264, 198] | SizeBox:SizeBox_0 | -23 | +30 | +16 | -68 | 0.60 | - | FAIL |
| toast Oak Logs | [1646, 541, 254, 62] | - | - | - | - | - | 0.00 | fill dark 33.1 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
