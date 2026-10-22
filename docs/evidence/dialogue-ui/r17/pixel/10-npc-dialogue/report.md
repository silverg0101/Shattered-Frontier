# 10-npc-dialogue: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| board frame | [486, 219, 951, 622] | CanvasPanel:VitalsCanvas | -486 | -219 | +969 | +458 | 0.29 | board fill 45.0, board fill low 36.6 | FAIL |
| portrait | [516, 267, 122, 124] | - | - | - | - | - | 0.00 | - | REVIEW |
| speaker name | [663, 284, 262, 25] | - | - | - | - | - | 0.00 | - | REVIEW |
| speech line | [663, 340, 290, 50] | - | - | - | - | - | 0.00 | - | REVIEW |
| divider | [516, 425, 467, 9] | - | - | - | - | - | 0.00 | - | REVIEW |
| row 1 (selected) | [513, 460, 472, 72] | - | - | - | - | - | 0.00 | selected fill 15.3 | FAIL |
| row 2 | [513, 546, 472, 71] | - | - | - | - | - | 0.00 | row fill 40.9 | FAIL |
| row 3 | [513, 631, 472, 71] | - | - | - | - | - | 0.00 | - | REVIEW |
| row 4 | [513, 716, 472, 71] | - | - | - | - | - | 0.00 | - | REVIEW |
| keycap 1 | [529, 469, 53, 54] | - | - | - | - | - | 0.00 | - | REVIEW |
| row 1 label | [606, 487, 138, 18] | - | - | - | - | - | 0.00 | - | REVIEW |
| quest art | [1024, 244, 391, 198] | - | - | - | - | - | 0.00 | - | REVIEW |
| quest title | [1044, 456, 228, 29] | - | - | - | - | - | 0.00 | - | REVIEW |
| quest summary | [1045, 498, 351, 20] | - | - | - | - | - | 0.00 | - | REVIEW |
| objectives heading | [1045, 548, 99, 21] | - | - | - | - | - | 0.00 | - | REVIEW |
| objective 1 | [1044, 584, 342, 24] | - | - | - | - | - | 0.00 | - | REVIEW |
| rewards heading | [1046, 668, 80, 16] | - | - | - | - | - | 0.00 | card fill 46.3 | FAIL |
| xp roundel | [1044, 701, 46, 45] | - | - | - | - | - | 0.00 | - | REVIEW |
| coin stack | [1231, 701, 44, 44] | - | - | - | - | - | 0.00 | - | REVIEW |
| accept | [1030, 765, 205, 54] | - | - | - | - | - | 0.00 | accept fill 9.2 | FAIL |
| decline | [1248, 765, 164, 54] | - | - | - | - | - | 0.00 | decline fill 39.1 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
