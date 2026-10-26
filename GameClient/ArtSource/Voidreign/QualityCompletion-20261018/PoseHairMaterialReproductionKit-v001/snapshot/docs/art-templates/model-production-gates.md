# Required model acceptance gates

This is the mandatory execution checklist for [model-pipeline.md](model-pipeline.md), including reused models and prototypes. Execute the applicable checks during normal model work; the user does not need to request a separate agent review. The authoring agent records its visual inspection. Another reviewer is optional unless the asset's approved acceptance profile explicitly requires one. An undecided design requirement remains open; do not invent user approval or add a new approval step for routine authoring.

This document specifies required behavior. It does not install an unattended worker or make existing tools enforce these gates. Use the implementation table below to distinguish a tool result, an author-performed check and an unimplemented check. Missing mandatory evidence prevents acceptance.

## 1. Declare the acceptance scope before work

Create an immutable acceptance profile/brief linked from the job's `artifacts` and generation profile. Pin its SHA-256 in the attempt manifest. The prototype defaults in `generate-model.md` and a `prototype-authored` surface profile do not define production budgets. Required fields are:

| Requirement | Record before the applicable stage |
| --- | --- |
| Intended use | Prototype/import/engine target, asset category, deliverable formats, intended placement and gameplay visual role; owner of unresolved decisions |
| Design comparison | Direct primary reference/package/hash, approved secondary views, silhouette landmarks, construction/material identity, hidden-side assumptions and tolerated differences |
| Scale and support | Dimensions in metres, allowed dimension error, axes, pivot, transforms, intended ground plane, authored support regions, contact/gap limits and intentional raised parts |
| View conditions | Closest expected camera distance, ordinary and distant views, field of view, target output resolution, reference comparison framing and required closeups |
| Geometry and UVs | Triangle/vertex/material/section budgets, representation and simplification rules, UV channels, coverage, padding, texel density and intentional overlap/mirrors; open/two-sided surfaces where intended |
| Materials | Material classes, required maps/channels, color spaces, normal convention, texture/streaming budgets, opacity/two-sided rules, relief scale and depth-method applicability |
| Attachments and motion | Semantic parts/parents/roots, permitted separation, rigid versus deforming regions, requested clips/cases/duration/frame rate, surface/contact limits and collision proxy limitations |
| Engine use | Exact UE version/build, platform/hardware/API, renderer and scalability paths, Nanite/LOD/collision needs, representative instance counts, memory/frame-time budgets and engine integration owner |
| Check policy | Required check IDs with limits/units and evidence method, allowed skips, visual acceptance criteria, review owner, maximum attempts and effective-time rules |

Declare which fields/checks apply to the current target and which remain future production requirements. A prototype can use the documented prototype scope while retaining explicit open production budgets; it must still satisfy its own declared shape, artifact and review criteria. A production request cannot use that exemption to omit necessary requirements.

Production limits must come from the applicable project/category authority or a reviewed asset profile. If absent, write `needs-review` and the smallest missing decision. Prototype exploration can continue within scope; an asset cannot claim production readiness without these limits. Each adjustable operating value uses a declared type, units, default, safe bounds, permission and effective time; formulas and algorithms stay in versioned tools/content. An unset displacement magnitude is disabled and unresolved, not a safe production value.

For Windows PC engine acceptance, apply the [graphics scalability policy](pc-graphics-scalability.md): 2560×1440 output/60 fps, adaptive device/quality profiles and representative scene evidence. Geometry/texture budgets above are scoped to the asset, tier and workload; no universal triangle count or atlas dimension is imposed. The separate [machine baseline candidate](../evidence/pc-graphics-baseline-20261018.md) records hardware without proving performance or defining minimum specs.

The job schema has no `production-ready` state. Use its existing `quality_target` and states; record the specific production criteria and their result in linked acceptance evidence. `engine-validated` covers only the named engine/hardware/use cases, and does not imply every production requirement is satisfied.

## 2. Reconcile identity and provenance before launching

Use direct catalog → job → reference-package links. Keep image-session ownership, selected reference history and completion rows unchanged. Reuse a suitable source/export first, then apply the same applicable gates as generated work.

