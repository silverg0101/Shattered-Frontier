# build-mode: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| catalogue panel frame | [33, 39, 523, 845] | SizeBox:SizeBox_0 | +0 | +0 | +0 | -26 | 0.97 | panel fill 4.5, panel fill 4.9 | FAIL |
| title Build Catalogue | [62, 52, 298, 40] | TextBlock:TextBlock_0 | +62 | -5 | -97 | +4 | 0.60 | - | FAIL |
| search field | [53, 111, 480, 41] | Overlay:Overlay_0 | -4 | -6 | +11 | +0 | 0.73 | field fill 2.1 | FAIL |
| tab Voxel Objects (selected) | [52, 161, 259, 41] | SizeBox:SizeBox_5 | -3 | -6 | +0 | +0 | 0.73 | selected tab fill 4.1, selected tab fill 3.0 | FAIL |
| tab Blueprints | [322, 161, 214, 41] | SizeBox:SizeBox_6 | -3 | -6 | +0 | +0 | 0.72 | tab fill 2.8 | FAIL |
| rail Structures (selected) | [43, 215, 95, 78] | SizeBox:SizeBox_8 | +6 | -9 | +0 | +0 | 0.71 | selected rail fill 4.9 | FAIL |
| rail Decor | [43, 305, 95, 83] | SizeBox:SizeBox_10 | +6 | -12 | +0 | +0 | 0.67 | rail fill 3.3 | FAIL |
| rail Terrain | [43, 398, 95, 83] | SizeBox:SizeBox_12 | +6 | -13 | +0 | +0 | 0.65 | rail fill 2.8 | FAIL |
| rail Favourites | [43, 490, 95, 83] | SizeBox:SizeBox_14 | +6 | -13 | +0 | +0 | 0.65 | rail fill 2.8 | FAIL |
| tile Timber Block | [155, 213, 123, 111] | SizeBox:SizeBox_16 | +4 | -4 | +0 | +0 | 0.87 | tile fill 5.4 | FAIL |
| tile Wall (selected) | [155, 327, 123, 106] | SizeBox:SizeBox_22 | +4 | +0 | +0 | +5 | 0.88 | selected tile fill 12.0 | FAIL |
| tile Floor | [285, 327, 121, 106] | SizeBox:SizeBox_23 | +4 | +0 | +2 | +5 | 0.87 | tile fill 3.7 | FAIL |
| tile Window | [413, 555, 123, 107] | SizeBox:SizeBox_30 | +6 | +8 | +0 | +4 | 0.75 | tile fill 3.0 | FAIL |
| material dropdown | [43, 673, 497, 50] | SizeBox:SizeBox_33 | +6 | +15 | -6 | +0 | 0.54 | field fill 2.2 | FAIL |
| required materials box | [43, 729, 497, 148] | Border:Border_21 | +6 | +15 | -6 | -42 | 0.71 | inset fill 1.7 | FAIL |
| action bar | [722, 882, 493, 38] | SizeBox:SizeBox_12 | -8 | +0 | +0 | +0 | 0.96 | bar fill 4.0 | FAIL |
| hotbar slot 1 (axe) | [688, 938, 92, 92] | SizeBox:SizeBox_1 | -22 | +0 | -4 | -4 | 0.56 | slot fill 18.6 | FAIL |
| hotbar slot 6 (sack) | [1154, 938, 92, 92] | SizeBox:SizeBox_11 | +12 | +0 | -4 | -4 | 0.77 | slot fill 18.9 | FAIL |
| vitals HP bar | [123, 948, 367, 34] | Overlay:Overlay_1 | +40 | -7 | +13 | -2 | 0.49 | hp red fill 10.2 | FAIL |
| vitals level badge | [24, 919, 102, 123] | Overlay:Overlay_0 | +14 | -25 | +13 | +35 | 0.54 | badge fill 2.7 | FAIL |
| minimap ring | [1640, 27, 250, 245] | SizeBox:SizeBox_18 | +11 | -5 | +0 | +5 | 0.90 | - | FAIL |
| minimap footer | [1677, 276, 188, 20] | SizeBox:SizeBox_20 | +9 | +0 | -8 | +12 | 0.60 | - | FAIL |
| quest tracker frame | [1629, 320, 274, 182] | - | - | - | - | - | 0.00 | fill dark 7.8 | FAIL |
| toast Rough Timber | [1631, 525, 269, 67] | - | - | - | - | - | 0.00 | fill dark 34.6 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
