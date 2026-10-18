# Player construction geometry and skin contract

User direction, 2026-10-18: include reusable player-construction building blocks in the asset library; define known shapes that accept multiple skins while preserving the planned complex construction system. This supplements `docs/art-templates/generate-images.md` and `docs/art-templates/asset-planning.md`, which are owned by the active image session and may still be uncommitted. It authorizes concept-image planning, not runtime geometry implementation or changes to gameplay rules.

## Source authority and current gap

[Construction Architecture §§3.1–3.5, 8–9, 16](../Game-Design/Legends_Shattered_Frontier_Construction_System_Architecture_v0.1.md) owns the 0.5 m grid, partial/sloped/corner/arched/curved shapes, structural support, shelter, material tiers, progression and functional catalogue. [Construction Integration §§3–4, 8](../Game-Design/Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md) owns versioned shape/material references, addressed voxel assemblies and authoritative validation/durable effects. These documents take precedence over art briefs.

The sources name shape families but do not enumerate their exact masks, pivots or dimensions. The four existing shelter concepts in the other session's `docs/art-templates/asset-lists/basic-greenlands-shelter-03.md` explicitly leave connection sizes unresolved. Preserve their images/history and use suitable styling; they are not verified modular geometry. Record geometry-fit review separately from image-complete status.

The [BLD-01 structural geometry interface](../decisions/BLD-01-voxel-geometry-interface.md) now locks the cell/solid, brush, surface and mesh compatibility contract. The **reference geometries below remain authoring targets**, not approved compiled masks, production content IDs, unlocks or cost/support values. Record `geometry-proposed` until GEO-01 supplies the approved per-cell compiled manifest and applicable GEO-02 compatibility evidence; never infer acceptance from an image. Arches and curves compile into independently editable member cells, not indivisible prefabs. Analytic circles are visual targets; the accepted polygonal solid and its clearance/error report govern final meshes. Final modeling uses approved deterministic base geometry and artwork for appearance. Image generation alone cannot certify dimensions or fit; engine-ready status additionally requires GEO-03.

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
- Structural orientation is grid-aligned; enumerate the shape's allowed proper cube rotations in 90-degree steps under BLD-01. Reflections require separately authored geometry; do not use negative scale, arbitrary rotation or artist scaling to repair a mismatch.
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

## Complete architectural material kits

Each architectural look is a **coordinated material kit** with a stable kit ID/version, one structural material-profile reference and an explicit cosmetic-style reference. It supplies matching treatment of surfaces, exposed edges, joints and openings across its compatible shapes. A swatch or one attractive wall is only a kit study; record missing components until the declared kit scope is complete.

| Required component family | Matching treatments to define and depict |
| --- | --- |
| Main faces | Exterior/interior wall faces, floor/top surfaces and undersides; common grain/stone-course scale, direction, palette and surface finish |
| Corners and exposed ends | Matching inside/outside corners, convex/concave slope corners, edge returns, end caps, beam/post ends and exposed partial-cell surfaces |
| Structural joints | Horizontal/vertical seams, stacked courses, wall-to-floor/ceiling joins, beam/post connections and T/cross intersections; declare which treatments are shared and which need a separate variant |
| Openings | Rectangular and arched jambs/reveals, lintels or arch rings, sills, thresholds and frame interfaces; keep the specified clear opening and door/shutter/gate interfaces intact |
| Roof and slope transitions | Matching slope faces, hips/valleys, ridge/eave/end treatments and wall-to-roof seams; separately index any attachment with different geometry |
| Curved surfaces | Matching inner/outer curve treatments, curved courses and straight-to-curve joins; preserve declared radii and contact faces |
| Boundaries between materials | Foundation-to-wall and timber-to-masonry or other selected tier transitions; each transition names both source kits and its geometry/interface references |

For each kit, record a finite component/variant coverage matrix: required role, compatible `geometry_ref/version`, source kit(s), material/style references, scale/orientation/joint rules, selected image package, state and remaining defects. Shared images may be referenced where they genuinely explain several roles; do not count one scene as multiple completed component assets. Mark a role not applicable only with a geometry-based reason and a named alternative that covers the assembly need. Do not omit difficult intersections to claim a complete kit.

Keep the same neutral geometry/contact references across skins. Stone courses, timber joints and ornament must meet consistently at module boundaries without moving contact planes, filling openings or changing collision/support. An appearance needing a different solid, silhouette envelope or clearance becomes a separately versioned geometry/attachment proposal. Material transitions do not merge gameplay profiles or confer the adjacent tier's properties.

Required kit QA includes an assembled test panel showing repeated faces across multiple cells, stacked and rotated joins, inside/outside corners, a T intersection, a framed rectangular opening, an arch, and slope/curve transitions applicable to the kit. Include a cross-material join using two named kits. Show close-ups of seams and a dimension/contact overlay; retain separately indexed component references. Label intended measurements as targets. Image QA verifies visual coherence; later mesh and engine tests verify numerical fit, collision, support and access.

## Batch order and variant matrix

The asset session owns its catalog and batches; integrate this addition there rather than editing another session's claimed rows from here. Keep existing essential jobs moving and add a finite construction subqueue with an explicit scope revision.