Pin and compare these identities before starting a batch and before every attempt:

1. Original reference bytes/path/hash and package manifest; source permissions or their explicit unresolved status.
2. Every prepared-input candidate, mask and report: source hash, native/prepared dimensions, crop, alpha/background method and settings, orientation and color handling. Visually inspect alpha edges, thin features, ground contact/shadows and complete silhouette.
3. **Effective inference input** bytes/hash after preprocessing, plus the runner's actual argument values. The original reference hash alone does not identify what the model consumed. Record further runner resizing/alpha handling, camera estimation and effective settings.
4. Backend repository/commit, weight identities/hashes, environment/runtime/library versions, device/driver, local compatibility patches, exact runner and helper snapshots/hashes. Pin Blender/exporter/importer versions and export settings separately.
5. Generation, material, render, acceptance and conditional motion profiles by path/hash/revision, seed/sampler/geometry/texture settings and export targets. Match the lock, profile, command, native run report and output manifest.

Any hash/settings disagreement is a `provenance.coherent` failure before launching the next dependent job. Resolve whether it is a stale record or different computation from saved bytes/logs. Preserve the original attempt and write a linked reconciliation/addendum; never replace historical hashes with guesses or claim a run used a script revision merely because it is installed now. If the effective input/tool revision cannot be recovered, mark that attempt unverifiable and require a new authorized attempt within the remaining budget. A post-hoc hash identifies inspected bytes, not producer attestation.

Claim the job and shared GPU ownership before inference or GPU rendering. Record owner, actual UTC lease times, process/output identity and release. Check other active owners and workers; an expired timestamp does not authorize terminating them. Only one local GPU inference/render job runs at a time, including subagents. Where the batch owns GPU generation and Voidreign is CPU-only, use confirmed CPU modes for concurrent work and retain the existing benchmark. Leases and serialization are manually coordinated today.

## 3. Record the evidence while executing each gate

Every attempt has a new immutable folder with input/tool/profile snapshots, effective command/settings, UTC timing, logs/exit status, outputs/hashes, previews, native reports, author inspection and remaining defects. A crashed process still gets its available log/crash evidence. Keep large selections and measurements in artifacts, with direct paths from the job.

Use a linked `gate-summary.json` or equivalent machine-readable attempt manifest. This is a **documented report contract, not a supplied schema or report generator**. For every required check record:

`check_id`, owning schema stage, attempt/revision, applicability, method (`tool`, `author-visual`, `manual-measurement`, `unimplemented`), tool/reviewer identity, input/effective-input/source/profile/tool hashes, expected limit/units, measured result, `passed|failed|needs-review|skipped`, exact evidence paths/hashes, inspected views/frames, reason and next action.

Place these reports in stage `check_paths`; link acceptance/provenance summaries through `artifacts`. Keep `model-job.schema.json` field names and enums. Do not add invented job fields or rename stages to match this checklist. `reference` owns brief/preflight/provenance readiness; `delivery` owns fresh-source/exchange/engine acceptance for the requested scope. Native tool reports remain intact beside the summary.

