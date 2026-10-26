# Combat feedback and defeat state

- **Area:** ui
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-d

## Goal
Show server-resolved damage over its receiver, correct generic NPC target data, and make normal production respawn wait for an eligible owner click. Close/Esc hides only the notice; the skull above vitals reopens it, and legitimate revival clears it.

## State
Candidate04d9a86b passed its Editor build/four tests and independent source review. Visual round1:9 pairs pass/1 burst fails;3 motion contacts pass/1 burst fails. Round2 changes only overlapping burst text: measured full-label/shadow/max-pop/float envelopes and a real-widget burst regression. Combined build/test and recapture are pending; no UE job started for this fix.

## Evidence
Prior candidate04d9a86b: `ShatteredFrontier.Characters.ManualRespawn`, `.VitalsHud`, `ShatteredFrontier.Hud.CombatFeedback`, `.GenericTarget`4/4 success. New `.Hud.CombatFeedback.BurstSeparation` awaits the combined gate. The selected actor regression asserts release boar 489 to 481 and Wolf name through NativeTick. Existing resurrection API succeeds while notice is closed in its isolated fixture. Logs and staged-only captures: main ignored `Artifacts/Orchestration/20261025-resume/ui-combat-feedback/`.

## Blocked on
Independent review, owner visual sign-off, and matching packaged network/service acceptance. Renderer inputs are staged and do not prove actual remote critical resolution, durable death/respawn or loot acceptance.

## Next step
Merge the spacing follow-up into integration B, run the combined gate including BurstSeparation, then capture the identical staged-only burst and obtain fresh round2 review. Packaged server-event and manual dead reconnect/click acceptance remain separate.

## Rules for this task
No balance, inventory or client damage authority changes. Owner explicitly authorized manual production normal respawn at the existing server wait. Existing spirit release/resurrection remain usable. WBP owns visual styling/layout. Main checkout is read-only except ignored evidence. At most three independent visual review rounds; builder never grades its work.

## Outcome
Code candidate ready; visual/network/owner gates pending.
