# settings-graphics: pixel diff, FAIL

REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-2 px (left/top/width/height), colour dE2000 <= 3. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).

| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| window frame | [336, 115, 1249, 810] | CanvasPanel:CanvasPanel_1 | +0 | +0 | +0 | +0 | 1.00 | panelfill 1.8, panelfill 2.5 | PASS |
| title Settings | [885, 144, 157, 46] | TextBlock:TextBlock_0 | -1 | -18 | +1 | +23 | 0.64 | - | FAIL |
| close x | [1515, 139, 46, 46] | SFHudButton:SFHudButton_0 | +0 | +0 | +0 | +0 | 0.97 | - | PASS |
| tab Graphics | [452, 207, 202, 55] | Border:Border_3 | +0 | +0 | +0 | +0 | 0.99 | mossfill 7.7 | FAIL |
| tab Audio | [655, 207, 198, 55] | Border:Border_4 | -1 | +0 | +0 | +0 | 0.99 | - | PASS |
| tab Controls | [852, 207, 204, 55] | Border:Border_5 | +0 | +0 | +0 | +0 | 0.99 | - | PASS |
| tab Interface | [1056, 207, 196, 55] | Border:Border_6 | +0 | +0 | +0 | +0 | 0.99 | - | PASS |
| tab Gameplay | [1253, 207, 209, 55] | Border:Border_7 | +0 | +0 | +0 | +0 | 0.99 | - | PASS |
| label Graphics Preset | [386, 293, 199, 31] | TextBlock:TextBlock_6 | +0 | +1 | -3 | +2 | 0.69 | - | FAIL |
| button Auto-Detect | [1277, 291, 126, 36] | SizeBox:SizeBox_8 | +0 | +0 | +0 | +0 | 0.98 | - | PASS |
| status Preset | [1444, 301, 93, 19] | TextBlock:TextBlock_8 | +0 | +1 | +1 | +1 | 0.68 | - | PASS |
| preset Low | [386, 340, 215, 47] | SizeBox:SizeBox_9 | +0 | +0 | +0 | +0 | 0.98 | platefill 1.9 | PASS |
| preset Medium | [610, 340, 222, 47] | SizeBox:SizeBox_10 | +0 | +0 | +0 | +0 | 0.98 | - | PASS |
| preset High selected | [842, 340, 231, 47] | SizeBox:SizeBox_11 | +0 | +0 | +0 | +0 | 0.98 | mossfill 2.7 | PASS |
| preset Epic | [1083, 340, 223, 47] | SizeBox:SizeBox_12 | +0 | +0 | +0 | +0 | 0.98 | - | PASS |
| preset Ultra | [1315, 340, 220, 47] | SizeBox:SizeBox_13 | +0 | +0 | +0 | +0 | 0.98 | - | PASS |
| heading Display | [388, 432, 92, 29] | TextBlock:TextBlock_14 | +0 | +0 | +1 | +3 | 0.65 | - | FAIL |
| stepper Resolution | [669, 478, 272, 37] | Overlay:Overlay_0 | +0 | +0 | +0 | +0 | 0.99 | - | PASS |
| stepper Window Mode | [669, 531, 272, 37] | Overlay:Overlay_1 | +0 | +0 | +0 | +0 | 0.98 | - | PASS |
| stepper Frame Rate | [669, 583, 272, 37] | Overlay:Overlay_2 | +0 | +1 | +0 | +0 | 0.97 | - | PASS |
| vsync On | [669, 640, 132, 39] | Overlay:Overlay_3 | +0 | -1 | +0 | +0 | 0.94 | - | PASS |
| vsync Off | [814, 640, 129, 39] | Overlay:Overlay_4 | +0 | -1 | +0 | +0 | 0.94 | - | PASS |
| render scale track | [440, 736, 342, 21] | Slider:Slider_0 | +0 | +0 | +0 | +0 | 0.97 | - | PASS |
| render scale value | [859, 726, 87, 40] | Overlay:Overlay_5 | +0 | +0 | +0 | +0 | 0.99 | - | PASS |
| motion blur On | [669, 792, 132, 39] | Overlay:Overlay_6 | +0 | +1 | +0 | +0 | 0.96 | - | PASS |
| motion blur Off | [814, 792, 129, 39] | Overlay:Overlay_7 | +0 | +1 | +0 | +0 | 0.96 | - | PASS |
| heading Quality | [1005, 432, 87, 28] | TextBlock:TextBlock_35 | +1 | +1 | -3 | +0 | 0.71 | - | FAIL |
| quality row View Distance | [1284, 475, 243, 31] | SizeBox:SizeBox_58 | +0 | +0 | +0 | +0 | 0.97 | - | PASS |
| quality row Shadows | [1284, 511, 243, 31] | SizeBox:SizeBox_67 | +0 | -1 | +0 | +0 | 0.94 | - | PASS |
| quality row Nanite Tessellation | [1284, 796, 243, 31] | SizeBox:SizeBox_139 | +0 | -2 | +0 | +0 | 0.87 | - | PASS |
| button Apply | [683, 861, 250, 54] | SizeBox:SizeBox_143 | +0 | +1 | +0 | +0 | 0.98 | - | PASS |
| button Revert | [985, 861, 248, 54] | SizeBox:SizeBox_144 | +0 | +1 | +0 | +0 | 0.98 | platefill 9.2 | FAIL |

Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).
