# Model-session prompt — playable construction sandbox

Copy this into the existing 3D-model session. It reprioritizes its next batch without erasing its current instructions, work or provenance. This session has not messaged that chat or claimed ownership of its files.

```text
Work in C:\Projects\Shattered-Frontier. Our next milestone is a playable
construction sandbox: a small Greenlands environment where I can build
a rough-timber shelter. Prioritize models useful for that milestone.

Read docs/dependency-map.md first, then only relevant sections of:
- docs/construction-sandbox-milestone.md
- docs/art-templates/generate-model.md
- docs/art-templates/asset-catalog.md and its directly linked asset packages
- docs/art-templates/player-construction-kit-contract.md
- docs/decisions/BLD-01-voxel-geometry-interface.md

Preserve your current modeling instructions and existing experiments.
Prepare a finite sandbox queue with stable asset IDs and explicit states.
Start with a batch of 3–5 reusable nonstructural assets, selected from the
existing waystone, oak tree, iron/rock outcrop and wayfinding-marker refs.
Reuse a suitable existing model instead of regenerating it. If an asset
has gameplay/resource implications, deliver visual geometry only; do not
create resource nodes, harvesting, claims or runtime behavior.

Use the existing ground as a visual target for reusable terrain materials;
do not turn a flat environment image into authoritative voxel terrain.
Complete a small coherent environment batch before signature creatures,
weapons, mounts or additional architectural styles. Keep those queued.

In parallel with environment preparation, establish rough-timber surface
appearance: exterior/interior faces, top/underside, exposed ends, corners,
joints and rectangular opening treatments. Preserve the complete kit
matrix, but prioritize the parts needed by the sandbox's pilot shapes.

Structural geometry is locked by BLD-01. Placement is on a 0.5 m grid;
shapes, pivots, rotations, contact planes and clear openings are canonical.
The initial geometry pilot is full cell, half slab and 1:1 slope. Arches
and curves are multi-cell brushes, not indivisible prefab walls.

For each structural model, require the owning geometry task's approved
GEO-01 manifest/hash and applicable GEO-02 fit evidence. Until those exist,
produce material/appearance studies or explicitly labelled prototypes;
record the exact missing geometry gate and continue unblocked scenery.
Never claim an inferred mesh or an image is an approved structural solid.
Build final structural bases deterministically from the approved geometry,
then apply appearance. No arbitrary scaling, negative-scale mirroring,
filled openings or decorative changes to collision/support/clearance.
Do not bake the whole shelter into one replacement construction prefab.

The ordinary door is a separate object: use Construction Architecture §16
for dimensions, record hinge/motion/access envelopes, and wait for the
approved sandbox opening/attachment interface before final fit approval.

For each selected model, deliver versioned editable .blend, GLB/FBX as
appropriate, referenced textures, inspected saved-source renders and
fresh-scene exchange-format reimport checks. Record actual dimensions,
axis/unit conversion, origin/pivot, triangles, materials/UVs, provenance,
source image package/hash, generated-versus-authored steps and defects.
Provide collision/LOD recommendations, not fabricated engine validation
or invented production budgets. Distinguish prototype, import candidate
and engine-validated statuses; use the existing art delivery conventions.

Preserve benchmarks and concurrent work. Do not modify Unreal code/maps,
the live server, environments or dependencies, or commit/push based on
this addendum. Ask only for a materially blocking decision; work on other
ready assets while structural interfaces are pending. At checkpoints show
previews and record exact completed/remaining IDs, blockers and next asset.
```
