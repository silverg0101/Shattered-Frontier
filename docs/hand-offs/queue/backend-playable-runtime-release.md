# Playable runtime release

- **Area:** backend
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b

## Goal

Deploy a compatible immutable Linux game/service/content release on the authorized LAN host, with matching client connection/gameplay evidence and rollback; otherwise record the precise external gate.

## State

Runnable durable profile/bootstrap candidate prepared; protected runtime unchanged. The unpublished five-schema PostgreSQL + two-host rehearsal passed and was cleaned. The old `e1-2a5b634429b75d46519ad2a28560b8953ed99831` game remains healthy on approved UDP17777. No game packaging/deployment until the integration freeze and UE slot grant.

## Evidence

2026-10-25: five focused Python regressions passed; both targeted .NET builds passed with zero warnings/errors. The service-only Linux rehearsal verified all five role/schema gates, authenticated and wrong-bearer probes, one fixture grant (2 items / revision 1 / one receipt), denied version changes, constrained singleton locks, stable private credentials, database-container recreation with retained state, nonempty-init refusal, 0600 files and no published ports. Owned containers/volume were removed; old E1 stayed healthy. Exact evidence is in the root orchestration runtime-release report.

Tracked-snapshot exporter/path fix independently passed at `da3ded63`; all earlier unverified-provenance artifacts remain superseded. The runnable rehearsal used 105 verified Linux hashes and service image `sha256:7526e7874ca42e961b5b3386ae150d52f676843217030285b0b263f51c9ad603` at pre-rebase `db08d72d`, plus the committed dependent-namespace recreation fix. This is pre-freeze enabling evidence, not remote playable/production acceptance. `/home/steam` resolves to `/srv/steam/steamhome`; diagnostic roots use that verified canonical home. Protected runtime inspection/application still requires a privileged operator; no bypass was attempted.

## Blocked on

Fresh security review of the runnable candidate; integration freeze/matching game+client packages; final privileged operator application.

## Next step

Independently review the runnable profile/bootstrap range, then rebuild matching game/service/content artifacts at the root-approved freeze. Only after package/provenance checks may the root agent request the concrete privileged operator step in `ops/steam-server/playable-release.md`; remote two-client gameplay/SQL/restart acceptance still follows.

## Rules for this task

Preserve old image, unrelated services/ports, least-privilege database roles and private credentials. No public binds, policy changes, new storage, or conversion of fixed development identities into a player feature. Only this lane may be edited; shared main permits ignored evidence only.

## Outcome

Pending.
