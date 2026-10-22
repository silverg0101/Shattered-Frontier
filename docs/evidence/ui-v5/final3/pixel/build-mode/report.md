# build-mode: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| catalogue panel frame | [33, 39, 523, 845] | SizeBox:SizeBox_0 | +0 | +0 | +0 | +22 | 0.97 | panel fill 7.3, panel fill 4.9 | FAIL |
| title Build Catalogue | [62, 52, 298, 40] | TextBlock:TextBlock_0 | +72 | +5 | -73 | +8 | 0.53 | - | FAIL |
| search field | [53, 111, 480, 41] | Overlay:Overlay_0 | +16 | +12 | -19 | +0 | 0.53 | field fill 2.1 | FAIL |
| tab Voxel Objects (selected) | [52, 161, 259, 41] | SizeBox:SizeBox_5 | +7 | +13 | +0 | +0 | 0.50 | selected tab fill 4.1, selected tab fill 3.0 | FAIL |
| tab Blueprints | [322, 161, 214, 41] | SizeBox:SizeBox_6 | +7 | +13 | +0 | +0 | 0.50 | tab fill 2.8 | FAIL |
| rail Structures (selected) | [43, 215, 95, 78] | SizeBox:SizeBox_8 | +16 | +12 | +0 | +0 | 0.54 | selected rail fill 4.9 | FAIL |
| rail Decor | [43, 305, 95, 83] | SizeBox:SizeBox_10 | +16 | +12 | +0 | +0 | 0.55 | rail fill 3.3 | FAIL |
| rail Terrain | [43, 398, 95, 83] | SizeBox:SizeBox_12 | +16 | +14 | +0 | +0 | 0.53 | rail fill 2.8 | FAIL |
| rail Favourites | [43, 490, 95, 83] | SizeBox:SizeBox_14 | +16 | +17 | +0 | +0 | 0.50 | rail fill 2.8 | FAIL |
| tile Timber Block | [155, 213, 123, 111] | SizeBox:SizeBox_16 | +14 | +17 | +0 | +0 | 0.59 | tile fill 5.4 | FAIL |
| tile Wall (selected) | [155, 327, 123, 106] | SizeBox:SizeBox_22 | +14 | +21 | +0 | +5 | 0.52 | selected tile fill 12.0 | FAIL |
| tile Floor | [285, 327, 121, 106] | SizeBox:SizeBox_23 | +14 | +21 | +2 | +5 | 0.52 | tile fill 3.7 | FAIL |
| tile Window | [413, 555, 123, 107] | SizeBox:SizeBox_30 | +16 | +29 | +0 | +4 | 0.45 | tile fill 3.0 | FAIL |
| material dropdown | [43, 673, 497, 50] | - | - | - | - | - | 0.00 | field fill 4.3 | FAIL |
| required materials box | [43, 729, 497, 148] | Border:Border_21 | +16 | +42 | -26 | -31 | 0.64 | inset fill 1.7 | FAIL |
| action bar | [722, 882, 493, 38] | SizeBox:SizeBox_12 | -8 | +0 | +0 | +0 | 0.96 | bar fill 3.8 | FAIL |
| hotbar slot 1 (axe) | [688, 938, 92, 92] | SizeBox:SizeBox_1 | -22 | +0 | -4 | -4 | 0.56 | slot fill 18.6 | FAIL |
| hotbar slot 6 (sack) | [1154, 938, 92, 92] | SizeBox:SizeBox_11 | +12 | +0 | -4 | -4 | 0.77 | slot fill 18.9 | FAIL |
| vitals HP bar | [123, 948, 367, 34] | VerticalBox:VerticalBox_0 | +40 | -32 | +13 | +50 | 0.33 | hp red fill 14.0 | FAIL |
| vitals level badge | [24, 919, 102, 123] | Overlay:Overlay_0 | +14 | -40 | +13 | +35 | 0.51 | badge fill 5.7 | FAIL |
| minimap ring | [1640, 27, 250, 245] | SizeBox:SizeBox_18 | +11 | -5 | +0 | +5 | 0.90 | - | FAIL |
| minimap footer | [1677, 276, 188, 20] | SizeBox:SizeBox_20 | +9 | +0 | -8 | +12 | 0.60 | - | FAIL |
| quest tracker frame | [1629, 320, 274, 182] | SizeBox:SizeBox_0 | -16 | +18 | +12 | -62 | 0.62 | fill dark 2.0 | FAIL |
| toast Rough Timber | [1631, 525, 269, 67] | - | - | - | - | - | 0.00 | fill dark 40.0 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