| Gate and owning stage | Evidence required to pass |
| --- | --- |
| `reference.ready` / `reference` | Identity chain above, brief/acceptance scope, inspected original and effective alpha input, structural GEO-01 hash and applicable GEO-02 evidence when needed, capability/runtime compatibility and ownership |
| `provenance.coherent` / `reference` | Original/prepared/effective input and runner/helper/runtime/lock/profile/command/run identities agree; unresolved historical disagreement has a linked reconciliation and blocks dependent acceptance |
| `geometry.reference` / `geometry` | Editable/raw geometry, measured scale/support/orientation, reference-aligned silhouette and landmark comparison, clay and calibrated material views; explicit findings for missing/fused/wrong forms, ground sheets and tilted parts |
| `geometry.hygiene` / `geometry` | Finite geometry and triangle/topology diagnostics, loose objects/elements, indexed and seam-aware islands, flipped/degenerate/duplicate/thin surfaces; classify candidate defects before editing |
| `topology.attachments` / `topology_uv` | Complete component disposition and semantic attachment graph, source-scoped selections/annotated views, repair/remap evidence, relevant rest/deformation checks; no unexplained fragments |
| `topology.uv` / `topology_uv` | UV coverage, overlap/mirror disposition, density/padding/seam checks at target scale, post-simplification silhouette and correspondence against accepted geometry |
| `materials.surface` / `materials` | Map manifest, actual channels/dimensions/color spaces/normal convention, calibrated/unlit/opposing-light inspection, seam/hidden-side coverage and reviewed relief scale; missing requested maps remain open |
| `rig.integrity` / `rig` | Reviewed semantic/root/protected selections, skeleton/pivots, rest shape, weights, rigid-region preservation and actual-surface attachment checks; skip only if irrelevant |
| `motion.surfaces` / `secondary_motion` | Full required timeline/cases, actual surface gaps/penetration/stretch/rigidity, roots and transitions, guide trajectory where used, preserved bake/profile; requested motion cannot be accepted from particles/bones alone |
| `delivery.source` / `delivery` | Fresh-process reopen using compatible Blender, inspected saved-source render/playback and resolved dependencies; crash or unreadable source fails this check |
| `delivery.correspondence` / `delivery` | Every requested format freshly imported into empty scene, importer/settings/version, geometric/semantic/UV/material/scale/animation correspondence and inspected views; limitations of each metric explicit |
| `delivery.engine` / `delivery` | Authorized exact UE build import, material/geometry/lighting/scalability comparisons, target hardware and representative instance performance evidence; remains open when engine work is outside scope |

Engine checks are required for an engine claim and the applicable production criteria. If this session delivers only a prototype, record unperformed engine work as an open future gate without claiming engine acceptance; it need not prevent the separately scoped prototype result. Distinguish work outside the current target from an inapplicable feature: a static prop can skip rigging, while a missing required normal map cannot be skipped merely because it has not been authored.

Derive stage state from **all applicable required checks**, including visual findings. A completed inference is not a passed geometry stage. A required failed check means `repair-required`, or `blocked`/`needs-review` when exhausted or awaiting a decision; a missing/unimplemented/unresolved check means `needs-review` unless a concrete blocker warrants `blocked`. `passed` requires every required applicable check passed; `skipped` requires an explicit inapplicability reason. A successful tool subcheck may remain passed inside a stage that is under review. Do not hide a geometry failure solely under topology or materials.

When reviewing legacy inconsistent summaries, preserve original native evidence and attempt history, append a dated reconciliation and correct the current job/catalog state. A schema-valid JSON, zero exit code or successful render does not establish a quality pass. Delivery of a labelled prototype may include open production gates; distinguish the prototype scope from the production claim.

## 4. Calibrate reference comparison before judging color/detail

Pin a render profile and scene: source hash, engine/device, dimensions/units, camera matrices/projection/crop/resolution, background, light positions/size/intensity/color, environment, view transform/look/exposure/gamma and texture interpretation. Keep geometry cameras aligned for A/B; an artistic reference may lack known lens/lighting, so document the approximation instead of inventing pixel similarity.

Render neutral gray and color controls with the asset, plus unlit base-color, clay, normal/roughness/metallic diagnostic views where applicable. Check controls for clipping or tint before diagnosing pale/washed-out textures. Compare neutral and opposing light directions, close material crops, front/both sides/rear/top/three-quarter and contact/attachment closeups; use intended gameplay distances as well as inspection closeups. Save the rendered pixels and inspection notes, not only scene settings.

The current `Review-GeneratedAsset.py` uses fixed softboxes, AgX Medium High Contrast and exposure `1`, with a separate clay exposure. It creates useful previews but does not calibrate photometric/color controls or automatically establish fidelity. Lighting/exposure could contribute to the Greenlands washed-out appearance; this is unconfirmed. Separate texture color from illumination before repainting.

Record fidelity by silhouette, proportions/orientation, distinctive construction, material identity/color, support and attachment, hidden surfaces and gameplay-distance readability. State each as accepted/different/unresolved with reference/render paths and the owning defect. Do not invent a similarity percentage or let an attractive beauty view overrule a failed diagnostic view.

