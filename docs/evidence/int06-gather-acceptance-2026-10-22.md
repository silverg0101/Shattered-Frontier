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
