# Gameplay host Exchange and reward snapshot follow-up

Run on 2026-10-21 with pinned .NET 10.0.402 and fresh disposable evidence roots:

| Command | Result |
| --- | --- |
| `tools/Test-GameplayHosts.ps1 -EvidenceRoot Artifacts/Persistence/GameplayHosts/exchange-final-03` | Pass: changed quest content after intent creation preserved original reward lines; two-character wire trade; concurrent buyers yielded one sale; trade/listing cancellation; trade offer expiry returned escrow; post-delivery host kill and immediate PostgreSQL restart recovered one sale through the sweeper. |
| `tools/Test-ExchangeDurable.ps1 -EvidenceRoot Artifacts/Persistence/ExchangeDurable/host-followup-01` | Pass: durable fixture and owned cleanup. |
| `tools/Test-CharacterStateDurable.ps1 -EvidenceRoot Artifacts/Persistence/CharacterStateDurable/reward-snapshot-01` | Failed at backup: shared `tools/Backup-ServiceStore.ps1` accepts only CharacterState schema version 1; this change requires version 2. Earlier fixture phases passed. The backup helper is outside the allowed edit paths. |

The host-only Exchange adapter uses existing durable settlement rows. `OfferTrade` requires pre-funded escrow; `ListItem` moves items to escrow before creating the listing. A crash between those two listing transactions can leave an orphaned escrow transfer. Closing that gap requires a durable listing intent or coordinator change in Exchange.Postgres, outside this task's allowed paths. The bearer token identifies the trusted game server; it is not player authentication.

## Coordinator closure (2026-10-21)

Fresh `tools/Test-GameplayHosts.ps1 -EvidenceRoot Artifacts/Persistence/GameplayHosts/codex-82b4d88f232d4b569001750f241b7442` passed. A host kill after listing escrow transfer followed by immediate PostgreSQL restart yielded one active listing and one transfer receipt; an aged pending intent without transfer was compensated with a tombstone at its escrow operation key. The existing purchase crash sweep and all wire legs passed. This run supersedes the listing gap and CharacterState backup failure reported above.

The same change passed `Test-CharacterStateDurable.ps1` at `Artifacts/Persistence/CharacterStateDurable/codex-776c30ec0c5b48ed974374154591ecc7`, `Test-ExchangeDurable.ps1` at `Artifacts/Persistence/ExchangeDurable/codex-479417a8541047029f460b6a1002395b`, `Test-InventoryLedgerDurable.ps1` at `Artifacts/Persistence/InventoryLedgerDurable/codex-706de262ac86466bbae5d8dfdfe1fb59`, and `Test-HousingDurable.ps1` at `Artifacts/Persistence/HousingDurable/codex-2d14320fd4b44bfba67d21ac84783dc5`. All used fresh owned roots and removed private storage.
