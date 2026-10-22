# trade: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| trade window frame | [506, 142, 908, 780] | Border:Border_1 | +0 | +0 | +0 | +0 | 1.00 | window fill 4.9, window fill 3.6 | FAIL |
| title Direct Trade with flourishes | [733, 160, 457, 42] | HorizontalBox:HorizontalBox_0 | +44 | -2 | -90 | +15 | 0.62 | - | FAIL |
| portrait well You | [545, 221, 103, 94] | Overlay:Overlay_1 | -19 | +2 | +0 | +0 | 0.66 | - | FAIL |
| name You | [668, 251, 55, 29] | TextBlock:TextBlock_1 | -25 | -4 | +1 | +18 | 0.27 | - | FAIL |
| portrait well Elira | [1000, 221, 103, 94] | Overlay:Overlay_15 | -29 | +2 | +0 | +0 | 0.54 | - | FAIL |
| name Elira | [1123, 251, 69, 28] | TextBlock:TextBlock_29 | -35 | -4 | +30 | +19 | 0.37 | - | FAIL |
| column divider diamond | [950, 225, 22, 545] | SizeBox:SizeBox_92 | -1 | -2 | +0 | -15 | 0.88 | - | FAIL |
| my offer slot 1 Iron Axe | [534, 327, 95, 95] | SizeBox:SizeBox_6 | -5 | +1 | +0 | +0 | 0.88 | slot fill 1.6 | FAIL |
| my offer slot 2 Oak Planks | [635, 327, 95, 95] | SizeBox:SizeBox_9 | -5 | +1 | +0 | +0 | 0.88 | slot fill 1.5 | FAIL |
| my offer slot 4 River Herbs | [836, 327, 95, 95] | SizeBox:SizeBox_15 | -4 | +1 | +0 | +0 | 0.90 | slot fill 1.4 | FAIL |
| my offer item names row | [552, 427, 372, 16] | - | - | - | - | - | 0.00 | - | REVIEW |
| my empty slot row 2 col 1 | [534, 452, 95, 91] | VerticalBox:VerticalBox_8 | -5 | -3 | +0 | +0 | 0.84 | empty slot fill 1.9 | FAIL |
| my empty slot row 3 col 4 | [836, 548, 95, 90] | VerticalBox:VerticalBox_15 | -4 | -2 | +0 | +1 | 0.89 | empty slot fill 2.1 | FAIL |
| their offer slot 1 Wolf Pelt | [989, 327, 95, 95] | SizeBox:SizeBox_50 | -15 | +1 | +0 | +0 | 0.71 | slot fill 1.9 | FAIL |
| their offer slot 4 Iron Ingot | [1291, 327, 95, 95] | SizeBox:SizeBox_59 | -14 | +1 | +0 | +0 | 0.73 | slot fill 1.0 | FAIL |
| my gold offered field | [545, 648, 360, 46] | SizeBox:SizeBox_43 | +12 | +0 | +0 | +0 | 0.93 | field fill 4.1 | FAIL |
| their gold offered field | [1015, 648, 360, 46] | SizeBox:SizeBox_87 | -12 | +0 | +0 | +0 | 0.93 | field fill 3.9 | FAIL |
| Offer Locked button (pressed) | [585, 704, 300, 70] | Overlay:Overlay_14 | +2 | +0 | +0 | +0 | 0.98 | moss fill 10.7 | FAIL |
| Reviewing offer status plate | [1036, 704, 300, 70] | SizeBox:SizeBox_89 | -4 | -2 | +0 | +0 | 0.92 | plate fill 5.7 | FAIL |
| warning line | [780, 794, 360, 29] | - | - | - | - | - | 0.00 | - | REVIEW |
| Cancel Trade button | [539, 842, 323, 61] | VerticalBox:VerticalBox_30 | +7 | -18 | +0 | +0 | 0.52 | button fill 2.6 | FAIL |
| Hold to Accept Trade button | [965, 839, 419, 67] | Overlay:Overlay_30 | -10 | -21 | +0 | +5 | 0.56 | brass fill 8.8, brass fill 24.1 | FAIL |
| minimap ring | [1610, 22, 284, 284] | VerticalBox:VerticalBox_1 | +41 | +0 | -34 | +2 | 0.83 | - | FAIL |
| quest tracker frame | [1612, 338, 286, 160] | - | - | - | - | - | 0.00 | dimmed fill 2.4 | REVIEW |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
