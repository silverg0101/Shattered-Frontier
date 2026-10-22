# defeated: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| defeated modal frame | [535, 191, 843, 630] | SizeBox:SizeBox_7 | +4 | -22 | -1 | +25 | 0.95 | modal fill 4.2, modal fill 4.3 | FAIL |
| title Defeated | [813, 236, 292, 66] | TextBlock:TextBlock_3 | -34 | -67 | +69 | +60 | 0.36 | - | FAIL |
| flourish divider with stag | [688, 316, 545, 30] | Overlay:Overlay_7 | -8 | -21 | +15 | +10 | 0.37 | - | FAIL |
| subtitle Defeated by Wild Boar | [816, 355, 286, 34] | TextBlock:TextBlock_4 | -10 | -16 | +22 | +13 | 0.58 | - | FAIL |
| respawn card Silverbrook Shrine (selected) | [620, 409, 333, 241] | SizeBox:SizeBox_10 | -12 | -5 | +7 | -1 | 0.91 | card fill 3.8 | FAIL |
| shrine card image | [626, 416, 320, 160] | SizeBox:SizeBox_10 | -18 | -12 | +20 | +80 | 0.63 | - | FAIL |
| shrine check badge | [894, 423, 46, 46] | SizeBox:SizeBox_9 | +2 | -5 | -8 | -8 | 0.55 | badge fill 25.7 | FAIL |
| shrine card name | [684, 576, 212, 28] | TextBlock:TextBlock_5 | -62 | +4 | +100 | +15 | 0.35 | - | FAIL |
| shrine card status Available now | [726, 608, 124, 24] | - | - | - | - | - | 0.00 | - | REVIEW |
| respawn card Bedroll | [974, 410, 324, 238] | SizeBox:SizeBox_13 | -2 | -6 | +16 | +2 | 0.92 | card fill 1.2 | FAIL |
| bedroll clock badge | [1242, 424, 46, 46] | SizeBox:SizeBox_12 | +18 | -6 | -8 | -8 | 0.34 | - | FAIL |
| bedroll card name | [1100, 576, 76, 28] | - | - | - | - | - | 0.00 | - | REVIEW |
| bedroll timer 00:30 | [1110, 608, 56, 24] | - | - | - | - | - | 0.00 | - | REVIEW |
| Hold to Respawn button | [709, 675, 501, 78] | Overlay:Overlay_12 | +11 | -7 | -21 | -22 | 0.56 | brass fill 22.1, brass fill 22.0 | FAIL |
| hold ring | [785, 682, 62, 62] | - | - | - | - | - | 0.00 | - | REVIEW |
| footer note | [800, 776, 318, 22] | TextBlock:TextBlock_11 | +16 | +0 | -31 | +10 | 0.63 | - | FAIL |
| vitals HP bar empty | [145, 949, 332, 26] | SizeBox:SizeBox_1 | +19 | -7 | +48 | +6 | 0.59 | empty hp fill 24.5 | FAIL |
| vitals stamina bar | [145, 986, 332, 26] | SizeBox:SizeBox_4 | +19 | +3 | +48 | +6 | 0.54 | stamina amber fill 6.0 | FAIL |
| vitals level badge | [28, 908, 102, 130] | Overlay:Overlay_0 | +10 | +13 | +14 | -14 | 0.66 | badge fill 1.5 | FAIL |
| hotbar slot 1 (sword) | [690, 938, 82, 86] | SizeBox:SizeBox_1 | -24 | +11 | +6 | +2 | 0.48 | slot fill 3.3 | FAIL |
| hotbar slot 6 (torch) | [1150, 938, 82, 86] | SizeBox:SizeBox_11 | +16 | +11 | +6 | +2 | 0.50 | slot fill 4.9 | FAIL |
| minimap ring | [1660, 15, 234, 231] | Image:Image_9 | +1 | +17 | -4 | -1 | 0.86 | - | FAIL |
| quest tracker frame | [1645, 305, 258, 196] | SizeBox:SizeBox_0 | -32 | +33 | +22 | -66 | 0.59 | dim fill 2.6 | FAIL |
| loot toast Raw Boar Meat | [1647, 530, 246, 48] | - | - | - | - | - | 0.00 | toast fill 11.6 | FAIL |
| loot toast Tough Hide | [1647, 588, 246, 48] | - | - | - | - | - | 0.00 | toast fill 9.3 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