## 5. Resolve components and attachments before repair

Inventory every asset object and within-mesh island. Preserve raw indexed counts and exact-coincident connectivity; UV/normal seam splits can inflate raw counts, while exact coincidence can connect intentionally touching parts. Preserve source/object/mesh hash, stable component IDs and element selections, counts, bounds/centroid/units, nearest relevant surface and measured or sampled gap method. The generic validator lists only a bounded set of representative indexed islands; use the dedicated inventory for complete review.

For **every** component, loose element and suspected thin bridge, record semantic role, expected parent/root, permitted separation, evidence views and one disposition: `intentional-separate`, `attached-by-rig-or-constraint`, `unexpected-detached`, or `unresolved`. Large inventories can use explicit component-ID groups with one shared rationale, provided coverage accounts for every ID and exceptions are listed. A count, size threshold, proximity, low visibility or parenting alone cannot classify a component. Include depth-tested, isolated and section views when ordinary views conceal it; examine relevant poses because hidden pieces can become visible.

Repair only reviewed source-hashed selections: reconnect/rebuild intended structure, correct roots/constraints/weights, or remove confirmed debris. Preserve before/after geometry and new-to-old correspondence, then inspect silhouette, retained surfaces, UVs/normals/materials and relevant motion. Thin bridges, approximate nearest-main-surface gaps and virtual attachment graphs require explicit review of the actual surface behavior. Never weld all seams, delete all small/hidden components or expand coordinate masks to force a result.

After remesh/simplification, deformation and each exchange reimport, repeat the relevant attachment checks using semantic correspondence. Do not reuse stale vertex/component IDs. Static assets skip animated cases with a reason, but still require rest/support/export checks. General semantic classification, thin-bridge detection and reusable animated attachment validation remain unimplemented; an applicable gate requires recorded manual inspection/measurements or stays open.

## 6. Route surface depth and engine quality deliberately

Use the [UE 5.8 surface profile](model-profiles/unreal58-surface-quality-20261018.json) as a prototype starting point and the surface route in `model-pipeline.md`. Verify feature availability against the installed engine build and official version documentation during engine integration. A target string or feature list does not establish that a project uses it successfully.

Keep silhouette-level broken stone, major ore masses, joined timber construction, trunk/branches and leaf silhouettes in geometry. Material work supplies small relief and finish; changing albedo cannot repair a tilted marker, fractured ore form or detached canopy. Surface-only references remain material work on a named base. Structural geometry still requires approved GEO-01 manifests and applicable GEO-02 evidence.

For each material save source/bake provenance for base color, normal, roughness, metallic where appropriate, opacity, and optional AO/height. Record resolution, precision, packing, UV channel, physical scale and measured/derived/generated status; evaluate seams, hidden-side coverage and lighting baked into albedo. Do not treat a grayscale albedo as verified height. Resolve OpenGL/DirectX tangent-normal convention explicitly at UE import and validate directional relief under changed light; do not silently assume the current profile encoding is the engine contract.

Create matched variants on the same accepted mesh/UVs: base surface, normal-only, and justified parallax or displacement. Pin measured relief magnitude/center/reference plane and compare grazing angles, silhouettes, intersections, seams, bounds, contact and shadows at near/ordinary/far views. Keep a disabled baseline. Accept a depth method only if its improvement and measured cost meet the asset's criteria. Record an inapplicability reason when height/displacement would not help. Strong parallax and displacement need an explicit combined comparison before sharing the same relief.

For foliage evaluate the actual leaf representation, opacity, normals/two-sided response, close/distant silhouette, masked material cost and shadow stability. Apply wind/WPO only when requested; then repeat attachment, deformation, bounds and shadows across motion extremes. Nanite or foliage options do not automatically solve bulky canopy geometry or reduce every cost.

