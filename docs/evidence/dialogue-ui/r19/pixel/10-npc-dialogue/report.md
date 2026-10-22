# 10-npc-dialogue: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| board frame | [486, 219, 951, 622] | Border:Border_2 | +0 | +0 | +0 | +0 | 1.00 | board fill 2.6, board fill low 2.8 | PASS |
| portrait | [516, 267, 122, 124] | Overlay:Overlay_0 | +0 | +0 | +0 | +0 | 1.00 | - | PASS |
| speaker name | [663, 284, 262, 25] | TextBlock:TextBlock_0 | -1 | -9 | +5 | +24 | 0.50 | - | FAIL |
| speech line | [663, 340, 290, 50] | TextBlock:TextBlock_1 | -1 | -8 | +33 | +11 | 0.73 | - | FAIL |
| divider | [516, 425, 467, 9] | SizeBox:SizeBox_2 | +0 | +0 | -1 | -1 | 0.89 | - | PASS |
| row 1 (selected) | [513, 460, 472, 72] | SizeBox:SizeBox_4 | +0 | +0 | +0 | +0 | 0.99 | selected fill 8.5 | FAIL |
| row 2 | [513, 546, 472, 71] | SizeBox:SizeBox_6 | +0 | -1 | +0 | +1 | 0.99 | row fill 4.4 | FAIL |
| row 3 | [513, 631, 472, 71] | SizeBox:SizeBox_8 | +0 | -1 | +0 | +1 | 0.99 | - | PASS |
| row 4 | [513, 716, 472, 71] | SizeBox:SizeBox_10 | +0 | -1 | +0 | +1 | 0.99 | - | PASS |
| keycap 1 | [529, 469, 53, 54] | SizeBox:SizeBox_3 | +0 | +1 | +0 | -1 | 0.98 | - | PASS |
| row 1 label | [606, 487, 138, 18] | TextBlock:TextBlock_2 | -2 | -10 | +8 | +20 | 0.45 | - | FAIL |
| quest art | [1024, 244, 391, 198] | SizeBox:SizeBox_11 | +0 | +0 | +0 | +0 | 1.00 | - | PASS |
| quest title | [1044, 456, 228, 29] | TextBlock:TextBlock_6 | -3 | -9 | +7 | +16 | 0.63 | - | FAIL |
| quest summary | [1045, 498, 351, 20] | TextBlock:TextBlock_7 | -1 | -6 | +11 | +11 | 0.63 | - | FAIL |
| objectives heading | [1045, 548, 99, 21] | TextBlock:TextBlock_8 | -1 | -6 | -6 | +10 | 0.64 | - | FAIL |
| objective 1 | [1044, 584, 342, 24] | HorizontalBox:HorizontalBox_0 | +0 | +0 | +2 | +0 | 0.99 | - | PASS |
| rewards heading | [1046, 668, 80, 16] | TextBlock:TextBlock_15 | -2 | -7 | -2 | +15 | 0.49 | card fill 3.2 | FAIL |
| xp roundel | [1044, 701, 46, 45] | SizeBox:SizeBox_18 | +0 | +0 | -1 | +0 | 0.98 | - | PASS |
| coin stack | [1231, 701, 44, 44] | SizeBox:SizeBox_19 | +0 | +0 | -1 | -1 | 0.96 | - | PASS |
| accept | [1030, 765, 205, 54] | SizeBox:SizeBox_20 | +0 | +0 | +0 | +0 | 1.00 | accept fill 3.2 | FAIL |
| decline | [1248, 765, 164, 54] | SizeBox:SizeBox_21 | +0 | +0 | +0 | +0 | 1.00 | decline fill 1.4 | PASS |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
