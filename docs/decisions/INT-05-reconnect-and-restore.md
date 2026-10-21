# INT-05: Player reconnect and character restore (approved 2026-10-21: F1a drain, F2a SavePosition, F3a gating)

## Problem

The MVP Player row needs reconnect, and the "Restart and package" journey needs a restarted server to give players back what they had. Today a reconnecting dev character gets its ledger backpack only on the next mutation. Its skills, XP and quest step start from zero, and its position is the player start. A disconnect during a commit drops the follow-up work: the ledger may grant the item, but the progression receipt (XP and quest count) is never sent.

## Decision

Everything below sits behind `USFGameplayServicesSettings::bRestoreOnConnect`, which is off by default. It also needs the guarded dev identity (`chr_dev_<name>`, BLD-02 step 3). Unbound connection ids never restore.

- **Restore on connect.** When the player's identity binds, the server reads three things in parallel:
  - the ledger container (`GetContainer`, then the backpack's existing `Reconcile`);
  - Progression `GetCharacterState`, for skills, lifetime XP, the quest step and the objective counts;
  - the saved position.

  Gathering, crafting, construction and quest actions are refused with "Restoring your character..." until all three reads succeed. Unavailable retries every 3 s. A position outside the world bounds or in another zone falls back to the player start. Restore logs `CHARACTER_RESTORED Character Container Items Level Skills Step Counts Position`.
- **Position.** Positions are stored in CharacterState through an additive Progression RPC:
  - `SavePosition(character_id, expected_position_revision, position{zone_id,x,y,z,yaw})`. It is a revision compare-and-set: a stale write returns FAILED_PRECONDITION and the server re-reads.
  - `CharacterStateReply` gains `position`, `position_revision` and `objective_counts`.

  The server saves on a typed interval (`PositionSaveIntervalSeconds`: default 30 s, bounds 5–600 s, server only, read at start) and once at logout. Only a pawn that moved at least 1 m is saved. Positions are a convenience, not economy state, so a lost save costs a few seconds of walking.
- **Disconnect mid-action (linkdead drain).** When the connection closes while the controller has pending work, the controller detaches from the connection instead of being destroyed. Pending work means:
  - a backpack mutation queued or in flight;
  - a gathering or crafting Committing phase;
  - a housing commit;
  - queued quest or progression receipts.

  The pawn stays where it was. Every outcome still resolves with its original operation id, and the follow-up receipts are sent. Then the controller saves the position and is destroyed (`CHARACTER_DRAINED Op... Seconds`), capped at `LinkdeadDrainSeconds` (default 30 s, bounds 5–120 s). A reconnect for the same character waits for its drained predecessor before restoring, so the restore reads the committed result. While linkdead, the pawn stays visible where it stood and takes no input. Boars never pick it as a target, and the pilot has no player-versus-player combat, so it cannot be attacked. Exactly-once comes from the existing operation-id receipts, not from the drain: the drain only makes sure the follow-ups are sent.
- **Server restart.** The same restore path runs. In-flight work on a killed server is resolved by the services' receipts. A gather whose ledger grant committed but whose progression receipt was never sent keeps its item but loses its XP and quest count. Follow-up **INT-06 (durable receipt outbox)** writes each follow-up receipt durably together with its commit, so a killed server never loses XP or quest counts.

## Acceptance

`tools/Test-Reconnect.ps1` runs on owned PostgreSQL with the ledger and gameplay hosts, two dev clients and the Full quest chain:
1. Alpha runs part of the chain.
2. Alpha disconnects during a gathering Committing phase, then reconnects.
3. The server restarts and both clients reconnect.
4. Alpha finishes the chain.

It asserts identical state across clients, the tracker resuming at the right step, the restored position within 1 m, and exactly-once DB rows. That means one ledger entry per gather operation, one progression and one quest receipt per source, and one reward payout.

## Not covered

- Real accounts and character selection (DEC-04).
- Combat health and cooldown restore.
- Node charges, which stay session-only.
- INT-06, the durable receipt outbox (above).

## Evidence (2026-10-21)

`tools/Test-Reconnect.ps1` passed (`Artifacts/Reconnect/run-20261021-084232-306`):
- The server dropped alpha with the first harvest's ledger grant in flight. The controller drained in 0.7 s (`CHARACTER_DRAINED ... TimedOut=0`), and the reconnected alpha was restored at step 1 with its saved position.
- After the UE server restarted, both characters were restored, and alpha resumed at step 3 within 1 m of its last save. The chain then finished.
- Durable counts (progression receipts, completed payouts, payouts, placements, quests, positions) were `9,1,1,1,1,1`, the same as the uninterrupted Full chain.

With `bRestoreOnConnect` off, Test-QuestPilot's Plain, Full, Full -Services, Combat, Gathering and Unified legs all pass. So do CombatPilot, GatheringPilot ×4, ConstructionNetwork, ClaimHousing and the automation filters (55 passed).
