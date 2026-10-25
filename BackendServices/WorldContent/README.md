# World content and spawn slots

`WorldContent` is the package-free World content loader, validator and in-memory spawn-slot model. It reads `content/world/zones/*.json` using the exact snake-case fields emitted by `GameClient/ArtSource/Greenlands/zone_record.py`. It does not open a listener or database. The caller supplies the clock, seeded random source, gathering definition respawn seconds, current scarcity index, player count, director pressure and optional persistence adapter. The caller persists gathering and elite/rare NPC snapshots through `ISpawnSlotStore` at the World report/drain boundary; common NPC slots may remain in memory. Definition tier classification remains the caller's catalogue responsibility.

From the repository root, with the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/WorldContent.Tool -- content/world/zones
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/WorldContent.Tests -- content/world/zones/greenlands_silverbrook.json
```

All three projects have committed package locks and use the backend central package policy. The test executable checks broken in-memory copies of the generated zone and fixed-clock slot behavior. World `spawn_groups` use the group area for population counts; a few existing gathering point positions are outside that authored area, so the validator enforces zone extent and level coverage for each point instead. The zone record has no `neighbors` border records yet; cross-zone border polyline and anchor checks await that content shape. The common Unreal combat adapter consumes approved zone records; durable storage schema, phase scheduling and Feature Control publication remain separate.

`WorldTuningCatalog.Keys` declares each supported control's label, unit, default, bounds, publishing permission and adoption boundary for the shared configuration workflow. `WorldTuning` validates an effective snapshot supplied by that workflow. The service does not accept console expressions or mutate live settings directly.

NPC spawning uses `NpcSpawnCatalog`: zone groups of `spawn_kind: npc` may have any valid group token and must reference declared profiles in `content/creatures/profiles.json`. Authored `respawn_tier` selects the existing World tuning; unknown IDs/tiers are rejected. `SpawnPoint.NpcDefensive` retains authored point behavior for the Unreal adapter. The gRPC field names remain compatible.

Focused reuse checks (includes a seventh content-only ranged definition and arbitrary group):

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/WorldContent.Tests -- content/world/zones/greenlands_silverbrook.json --npc-reuse
```

Gameplay.Host deployment may select `GAMEPLAY_ZONE_ID` from approved records in `content/world/zones`; the shipped default remains `zone/greenlands_silverbrook`. This is deployment-owned, restart-bound content selection, not a live console path override. Unknown/ambiguous IDs abort startup. Protected areas, gathering and NPC slots use the same selected zone record.
