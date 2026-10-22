# CLM-01 nonvisual implementation evidence — 2026-10-22

This worktree adds the package-free CLM-01 `ClaimGridService` and authored `ClaimBandMap` alongside the older square-claim service. Its shipped typed snapshot defaults to 1,200 alpha points, three claim slots, 400 + 40/level after alpha, 400/1,600/3,600 m² caps and disabled upkeep. Growth pins cell prices, shares the owner's budget, checks authored town/road/spawn/border clearances, claim spacing and reach, and shrink returns points. `Contains` checks the footprint and anchor-relative vertical band for TER-02. The band CLI reads the authored Silverbrook files.

Focused checks, using the pinned 10.0.402 SDK executable from the main checkout because this worktree lacks its ignored `Artifacts/Persistence/Toolchains` directory:

```text
tools/Test-ClaimGrid.ps1 -Sdk C:\Projects\Shattered-Frontier\Artifacts\Persistence\Toolchains\dotnet-10.0.402\dotnet.exe
  ClaimTerritory: focused assertions passed
  cell [900,900]: wilderness, 811.12 m from town boundary, unblocked
dotnet run --project BackendServices/Placement.Tests -- content/world/zones/greenlands_silverbrook.json content/crafting/recipes_greenlands_starter.json
  16 focused checks passed
```

**Integration gap.** Housing.Postgres and its gRPC service still persist square claims and do not commit cell growth, points, terrain adoption and the DEC-02 placement decision in one transaction. `Test-ClaimGrid.ps1` is model-only; it is not the requested owned-PostgreSQL two-client runner. The zone record lacks complete water, resource, slope and public-corridor mask data, and engulfment validation is not implemented. Consequently CLM-01 J3, J7 and the dependent J4 client gate are not accepted. No editor C++ was touched. The existing durable outside-claim gate remains in force.
