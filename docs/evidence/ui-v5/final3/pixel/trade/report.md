# trade: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| trade window frame | [506, 142, 908, 780] | Border:Border_1 | -7 | +9 | +14 | -89 | 0.87 | window fill 2.3, window fill 4.2 | FAIL |
| title Direct Trade with flourishes | [733, 160, 457, 42] | HorizontalBox:HorizontalBox_0 | -24 | +7 | +44 | +43 | 0.35 | - | FAIL |
| portrait well You | [545, 221, 103, 94] | Overlay:Overlay_1 | +4 | +39 | -31 | -22 | 0.36 | - | FAIL |
| name You | [668, 251, 55, 29] | - | - | - | - | - | 0.00 | - | REVIEW |
| portrait well Elira | [1000, 221, 103, 94] | - | - | - | - | - | 0.00 | - | REVIEW |
| name Elira | [1123, 251, 69, 28] | - | - | - | - | - | 0.00 | - | REVIEW |
| column divider diamond | [950, 225, 22, 545] | - | - | - | - | - | 0.00 | - | REVIEW |
| my offer slot 1 Iron Axe | [534, 327, 95, 95] | VerticalBox:VerticalBox_4 | +21 | +19 | -19 | -11 | 0.57 | slot fill 12.2 | FAIL |
| my offer slot 2 Oak Planks | [635, 327, 95, 95] | VerticalBox:VerticalBox_5 | +8 | +19 | -19 | -11 | 0.60 | slot fill 13.2 | FAIL |
| my offer slot 4 River Herbs | [836, 327, 95, 95] | VerticalBox:VerticalBox_7 | -17 | +19 | -19 | -11 | 0.41 | slot fill 13.7 | FAIL |
| my offer item names row | [552, 427, 372, 16] | - | - | - | - | - | 0.00 | - | REVIEW |
| my empty slot row 2 col 1 | [534, 452, 95, 91] | VerticalBox:VerticalBox_8 | +21 | -10 | -31 | -27 | 0.37 | empty slot fill 1.7 | FAIL |
| my empty slot row 3 col 4 | [836, 548, 95, 90] | - | - | - | - | - | 0.00 | empty slot fill 5.6 | FAIL |
| their offer slot 1 Wolf Pelt | [989, 327, 95, 95] | VerticalBox:VerticalBox_18 | -6 | +19 | -19 | -11 | 0.53 | slot fill 12.1 | FAIL |
| their offer slot 4 Iron Ingot | [1291, 327, 95, 95] | - | - | - | - | - | 0.00 | slot fill 4.7 | FAIL |
| my gold offered field | [545, 648, 360, 46] | Overlay:Overlay_14 | +8 | +2 | -60 | +10 | 0.65 | field fill 23.2 | FAIL |
| their gold offered field | [1015, 648, 360, 46] | SizeBox:SizeBox_88 | -38 | +0 | -60 | +10 | 0.56 | field fill 5.3 | FAIL |
| Offer Locked button (pressed) | [585, 704, 300, 70] | - | - | - | - | - | 0.00 | moss fill 9.0 | FAIL |
| Reviewing offer status plate | [1036, 704, 300, 70] | - | - | - | - | - | 0.00 | plate fill 5.3 | FAIL |
| warning line | [780, 794, 360, 29] | - | - | - | - | - | 0.00 | - | REVIEW |
| Cancel Trade button | [539, 842, 323, 61] | TextBlock:TextBlock_59 | +14 | +4 | -27 | -33 | 0.42 | button fill 28.9 | FAIL |
| Hold to Accept Trade button | [965, 839, 419, 67] | Overlay:Overlay_29 | -14 | -53 | +1 | +21 | 0.28 | brass fill 14.5, brass fill 25.2 | FAIL |
| minimap ring | [1610, 22, 284, 284] | VerticalBox:VerticalBox_1 | +41 | +0 | -34 | +2 | 0.83 | - | FAIL |
| quest tracker frame | [1612, 338, 286, 160] | SizeBox:SizeBox_0 | +1 | +0 | -6 | -30 | 0.80 | dimmed fill 2.8 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