In the authorized UE session, save the exact import settings, scale/pivot/orientation, asset/material paths, compression/sRGB/normal settings, collision/LOD/Nanite choices and dependencies. Use fixed exposure/cameras and compare supported Nanite, Lumen and VSM/scalability paths with representative terrain/backgrounds. Capture stills and motion for shimmer/popping/shadow artifacts, texture streaming/mips and intended instance density. Record hardware/API/build, resolution, timing method, warmup, GPU/CPU frame time, memory and asset counts against declared budgets. Test the intended lower scalability/platform path too. Unsupported or unmeasured features stay open/disabled; enabling every feature is not a quality criterion.

## 7. Prove delivery correspondence and close the ledger

Open the saved source with a compatible runtime in a fresh process. Record failures rather than changing the source to hide a version problem. The Waystone 5.2.2 source failing under Blender 5.1.1 is a compatibility blocker; existing previews do not replace a successful reopen/render.

For each new export, compare freshly imported geometry with the accepted source in a common unit/transform frame: bounds/scale/pivot/orientation, bidirectional surface coverage/deviation, missing/extra objects/parts, normals, UVs, materials/textures and semantic attachments. Preserve exact retained-geometry/attribute maps where topology is retained; use an explicit reviewed surface correspondence when exporter topology changes. Counts and bounds alone can miss swapped, missing or wrongly placed parts.

For motion also compare rest pose, hierarchy/connectivity, weights, clip start/end/rate/duration, all required bone/guide trajectories and actual deformed surfaces at sufficient frames and transitions. State unsampled intervals. Import-contract adapters are versioned deliverables with source/export hashes; test default import first and record when a target application needs an adapter. A Blender-specific fix does not prove UE import behavior.

Apply one initial attempt plus at most two repairs **per owning failed stage**, or the stricter profile limit. Record each actual repair/rebuild, changed variables and invalidated downstream evidence. A rejected preprocessing candidate belongs to the reference-stage history; an inference that never launched is not a geometry run. Preserve legacy folder names/counters and reconcile their meaning rather than renumbering history. Reinspection/read-only diagnosis is not a repair. New filenames, profiles, subagents or a later session do not reset an exhausted stage; further work requires an explicitly revised authorized scope.

On exhaustion, missing capability/authority or essential unknown, save exact check/blocker, attempt count/paths, last usable source, smallest required fix/decision, owner and next action. Release owned resources and continue another ready asset or independent stage. Never loosen limits, conceal defects, mark missing checks passed or silently substitute a different backend/reference.

Before handoff, ensure job/catalog agree, profile and effective-input evidence cohere, all required gates have a disposition, source/export/previews/report hashes resolve and failed history remains linked. Invalidate downstream evidence after dependency changes. Publish the requested scoped result with open gates visible; visual preview delivery alone cannot promote it.

## Implementation boundary and automation backlog

| Capability | Available now | Required completion method / missing implementation |
| --- | --- | --- |
| Records | `model-job.schema.json`, job/catalog conventions | Schema validation and status/evidence reconciliation must be invoked/recorded; no queue/lease/gate state machine enforces them |
| Input/inference | Explicit paths/names in Pixal runner; per-image matte/report in `prepare_model_input.py` | Author reviews input; reconcile actual script/profile/run/effective-image hashes; generic preflight/provenance validator remains to implement |
| Preview/basic reimport | `Review-GeneratedAsset.py`, `Validate-GeneratedAsset.py` | Validator checks source reopen/render, images/UV existence, dimensions and triangle counts; author checks appearance. Missing formats must fail their required gate. No general UV-quality, finite/topology, semantic or surface-correspondence pass is supplied |
| Component diagnostics | `Inspect-MeshComponents.py`, plotting/explicit guarded removal tools | Invoke complete inventory; author classifies all parts, reviews near-contact/thin bridges and verifies attachments. Automatic semantics and generic integration are absent |
| Deeper geometry/motion tools | Triangle/coincident/visibility/support/correspondence and feature-motion tools listed in the quality runbook | Inspect exact applicability and pin per-asset profiles. Voidreign profile counts/masks/rigs/checks are evidence for that asset; they are not default acceptance for Greenlands |
| Calibrated visual/PBR review | Saved views, texture guide/projection tools for supported authored profiles | Author pins calibration/diagnostics and checks maps; generic control-scene/calibration, complete PBR/height authoring and automatic fidelity acceptance are absent |
| Engine acceptance | UE surface-quality profile and comparison requirements | Authorized engine operator performs imports/renders/performance checks. No generic UE acceptance runner or completed Greenlands engine evidence exists |
| Scheduling/retry enforcement | Written owner/lease/attempt rules | Agent coordinates and records manually. Implement atomic leases, GPU arbitration, attempt/dependency tracking, native-report normalization and fail-closed promotion before claiming unattended operation |

