# Worked timber rectangular opening pieces — batch 36

- Phase / scope revision: Intermediate construction kits / 1 (2026-10-18)
- Sources: `docs/art-templates/player-construction-kit-contract.md` §§2–3, 8–9; Construction Architecture §§3.2, 8–9; matrix 07.
- Owner: Codex
- Dependencies: KIT-WT faces/joints; reuse baseline rough-timber and masonry opening packages as style/interface references.
- Geometry note: contract currently gives no numeric rectangular-aperture dimensions; do not invent values. Match source opening silhouette/contact planes, with geometry fit unresolved.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
| --- | --- | --- | --- | --- | --- | --- |
| BLD-WT-RectOpeningJamb-01 | Worked timber / RectOpeningJamb: jamb/reveal appearance | Construction matrix; contract opening targets | P1; KIT-WT; geometry/interface authority required | Codex / blocked | proposed / needs-segmentation | [v001 review record](../../GameClient/ArtSource/BLD-WT-RectOpeningJamb-01/References/v001/); style evidence only; host aperture and scale unresolved |
| BLD-WT-RectOpeningLintel-01 | Worked timber / RectOpeningLintel: lintel appearance | Construction matrix; contract opening targets | P1; KIT-WT; geometry/interface authority required | Codex / blocked | proposed / needs-review | [v001 review record](../../GameClient/ArtSource/BLD-WT-RectOpeningLintel-01/References/v001/); visual includes unsupported framing relationships; no clean selection |
| BLD-WT-RectOpeningSill-01 | Worked timber / RectOpeningSill: sill appearance | Construction matrix; contract opening targets | P1; KIT-WT; geometry/interface authority required | Codex / blocked | proposed / needs-review | [v001 review record](../../GameClient/ArtSource/BLD-WT-RectOpeningSill-01/References/v001/); retain corrected wood appearance; no host contour |
| BLD-WT-RectOpeningThreshold-01 | Worked timber / RectOpeningThreshold: threshold appearance | Construction matrix; contract opening targets | P1; KIT-WT; geometry/interface authority required | Codex / blocked | proposed / needs-review | [v001 review record](../../GameClient/ArtSource/BLD-WT-RectOpeningThreshold-01/References/v001/); floor material and contact boundary unresolved |
| BLD-WT-RectDoorInterface-01 | Worked timber / RectDoorInterface: door/frame surface or attachment interface | Construction matrix `RectDoorInterface`; batch 19 is RT/SM appearance precedent only | P1; KIT-WT; host aperture and interface route required | Codex / blocked before image generation | proposed / needs-review | [v001 blocker record](../../GameClient/ArtSource/BLD-WT-RectDoorInterface-01/References/v001/); aperture, frame boundary, hinge/stop contact and clearances undefined; 0/2 outputs |
| BLD-WT-RectWindowInterface-01 | Worked timber / RectWindowInterface: window/frame surface or attachment interface | Construction matrix `RectWindowInterface`; batch 19 is RT/SM appearance precedent only | P1; KIT-WT; host aperture and interface route required | Codex / blocked before image generation | proposed / needs-review | [v001 blocker record](../../GameClient/ArtSource/BLD-WT-RectWindowInterface-01/References/v001/); aperture, sill/frame contact and clearances undefined; 0/2 outputs |

## Component briefs

## Buildability route and explicit unknowns (reviewed 2026-10-18)

Intended route for the four wall-piece IDs: **proposed exposed-surface treatment** on a future approved rectangular-opening carrier, not new structural solids or member assemblies. The door and window interface routes remain unresolved between a host surface treatment and separately defined attachments. None is mapped to an approved carrier/interface, so all six jobs remain blocked before generation or model-ready selection.

Known frame constraints: metres; 0.5 m structural grid; BLD-01 X/Y/Z frame; carrier origin must be the minimum-grid-corner frame. Treatment has no independent pivot or rotation; it must inherit the approved carrier's exact allowlist. Unknown and not inferred: opening width/height/depth, axis-to-aperture assignment, pivot coordinates, opening contour, collision boundary, reveal depth, side-specific exposed faces, contact planes, trim/member boundaries, repeat scale/phase, rotations and clearances.

Named intended counterpart interfaces (contours unresolved): jamb left/right to `BLD-WT-RectOpeningLintel-01` at head and `BLD-WT-RectOpeningSill-01` or `BLD-WT-RectOpeningThreshold-01` at base; lintel to both jambs and opening host above; sill to both jambs and opening host below; threshold to both jambs, floor and a future door interface. `BLD-WT-RectDoorInterface-01` has no approved host aperture, frame boundary, hinge/stop plane or clearance; `BLD-WT-RectWindowInterface-01` has no approved host aperture, frame/sill anchor, infill shape or clearance. Their component routes remain unresolved. The contract and matrix contain no approved rectangular-aperture geometry definition. GEO-01 approved manifest/hash and applicable GEO-02 opening/repeat/rotation evidence are missing; the pilot `prototype-subset` is not acceptance evidence.

The prior cached images were visually reviewed on 2026-10-18. They remain unselected appearance evidence outside the selected-deliverable count because their exact prompts were not recoverable from the package/repository record and the route fails the gate. Preserve their source files and original tool-call provenance. Findings: jamb sheet depicts a whole opening/wall and its first clean attempt incorrectly included a head member; corrected clean attempt isolates a pair of posts but still has no scale/host contour. Lintel sheet shows a framed opening rather than a standalone lintel; clean plank view is appearance-only. First sill sheet includes plaster (rejected); corrected wood sheet is close-up collage, useful only as appearance evidence. Threshold sheet includes a tiled floor and multiple panels. No replacement is authorized until the host contour and counterpart interfaces are approved.

Declare two selected outputs per eventual job: `concept-sheet.png`, `asset-view.png`; `additional_images: 0`. The full named component must dominate the concept sheet; clean view must show one intact treatment only, and can be generated only after route/interface review. Current selected images: 0/2 for each job. No image establishes numerical fit.

## Progress

- Planned: 0; in-progress: 0; image-complete: 0; blocked: 6; deferred: 0.
- Exact current queue head: `BLD-WT-RectOpeningJamb-01` (blocked pending approved aperture geometry, interface contours and recoverable prompt provenance). Door/window interface jobs also have explicit v001 blocker records and 0/2 outputs. No ready asset within this type.
