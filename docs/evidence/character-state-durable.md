# Character state PostgreSQL fixture

On 2026-10-21, `tools/Test-CharacterStateDurable.ps1 -EvidenceRoot Artifacts/Persistence/CharacterStateDurable/review-20261021-07` passed all 17 assertions using the pinned .NET 10.0.402 SDK, Npgsql 10.0.3 and PostgreSQL 18.6 on an owned loopback cluster. `harness.json` reports passing status and owned private storage removed.

The fixture covers committed XP, skill and quest step state plus reward intent after immediate-stop WAL recovery, replay with the same source IDs, one winner for concurrent same-revision applies, fail-closed outage and same-source recovery, SQL class 22 rejection, and refusal of a newer schema marker. Progression.Tests and QuestContent.Tests pass separately.

This is synthetic single-database evidence. It does not demonstrate authenticated producer admission, reward-service delivery, production migrations or backup/restore.
