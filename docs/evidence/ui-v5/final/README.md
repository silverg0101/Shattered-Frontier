# UI v5 final comparison — 2026-10-21

Editor target: `ShatteredFrontierEditor Win64 Development` succeeded (15 actions; one `SFHudStyle.h:219` deprecation warning). `py -3 tools/Process-KitV5.py` resampled 186 pieces; `tools/Import-HudKit.py --save` reported the v5 kit and scene textures imported with font. Captures used a 1920×1080 `-game` client. In the untouched gameplay start frame, build mode was **off**; no catalogue or placement preview was visible. The build-mode frame follows B/scenario input.

Every `*-compare.png` puts the approved concept on the left and the live frame on the right. The two "current-attempt" pairs document captures that did not reach the intended screen. Earlier live frames are labelled in the image and below; they are not represented as fresh post-import captures.

| Screen | Verdict | Remaining differences / capture limit |
| --- | --- | --- |
| Gameplay HUD | Does not match | Vitals, hotbar, minimap and quest tracker render, but there is no target frame, prompt or toasts; mannequin and Silverbrook blockout differ strongly from the forest/boar concept. |
| Build mode | Does not match | Catalogue and prompt bar render after B. Only four populated structure tiles, labels spill past tiles, material/status text is crowded, and the construction sandbox lacks the concept's green placement wall and world. |
| Inventory + tooltip | Current capture incomplete; earlier UI does not match | `inventory-current-attempt-compare.png` records that the I/hover attempt showed gameplay only. The earlier `inventory-compare.png` shows the panel and pilot tooltip, but it is not a post-import verification; equipment/slot detail and tooltip placement differ from concept. |
| Market board | Does not match | Main hierarchy and detail card render. Inventory remains visible behind the modal; listing table lacks concept columns/grid and uses different content; transparency makes background compete with text. |
| Trade | Does not match | Two offer columns and actions render. Inventory remains visible behind it; modal is larger, portraits/items and states differ, empty slots are plain, and some labels clip. |
| Gathering | State not captured; does not match | Scenario frame shows the field and HUD, without gathering skills, channel ring, target frame or gain toast. |
| Crafting station | Does not match | Workbench panel renders, but two headings overlap, recipe labels spill into the detail column, and the concept's queue, station/world art and spacing are absent. |
| Defeated | Closest live modal, still does not match | Defeated panel, respawn cards and countdown render. Background has no visible boar/body, character remains upright, automatic respawn wording differs, and the concept's world/quest context is absent. |
| Loading screen | Earlier capture close; current capture incomplete | `loading-screen-compare.png` uses today's earlier `docs/evidence/ui-v5/9-loading-screen-live.png`: art and hierarchy are close, but emblem/title sizing and loading bar styling differ. `loading-current-attempt-compare.png` shows why the current timed collector cannot prove loading: it starts after pawn creation. A startup desktop capture did not reach the game window. |

Live source frames: `Artifacts/ui-v5-final/{gameplay,buildstation,panels,gathering,defeated,inventory3,loading}/frames/` (ignored runtime artifacts). Earlier inventory and loading source frames: `docs/evidence/ui-v5/{inventory-live.png,9-loading-screen-live.png}`. The editor and all capture-owned Unreal processes were closed after capture. None of the nine screens can be accepted as the same design from this evidence.