Manual checks are required work during the batch, not a reason to wait for the user to ask for review. If they cannot be performed with the available evidence/tools, record the exact open gate. Implementation of the backlog is separate from documenting these requirements.

## Greenlands findings to apply on the next authorized asset revision

The 2026-10-18 review found no production-ready asset in this batch. Current job history remains authoritative; this checklist does not rewrite it or authorize another inference. Reconcile inconsistent geometry passes and provenance first, then establish production profiles and use this order:

| Asset | Required next geometry/appearance decision |
| --- | --- |
| Waystone | Reopen with compatible Blender; retain useful source, inspect silhouette-level stone damage and use material detail for smaller relief |
| Marker | Resolve approved upright orientation, joined timber foot and support; classify sheets/debris, then rebuild poor major forms if cleanup cannot restore them |
| Iron | Classify the 57 exact-coincident components for the recorded source; retain sound rock, resolve skirt/shards and rebuild major ore masses if needed |
| Oak | Classify the 128 exact-coincident components for the recorded source; retain useful trunk/branches and replace unsuitable clumpy foliage when required by intended viewing distance |

These counts identify the reviewed revisions, not defect totals or IDs valid after repair. Calibrated lighting must determine whether pale color is a texture or presentation issue. Normal/height maps, full PBR and UE comparisons remain open until executed and recorded. Reuse suitable geometry before spending another inference attempt.

### Saved-source material channel audit

`tools/Audit-AssetMaterialChannels.py --profile <profile> --output <new folder>` runs in CPU background Blender and records active-surface shader connectivity, channel/image socket paths, map dimensions/color spaces and dependencies. Use it to distinguish a retained4K base-color texture from actual normal, emission or other required channel content. Nested groups are marked incomplete. This supplements the material gate; it does not replace neutral/opposing-light renders, per-material coverage, normal convention/relief checks or visual acceptance.

### Inspect actual channel values

Connectivity alone does not prove useful PBR data. `Audit-AssetMaterialChannels.py` optionally accepts an explicit `pixel_statistics_images` list: it measures all decoded channels of each named image (bounded to4096 square), retaining image color space and marking unused UV space as included. Constant or nearly constant maps require a material-class review rather than assuming that a large texture provides distinct bone, leather, hair and metal behavior. Whole-image statistics do not establish semantic UV coverage.

For a direct active Principled material, the closeup renderer's source mode accepts `--channel base-color|roughness|metallic --material <name>`. It temporarily routes that socket to unit emission and uses Standard display with no look/exposure adjustment. These camera previews retain source image color spaces and occlusion; they are not UV bakes or calibrated scalar measurements. Inspect channels separately, then declare/verify semantic masks and relief/effect requirements before repainting. Do not infer height or normal data from albedo highlights/shadows.

### Bounded semantic material correction

`Build-MountSemanticMaterial.py` creates a prototype material copy from a frozen semantic face selection and source hash. Only wholly selected triangles receive the new slot; mixed boundary faces retain the original material and are counted for review. A semantic proposal does not become approved merely because assignment is exact. Declare the physical material change and its controls in the mount profile; keep hardware and other classes outside the correction.

Fresh-process verification checks exact face assignment, the retained original shader structure and copied shader structure apart from the declared channel. Independently check geometry/UV/image/weight/bind retention and saved-source appearance. An exact-no-material-change checker should report the intentionally changed material signature/face slots as different. Preserve that raw result and add a separate scoped explanation tied to hashes and exact assignment evidence; do not alter a failed boolean or relax its contract. Full coverage, animation, exchange and runtime acceptance remain separate.
