# Personal claim territory model

## CLM-01 cell model (implementation in progress)

`ClaimGridService` is a separate process-local model for the CLM-01 rules. `ClaimBandMap` reads the Silverbrook zone and town layout; it derives the nearest established settlement boundary and the authored protected, road, spawn and border masks. `tools/claim_bands.py X Y` prints a one-metre cell's band and known clearance failures. Growth pins each new cell's price, shares the owner's points across three claims, enforces reach and claim area cap, and returns points on shrink. `Contains` is the TER-02 footprint and vertical-band hook. The alpha snapshot has 1,200 points and upkeep disabled; setting `AlphaFixedPoints` to zero gives 400 + 40 per level. Existing claims retain pinned prices and height when effective settings change. Inputs are server-derived cells; the service does not trust client prices.

`GrowForFootprint` derives the complete 1 m cell reach ring from a server-validated piece footprint before pricing; a remote piece is refused before its ring can bridge the gap. `ShrinkToFootprints` retains only cells supported by remaining pieces. An empty claim remembers its former cells for a placement within the 24-hour archive window. These helpers remain process-local and must not be used as a durable placement receipt.

The new model is not yet connected to Housing.Postgres or the production placement RPC. The existing `ClaimService` and durable Housing paths below still use prepaid square reservations. A durable adapter must commit cell growth, terrain adoption, material decision, and placement under one recoverable `cba_` action before client use. The model also still needs water, resource, slope and engulfment masks, which the current authored zone record does not fully expose. No claim points should be presented to players until those gates exist.

`ClaimService` is a package-free deterministic, process-local model. The caller supplies an authoritative `TimeProvider`, a validated zone record, and an archive callback. It reserves one live `clm_` plot per character, rejects overlap with the Silverbrook protected town and its 5 m clearance, enforces a 5 m gap between live claims, and uses one lock for first-winner reservation. The zone loader reads protected polygons and extent from `content/world/zones/greenlands_silverbrook.json`; no town coordinates are duplicated here. IDs are caller-supplied canonical-style identities; authentication and cross-zone identity uniqueness belong to the future service adapter.

Each mutation requires an operation ID and expected revision. Repeating an ID with the same arguments returns the original receipt; changing its arguments returns `OperationConflict`. Reservations compare the service revision and claim edits compare the claim revision. Read-only access checks return stable `Denial` values and the permission revision for a downstream commit fence. The owner holds all three MVP rights. Explicit per-character grants cover build, open doors, and use containers independently. There is no friend or guild inference. Build is denied after grace, while assigned doors remain available. The `ClaimRight` mask is intentionally small; container inspection/deposit/withdraw need the later construction and Inventory contracts.

The model tracks funded time, grace, delinquency, final notice, archive and owner recovery. Grace and notice deadlines are pinned when an unpaid episode starts. Archive failure retains the land and retries on the next service operation; the callback must durably commit terrain, structures, inventory recovery and release eligibility **before** returning true. `Recover` records the owner's retrieval of an archived claim, but this model does not move items. An archived plot never restores structures on a newly occupied location. The caller must durably store claims, operation receipts, deadlines and archive decisions and implement authenticated zone ownership, inventory debits and construction action fencing. This model alone does not satisfy those contracts or reconcile material costs.

## Effective settings

Settings are typed integer metre/day fields with shipped defaults and validation. They are supplied as one effective snapshot through `SetEffectiveSettings`, intended to come from the shared authenticated config-service workflow, never from a client expression or local console override. All are realm scoped; the publishing permission is economy tier 3. The model has no config-service adapter yet. Existing unpaid episodes keep pinned deadlines; new settings apply to new plots or new unpaid episodes.

| Plain-language control | Field | Default | Bounds | Effective time |
| --- | --- | ---: | ---: | --- |
| Personal plot side (metres) | `SideMetres` | 40 | 10–40 | New reservations; 40 m shipped ceiling |
| Clear spacing (metres) | `SpacingMetres` | 5 | 5–20 | New reservations |
| Unpaid grace (real days) | `GraceDays` / `tune.building.grace_days` | 7 | 7–30 | New unpaid episode |
| Reclamation threshold (real days) | `ReclamationDays` / `tune.building.reclamation_days` | 30 | 30–90 | New unpaid episode |
| Final notice (real days) | `FinalNoticeDays` / `tune.building.final_notice_days` | 7 | 7–30 | New unpaid episode |

Spatial bounds are local shipped model ceilings, not new Registry keys or published controls. Increasing the 40 m entitlement requires compatible content review. `Pay` represents confirmed external upkeep funding, up to 30 days; it does not calculate or debit material costs. The production cost formula, receipt and all-or-none inventory transaction remain with the Construction Integration contract.

From the repository root, using the pinned backend SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/ClaimTerritory.Tests -- content/world/zones/greenlands_silverbrook.json
```
