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

**Owned fixture coverage.** `tools/Test-ClaimGrid.ps1 -TwoClient` runs the focused model and band checks, then the owned PostgreSQL/two-client fixture in `tools/Test-ClaimHousing.ps1`. Each run writes under `docs/evidence/clm01-claim-grid/run-<timestamp>/`; `coverage.json` records each CLM-01 acceptance test 2–9 as NotRun or Partial, and the committed `summary.json` records the legacy fixture result. Host build output and raw logs stay in the ignored run-private folder; PostgreSQL data and synthetic credentials are removed by fixture cleanup and never committed.

The 2026-10-22 run `run-20261022-100321-674` passed its legacy restart and permission flow; the sanitized `summary.json` records identical content before/after restart, unchanged alpha inventory, committed placement/removal decisions, and PostgreSQL cleanup. The editor `Build.bat ShatteredFrontierEditor Win64 Development` also succeeded. `ClaimTerritory.Tests` now checks 4 × 4 footprint → 144 priced cells, edge growth, remote refusal, supported-cell shrink and reuse before empty-claim expiry. These are process-local model assertions; `coverage.json` retains the live test 2–9 limits.

After replay onto `origin/main`, the same targeted model check and editor Build.bat passed. The fresh owned fixture `run-20261022-100837-339` also passed; its sanitized summary confirms the same content digest across restart, unchanged inventory and owned PostgreSQL cleanup. Its coverage file continues to mark test 8 partial and tests 2–7 and 9 unrun.

**Integration gap.** The existing fixture only exercises a legacy square plot: persisted structure mask restore, restart, and the older two-client permission flow. This is a partial check for acceptance test 8, not proof of sparse cell masks, points, or anchor restore. Tests 2–7 and 9 remain NotRun because the client runner has no sparse-growth actions for reach, cap, shrink, blocked reasons, engulfment, multiple claims or empty archive. The J3 Housing/Postgres transaction and terrain adoption remain separate acceptance gates; no CLM-01 tests 2–9 are reported as passing. The zone record also lacks complete water, resource, slope and public-corridor mask data. The existing durable outside-claim gate remains in force.
