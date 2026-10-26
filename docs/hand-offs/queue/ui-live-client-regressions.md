# Live client fixes and remaining loot acceptance

- **Area:** ui
- **Owner of the next step:** agent (root orchestrator)
- **Updated:** 2026-10-26
- **Worktree:** C:\Projects\SF-lane-b

## Goal

Fix the owner-reported packaged-client regressions, review the resulting behavior, then finish the two-player loot acceptance on matching deployed packages.

## State

| Item | Status / task |
| --- | --- |
| Combat camera angle and framing | Integrated; targeted tests passed. Native selected-target framing and 200% versus 400% mouse motion remain open. |
| Mouse sensitivity in Settings | H/V controls, Apply/Reset and persistence passed; independent Settings review 2 PASS/0 FAIL. Owner sign-off and packaged input check pending. |
| Defeat wording and actual server countdown | Integrated; targeted tests and independent staged visual review passed. Packaged server/death reconnect acceptance and owner sign-off pending. |
| Manual respawn, closable death notice, clickable skull above HP | Integrated; explicit ready click and revival tests passed. Real service reconnect/revival acceptance pending. |
| Replace HIT with receiver damage and distinct critical hits | Integrated; bounded burst widget regression and fresh round 2 visual review passed with no fix list. Network acceptance and owner sign-off pending. |
| NPC target name and maximum health | Actual selected-actor regression passed, including production boar 489 to 481 and Wolf name. Packaged observation pending. |
| Building hotbar icons and catalogue previews missing | UI cook roots, static asset checks and actual WBP regression passed. Package presence/native view pending. |
| Drag into/out of hotbar broken | Catalogue/bag shortcut assignment, swap and deliberate removal regression passed. Native packaged drag pending. |
| MetaHuman avatar instead of Unreal mannequin | Existing 1,181-file assembly cache hash verified; client-only cook contract passed; Orlando spawned in Editor. Packaged closure/animation/review pending. |
| Gathering skill Raise/Hold/Lower controls | Real caps confirmed; unsupported service controls hidden/guarded; combined tests passed. Durable six-skill support remains a production gap. |
| Two-player public ground loot and persistence | Now the automated netcheck `loot-handoff` (`pwsh tools/Test-CombatPilot.ps1 -Clients 2 -Services -LootHandoffNetcheck`). Passed 2026-10-26 in lane e (94 s): alpha took 2 boar hides from its own corpse and dropped them, beta collected them with the shared action and no window, ledger `alphaHide=0, betaHide=2`, one drop and one take receipt, corpse eligible to alpha only, ground empty; unchanged after a server restart and reconnect of both clients. Editor/headless with the local PostgreSQL and hosts; a manual owner playtest on packages is optional, not a gate. Live run on dbe514e7 (2026-10-26) **blocked, not run**: see Live loot attempt. |

## Evidence

Frozen `ff59b35dd17fef0edd1a7b7f5b9161b867370bc8` is deployed on LAN `192.168.50.133:17777`. Matching packaged clients connected and normal deaths/respawns were observed. No corpse loot, inventory transfer or item receipt was produced. Both owned clients were subsequently closed for the camera build. Runtime handoff commit `613c4e73` retains exact image identities and rollback.

Lane B integrated `cfb9c752` plus existing-store release documentation `6cd197df`; it has not been deployed. Independent source review found no actionable P1/P2. Fifteen distinct targeted Editor/gameplay checks passed at `cfb9c752`, with final feedback/widget regressions run after the Slate measurement fix. Static UI-art/cook checks and all nine face-class hashes pass. Fresh independent staged combat round 2 passed the one prior burst failure (7 frames, no fix list); unchanged round 1 views retain their pass. Staged screenshots do not prove live network behavior or owner sign-off.

Owner screenshots: `codex-clipboard-dbf9067a-2a14-470e-95b6-6d1560bb7be2.png` (death wording), `codex-clipboard-d0b3a4b0-fcf9-430d-a2a8-697ca812dadf.png` (missing building imagery). Raw evidence and fixture-position setup are under ignored `Artifacts/Orchestration/20261025-resume/` and lane e `Artifacts/PackagedAcceptance/LAN-ff59b35d-20261025-190917/`.

## Live loot attempt (2026-10-26, dbe514e7 on 192.168.50.133)

Not run; no client connected, no SQL write, no restart. Live game `8dc672a2…` runs `-SFDevIdentity` (dev admission open) but not `-LootHandoffNetcheck`. Blockers: (1) the packaged dbe514e7 client has no driver: `SFLootHandoffNetcheck.cpp` arrived in e723914e, after dbe514e7, and it and `SFLootWorkflowNetcheck.cpp` are `#if WITH_EDITOR`. (2) Editor `-game` clients from lane e are no substitute: the boar kill is the server half of the fixture (`FSFLootWorkflowNetcheck::Tick` with server authority: teleport plus `ReceivePilotDamage(1000000)`), absent from the packaged Linux server and barred live as a cheat kill. The client driver only waits for a corpse and has no combat. Exit: a person plays Alpha's real boar kill, or a client-side combat driver lands in a packaged build. Live DB baseline was not read: socket psql as `sf_admin` asks for a password, which was not handled.

## Blocked on

Matching-package verification, packaged native/network acceptance and owner sign-off for visual changes. One Unreal-heavy job at a time. The existing ff59 release remains live until a compatible server/client update is ready and visual approval is obtained.

## Next step

Freeze and inspect matching packages from the integrated revision, capture real native camera/avatar/hotbar/combat behavior, and prepare the owner review packet. After owner sign-off, apply the compatible existing-store server/client release and resume live loot and persistence acceptance. Continue other ready gameplay work while that gate is pending.

## Rules for this task

- Preserve the shared main checkout and sealed ff59 archives. Work in named lanes; commit and push precise changes.
- Do not claim loot acceptance from editor fixtures or seeded positions. The disconnected beta position seed was test setup only; no items or NPC health were seeded.
- Required live sequence: alpha obtains its own corpse loot into inventory, deliberately drops it, beta visibly sees the public ground model/highlight and uses the normal action to collect directly into beta inventory, then SQL proves alpha debit/beta once-only credit and personal-loot privacy. Controlled restart/reconnect must preserve the results.
- Keep death authority, revival and inventory invariants. Do not change combat balance to make a test pass.

## Outcome

Open. No release acceptance claimed.
