# 10-npc-dialogue: pixel diff, FAIL

Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (match < 0.5).

| element | concept rect | dx | dy | dw | dh | match | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| board frame | [486, 219, 951, 622] | - | - | - | - | 0.36 | board fill 2.6, board fill low 2.8 | FAIL |
| portrait | [516, 267, 122, 124] | - | - | - | - | 0.16 | - | FAIL |
| speaker name | [663, 284, 262, 25] | +1 | +0 | +3 | +1 | 0.83 | - | FAIL |
| speech line | [663, 340, 290, 50] | - | - | - | - | 0.23 | - | FAIL |
| divider | [516, 425, 467, 9] | +14 | +0 | -34 | +0 | 0.85 | - | FAIL |
| row 1 (selected) | [513, 460, 472, 72] | +17 | +12 | -19 | -16 | 0.56 | selected fill 0.8 | FAIL |
| row 2 | [513, 546, 472, 71] | +2 | +1 | -11 | -4 | 0.60 | row fill 3.3 | FAIL |
| row 3 | [513, 631, 472, 71] | +2 | +1 | -11 | -4 | 0.59 | - | FAIL |
| row 4 | [513, 716, 472, 71] | +1 | +2 | -3 | -16 | 0.59 | - | FAIL |
| keycap 1 | [529, 469, 53, 54] | +4 | +2 | -5 | -6 | 0.68 | - | FAIL |
| row 1 label | [606, 487, 138, 18] | - | - | - | - | 0.45 | - | FAIL |
| quest art | [1024, 244, 391, 198] | - | - | - | - | 0.31 | - | FAIL |
| quest title | [1044, 456, 228, 29] | +0 | +0 | -3 | +0 | 0.82 | - | FAIL |
| quest summary | [1045, 498, 351, 20] | +0 | +1 | -16 | +0 | 0.78 | - | FAIL |
| objectives heading | [1045, 548, 99, 21] | +1 | +1 | -2 | +0 | 0.74 | - | PASS |
| objective 1 | [1044, 584, 342, 24] | +0 | +1 | -2 | +0 | 0.52 | - | PASS |
| rewards heading | [1046, 668, 80, 16] | +2 | +0 | +1 | +0 | 0.71 | card fill 3.6 | FAIL |
| xp roundel | [1044, 701, 46, 45] | - | - | - | - | 0.34 | - | FAIL |
| coin stack | [1231, 701, 44, 44] | +0 | +8 | +2 | -8 | 0.65 | - | FAIL |
| accept | [1030, 765, 205, 54] | - | - | - | - | 0.48 | accept fill 7.0 | FAIL |
| decline | [1248, 765, 164, 54] | +0 | -1 | +0 | +2 | 0.78 | decline fill 1.5 | PASS |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
