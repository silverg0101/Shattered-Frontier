# Combat feedback and defeat state

- **Area:** ui
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-d

## Goal
Show server-resolved damage over its receiver, correct generic NPC target data, and make normal production respawn wait for an eligible owner click. Close/Esc hides only the notice; the skull above vitals reopens it, and legitimate revival clears it.

## State
Review candidate implemented. Editor build and four focused tests pass. 29 in-engine 1920x1080 rendering-fixture captures show normal/critical/burst/snapshot damage and waiting/ready/accepted/closed-skull/reopened/revived states. Independent source/visual review and owner sign-off remain pending.

## Evidence
`ShatteredFrontier.Characters.ManualRespawn`, `.VitalsHud`, `ShatteredFrontier.Hud.CombatFeedback`, `.GenericTarget`: current-code 4/4 success. The selected actor regression asserts release boar 489 to 481 and Wolf name through NativeTick. Existing resurrection API succeeds while notice is closed in its isolated fixture. Logs and staged-only captures: main ignored `Artifacts/Orchestration/20261025-resume/ui-combat-feedback/`.

## Blocked on
Independent review, owner visual sign-off, and matching packaged network/service acceptance. Renderer inputs are staged and do not prove actual remote critical resolution, durable death/respawn or loot acceptance.

## Next step
Integrate the review branch with camera/building changes, review pairs/motion, and run bounded matching-package server-event and manual dead reconnect/click acceptance.

## Rules for this task
No balance, inventory or client damage authority changes. Owner explicitly authorized manual production normal respawn at the existing server wait. Existing spirit release/resurrection remain usable. WBP owns visual styling/layout. Main checkout is read-only except ignored evidence. At most three independent visual review rounds; builder never grades its work.

## Outcome
Code candidate ready; visual/network/owner gates pending.
