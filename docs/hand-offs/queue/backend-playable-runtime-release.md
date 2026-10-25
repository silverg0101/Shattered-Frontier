# Playable runtime release

- **Area:** backend
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b

## Goal

Deploy a compatible immutable Linux game/service/content release on the authorized LAN host, with matching client connection/gameplay evidence and rollback; otherwise record the precise external gate.

## State

Service artifact candidate built; protected runtime unchanged. Current host runs the old `e1-2a5b634429b75d46519ad2a28560b8953ed99831` E1 image, with no visible durable service/PostgreSQL process or listener. No game packaging or deployment until the integration freeze and UE slot grant.

## Evidence

2026-10-25 SSH inventory: game healthy, only Forge/Tailscale alongside it; 76 GiB free on existing game volume. Runtime directory is private; `sudo -n` requires a password. Contracts: `ops/steam-server/README.md`, `docs/e1-linux-server-build.md`, both host READMEs. Current host contracts use loopback-only HTTP/2 and private development bearers, not production session authentication.

Independent review reproduced a P2 provenance defect: the earlier exporter included ignored checkout content. Every pre-fix image/artifact, including `0a3f3adb`/`sha256:388679586d6a1dd3de61118a7519ae278a8e7948f1dbb665be2c39cd5439f0f5`, is superseded and must not be a final release. The fix builds/copies only an isolated tracked Git archive, rejects checkout/output/snapshot overlap before writes, and retains the frozen SDK/locks/proto closure. Focused synthetic ignored-content/source and overlap regressions pass; fresh portable validation follows. This remains enabling work, not service/database/gameplay acceptance.

## Blocked on

Integration freeze/UE slot; independent candidate review and runnable bootstrap/profile preparation; final privileged operator application.

## Next step

Independently review the six source files in this release candidate, then prepare the minimal runnable bootstrap/profile before packaging the frozen revision and requesting final operator application. The bounded constraints are in `ops/steam-server/playable-release.md`.

## Rules for this task

Preserve old image, unrelated services/ports, least-privilege database roles and private credentials. No public binds, policy changes, new storage, or conversion of fixed development identities into a player feature. Only this lane may be edited; shared main permits ignored evidence only.

## Outcome

Pending.
