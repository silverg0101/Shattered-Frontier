# defeated: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| defeated modal frame | [535, 191, 843, 630] | Border:Border_1 | -36 | +0 | +79 | +0 | 0.91 | modal fill 4.1, modal fill 4.1 | FAIL |
| title Defeated | [813, 236, 292, 66] | TextBlock:TextBlock_3 | +4 | +0 | -6 | +34 | 0.65 | - | FAIL |
| flourish divider with stag | [688, 316, 545, 30] | - | - | - | - | - | 0.00 | - | REVIEW |
| subtitle Defeated by Wild Boar | [816, 355, 286, 34] | TextBlock:TextBlock_4 | +21 | +15 | -40 | +4 | 0.32 | - | FAIL |
| respawn card Silverbrook Shrine (selected) | [620, 409, 333, 241] | SizeBox:SizeBox_10 | -4 | +17 | +0 | +0 | 0.85 | card fill 2.8 | FAIL |
| shrine card image | [626, 416, 320, 160] | SizeBox:SizeBox_8 | -3 | +16 | +0 | +0 | 0.80 | - | FAIL |
| shrine check badge | [894, 423, 46, 46] | SizeBox:SizeBox_9 | -4 | +17 | +0 | +0 | 0.41 | badge fill 28.7 | FAIL |
| shrine card name | [684, 576, 212, 28] | - | - | - | - | - | 0.00 | - | REVIEW |
| shrine card status Available now | [726, 608, 124, 24] | TextBlock:TextBlock_5 | -60 | -8 | +110 | +12 | 0.35 | - | FAIL |
| respawn card Bedroll | [974, 410, 324, 238] | SizeBox:SizeBox_13 | -4 | +16 | +9 | +3 | 0.84 | card fill 1.8 | FAIL |
| bedroll clock badge | [1242, 424, 46, 46] | SizeBox:SizeBox_12 | +2 | +16 | +0 | +0 | 0.44 | - | FAIL |
| bedroll card name | [1100, 576, 76, 28] | - | - | - | - | - | 0.00 | - | REVIEW |
| bedroll timer 00:30 | [1110, 608, 56, 24] | TextBlock:TextBlock_7 | -11 | -8 | +20 | +12 | 0.49 | - | FAIL |
| Hold to Respawn button | [709, 675, 501, 78] | Overlay:Overlay_12 | +0 | +16 | +0 | -6 | 0.70 | brass fill 20.1, brass fill 36.5 | FAIL |
| hold ring | [785, 682, 62, 62] | - | - | - | - | - | 0.00 | - | REVIEW |
| footer note | [800, 776, 318, 22] | TextBlock:TextBlock_10 | -94 | -5 | +191 | +9 | 0.44 | - | FAIL |
| vitals HP bar empty | [145, 949, 332, 26] | VerticalBox:VerticalBox_0 | +18 | -33 | +48 | +58 | 0.25 | empty hp fill 15.0 | FAIL |
| vitals stamina bar | [145, 986, 332, 26] | Overlay:Overlay_4 | +18 | -18 | +48 | +6 | 0.26 | stamina amber fill 6.0 | FAIL |
| vitals level badge | [28, 908, 102, 130] | Overlay:Overlay_0 | +10 | -29 | +13 | +28 | 0.60 | badge fill 5.1 | FAIL |
| hotbar slot 1 (sword) | [690, 938, 82, 86] | SizeBox:SizeBox_1 | -24 | +0 | +6 | +2 | 0.59 | slot fill 3.3 | FAIL |
| hotbar slot 6 (torch) | [1150, 938, 82, 86] | SizeBox:SizeBox_11 | +16 | +0 | +6 | +2 | 0.62 | slot fill 5.3 | FAIL |
| minimap ring | [1660, 15, 234, 231] | Image:Image_10 | +1 | +17 | -4 | -1 | 0.86 | - | FAIL |
| quest tracker frame | [1645, 305, 258, 196] | SizeBox:SizeBox_0 | -32 | +33 | +28 | -76 | 0.56 | dim fill 2.6 | FAIL |
| loot toast Raw Boar Meat | [1647, 530, 246, 48] | - | - | - | - | - | 0.00 | toast fill 11.1 | FAIL |
| loot toast Tough Hide | [1647, 588, 246, 48] | - | - | - | - | - | 0.00 | toast fill 8.9 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
