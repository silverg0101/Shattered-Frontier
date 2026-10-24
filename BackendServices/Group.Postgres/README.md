# Premade group persistence

`Gameplay.Host` maps `sf.group.v1.GroupService` when `GAMEPLAY_GROUP_CONNECTION` is set. The trusted dedicated server keeps the current party rules in its session model, sends accepted create/join/leave/kick/role changes to the host, and restores connected members by character ID after reconnect or world-server restart. Invitations still expire in the Unreal session. This slice covers five-member same-realm premades; Redis live state, LFG, extended parties, and group loot remain later design work.

Apply `BackendServices/Group.Postgres/Schema.sql` through `SchemaMigration.Tool group` under an owner role before host startup. Grant the runtime role `USAGE` on `"group"` and `SELECT,INSERT,UPDATE,DELETE` on its tables only. The runtime store never executes DDL. `"group"` must remain quoted in SQL. `GroupControls` declares a shipped five-member limit with bounds 2–5, effective on host restart; the session invite TTL and XP radius remain in `USFPartySettings`.

Focused check: `pwsh tools/Test-GameplayHosts.ps1 -GroupOnly -EvidenceRoot <fresh Artifacts/Persistence/GameplayHosts child>` with the pinned SDK. It checks wire replay, conflict, membership, roles, DML-only access, and host restart.
