# Character state PostgreSQL fixture

On 2026-10-21, `tools/Test-CharacterStateDurable.ps1 -EvidenceRoot Artifacts/Persistence/CharacterStateDurable/review-20261021-07` passed all 17 assertions using the pinned .NET 10.0.402 SDK, Npgsql 10.0.3 and PostgreSQL 18.6 on an owned loopback cluster. `harness.json` reports passing status and owned private storage removed.

The fixture covers committed XP, skill and quest step state plus reward intent after immediate-stop WAL recovery, replay with the same source IDs, one winner for concurrent same-revision applies, fail-closed outage and same-source recovery, SQL class 22 rejection, and refusal of a newer schema marker. Progression.Tests and QuestContent.Tests pass separately.

This is synthetic single-database evidence. It does not demonstrate authenticated producer admission, reward-service delivery, production migrations or backup scheduling.

On 2026-10-21, `Test-CharacterStateDurable.ps1 -EvidenceRoot Artifacts/Persistence/CharacterStateDurable/service-store-20261021-a` passed with the shared backup/restore tools. A later progression revision remained absent from the restored fresh instance; the restore checked all five table counts and content digests, progression XP and skill caps, receipt references, and quest step bounds. A flipped dump byte and a newer source schema were refused. Owned source and restore clusters were removed. The restore check cannot reconstruct the full historical order of quest events because the persisted receipt rows contain no event timestamp; it checks the stored step against its revision and time bounds. The manifest is a checksum, not an authenticity signature.
