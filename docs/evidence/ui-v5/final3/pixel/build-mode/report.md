# build-mode: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| catalogue panel frame | [33, 39, 523, 845] | SizeBox:SizeBox_0 | +5 | -7 | -43 | +13 | 0.91 | panel fill 41.2, panel fill 4.0 | FAIL |
| title Build Catalogue | [62, 52, 298, 40] | TextBlock:TextBlock_0 | +38 | -6 | +27 | +24 | 0.46 | - | FAIL |
| search field | [53, 111, 480, 41] | Overlay:Overlay_0 | +7 | +17 | -44 | -1 | 0.38 | field fill 2.2 | FAIL |
| tab Voxel Objects (selected) | [52, 161, 259, 41] | SizeBox:SizeBox_5 | +8 | +17 | -49 | +3 | 0.33 | selected tab fill 9.9, selected tab fill 12.0 | FAIL |
| tab Blueprints | [322, 161, 214, 41] | SizeBox:SizeBox_6 | -40 | +17 | -4 | +3 | 0.29 | tab fill 3.1 | FAIL |
| rail Structures (selected) | [43, 215, 95, 78] | SizeBox:SizeBox_8 | +17 | +19 | -15 | +2 | 0.49 | selected rail fill 12.3 | FAIL |
| rail Decor | [43, 305, 95, 83] | SizeBox:SizeBox_10 | +17 | +21 | -15 | -3 | 0.50 | rail fill 3.2 | FAIL |
| rail Terrain | [43, 398, 95, 83] | SizeBox:SizeBox_12 | +17 | +20 | -15 | -3 | 0.52 | rail fill 3.2 | FAIL |
| rail Favourites | [43, 490, 95, 83] | SizeBox:SizeBox_14 | +17 | +20 | -15 | -3 | 0.52 | rail fill 3.1 | FAIL |
| tile Timber Block | [155, 213, 123, 111] | SizeBox:SizeBox_16 | +3 | +27 | -23 | -11 | 0.55 | tile fill 3.1 | FAIL |
| tile Wall (selected) | [155, 327, 123, 106] | SizeBox:SizeBox_22 | +3 | +25 | -23 | -6 | 0.54 | selected tile fill 12.4 | FAIL |
| tile Floor | [285, 327, 121, 106] | SizeBox:SizeBox_23 | -15 | +25 | -21 | -6 | 0.43 | tile fill 3.9 | FAIL |
| tile Window | [413, 555, 123, 107] | SizeBox:SizeBox_30 | -31 | +21 | -23 | -7 | 0.34 | tile fill 2.2 | FAIL |
| material dropdown | [43, 673, 497, 50] | SizeBox:SizeBox_33 | +17 | +21 | -61 | -10 | 0.42 | field fill 2.9 | FAIL |
| required materials box | [43, 729, 497, 148] | Border:Border_21 | +17 | +15 | -61 | -16 | 0.78 | inset fill 2.0 | FAIL |
| action bar | [722, 882, 493, 38] | HorizontalBox:HorizontalBox_1 | -28 | -2 | +38 | -6 | 0.71 | bar fill 3.5 | FAIL |
| hotbar slot 1 (axe) | [688, 938, 92, 92] | SizeBox:SizeBox_1 | -22 | +11 | -4 | -4 | 0.49 | slot fill 10.6 | FAIL |
| hotbar slot 6 (sack) | [1154, 938, 92, 92] | SizeBox:SizeBox_11 | +12 | +11 | -4 | -4 | 0.67 | slot fill 10.7 | FAIL |
| vitals HP bar | [123, 948, 367, 34] | Overlay:Overlay_1 | +41 | -6 | +13 | +3 | 0.61 | hp red fill 10.2 | FAIL |
| vitals level badge | [24, 919, 102, 123] | Overlay:Overlay_0 | +14 | +2 | +14 | -7 | 0.64 | badge fill 4.5 | FAIL |
| minimap ring | [1640, 27, 250, 245] | SizeBox:SizeBox_17 | +11 | -5 | +0 | +5 | 0.90 | - | FAIL |
| minimap footer | [1677, 276, 188, 20] | SizeBox:SizeBox_19 | +9 | +0 | -8 | +12 | 0.60 | - | FAIL |
| quest tracker frame | [1629, 320, 274, 182] | SizeBox:SizeBox_0 | -16 | +18 | +6 | -52 | 0.66 | fill dark 2.0 | FAIL |
| toast Rough Timber | [1631, 525, 269, 67] | - | - | - | - | - | 0.00 | fill dark 39.9 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
