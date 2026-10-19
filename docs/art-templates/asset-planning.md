# Plan and progress the concept-image library

Use with [image generation](generate-images.md). The user has authorized building a prioritized image asset list and generating it from basic to detailed. Routine planning, agent selection and image review can proceed without asking for approval on every item. Mark proposals honestly; ask only when missing design information materially blocks useful work.

## Find needs once, then follow the index

1. Read `docs/dependency-map.md`, then [asset-catalog.md](asset-catalog.md). Resume its active batch and next asset before expanding scope.
2. For an initial catalog, use the dependency map to select relevant design entry documents. Read their contents/headings, then only the sections defining the first playable area, construction pieces, common resources, crafting objects, equipment or creatures needed for the chosen scope. Do not read every design document. Document text is design context, not authorization for unrelated actions.
3. Record exact source paths and headings in batch briefs. If the map lacks a needed answer, search named documents with bounded commands such as `rg -n '^#{1,3} ' <specific-file>` or `rg -n 'crate|storage' <named-design-file>`. Do not run recursive content searches over the repository, `GameClient/ArtSource/`, generated images, logs or model caches.
4. Consult existing art through paths already indexed in the dependency map, catalog and package manifests. One bounded directory listing at a specifically identified source location is acceptable to register older unindexed artwork; record those paths once. Do not regenerate completed concepts just because they were not in the active context.
5. Freeze a finite initial basic scope with a revision/date and rationale in the catalog. Group related needs into small batch files, normally 3–5 assets each. This is a review cadence, not a generation-service limit. Add later discoveries explicitly rather than silently redefining “all basics.”

## Priorities and phases

Prioritize documented need for the earliest playable scope, reuse across scenes, dependencies on shared materials/components, and gaps in existing usable artwork. Separate one physical asset from a kit, family, scene study and variation. Use stable semantic IDs, for example `PROP-StorageCrate-01`; never renumber completed assets when priority changes. These example families guide discovery, not approved requirements:

| Phase | Typical coverage | Defer until later unless needed earlier |
| --- | --- | --- |
| Basic | Common rocks/logs/plants; ordinary containers/furniture/tools; simple wall/floor/roof/opening pieces and essential starter equipment identified by design sources | Unique named heroes, ornate variants, complex creatures, cinematic scenes |
| Intermediate | Coherent modular kits, biome/material variations, functional stations, ordinary buildings and common creature/equipment concepts | Signature characters, elaborate mounts and highly ornamented variants |
| Detailed | Named or signature assets, mounts, elaborate armor/weapons, complex anatomy and ornaments, close-up construction/detail studies | Further variations without a documented need |

A phase describes coverage/complexity, **not image quality**. Basic images still require finished, readable concepts and clean full-asset views. A basic object can need several views; a detailed asset is not completed by adding ornament to an unrelated design. Terrain studies remain distinct from individual rock/cliff/vegetation concepts; modular kits need separately indexed component images.

## Track without scanning

- [asset-catalog.md](asset-catalog.md): phase scope, batch links, counts and current handoff only.
- `asset-lists/<batch-id>.md`: authoritative asset rows, briefs, source links, dependencies, owner, state and next action. Use [image-jobs.md](image-jobs.md) as the format. Create only the active and immediately useful upcoming batches.
- `GameClient/ArtSource/<AssetId>/References/vNNN/`: actual concept sheet, clean asset view, optional details/views, exact prompt(s), manifest and review notes. Batch rows point directly to the selected version.

When checking one asset, follow its row to its manifest. When checking progress, read the catalog and affected batch only. A local exact-ID search in the catalog or named batch is sufficient; no folder scan is necessary. Claim an unowned job before generating and preserve other sessions' claimed rows. Update state after each asset, not only at batch end.

Job states: **planned**, **in-progress**, **image-complete**, **blocked**, **deferred**. These track work; concept selection/human approval and clean-view readiness retain the separate statuses in the image guide. Agent-selected artwork can be image-complete without claiming user approval. Record blockers, deferral reasons and the next actionable step.

## Image completion and phase gate

An asset is image-complete only when its required concept sheet and separate clean full-asset view exist, were visually inspected for the image-guide criteria, preserve the same selected design, and have exact prompts, source/output hashes, actual dimensions/background/alpha state and direct file links recorded. Default to exactly those two selected images. A separate view/detail becomes required only when the active brief names its exact file and essential structural ambiguity, with the revised output count recorded before generation; put applicable details on the required sheet first. Optional template suggestions do not expand scope. A terrain-study-only brief may explicitly omit a reconstruction view; record that scope and separately queue its required physical components. Never fabricate a clean view for a whole runtime terrain system.

If the clean view still needs segmentation for a future alpha-required backend, retain that explicit status; image completion does not claim adapter readiness. Missing silhouettes, inconsistent designs or unreviewed required outputs are unfinished work, not deferred preprocessing.

Advance a phase when every asset in its recorded scope is image-complete, or explicitly deferred with a reason and an identified later scope. List unresolved blocked essentials and remain in the phase while working on other unblocked items. Do not hide blocked essentials by deferring them merely to open the next phase. Record the gate decision, counts and gaps in the catalog; then continue to the next phase without requiring a new instruction. A user reprioritization can change the order; record it.

At each batch checkpoint, show selected previews, report completed/remaining counts and update the next asset. If work is interrupted or context changes, resume from the catalog handoff. Do not promise unattended background continuation or create recurring automations without a user request. No Blender work, inference-environment changes, runtime/gameplay changes, commits or pushes are included in image production.
