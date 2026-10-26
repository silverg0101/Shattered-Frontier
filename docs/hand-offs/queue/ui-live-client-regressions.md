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
| Combat camera angle and framing | Implementing: `gameplay-camera-mouse-controls.md`, lane a. |
| Mouse sensitivity in Settings | Implementing: horizontal/vertical controls, Apply/Reset and per-player persistence; same task. |
| Defeat wording and actual server countdown | Implementing: `ui-combat-feedback-defeat-state.md`, lane d. |
| Manual respawn, closable death notice, clickable skull above HP | Owner requires explicit click after eligibility; closing keeps player dead and able to watch/be revived. Same task. |
| Replace HIT with animated damage above the receiver; distinct critical hits | Implementing: same task. Use resolved server damage and existing game styling. |
| NPC target name and maximum health | Confirmed display defect: production boar 489 HP, HUD showed fixture maximum 60. Same task. |
| Building hotbar icons and catalogue previews missing | Investigating: `ui-building-hotbar-regressions.md`, lane c. |
| Drag into/out of hotbar broken | Same task; deliberate shortcut removal must differ from Escape/focus cancellation and physical inventory drops. |
| MetaHuman avatar instead of Unreal mannequin | Open: `gameplay-player-metahuman-packaged.md`. |
| Gathering skill Raise/Hold/Lower controls | Open: `ui-gathering-skill-controls.md`; check actual skill-cap rules first. |
| Two-player public ground loot and persistence | Pending; required sequence below has not passed. |

## Evidence

Frozen `ff59b35dd17fef0edd1a7b7f5b9161b867370bc8` is deployed on LAN `192.168.50.133:17777`. Matching packaged clients connected and normal deaths/respawns were observed. No corpse loot, inventory transfer or item receipt was produced. Both owned clients were subsequently closed for the camera build. Runtime handoff commit `613c4e73` retains exact image identities and rollback.

Owner screenshots: `codex-clipboard-dbf9067a-2a14-470e-95b6-6d1560bb7be2.png` (death wording), `codex-clipboard-d0b3a4b0-fcf9-430d-a2a8-697ca812dadf.png` (missing building imagery). Raw evidence and fixture-position setup are under ignored `Artifacts/Orchestration/20261025-resume/` and lane e `Artifacts/PackagedAcceptance/LAN-ff59b35d-20261025-190917/`.

## Blocked on

Client fixes and matching-package verification. Visual changes require fresh independent Sol review and owner sign-off. One Unreal-heavy job at a time; three available subagent slots are active.

## Next step

Finish the three active child tasks; route the MetaHuman and gathering-skill tasks as slots free. Keep every row current and distinguish implemented, tested, owner-approved and deployed.

## Rules for this task

- Preserve the shared main checkout and sealed ff59 archives. Work in named lanes; commit and push precise changes.
- Do not claim loot acceptance from editor fixtures or seeded positions. The disconnected beta position seed was test setup only; no items or NPC health were seeded.
- Required live sequence: alpha obtains its own corpse loot into inventory, deliberately drops it, beta visibly sees the public ground model/highlight and uses the normal action to collect directly into beta inventory, then SQL proves alpha debit/beta once-only credit and personal-loot privacy. Controlled restart/reconnect must preserve the results.
- Keep death authority, revival and inventory invariants. Do not change combat balance to make a test pass.

## Outcome

Open. No release acceptance claimed.
