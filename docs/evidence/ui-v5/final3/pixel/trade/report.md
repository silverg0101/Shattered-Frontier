# trade: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| trade window frame | [506, 142, 908, 780] | Border:Border_1 | +0 | +0 | +0 | +0 | 1.00 | window fill 2.2, window fill 8.2 | FAIL |
| title Direct Trade with flourishes | [733, 160, 457, 42] | HorizontalBox:HorizontalBox_0 | +62 | -2 | -127 | +6 | 0.65 | - | FAIL |
| portrait well You | [545, 221, 103, 94] | Overlay:Overlay_1 | +3 | -7 | -7 | +2 | 0.83 | - | FAIL |
| name You | [668, 251, 55, 29] | TextBlock:TextBlock_1 | -10 | -8 | -9 | +9 | 0.45 | - | FAIL |
| portrait well Elira | [1000, 221, 103, 94] | Overlay:Overlay_15 | -29 | -7 | -7 | +2 | 0.46 | - | FAIL |
| name Elira | [1123, 251, 69, 28] | TextBlock:TextBlock_29 | -42 | -8 | +11 | +10 | 0.27 | - | FAIL |
| column divider diamond | [950, 225, 22, 545] | SizeBox:SizeBox_94 | -1 | -11 | +0 | +1 | 0.88 | - | FAIL |
| my offer slot 1 Iron Axe | [534, 327, 95, 95] | SizeBox:SizeBox_6 | +17 | -6 | +0 | +0 | 0.63 | slot fill 1.6 | FAIL |
| my offer slot 2 Oak Planks | [635, 327, 95, 95] | SizeBox:SizeBox_9 | +17 | -6 | +0 | +0 | 0.63 | slot fill 1.5 | FAIL |
| my offer slot 4 River Herbs | [836, 327, 95, 95] | SizeBox:SizeBox_15 | +18 | -6 | +0 | +0 | 0.61 | slot fill 1.4 | FAIL |
| my offer item names row | [552, 427, 372, 16] | - | - | - | - | - | 0.00 | - | REVIEW |
| my empty slot row 2 col 1 | [534, 452, 95, 91] | VerticalBox:VerticalBox_8 | +17 | -10 | +0 | +0 | 0.58 | empty slot fill 1.9 | FAIL |
| my empty slot row 3 col 4 | [836, 548, 95, 90] | VerticalBox:VerticalBox_15 | +18 | -9 | +0 | +1 | 0.58 | empty slot fill 2.1 | FAIL |
| their offer slot 1 Wolf Pelt | [989, 327, 95, 95] | SizeBox:SizeBox_51 | -15 | -6 | +0 | +0 | 0.65 | slot fill 1.9 | FAIL |
| their offer slot 4 Iron Ingot | [1291, 327, 95, 95] | SizeBox:SizeBox_60 | -14 | -6 | +0 | +0 | 0.66 | slot fill 1.0 | FAIL |
| my gold offered field | [545, 648, 360, 46] | SizeBox:SizeBox_43 | +3 | -7 | +0 | +0 | 0.73 | field fill 4.5 | FAIL |
| their gold offered field | [1015, 648, 360, 46] | SizeBox:SizeBox_88 | -44 | -7 | +0 | +0 | 0.59 | field fill 4.1 | FAIL |
| Offer Locked button (pressed) | [585, 704, 300, 70] | Border:Border_17 | -37 | +0 | +8 | +4 | 0.76 | moss fill 6.2 | FAIL |
| Reviewing offer status plate | [1036, 704, 300, 70] | SizeBox:SizeBox_91 | -65 | +0 | +0 | +0 | 0.64 | plate fill 5.5 | FAIL |
| warning line | [780, 794, 360, 29] | - | - | - | - | - | 0.00 | - | REVIEW |
| Cancel Trade button | [539, 842, 323, 61] | VerticalBox:VerticalBox_30 | +17 | -24 | +0 | +11 | 0.52 | button fill 2.8 | FAIL |
| Hold to Accept Trade button | [965, 839, 419, 67] | Overlay:Overlay_30 | -20 | -21 | +0 | +5 | 0.54 | brass fill 6.7, brass fill 21.2 | FAIL |
| minimap ring | [1610, 22, 284, 284] | VerticalBox:VerticalBox_1 | +41 | +0 | -34 | +2 | 0.83 | - | FAIL |
| quest tracker frame | [1612, 338, 286, 160] | - | - | - | - | - | 0.00 | dimmed fill 2.3 | REVIEW |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
