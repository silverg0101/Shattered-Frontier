# Combat feedback and defeat state

- **Area:** ui
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b (integrated)

## Goal
Show server-resolved damage over its receiver, correct generic NPC target data, and make normal production respawn wait for an eligible owner click. Close/Esc hides only the notice; the skull above vitals reopens it, and legitimate revival clears it.

## State
Integrated at `cfb9c752`: server-authored receiver damage and criticals, generic NPC maximum/name, explicit eligible manual respawn, closable notice and skull reopen. The combined Editor build passed. The four original focused checks and `ShatteredFrontier.Hud.CombatFeedback.BurstSeparation`/widget discovery checks passed. Independent visual round 2 passed the sole burst fix (1 PASS/0 FAIL, no fix list); round 1's unchanged views passed. Owner sign-off and packaged network/service behavior remain open.

## Evidence
Combined 15 distinct targeted checks passed at `cfb9c752`; logs in lane B `Artifacts/ClientIntegration/20261026/`, final burst log `combat-feedback-widget-burst.log`. The selected actor regression asserts release boar 489 to 481 and Wolf name through NativeTick. Existing resurrection API succeeds while notice is closed in its isolated fixture. Visual evidence and review: main ignored `Artifacts/Orchestration/20261025-resume/ui-combat-feedback/independent-round2/REVIEW.md` and the round 1 review.

## Blocked on
Owner visual sign-off and matching packaged network/service acceptance. Renderer inputs are staged and do not prove actual remote critical resolution, durable death/respawn or loot acceptance.

## Next step
Prepare the owner review packet from the passed rounds. On matching packages, verify server damage/critical values, defeated reconnect, manual ready/pending/retry click, notice close/reopen and revival.

## Rules for this task
No balance, inventory or client damage authority changes. Owner explicitly authorized manual production normal respawn at the existing server wait. Existing spirit release/resurrection remain usable. WBP owns visual styling/layout. Main checkout is read-only except ignored evidence. At most three independent visual review rounds; builder never grades its work.

## Outcome
Integrated and independently reviewed; packaged network and owner gates pending.
