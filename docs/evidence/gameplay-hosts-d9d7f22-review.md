# INT-04 review repair evidence (2026-10-21)

- `tools/Test-GameplayHosts.ps1 -EvidenceRoot Artifacts/Persistence/GameplayHosts/review-d9d7f22-20261021-f`: pass; all three wire services, durable replay, post-refund host fault, immediate PostgreSQL restart, recovered single refund, stale revision removal with a new operation ID, concurrent place/remove, and outage mapping. Owned private storage removed.
- `tools/Test-HousingDurable.ps1 -EvidenceRoot Artifacts/Persistence/HousingDurable/review-d9d7f22-20261021-a`: pass; includes `Housing.Postgres.Tests` seed, restart, concurrent and outage phases, backup/restore and schema refusal. Owned private storage removed.
- Pinned SDK `dotnet run --project BackendServices/ClaimTerritory.Tests -- content/world/zones/greenlands_silverbrook.json`: pass.
- Pinned SDK `dotnet run --project BackendServices/Placement.Tests -- content/world/zones/greenlands_silverbrook.json content/crafting/recipes_greenlands_starter.json`: pass. The process-local Placement model still uses its own floor refund rule; the durable housing host uses `HousingRefundRounding.UpToWholeUnit`.

The fixture verifies recovery on host restart. Continuous background reconciliation and cross-service atomicity remain outside this slice.
