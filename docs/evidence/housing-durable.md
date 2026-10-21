# Housing PostgreSQL durability fixture

On 2026-10-21, `tools/Test-HousingDurable.ps1 -EvidenceRoot Artifacts/Persistence/HousingDurable/codex-v4-20261021-03` passed with pinned .NET 10.0.402 and PostgreSQL 18.6. `harness.json` reports pass and private storage removed. The existing seed, immediate-stop restart, contention, replay, permission, and outage phases passed.

The fixture applied base Housing schema and the additive gameplay v4 migration. The shared Housing backup manifest records version 4 and all seven Housing tables, including `claim_geometry` and `removals`, plus four Inventory tables. Backup refused aged pending placement and removal fixtures, then accepted both within the shipped 3600-second window. Restore rejected a 60-second window, accepted the default, matched backup-point claim revision and every table digest, and refused a flipped dump byte. The adapter and backup helper refused schema v5.

Restore checks live-cell uniqueness, claim geometry, pending actions, committed placement debits, committed removal refunds, and compensated refund tombstones. Gameplay v4 has no removal timestamp, so the pending-removal window conservatively uses the linked placement's `created_at`; this can reject a recent removal of an older placement. The fixture uses synthetic credentials and does not establish production backup scheduling or recovery workers.
