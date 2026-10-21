# Gameplay host completion evidence (2026-10-21)

- `tools/Test-GameplayHosts.ps1 -EvidenceRoot Artifacts/Persistence/GameplayHosts/codex-final-20261021053657`: pass; owned private cluster removed. The fixture checks crafting over gRPC and replay, quest reward delivery exactly once after a host death between ledger Grant and intent completion, PostgreSQL immediate restart, abandoned placement compensation without client retry, and a two-host advisory-lock sweep.
- `tools/Test-HousingDurable.ps1 -EvidenceRoot Artifacts/Persistence/HousingDurable/codex-20261021053509`: pass, including restart, compensation, contention, backup/restore, and schema refusal; private cluster removed.
- `tools/Test-CharacterStateDurable.ps1 -EvidenceRoot Artifacts/Persistence/CharacterStateDurable/codex-20261021053551`: pass; private cluster removed.

The reward bundle is a single completion token in this content slice. The housing removal table has no reservation time, so the sweeper closes pending removals on its next pass. Exchange is not hosted by Gameplay.Host.
