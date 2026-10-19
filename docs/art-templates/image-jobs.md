# Concept-image batch format

Use with [the image-only workflow](generate-images.md) and [asset planning](asset-planning.md). [asset-catalog.md](asset-catalog.md) is the progress entry point. This file is the template for indexed batch files, not a second live queue or the gameplay asset registry. No batch is assigned yet; category examples are not approved requirements. Existing Voidreign benchmark references remain fixed while model testing continues.

Create `asset-lists/<batch-id>.md` when a user brief or documented project need is selected. Include batch ID, phase, scope revision, source paths/headings, owner and dependencies. Link it from the catalog. Work in small reviewable batches; mark proposals clearly and claim only assigned/unowned rows.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
| --- | --- | --- | --- | --- | --- | --- |

For each row add a short brief with defining features, required images, component boundaries and unknowns. Job states: planned, in-progress, image-complete, blocked, deferred. Concept statuses: proposed, agent-selected, user-approved. Clean-view statuses: needs-review, needs-segmentation, ready-for-model-test. Keep progress in rows and update the catalog summary together; do not invent completion/approval dates. Record reasons for blocking or deferral.

Declare the exact output budget before generation. Default per row: `required_images: [concept-sheet.png, asset-view.png]`, `selected_image_count: 2`, `additional_images: 0`. Put applicable technical views/details on the concept sheet. Any necessary separate image must have a named filename, purpose and essential unresolved question in this brief, with the revised exact count; optional template language does not create a requirement. A scope-specific omission allowed by asset planning must also be explicit. Request one named image per tool call and stop after the declared list passes review. Rejected/repair attempts remain in provenance, outside the selected-deliverable count. Existing completed packages/history remain unchanged.
