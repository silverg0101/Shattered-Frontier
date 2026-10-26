# Concept images to validated model assets

Mount finishing uses the shared [secondary-motion procedure](mount-secondary-motion.md) plus a separate source-hashed profile/settings index for each mount. Reuse algorithms, artifact contracts and checks; author anatomy, clumps/roots, skin correspondence, colliders, tuning and acceptance policy for the new asset. [Voidreign's profile index](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/MountProfile.md) is a worked example, not a starting preset to apply unchanged.

Version2 workflow contract, 2026-10-18. This defines required agent work and the data an eventual batch application must persist. It is **not an implemented universal batch generator**. Existing Radeon and Voidreign tools are examples with asset-specific assumptions; capability and adapter gates below prevent treating them as a generic service.

The [quality runbook](model-quality-runbook.md) gives the practical sequence, semantic selection contract and confirmed repair recipes. Keep its implementation limits separate from this workflow's required future capabilities.

The [required acceptance gates](model-production-gates.md) are part of every execution, including prototypes and reused models. They specify preflight provenance, production profiles, calibrated reference comparison, complete component disposition, PBR/depth comparisons and source/export/engine correspondence. The author performs and records applicable manual/visual checks during ordinary work; a separate agent review is optional. Missing tools or evidence remain open gates. Written requirements do not imply automatic enforcement.

## Start and resume

1. Read `docs/dependency-map.md`, [generate-model.md](generate-model.md), this document and [model-production-gates.md](model-production-gates.md).
2. Open [model-catalog.md](model-catalog.md), its named job JSON and exact artifact/check paths. Image references remain owned by [asset-catalog.md](asset-catalog.md) and its named image batches. Do not search whole folders to reconstruct status or alter image-completion history.
3. Apply the task's milestone/category handoff. For the construction sandbox, [construction-sandbox-model-handoff.md](construction-sandbox-model-handoff.md) and its geometry owners take precedence over generic inference. Start a finite batch of3–5 useful assets, reusing suitable existing models. Keep prototypes and blocked work visible.
4. Read the job's last check result and exact next action; reconcile original/effective input, tool/runtime/lock/profile and output hashes before resuming. Pin acceptance scope and render settings. Continue at the earliest invalidated stage, not the beginning of the pipeline. Legacy summaries that call an inference a geometry pass require a linked reconciliation and corrected current state before dependent work.

## Job record and ownership

One JSON job per asset/version under `docs/art-templates/model-jobs/`; format is specified by [model-job.schema.json](model-job.schema.json). The catalog stores direct job links, priority, summary state and next action. Jobs own stage state, attempt paths and blockers; output manifests own detailed evidence. Update job and catalog together, never maintain competing status rows.

Minimum job data: stable asset/job IDs, image batch/package/manifest, explicitly selected clean image and SHA-256, approved generation profile path/hash, requested deliverables and quality target, source permissions/licenses, stage states, attempt counters, exact outputs/check reports, unresolved defects and next action. Use repo-relative paths inside records. Record actual UTC timestamps when taking/releasing a job; do not invent them. An active owner/lease prevents two sessions writing the same job. An expired lease requires inspecting the recorded process/output before recovery, not blindly starting another GPU worker.

Each attempt writes a new immutable folder. Keep attempts that failed, their settings, logs and partial reports. Write status atomically after artifacts are saved. A cache key includes input hash, backend/source/weight/runtime revisions, preprocessing, seed, sampler and geometry/material settings. A stage can be reused only when its input dependency hashes match. A new reference or geometry invalidates dependent UV, textures, rig, deformation and export evidence. A texture-only edit normally preserves geometry/rig evidence but invalidates material/export renders.

## Asset analysis comes before choosing a model

Create an asset brief with components, dimensions/units, up/forward axes, pivot, silhouette landmarks, material classes, expected moving parts, contact/attachment regions and hidden-surface uncertainty. Specify a category/profile; do not copy horse scale, mane masks, bone positions or naming rules to other assets.

| Asset/feature | Route and conditional work |
| --- | --- |
| Static prop, rock, waystone | Existing model or image-to-mesh; geometry/material/UV/export checks; no invented skeleton |
| Ordinary articulated object, e.g. door | Separate rigid parts and specified pivots/motion/clearance envelope; behavior belongs to implementation |
| Creature or mount | Anatomy/stance, semantic part boundaries, skeleton and skinning if requested; rider fit only for rideable profile |
| Mane, tail, fur, feathers | Identify the actual feature; choose skinned clumps/cards/groom as appropriate; root/weight/collision/deformation gates |
| Cloth, straps, hanging chains | Separate attachment and material behavior; cloth or articulated chain solver as appropriate, not a hair recipe |
| Tree/foliage | Trunk/branches/leaves, alpha and wind representation/LODs as requested; no horse-style chain masks |
| Structural construction part | Deterministic approved geometry, not inferred image solids; require GEO-01 manifest/hash and applicable GEO-02 fit evidence under BLD-01 |
| Surface/finish reference | Material/appearance workflow applied to a specified base or labelled study; do not turn a face texture into a new structural solid |

For opaque studio references, `tools/trellis2/prepare_model_input.py` records a per-image RGBA matte and preview before inference. Border-connected flood fill is the baseline; optional five-iteration GrabCut trimap cleanup can reduce connected floor shadows. Preserve the source, every candidate mask, hash and visual review. A changed matte invalidates geometry and downstream stages. Never reuse another asset's mask or assume segmentation removed contact shadows. The Pixal adapter must preserve prepared RGBA alpha through preprocessing.

Feature absence means a stage is explicitly `skipped` with a reason. Uncertain separation or unseen design is a defect/assumption to resolve; it is not permission to invent structure or mark a stage passed.

For concept prototypes, an existing agent-selected clean view marked ready-for-model-test may be used within the authorized scope; do not invent a new user-approval checkpoint. A prototype-authored profile can support a labelled experiment. Required structural geometry authority and any explicit user restrictions remain separate gates.

## Stages and acceptance gates

| Stage | Required output | Checks before passing |
| --- | --- | --- |
| reference | Selected clean single-asset image, source package/hash and analysis brief | One complete object; usable alpha/background, native dimensions and consistent identity. Concepts/details/rider/effects remain separate. Reject clipped objects, sheets passed as single views or conflicting views. |
| geometry | Raw inference/cache and editable mesh candidate, import transform and scale | Render front/side/rear/three-quarter; compare silhouette/anatomy/landmarks; inspect finite vertices, invalid/degenerate faces, disconnected components, flipped surfaces and contact/attachment regions. Open surfaces may be intentional; watertightness is profile-specific. Ground-minimumZ is not proof all feet touch. |
| topology_uv | Component/semantic map, topology/UV candidate and geometry hash | Preserve defining geometry. Check UV coverage/overlaps/texel density/padding against profile; intentional mirrors recorded. Moving regions need sound topology and anchored boundaries. Rebuild fused hair/tack when weights cannot isolate it. Never quietly replace original hair with test geometry. |
| materials | Base color, roughness/metallic and any requested normal/emission maps, texture manifest | Actual map dimensions/channels/colorspace, missing maps, seams, projection coverage and neutral-light render. Separate baked-lighting defects from material detail. A4K atlas is not a higher-resolution geometry grid or proof of new detail. |
| rig | Asset-appropriate skeleton/parts, rest pose, weight/attachment maps | Joint/root/pivot placement, valid normalized weights, body vertices outside deformation masks, rest-pose shape and bone-axis/scale conventions. No rig if category does not need one. |
| secondary_motion | Selected-feature simulation/animation and collider/attachment profile | Motion/stop/turn cases appropriate to asset; test rider only when relevant. Check roots, chain error, finite/bounded motion, deformed surfaces, collision/clearance and transitions. Distinguish a rigid motion harness from walking legs and baked animation from runtime simulation. |
| delivery | Versioned .blend, suitable GLB/FBX, textures, inspected previews, manifest | Fresh-process source reopen/render; empty-scene reimport of every delivered format; verify axes/scale/materials, animation/skin playback and dependencies. Export success alone is insufficient. |

Numerical limits come from the **pinned, typed profile**, with units, bounds, intended use and versioned validation rules. If no production budget, rig definition, clearance requirement or applicable threshold exists, mark that acceptance criterion `needs-review`; do not invent a project requirement or promote the result to production. Offline author controls take effect on the next new bake. Runtime tuning, permissions and effective-time behavior follow the project's shared configuration architecture.

Every applicable gate records tool results **and** actual visual inspection, including the inspected reference/render paths and views/frames. The author executes this checklist without waiting for a separate review request. A process exit code, contact count, valid file or high polygon count cannot substitute for appearance, animation or surface-collision review. Independent review may supplement the author's required evidence when authorized and useful under repository delegation rules.

### Loose geometry and attachment gate

Required for every model, including static assets. Check both separate objects and **disconnected vertex/edge/face islands inside a single mesh object**. One mesh object does not establish connectivity. Inventory loose vertices/edges, isolated face fragments, floating fragments, and suspicious thin bridges that can hide a nearly detached part. Use the geometry stage to detect candidates and topology/UV to resolve their intended attachment before final rigging or delivery acceptance.

For each component/candidate record an ID scoped to the mesh hash, source object and element selection, element counts, bounds/centroid, dimensions in metres, relative size, nearest relevant surface and separation distance. Preserve that selection as an artifact so another agent can locate it directly. Size/distance limits belong to the typed category profile; absent limits require review. Tiny components are candidates, not automatic errors: chain links, teeth, leaves, hair cards, charms, equipment and effects may intentionally be separate.

Compare candidates with the reference and a **semantic attachment graph**: main body/assembly, expected part, expected parent/root/joint/constraint, and permitted separation/motion. Classify each as `intentional-separate`, `attached-by-rig-or-constraint`, `unexpected-detached`, or `unresolved`. Proximity alone is not proof of attachment, and a parenting relationship alone does not prove correct skinning or motion. A disconnected ornament may be valid; an apparently connected strand may still detach visually because of a thin bridge or incorrect weights.

Save an annotated inspection view and a machine-readable component/attachment report, with per-candidate classification, evidence and disposition. Unexpected pieces require a recorded geometry repair: reconnect/rebuild, correct attachment/weights, or remove a confirmed stray fragment. Never delete every small island, join all objects, or weld nearby surfaces blindly. Recheck silhouette, UVs/materials and affected rigging after repair. Unresolved or unexpected detached parts keep the relevant gate `needs-review`/`repair-required`; they cannot be hidden by a successful file export.

Repeat attachment checks in the rest pose **and over the relevant animation cases**. Detect floating parts, root/attachment drift and unintended motion of nominally rigid pieces, including components that are topologically connected but pull away under deformation. Inspect actual deformed surfaces, not just bone positions or simulated center points. Recheck after simplification/remeshing and after each exchange-format reimport; topology IDs may change, so retain geometry hashes and an explicit semantic correspondence rather than reusing stale selections. Static profiles skip animated cases with a reason, not the rest/export attachment checks.

The standalone `tools/Inspect-MeshComponents.py` now supplies a CPU-only object/island inventory, loose-element counts, mesh-scoped selections and sampled gaps. Its default `--connectivity coincident` analyzes exact-position links without editing the model, alongside the raw index-island count. This distinction is essential for meshes split at UV/normal seams: many indexed islands can describe a continuous surface. Exact coincidence can also link intentionally touching parts, so neither inventory count proves a defect or an intended attachment. Gaps are sampled to the largest component, not exact all-pairs distances.

Run it with background Blender, an explicit source and asset collection, and a new output folder. Use `--assembly-root <rigid-root-name>` only when needed to remove a unit-scale motion transform. Reports and NPZ selections are direct resumable artifacts. `tools/Plot-MeshComponentReview.py` makes CPU-only identification diagrams; it draws selected candidates through the surface and is not an occlusion test. `tools/Repair-MeshComponents.py` removes only explicitly reviewed components into a new revision, guarded by source/mesh hashes. The current post-repair motion checker, `tools/Validate-VoidreignComponentRepair.py`, is specific to the Voidreign rig profile.

Automatic semantic classification, near-contact/thin-bridge analysis and general deformed-surface attachment validation remain open capabilities. Integrating the standalone inventory into every batch adapter is also pending; existing island counters may not account for seams. Until those checks are available, perform and record bounded manual inspection. Absence of a detector/report or unresolved semantics is an open gate, not a pass. [Voidreign component review](../../GameClient/ArtSource/Voidreign/ComponentReview-20261018/README.md) records230 seam-aware components, two reviewed removals and228 unclassified components; those counts are not defect totals.

### Machine-check report contract

An eventual worker normalizes native tool reports while retaining their original files. Until then the author writes the linked gate summary required by [model-production-gates.md](model-production-gates.md), distinguishing tool, visual, manual-measurement and unimplemented checks. For each check store a stable check ID, stage/attempt ID, `passed|failed|needs-review|skipped`, input and profile hashes, tool revision, measured value/units where applicable, the owning profile limit, evidence path and concise reason. Stage summary must be derived from all required checks; missing evidence is not a pass. File outputs and checks reference the same attempt hash. Existing native reports have different formats; a normalizer and machine enforcement are still implementation work, not supplied by this document.

Keep visual review as a separate required check where appearance matters. State whether it was performed by the author, a separate reviewer or the user, which views/frames were examined and remaining findings. Do not invent a numerical similarity threshold and call it design approval.

## Texture workflow

Save the generated material/attribute volume before decimation/export when supported. Bake2K/4K comparisons onto the same mesh/UVs, from the same volume and matching cameras/lights. Increasing an existing PNG size cannot recover missing source detail.

For local refinement: render registered base-color plus normal/depth/position guides; edit concept-conditioned views with fixed camera/silhouette requirements; inspect image registration; project only onto correctly visible semantic surfaces; feather/blend seams; bake into the atlas; inspect several angles and hidden-side coverage. Record exact prompts, image tool, source/output hashes, matrices and masks. Preserve original maps. Lighting painted into albedo, invented rivets/edges and mismatched material channels require correction or explicit defect status. A single-view projection does not finish the back of the asset or supply a true normal map.

### Unreal Engine 5.8 surface-depth and lighting route

Treat engine-quality surface maps as a distinct materials-stage deliverable, not as an automatic property of image-to-3D output. For each material, record the source and hash for base color, tangent-space normal, roughness, metallic only where the material is metal, and optional AO/height. Record resolution, channel packing, color space, UV set, physical scale, and whether each map is measured, derived, generated, or still missing. Keep baked lighting out of base color. A high-resolution color atlas alone does not satisfy surface-depth work.

Use the following routes only when supported by the asset and reference:

- **Static stone, ore, and rough timber:** author or derive a grayscale height/displacement map only when the relief is visible and its scale can be reviewed. Test UE 5.8 Nanite programmable displacement on a static mesh where added geometry changes the silhouette or close-up shading. Record the displacement magnitude in metres and center value as typed per-material profile settings; begin with displacement disabled (`magnitude_m: 0`) until a map and scale are reviewed. Compare the base mesh, normal-only, and displaced result under matched views and virtual-shadow-map lighting. Nanite displacement can raise cost and culling bounds, and large magnitudes can hurt performance and shadows; enable displacement fade only after measuring the visible distance. [Epic's Nanite workflow](https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-naniteenabled-content) describes the displacement input, magnitude/center, and these costs. UE 5.8 also adds runtime controls for Nanite tessellation in directional and local virtual-shadow-map passes in its [release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US).
- **Small relief without a supported displacement route:** use a reviewed normal map, with parallax/Bump Offset only when the camera distance and surface depth justify it. Record its material-instance height ratio and reference plane. Epic documents typical Bump Offset height ratios of `0.02–0.1`; these are a starting range, not an asset-wide default. Do not stack strong parallax and Nanite displacement on the same relief without a matched visual review. [Epic's Bump Offset guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-bump-offset-in-unreal-engine?lang=en-US).
- **Foliage:** keep leaf opacity, normals, roughness and wind response in the foliage/material route. Use World Position Offset or a foliage animation only when motion is requested and the authored leaf representation supports it. UE 5.8 adds a Nanite Pixel Programmable Distance property for foliage types; evaluate it on representative close and distant views instead of applying it to every tree. Record Nanite, masked-material, WPO and shadow behavior separately. [UE 5.8 release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US).
- **Scene presentation:** for supported high-end targets, validate Nanite geometry with Virtual Shadow Maps and Lumen GI/reflections together; compare the result with the intended platform scalability path. UE 5.8 includes a Lumen Medium quality path, so record the project scalability target and profile each path instead of assuming Cinematic/Epic settings ship everywhere. See [Lumen technical details](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine) and [Virtual Shadow Maps](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine).

The material profile must give each adjustable map resolution, displacement magnitude, center, parallax ratio, fade distance, and foliage-motion bound a type, unit, shipped default, safe range, permission, and effective time. Keep algorithms and shader formulas versioned in project content/code; do not store free-form console expressions. These asset profiles do not change project renderer settings or claim runtime acceptance. Missing maps and engine-view checks stay open until a scoped UE 5.8 import/render comparison is performed.

Use the required gates' matched base/normal/depth variants, normal-convention validation, calibrated controls and target-hardware/instance-density measurements. Verify capabilities against the installed UE build before enabling them. Major ore forms, timber joints, leaf silhouettes and broken-stone outlines remain geometry work. More costly rendering features require visible benefit within the declared budget, not blanket enablement.

## Rigging and motion generalization

Derive semantic regions and roots from each asset's actual geometry, then create or adapt a rig. Store per-asset selections/weights as hashed data. Geometric heuristics must expose diagnostic masks and pass review; world-coordinate thresholds from Voidreign are not reusable segmentation. If a feature is fused to the body, either repair topology or demonstrate that feathered skinning does not pull adjacent rigid regions. Additional procedural clumps are valid diagnostics only when labelled; they cannot count as acceptance of the supplied mesh.

Keep the original rest mesh immutable. Verify deformed geometry rather than only simulated center points. Check body vertices outside masks, attachment drift, weight sum, segment/pose error, and rider/environment contact over time. Collision proxies are approximations: solver contact activity does not prove the rendered surface clears the rider or environment. Compare baked skeleton transforms to simulation trajectories and reimported clips. Runtime performance/collision acceptance requires an explicitly scoped engine test, not a Blender render.

## Error correction and stop conditions

Default finite policy: **one initial attempt plus at most two repairs per failed stage**, with a reason, changed variable and before/after evidence for each. The authoring profile may set a stricter bounded policy. This is workflow guidance; an executable worker must enforce it and serialize GPU ownership before unattended execution is claimed.

| Failure | Repair the owning stage |
| --- | --- |
| Missing/clipped/ambiguous input | reference: correct selection/background/view using same design; preserve prior input |
| Runtime/import/OOM | capability/preflight: use a verified supported adapter/preset; record effective changes. No silent CPU/device/model fallback or unapproved dependency installs |
| Wrong silhouette, limbs, merged tack | geometry/topology: retry one recorded variable or repair components; textures cannot repair anatomy |
| Floating/detached fragments or parts separating in motion | geometry/topology/rig: locate the recorded component, classify intended attachment, repair only confirmed defects, then repeat rest/deformation/export attachment checks |
| Soft detail, seams, mottled paint | materials/UV: inspect source attributes, maps, registration, projection/bake and channel hookup before increasing resolution |
| Body moves with hair, roots detach | rig/segmentation: fix actual selection/weights/root binding, not shader or camera |
| Contact penetration/exploding motion | motion/rig/colliders: reproduce frame/case, fix appropriate constraints/geometry, rerun that case and affected exports |
| File exists but reimport differs | delivery: fix exporter/transform/packing/clip mapping, then fresh reimport/render/playback |

On exhausted attempts, incompatible adapter, missing geometry authority or essential design ambiguity: mark the job `blocked` or `needs-review` with exact failed check, attempt paths, smallest needed decision/fix and next action. Continue other ready assets. Do not endlessly regenerate, replace the concept, hide a defect or mark skipped checks passed.

## Status and handoff

Job states: `queued`, `in-progress`, `needs-review`, `blocked`, `prototype-complete`, `import-candidate`, `engine-validated`, `deferred`. Stage states: `pending`, `running`, `passed`, `repair-required`, `needs-review`, `blocked`, `skipped`. A prototype may be complete for its requested scope while recording defects that prevent import/production acceptance. `engine-validated` requires linked evidence from the authorized engine session; visual asset work does not grant authority to modify maps/code/server or deploy.

At checkpoints save: completed/current/next stable IDs, current stage, direct artifacts/previews, check outcomes, remaining defects, exact blockers and next action. Resume prompts reference this document, a named catalog job/batch and approved profile. Avoid generic “do all assets” prompts without a finite queue, owner, repair budget and quality target.

## Current implementation evidence and gaps

- [Radeon backends](../../tools/trellis2/CHALLENGERS.md): local Pixal/Hunyuan/Tripo work on7900XTX with pinned versions. `run_pixal.py` accepts explicit image/reference paths, output basename and per-run sampler steps, token cap, texture size, export decimation, and remesh settings. Defaults preserve the recorded 12-step/49,152-token/2K/300K benchmark; these are generation/export settings, not universal runtime budgets. `SF_ASSET_LAB_ROOT` selects the local installation root. The verified RX7900XTX requirement remains enforced. Pin the runner hash and all effective flags in a new profile before inference; historical profiles and results remain immutable. Verify each adapter's remaining assumptions, actual effective input and pinned helper revisions before another asset. This limited generalization does not generalize Voidreign dimensions, masks, rig or all validators.
- [Texture trial](../../GameClient/ArtSource/Voidreign/PixalDetail-20261018/README.md): controlled2K/4K baking, saved attributes and visible-side head/saddle projection. This is not a general semantic texture authoring service.
- [Actual-mesh hair prototype](../../GameClient/ArtSource/Voidreign/HairMotion-20261018/README.md): selected original mane/tail geometry, offline bone-chain bake, diagnostic playback and checks. Its coordinates, masks, colliders and test motion are asset-specific.
- [Required acceptance gates](model-production-gates.md): mandatory agent-executed checklist and implementation matrix; incorporates the Greenlands review findings without changing its historical attempts. Generic calibrated render, provenance/acceptance validation, source/export surface correspondence and UE comparison tooling remain incomplete.
- Before unattended batches: complete applicable backend adapters, machine-enforced job/schema/lease/GPU handling, dependency/attempt tracking, report normalization, semantic profiles and validators. Preserve the distinctions between available tools, per-asset authored work and planned automation. Documentation and one successful asset do not prove universal automation.

## Copyable model-session addendum

```text
Read docs/art-templates/model-pipeline.md and resume through
docs/art-templates/model-catalog.md. Execute the required checklist in
docs/art-templates/model-production-gates.md during the batch; no separate
agent review request is needed. Create or select a finite named queue
with stable asset IDs and direct approved reference-package links.
Pin acceptance and calibrated-render profiles plus coherent original/effective
input, generation/runtime/tool and export identities. Reuse suitable models and accepted
stage outputs. Route material-only references separately from geometry.
Follow conditional category/feature stages rather than copying Voidreign
hair masks, dimensions or rigs. Preserve actual supplied mesh unless a
recorded geometry repair/rebuild is necessary.
Record tool, author-visual, manual and unimplemented checks separately. Inspect
the saved views against references and classify all components/attachments.
Do not mark geometry passed merely because inference completed. Record every
attempt, defect and check at its owning stage. Use one initial
attempt plus at most two repairs per failed stage, then record the exact
blocker and continue other ready assets. Reopen saved sources, reimport
exports and inspect animation/materials where requested. Do not promote
prototypes to import candidates or engine-validated assets without evidence.
Follow the milestone handoff's geometry/ownership gates and restrictions.
```

### Exercised quality adapter boundary

The [Voidreign quality revision](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/README.md) now exercises source-hashed semantic/root proposals, authored root repair, coherent proposed attachment weights, settled guide bakes, all-frame actual surface/gap checks and CPU playback. `Run-VoidreignQuality.py` is a locked single-asset replay, not the generic lease/queue worker or report normalizer described above. It reproduces known failed surface checks; no automatic promotion follows its exit code. Texture-only invariants and candidate support inspection are separate checks. Historical230/228component inventories remain evidence for earlier sources; current repaired mesh has219components with incomplete semantic dispositions, not219defects. Consult the quality job for current paths/attempt counts.
