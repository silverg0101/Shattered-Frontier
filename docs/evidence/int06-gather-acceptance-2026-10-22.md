# INT-06 gather acceptance (2026-10-22)

Worktree: `SF-wt-int06`, branch `int06-accept`. Commands used the worktree's `.uproject`, pinned .NET 10.0.402 and PostgreSQL 18.6, synthetic per-run credentials, and run-private host outputs under each evidence root. No Steam or shared server action was used.

| Check | Result | Evidence |
| --- | --- | --- |
| `Build.bat ShatteredFrontierEditor Win64 Development -Project=<this worktree>/GameClient/ShatteredFrontier.uproject -WaitMutex -NoHotReloadFromIDE` | Passed after `GrantWithIntent` change | Local `int06-editor-build-capability.log`; `Result: Succeeded`, 20.76 s |
| `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests ShatteredFrontier.Backend.Grpc.GatherFollowUpIntent" -TestExit="Automation Test Queue Empty"` | Passed, one test | Local `int06-gather-followup-automation-final.log`; `Test Completed. Result={Success}` |
| `tools/Test-GameplayHosts.ps1 -EvidenceRoot Artifacts/Persistence/GameplayHosts/int06-capability-20261022-010202` | Passed; owned database removed | `harness.json` status `pass`; `results.txt` includes missing-intent refusal, committed intent after host kill, XP=1, quest oak=1, replay/conflict/omit checks |
| `tools/Test-QuestPilot.ps1 -Leg Full -Services -Int06KillAfterGrant -Port 27875` | Passed; private PostgreSQL removed | `Artifacts/QuestPilot/full-20261022-011115-670/results.json`: `status=Passed`, `counts=1,1,2`, oak=1; restart log shows `QUEST_PILOT_RESTORED ... Active=mine_iron`, restored client sees step 2 |

The gather client now calls `GrantWithIntent` when it sends an immutable follow-up. The method requires the intent before the host enters the ledger transaction. An older InventoryLedger host does not implement this RPC and returns `UNIMPLEMENTED` without granting. Legacy producers continue to use `Grant`.

The older `Test-GatheringPilot.ps1 -Ledger -ExitAfterCommit` fixture failed before Grant: its sandbox has no quest component, so `GATHERING_FOLLOW_UP_CAPTURE_FAILED` and code 14 are expected under the new required-intent contract. The first Silverbrook Full services attempt could not start combat because the package-free `WorldContent.Tool` Debug DLL was absent; building it with the pinned SDK resolved that prerequisite. Subsequent attempts exposed that the restart leg must enable `bRestoreOnConnect` and a stable fixture identity; both are now confined to the INT-06 fixture mode. The passing run first observed host-down code 14 with unchanged node charges, then the committed grant and immediate server exit. The restarted server and client used the same synthetic character.

The relay is still **not restore-fenced**. B-04 currently has a synthetic Stage S fixture, not an external enrollment proof surviving database restore. Relay activation after restore must remain gated on that separate contract and its owner acceptance.

## After rebase onto `origin/main`

Rebased INT-06 implementation commit: `b6cc6c0a`. The same focused checks were repeated against that tree:

| Check | Result |
| --- | --- |
| Editor `Build.bat` against this worktree's `.uproject` | Passed; `Result: Succeeded`, 35.23 s (`int06-editor-build-rebased.log`) |
| `ShatteredFrontier.Backend.Grpc.GatherFollowUpIntent` | Passed; one test, `Result={Success}` (`int06-gather-followup-automation-rebased.log`) |
| `Test-GameplayHosts.ps1` | Passed; `Artifacts/Persistence/GameplayHosts/int06-rebased-20261022-011739/harness.json` reports `pass` and private storage removed; `results.txt` includes XP=1, quest oak=1, and host kill recovery |
| `Test-QuestPilot.ps1 -Leg Full -Services -Int06KillAfterGrant -Port 27876` | Passed; `Artifacts/QuestPilot/full-20261022-011902-160/results.json` reports `counts=1,1,2`, oak=1 and cleanup removed; restart log shows `Active=mine_iron`, restored client step 2 |

## Durable gathering rerun (2026-10-24)

The 255eceeb durable-node and 39cf96da Unreal Harvest changes invalidated the old Grant log hook and made the prior `Harvest` RPC name unsafe against an older host that ignores new request fields. The client now calls the distinct `HarvestWithIntent` method and logs `GATHERING_HOST_CAPABILITY_REQUIRED` on `UNIMPLEMENTED`. A new host refuses legacy `Harvest`. The focused legacy-handler fixture invokes `HarvestWithIntent`, receives `UNIMPLEMENTED`, and confirms the legacy mutation handler was not called. The dedicated-server fixture uses `UnrealEditor-Cmd.exe -server` with owned hosts and database; it exits on a granted Harvest reply before the client applies the item or quest follow-up.

| Check | Result | Evidence |
| --- | --- | --- |
| Editor build after regenerated gathering stub | Passed | `Artifacts/int06-editor-build-20261024.log`, `Result: Succeeded` |
| `ShatteredFrontier.Backend.Grpc.GatherFollowUpIntent` | Passed 1/1 | `Artifacts/int06-gather-automation-20261024.log` |
| `ShatteredFrontier.Gathering` | Passed 16/16 | `Artifacts/int06-gathering-tests-20261024.log` |
| `Test-GameplayHosts.ps1 -GatheringOnly` | Passed 10/10; private database removed | `Artifacts/Persistence/GameplayHosts/int06-gather-20261024-02/{results.txt,harness.json}`; includes old-host `UNIMPLEMENTED` and both crash gaps |
| `Test-GameplayHosts.ps1` | Passed 48/48; private database removed | `Artifacts/Persistence/GameplayHosts/int06-full-20261024-01/{results.txt,harness.json}` |
| `Test-QuestPilot.ps1 -Leg Full -Services -Int06KillAfterGrant -Port 27880` | Passed; private database removed | `Artifacts/QuestPilot/full-20261024-090907-881/results.json`: `counts=1,1,1,2,1`, oak=1, restored client step 2; one progression and one quest source receipt before and after restart |

The five dedicated-server counts are delivered harvest receipt, ledger operation receipt, delivered outbox row, progression and quest source receipts, and saved oak node with two charges. The runner checks the same receipt and charge counts after restart. The host-down phase checks no grant while the node snapshot is unavailable. Two earlier runs failed in the runner before this pass: its old Grant log expectation no longer applied, then its private CharacterState schema stopped at v4 while the host required v7. The runner now checks the durable-node path and installs v5–v7. The independent B-04 restore fence remains an external release gate.
