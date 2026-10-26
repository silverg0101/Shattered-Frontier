# Live client fixes and remaining loot acceptance

- **Area:** ui
- **Owner of the next step:** agent (root orchestrator)
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b

## Goal

Fix the owner-reported packaged-client regressions, review the resulting behavior, then finish the two-player loot acceptance on matching deployed packages.

## State

| Item | Status / task |
| --- | --- |
| Combat camera angle and framing | Code/tests passed: `gameplay-camera-mouse-controls.md`, source `209bf3bd`. Actual selected-target visual/input-motion acceptance remains open. |
| Mouse sensitivity in Settings | H/V controls, Apply/Reset and local persistence passed; independent Settings review 2 PASS/0 FAIL. Owner sign-off and packaged input check pending. |
| Defeat wording and actual server countdown | Implemented: `ui-combat-feedback-defeat-state.md`, candidate `04d9a86b`; four targeted checks passed. Rendering review pending. |
| Manual respawn, closable death notice, clickable skull above HP | Same candidate: explicit click after eligibility; closing keeps player dead and able to watch/be revived. Real service reconnect/revival acceptance pending. |
| Replace HIT with animated damage above the receiver; distinct critical hits | Same candidate: resolved server damage, bounded animated entries. Staged rendering evidence exists; network acceptance pending. |
| NPC target name and maximum health | Same candidate fixes production boar 489 versus fixture HUD maximum 60; actual selected-actor regression passed. |
| Building hotbar icons and catalogue previews missing | Candidate `665b3cad`, `ui-building-hotbar-regressions.md`: missing UI cook roots restored; static checks pass, Unreal/package verification pending. |
| Drag into/out of hotbar broken | Same candidate: catalogue/bag shortcut assignment, swap and deliberate shortcut removal separated from cancellation/physical item drops. Runtime verification pending. |
| MetaHuman avatar instead of Unreal mannequin | `db946859`, `gameplay-player-metahuman-packaged.md`: verified existing 1,181-file assembly cache and client-only cook preflight. Orlando spawned in Editor rendering; packaged closure/review pending. |
| Gathering skill Raise/Hold/Lower controls | `144a0520`, `ui-gathering-skill-controls.md`: real caps confirmed, durable preference gap recorded; unsupported service controls hidden/guarded. Combined tests pending. |
| Two-player public ground loot and persistence | Pending; required sequence below has not passed. |

## Evidence

Frozen `ff59b35dd17fef0edd1a7b7f5b9161b867370bc8` is deployed on LAN `192.168.50.133:17777`. Matching packaged clients connected and normal deaths/respawns were observed. No corpse loot, inventory transfer or item receipt was produced. Both owned clients were subsequently closed for the camera build. Runtime handoff commit `613c4e73` retains exact image identities and rollback.

Lane b integration candidate `fd39c823` combines the source branches above; it has not been deployed. Independent source review found no actionable P1/P2 in camera, skill capability, hotbar or combat/death changes. Static UI-art/cook checks and all nine face-class hashes pass. Editor/gameplay tests must now run against the combined source; staged screenshots do not prove live network behavior.

Owner screenshots: `codex-clipboard-dbf9067a-2a14-470e-95b6-6d1560bb7be2.png` (death wording), `codex-clipboard-d0b3a4b0-fcf9-430d-a2a8-697ca812dadf.png` (missing building imagery). Raw evidence and fixture-position setup are under ignored `Artifacts/Orchestration/20261025-resume/` and lane e `Artifacts/PackagedAcceptance/LAN-ff59b35d-20261025-190917/`.

## Blocked on

Client fixes and matching-package verification. Visual changes require fresh independent Sol review and owner sign-off. One Unreal-heavy job at a time; three available subagent slots are active.

## Next step

Run one combined Editor build and the exact child-task checks in lane b, then complete independent visual review/owner approval, freeze matching packages and resume the required live loot sequence. Keep every row current and distinguish implemented, tested, owner-approved and deployed.

## Rules for this task

- Preserve the shared main checkout and sealed ff59 archives. Work in named lanes; commit and push precise changes.
- Do not claim loot acceptance from editor fixtures or seeded positions. The disconnected beta position seed was test setup only; no items or NPC health were seeded.
- Required live sequence: alpha obtains its own corpse loot into inventory, deliberately drops it, beta visibly sees the public ground model/highlight and uses the normal action to collect directly into beta inventory, then SQL proves alpha debit/beta once-only credit and personal-loot privacy. Controlled restart/reconnect must preserve the results.
- Keep death authority, revival and inventory invariants. Do not change combat balance to make a test pass.

## Outcome

Open. No release acceptance claimed.
