# TER-01 — Mesh Terrain fit check

Status: proposed spike, 2026-10-19. Not started. A bounded spike that ends in a decision note. It does not change [DEC-03](DEC-03-terrain.md)'s adopted representation, in which voxel density is the terrain authority.

## Why

UE 5.8 is reported to ship Mesh Terrain as an **experimental** feature: a true 3D mesh terrain with overhangs and tunnels, sculpted or imported from heightmaps or meshes, with non-destructive modifiers and PCG and World Partition support. Confirm that against Epic's 5.8 release notes before the spike.

DEC-03 leaves the client surface and mesh adapter open. Silverbrook needs terrain for hills, Northridge's high ground and the waterfalls, plus a northern mountain backdrop ([zone plan](../greenlands-silverbrook-zone-plan.md), gaps 1 and 3). The [world pipeline](../settlement-world-pipeline.md)'s PCG graphs need a surface to sample.

## Constraints

- **Copy only:** work on a copy of `/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook`. Never convert the main map.
- **Lighting:** leave lighting alone.
- **Scripts:** use committed scripts only, under `/Game/Frontier/`.

## Questions, each answered with evidence

1. **Splines:** can road and river splines from the layout file carve or flatten the terrain through modifiers, and does it regenerate when a spline moves?
2. **PCG sampling:** do the pipeline's PCG graphs sample its surface correctly, including normals, height and exclusions?
3. **Runtime edits:** can it be modified at runtime, on both server and client, from DEC-03 voxel edits? Is its collision deterministic between server and client?
4. **Linux server:** can a dedicated Linux server build load it? What are its memory and collision costs?
5. **Voxel path:** is there a path from its surface to DEC-03 base density, by sampling the mesh into 0.5 m voxels for `base_content_version`?

If the answer to question 3 is no, Mesh Terrain can only be the authoring source for the published base terrain, or non-editable backdrop such as the northern mountains. It cannot be the editable player-terrain adapter.

## Outcome

To be recorded here when the spike finishes. Pick one:
- **Keep:** use it as the client surface adapter.
- **Authoring only:** use it to author the base terrain that is sampled into voxels.
- **Backdrop only:** use it for non-editable distant terrain.
- **Reject.**

Include the evidence for each question. Link the outcome from DEC-03's revisit triggers, as the pointer there already does.
