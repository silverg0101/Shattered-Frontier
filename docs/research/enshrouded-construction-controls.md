# Enshrouded construction controls reference

Researched 2026-10-18 for UI-01. This is a comparison and proposed direction, not approved final bindings or delivered Shattered Frontier UI. Our current B/1–3/R/mouse bindings remain temporary.

## What the sources establish

Keen's [Controls & Keybinds support article](https://enshrouded.zendesk.com/hc/en-us/articles/16176192031773-Controls-Keybinds), updated March 2026, confirms separate keyboard/mouse and controller rebinding. Its [Wake of the Water update](https://enshrouded.com/en-US/news/enshrouded-wake-of-the-water-update), November 2025, documents changed defaults, customizable input modalities such as tap/long press, and binding-conflict indication. Defaults copied from older guides therefore need version checks.

The following are **reported PC defaults**, from the October 17, 2026 [control-table article and reproduced bindings-screen images](https://allthings.how/enshrouded-1-0-controls-and-default-keybinds/). We did not run Enshrouded to verify them. This is a small reference subset, not its entire control map.

| Task | Reported input |
| --- | --- |
| Construction menu | Tab |
| Build / dismantle | Left / right mouse |
| Shape | Mouse wheel; number slots |
| Category | Alt + wheel |
| Material | Ctrl + wheel |
| Quarter-turn | Tap R |
| Snap assistance / undo | X / Y |
| Sample material | F or middle mouse |
| Placement offset | Q + wheel; contextual raise/sink or forward/back |

The important pattern is contextual reuse: the equipped tool determines which actions keys operate, and modifiers provide precision controls. A large catalogue does not require a unique key for every piece. The older [wiki controls table](https://enshrouded.wiki.gg/wiki/Controls) differs on some camera/rotation bindings; it is corroboration for the broad interaction pattern, not the authority for a single current preset. Exact preset/version testing is still needed before claiming parity.

Keen's [Pact of the Flame update](https://enshrouded.com/en-US/news/enshrouded-pact-of-the-flame-update), January 2025, distinguishes structural blocks/shapes restricted to quarter-turns from decoration with broader rotation, with additional restrictions for interactive furniture. It also introduces a first-person building camera. These distinctions matter more than copying every modifier key.

## Recommended direction for our construction UI

Adopt a contextual construction tool with two connected surfaces: a searchable catalogue for deliberate selection and a compact in-world palette for rapid building. Choose geometry independently from material tier and appearance; a skin must not silently change occupancy, support or permissions. Show the selected tool/mode, shape, material and orientation beside the preview.

Use one familiar primary placement action, a distinct demolition action and a quarter-turn rotation action. Offer direct favourite slots plus previous/next selection. Make the basic shelter possible without modifier chords; expose axis orientation, plane lock, depth/offset and line/rectangle repeats as visible precision controls. All actions need remapping, conflict checks, device-appropriate prompts and a controller navigation path. This is a proposed product requirement, not an implemented input-framework migration.

For structural solids, the 0.5 m address lattice and each shape's allowed subset of the 24 proper cube orientations remain mandatory. A future snap-assistance toggle may disable target/feature attraction, but cannot enable off-grid structural placement. Fine positioning, free rotation and scaling require category-specific rules for decoration and must preserve functional access/collision. R should perform a predictable quarter-turn around the selected axis; cycling all 24 orientations is only the current prototype shortcut.

Preview must explain the actual candidate: occupied cells, contact/anchor/support status, affected footprint, material cost/source, permissions and the first actionable rejection. Do not rely on red/green alone. Show a clear target outline for removal and distinguish pending authority validation from confirmed placement. Construction §3.4 already requires explicit confirmation when demolition will collapse additional pieces; make that consequence visible before committing. This is the acting builder's confirmation, not an additional property-owner veto. In the development sandbox, unlimited stock and incomplete checks stay conspicuous.

An undo-like command must be a fresh validated compensating action under our contribution/revision rules. Preserve Construction §3.1's eligible 60-second full-material reclaim and its damage/occupancy/combat/siege restrictions; ordinary demolition has its separate intact-material return rule. It cannot restore a previous snapshot over another player's edits, duplicate refunds or bypass current permissions/support. A material sampler should copy accessible selection information, never ownership, stock or authority. First-person build camera is a useful option to prototype; it is not required to finish the first support implementation.

## Next UI-01 assignment

Create a small interaction prototype and action matrix covering: enter/exit construction; browse/search/favourite a shape; choose material; rotate by axis; place a snapped single member; lock a plane and repeat a line; inspect a blocked preview; remove with collapse confirmation; cancel without editing. Include keyboard/mouse and controller, focus/modal conflicts, remapping and non-colour feedback. Keep terrain/decor/structural action contexts explicit. Review against Construction §3.1/3.4 and BLD-01 before final bindings.

Ask the user to assess the catalogue/palette layout and common-action comfort at that prototype checkpoint. No decision about every hotkey is needed now. The current sandbox checkpoint does not deliver this interface, remapping, first-person camera, line brush or safe undo.
