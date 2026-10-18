# Player construction geometry and skin contract

User direction, 2026-10-18: include reusable player-construction building blocks in the asset library; define known shapes that accept multiple skins while preserving the planned complex construction system. This supplements `docs/art-templates/generate-images.md` and `docs/art-templates/asset-planning.md`, which are owned by the active image session and may still be uncommitted. It authorizes concept-image planning, not runtime geometry implementation or changes to gameplay rules.

## Source authority and current gap

[Construction Architecture §§3.1–3.5, 8–9, 16](../Game-Design/Legends_Shattered_Frontier_Construction_System_Architecture_v0.1.md) owns the 0.5 m grid, partial/sloped/corner/arched/curved shapes, structural support, shelter, material tiers, progression and functional catalogue. [Construction Integration §§3–4, 8](../Game-Design/Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md) owns versioned shape/material references, addressed voxel assemblies and authoritative validation/durable effects. These documents take precedence over art briefs.

The sources name shape families but do not enumerate their exact masks, pivots or dimensions. The four existing shelter concepts in the other session's `docs/art-templates/asset-lists/basic-greenlands-shelter-03.md` explicitly leave connection sizes unresolved. Preserve their images/history and use suitable styling; they are not verified modular geometry. Record geometry-fit review separately from image-complete status.

The **reference geometries below are new, explicit art proposals**, not already accepted runtime masks, production content IDs, unlocks or cost/support values. They give this image session a deterministic brief now. Record `geometry-proposed` until the construction/engine content author accepts the mask/pivot/connection definition; never infer such acceptance from an image. Later modeling must build the declared geometry from parameters and verify it numerically, using artwork for appearance. Image generation alone cannot certify dimensions or fit.

## Separate shape, material and cosmetic identity

| Layer | Owns | What a variant may change |
| --- | --- | --- |
| Shape definition + version | Occupied volume, empty openings, boundary contact masks, pivot, permitted orientations and connection interfaces | Changes require a new geometry definition/version and validation |
| Structural material profile + version | Rough timber, worked timber, stone masonry, reinforced masonry or metal-framed masonry; existing cost, mass, capacity, weather/fire/siege properties | Switching tiers is a gameplay material change with existing payment/support rules |
| Cosmetic skin + version | Grain, stone courses, color, joinery appearance, trim and ornament inside the approved render envelope | Appearance only; preserve shape, collision, contacts, openings, access and all gameplay properties |
| Functional definition + version | Doors, storage, workstations, power/fluid/logic and other interactions, attachment/port/clearance envelopes | A decorative variant cannot change function, inventory capacity or permissions |
| Assembly/blueprint | A composition of versioned voxel shapes and separately defined functional objects | No free materials, permissions, contents or structural capability |

A stone-looking surface on a timber gameplay profile must not be presented as stone-tier strength. Material-tier identity and damage/condition cues remain readable. Cosmetic weathering must not impersonate authoritative damage state. Material quality is cosmetic under the owning design; it does not grant support or health bonuses.

## Canonical reference frame

- Units: meters in briefs, with exact conversion recorded for future Blender/Unreal assets. One structural cell has edge **0.5 m**.
- Construction-local axes: X = module length/run, Y = depth, Z = up. Record the later engine/model axis conversion explicitly. Origin/pivot is the minimum grid corner of the declared module envelope; an animated door additionally names its hinge pivot.
- For a one-cell primitive use normalized coordinates `(u,v,w)=(x,y,z)/0.5`, each in `[0,1]`. Internal fractional surfaces do not create a different world placement grid. Anchors remain on the 0.5 m grid.
- Default structural orientation is grid-aligned; enumerate authored 90-degree rotations and mirrors. An orientation that changes a partial shape's axis requires a named compatible mask. Do not use arbitrary rotation or artist scaling to repair a mismatch.
- Each definition records its occupied boundary polygons on `−X,+X,−Y,+Y,−Z,+Z`; face contact, not bounding-box adjacency, determines possible structural transfer. Openings and empty portions remain empty in every skin.
- For the initial reference set, surface variation stays inside the canonical outer envelope. Joints, openings, contact planes and clearances stay fixed. Larger trim, overhangs or different silhouettes are separately defined attachments/geometry variants, with their own collision/access checks.
- Formulas below describe versioned authored geometry, not operator-editable expressions. Collision, occupancy, support and shelter masks must later be authored and tested against the selected geometry; concept art is not their source of truth.

