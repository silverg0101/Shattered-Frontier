# defeated: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| defeated modal frame | [535, 191, 843, 630] | Border:Border_1 | +0 | +0 | +0 | +0 | 1.00 | modal fill 4.9, modal fill 4.8 | FAIL |
| title Defeated | [813, 236, 292, 66] | TextBlock:TextBlock_3 | -5 | +14 | +5 | -8 | 0.61 | - | FAIL |
| flourish divider with stag | [688, 316, 545, 30] | Overlay:Overlay_7 | -4 | +16 | +0 | +0 | 0.30 | - | FAIL |
| subtitle Defeated by Wild Boar | [816, 355, 286, 34] | TextBlock:TextBlock_4 | +18 | +20 | -42 | -6 | 0.42 | - | FAIL |
| respawn card Silverbrook Shrine (selected) | [620, 409, 333, 241] | SizeBox:SizeBox_11 | -7 | +13 | +0 | +0 | 0.86 | card fill 4.8 | FAIL |
| shrine card image | [626, 416, 320, 160] | SizeBox:SizeBox_9 | -7 | +12 | +0 | +0 | 0.83 | - | FAIL |
| shrine check badge | [894, 423, 46, 46] | SizeBox:SizeBox_10 | -7 | +13 | +0 | +0 | 0.43 | badge fill 18.6 | FAIL |
| shrine card name | [684, 576, 212, 28] | - | - | - | - | - | 0.00 | - | REVIEW |
| shrine card status Available now | [726, 608, 124, 24] | TextBlock:TextBlock_5 | -63 | -4 | +107 | -5 | 0.35 | - | FAIL |
| respawn card Bedroll | [974, 410, 324, 238] | SizeBox:SizeBox_14 | -7 | +12 | +9 | +3 | 0.87 | card fill 4.7 | FAIL |
| bedroll clock badge | [1242, 424, 46, 46] | SizeBox:SizeBox_13 | -1 | +12 | +0 | +0 | 0.56 | - | FAIL |
| bedroll card name | [1100, 576, 76, 28] | - | - | - | - | - | 0.00 | - | REVIEW |
| bedroll timer 00:30 | [1110, 608, 56, 24] | TextBlock:TextBlock_7 | -14 | -4 | +18 | -5 | 0.49 | - | FAIL |
| Hold to Respawn button | [709, 675, 501, 78] | Overlay:Overlay_12 | -3 | +12 | +0 | -6 | 0.77 | brass fill 15.4, brass fill 29.6 | FAIL |
| hold ring | [785, 682, 62, 62] | - | - | - | - | - | 0.00 | - | REVIEW |
| footer note | [800, 776, 318, 22] | TextBlock:TextBlock_10 | -67 | -4 | +129 | -2 | 0.41 | - | FAIL |
| vitals HP bar empty | [145, 949, 332, 26] | Overlay:Overlay_1 | +18 | -8 | +48 | +6 | 0.56 | empty hp fill 23.2 | FAIL |
| vitals stamina bar | [145, 986, 332, 26] | Overlay:Overlay_4 | +18 | -3 | +48 | +6 | 0.65 | stamina amber fill 6.0 | FAIL |
| vitals level badge | [28, 908, 102, 130] | Overlay:Overlay_0 | +10 | -14 | +13 | +28 | 0.61 | badge fill 4.9 | FAIL |
| hotbar slot 1 (sword) | [690, 938, 82, 86] | SizeBox:SizeBox_1 | -24 | +0 | +6 | +2 | 0.59 | slot fill 3.1 | FAIL |
| hotbar slot 6 (torch) | [1150, 938, 82, 86] | SizeBox:SizeBox_11 | +16 | +0 | +6 | +2 | 0.62 | slot fill 5.3 | FAIL |
| minimap ring | [1660, 15, 234, 231] | Image:Image_10 | +1 | +17 | -4 | -1 | 0.86 | - | FAIL |
| quest tracker frame | [1645, 305, 258, 196] | SizeBox:SizeBox_0 | -32 | +33 | +28 | -79 | 0.55 | dim fill 3.2 | FAIL |
| loot toast Raw Boar Meat | [1647, 530, 246, 48] | - | - | - | - | - | 0.00 | toast fill 3.1 | FAIL |
| loot toast Tough Hide | [1647, 588, 246, 48] | - | - | - | - | - | 0.00 | toast fill 6.5 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
