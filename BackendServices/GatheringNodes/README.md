# Gathering node MVP

`GatheringNodeService` creates deterministic active node instances from gathering groups in a validated zone record. This slice supports `oak_tree` wood and `stone_outcrop` mined stone through caller supplied definitions. Silverbrook has no iron vein group, so stone is the available ore category pilot. Unsupported definitions are excluded and no WorldContent file is changed.

The injected `TimeProvider` drives depletion and regrowth. Commands carry operation ID, player ID, node ID and generation; a process local receipt makes retries identical, and a lock serializes the last charge. Generation increments at every regrowth, including each partial charge. Retrying an old operation returns its receipt; a new operation carrying an old generation is rejected. Restart persistence, inventory grants, range and tool checks, network authority, World scarcity, spawn point rotation, and Crafting quality rolls are integration work.

Definitions declare charges (1–20 harvests) and yield (1–100 units per harvest). `NodeSettings` declares respawn delay (30–3600 seconds; shipped 300), jitter (0–25%; shipped 10%), and full or partial regrowth (shipped full). Configuration is captured at service creation. A future shared configuration publisher can supply a validated settings snapshot; operators cannot edit formulas. These settings are realm controlled, require `world.tuning.publish`, and apply to newly created service instances; scheduled timers retain their captured values. `stone_outcrop` remains a mining stand in until authored iron content exists.

Run the focused executable from the repository root with the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/GatheringNodes.Tests -- content/world/zones/greenlands_silverbrook.json
```