## Finite reference shape set — proposal v1

These document-local names are reference keys, not Registry registrations. Bounds are X × Y × Z; rotations may orient the same compatible definition differently. A skin never changes the stated solid.

| Reference key | Exact nominal geometry | Purpose / interface check |
| --- | --- | --- |
| `cell-full` | 0.5 × 0.5 × 0.5 m; solid full normalized cube | Repeated foundations, walls, floors and massing; six full square contacts |
| `slab-half` | One-cell envelope; solid `w ≤ 1/2` | Half-height layer; authored axis variants supply matching thin panels |
| `slab-quarter` | One-cell envelope; solid `w ≤ 1/4` | Quarter-height layer/detail; no implied smaller placement grid |
| `post-quarter` | One-cell envelope; solid `u ≤ 1/2` and `v ≤ 1/2`, full height | Quarter-volume post; its smaller contact area must remain visible in the geometry definition |
| `slope-1to1` | One-cell envelope; solid `w ≤ u` | 0.5 m rise/run wedge; roof/ramp reference, without a walkability claim |
| `corner-outer` | One-cell envelope; solid `w ≤ min(u,v)` | Convex two-slope corner; verify contact contours against wedge neighbors |
| `corner-inner` | One-cell envelope; solid `w ≤ max(u,v)` | Concave two-slope corner; union of the matching wedges, not a filled outer-corner skin |
| `step-two` | One-cell envelope; solid `w ≤ 1/2` for `u < 1/2`, otherwise full height | Two 0.25 m treads/risers; repeatable stair geometry study. Gameplay navigation/headroom acceptance remains separate |
| `arch-2m` | Envelope 2 × 0.5 × 3 m. In XZ, outer shape is rectangle `[0,2]×[0,2]` plus upper semicircle centered `(1,2)`, radius 1 m. Subtract opening rectangle `(0.5,1.5)×[0,2]` plus upper semicircle same center, radius 0.5 m. Extrude through Y | Matching jambs and semicircular opening, unchanged by skins. This is a structural opening study, not a replacement for the ordinary functional door definition |
| `curve-quarter-r2` | Envelope 2 × 2 × 0.5 m; first-quadrant quarter-annulus `1.5² ≤ x²+y² ≤ 2²`, full Z height | 0.5 m-thick curved wall course. Four 90-degree copies form a ring; straight ends must meet matching wall-thickness profiles |

For partial shapes, distinguish the **cell/module envelope** from the actual solid bounds. An empty part of a cell is not collision or support. Arch/curve definitions span multiple cells; later runtime masks must address those members without changing the outer shape or inventing full-cell occupancy. Derived corner/end/junction variants are separate records when their occupied volume differs.

## Assemblies and the planned complexity

The construction system remains voxel-based, with functional objects as a separate layer. Foundation/wall/floor/roof visual examples are recipes composed from the reference geometry, not a replacement four-piece building system.

| Coverage family | Required references / interfaces |
| --- | --- |
| Foundations, floors, ceilings, walls and framing | Repeatable courses, full/partial cells, posts/beams, corners, exposed ends and intersections; show contact faces and material transition seams |
| Roofs and vertical circulation | Slopes, hips/valleys, ridges/eaves/end conditions, stairs/landings, ramp transitions and headroom. New trims need explicit attachment bounds |
| Openings and curved structures | Rectangular and arched openings, matching door/shutter/gate frames, curved courses/tower segments, joins between straight and curved geometry |
| Structural/environment states | Supported/unsupported examples, surface damage/repair reference, roof/wall enclosure, chimney/smoke paths, hearth lining and weather-facing seams; inherit existing mechanics |
| Essential functional content | Door, bedroll/bed, chest/Builder's Cache, campfire/hearth, furniture/comfort and five workshop kinds with their existing footprints and interaction approach |
| Advanced functional content | Windmill/waterwheel/generator, battery/heater/pump/tank, conveyor/wire/pipe, switch/sensor/logic/delay/alarm/actuator, defenses/siege/ownership anchor; separately indexed bodies, ports, moving parts and placement/access envelopes |