1. **Geometry briefs:** establish the ten reference records, orthographic dimension/contact diagrams and neutral unornamented views, in batches of 3–5. These diagrams establish intended art geometry; they do not prove runtime masks.
2. **Material-kit scope:** register a baseline kit for each of the five existing structural tiers and freeze its required component matrix. Start with rough timber and stone masonry; queue worked timber, reinforced masonry and metal-framed masonry explicitly. Produce swatches/detail grammar first, then complete matching faces, corners, joints, openings and applicable roof/curve transitions. A swatch does not complete a kit.
3. **Compatibility pilot:** depict `cell-full`, `slope-1to1`, `arch-2m` and `curve-quarter-r2` in rough timber and stone masonry: eight separately indexed shape/material concept packages. Add the kit assembly/seam tests above; these packages alone do not complete either kit. Identify combinations as coverage studies, not gameplay unlock approvals.
4. **Cosmetic pilot:** establish `cell-full` in worked timber with a plain skin, then add its ornamented appearance-only variant: two packages sharing one shape/material profile. Keep dimensions and interfaces identical.
5. **Coverage expansion:** complete the declared baseline material-kit matrices and remaining shape/material combinations justified by progression/content, then functional families and more elaborate architectural styles. Each new architectural style receives its own matching-component matrix and assembly checks. Record missing essentials; neither the pilot nor a collection of swatches exhausts construction.

Reuse the four completed shelter concepts as style references or scene studies. If a selected image conflicts with a geometry brief, keep its historical version and create a new directly linked revision or geometry-fit task. Do not silently stretch a model, relabel an incompatible shape as a skin, overwrite a benchmark, or mark old artwork numerically validated.

Each indexed component/variant records: stable asset ID; source sections; `geometry_ref/version`; proposal/acceptance status; unit frame, envelope, solid and pivot; permitted orientations; interface/contact diagram; material-profile reference; cosmetic-skin reference; functional definition/ports and clearances if applicable; exact image package and compatibility-test references. Geometry/interface changes create a new version; cosmetic variants retain the same geometry reference.

## Required outputs and acceptance

For each component retain the normal concept sheet, separate clean full-asset image, exact prompts/references/hashes and real image metadata. Add front/side/top views, dimension annotations, a contact/opening diagram, and a neutral geometry silhouette. For each pilot variant, compare against that same silhouette and show an assembled repeat/corner/opening sample; identify mating components by geometry reference. Complex/hidden geometry needs a section or exploded view.

Image QA checks silhouette, contact planes, intended measurements, unobstructed apertures, repeated joins, tile direction, skin consistency and material-tier readability; discrepancies are actionable defects. Label diagram measurements as design targets rather than measured image geometry. Keep image completion, geometry-definition acceptance, future numerical mesh-fit verification and runtime collision/support/navigation acceptance as separate statuses. A scene study cannot substitute for individually indexed components.

For later modeling, start from deterministic geometry matching the record, then apply the approved appearance. Verify dimensions, pivots, rotated/mirrored joins, occupied/contact masks, collision and functional clearances numerically before engine readiness. No image-to-mesh inference result may redefine those interfaces.

## Addendum for the existing image-session prompt

Paste this after the current instructions, or ask that session to read this file:

```text
Also read docs/art-templates/player-construction-kit-contract.md and
docs/decisions/BLD-01-voxel-geometry-interface.md. BLD-01 locks the structural
interface; final meshes require approved GEO-01 geometry and GEO-02 fit checks.
Include the player-construction library in the finite indexed asset scope.
Preserve the existing 0.5 m voxel system, complex shape families, five
gameplay material tiers and functional/automation/environment families.
Use its reference shape targets and locked interface records; distinguish
geometry acceptance from image completion. Produce reusable shape concepts
with distinct gameplay material profiles and appearance-only cosmetic skins.
Retain each material tier's gameplay identity; skins change appearance only.
Preserve the same shape's occupied volume, joints, openings, collision and
clearances across these variants.
Create complete architectural material kits, not just isolated surface looks.
For each kit, index matching main faces, inside/outside corners, exposed ends,
structural joints, rectangular/arched opening treatments, and applicable
roof/slope/curve transitions. Name both kits at cross-material boundaries.
Keep a finite required-component matrix for each kit/material/style and track
its missing parts; a swatch, single wall or demonstration scene cannot
complete the kit. Start with rough timber and stone masonry; explicitly queue
the other three structural tiers. Validate repeated/stacked/rotated joins,
corners, T intersections, openings and cross-material seams in assembly
studies, keeping each component separately indexed. Preserve the fixed shape
and functional clearances across skins; separately version geometry changes.
Follow its geometry/material/compatibility pilot order in 3–5-asset batches.
Reuse suitable completed shelter artwork as styling references; preserve
its history and flag unresolved geometry rather than claiming numerical fit.
Keep individual components, skins and assembled demonstration scenes indexed
separately. Use the existing provenance/visual-review/package workflow.
Generate images only; do not run Blender, change gameplay or environments,
commit/push, or modify another session's claimed work.
```
