# Gameplay host Exchange and reward snapshot follow-up

Run on 2026-10-21 with pinned .NET 10.0.402 and fresh disposable evidence roots:

| Command | Result |
| --- | --- |
| `tools/Test-GameplayHosts.ps1 -EvidenceRoot Artifacts/Persistence/GameplayHosts/exchange-final-03` | Pass: changed quest content after intent creation preserved original reward lines; two-character wire trade; concurrent buyers yielded one sale; trade/listing cancellation; trade offer expiry returned escrow; post-delivery host kill and immediate PostgreSQL restart recovered one sale through the sweeper. |
| `tools/Test-ExchangeDurable.ps1 -EvidenceRoot Artifacts/Persistence/ExchangeDurable/host-followup-01` | Pass: durable fixture and owned cleanup. |
| `tools/Test-CharacterStateDurable.ps1 -EvidenceRoot Artifacts/Persistence/CharacterStateDurable/reward-snapshot-01` | Failed at backup: shared `tools/Backup-ServiceStore.ps1` accepts only CharacterState schema version 1; this change requires version 2. Earlier fixture phases passed. The backup helper is outside the allowed edit paths. |

The host-only Exchange adapter uses existing durable settlement rows. `OfferTrade` requires pre-funded escrow; `ListItem` moves items to escrow before creating the listing. A crash between those two listing transactions can leave an orphaned escrow transfer. Closing that gap requires a durable listing intent or coordinator change in Exchange.Postgres, outside this task's allowed paths. The bearer token identifies the trusted game server; it is not player authentication.