Use the exact functional dimensions and clearance rules in Construction §16 instead of forcing every object into a one-cell block. Respect bootstrap/unlock rules: geometry coverage does not make every shape/material/function available to a new character. Existing support, heat/smoke/fire, weather, shelter, repair/salvage, permissions, power/fluid networks and siege behavior remain in scope; pretty buildings alone do not complete the construction library.

## Batch order and variant matrix

The asset session owns its catalog and batches; integrate this addition there rather than editing another session's claimed rows from here. Keep existing essential jobs moving and add a finite construction subqueue with an explicit scope revision.

1. **Geometry briefs:** establish the ten reference records, orthographic dimension/contact diagrams and neutral unornamented views, in batches of 3–5. These diagrams establish intended art geometry; they do not prove runtime masks.
2. **Material grammar:** one swatch/detail study per existing structural tier, including a common scale and contact/joint rules. Swatches are material references, not completed shape assets.
3. **Compatibility pilot:** depict `cell-full`, `slope-1to1`, `arch-2m` and `curve-quarter-r2` in rough timber and stone masonry: eight separately indexed shape/material concept packages. Identify combinations as coverage studies, not gameplay unlock approvals.
4. **Cosmetic pilot:** establish `cell-full` in worked timber with a plain skin, then add its ornamented appearance-only variant: two packages sharing one shape/material profile. Keep dimensions and interfaces identical.
5. **Coverage expansion:** complete remaining shape/material combinations justified by the owning progression/content plan, then functional families and more elaborate architectural styles. Record the finite matrix and missing essentials; do not claim the pilot exhausts construction.

Reuse the four completed shelter concepts as style references or scene studies. If a selected image conflicts with a geometry brief, keep its historical version and create a new directly linked revision or geometry-fit task. Do not silently stretch a model, relabel an incompatible shape as a skin, overwrite a benchmark, or mark old artwork numerically validated.

Each indexed component/variant records: stable asset ID; source sections; `geometry_ref/version`; proposal/acceptance status; unit frame, envelope, solid and pivot; permitted orientations; interface/contact diagram; material-profile reference; cosmetic-skin reference; functional definition/ports and clearances if applicable; exact image package and compatibility-test references. Geometry/interface changes create a new version; cosmetic variants retain the same geometry reference.

## Required outputs and acceptance

For each component retain the normal concept sheet, separate clean full-asset image, exact prompts/references/hashes and real image metadata. Add front/side/top views, dimension annotations, a contact/opening diagram, and a neutral geometry silhouette. For each pilot variant, compare against that same silhouette and show an assembled repeat/corner/opening sample; identify mating components by geometry reference. Complex/hidden geometry needs a section or exploded view.

Image QA checks silhouette, contact planes, intended measurements, unobstructed apertures, repeated joins, tile direction, skin consistency and material-tier readability; discrepancies are actionable defects. Label diagram measurements as design targets rather than measured image geometry. Keep image completion, geometry-definition acceptance, future numerical mesh-fit verification and runtime collision/support/navigation acceptance as separate statuses. A scene study cannot substitute for individually indexed components.

For later modeling, start from deterministic geometry matching the record, then apply the approved appearance. Verify dimensions, pivots, rotated/mirrored joins, occupied/contact masks, collision and functional clearances numerically before engine readiness. No image-to-mesh inference result may redefine those interfaces.

## Addendum for the existing image-session prompt

Paste this after the current instructions, or ask that session to read this file:

```text
Also read docs/art-templates/player-construction-kit-contract.md.
Include the player-construction library in the finite indexed asset scope.
Preserve the existing 0.5 m voxel system, complex shape families, five
gameplay material tiers and functional/automation/environment families.
Use its explicit proposed reference shapes and interface records; distinguish
geometry acceptance from image completion. Produce reusable shape concepts
with multiple material appearances and cosmetic skins without changing the
same shape's occupied volume, joints, openings, collision or clearances.
Follow its geometry/material/compatibility pilot order in 3–5-asset batches.
Reuse suitable completed shelter artwork as styling references; preserve
its history and flag unresolved geometry rather than claiming numerical fit.
Keep individual components, skins and assembled demonstration scenes indexed
separately. Use the existing provenance/visual-review/package workflow.
Generate images only; do not run Blender, change gameplay or environments,
commit/push, or modify another session's claimed work.
```
