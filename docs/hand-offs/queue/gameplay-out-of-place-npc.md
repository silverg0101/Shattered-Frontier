# Out-of-place cute NPC that kills attackers

- **Area:** gameplay
- **Parent:** `art-forge-creatures.md` for the model part, which explains how we work and what to track where.
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** lane `e`. Run `pwsh tools/Enter-Lane.ps1 -Name e` to get `C:\Projects\SF-lane-e`. If lane `e` is busy, use any free lane. Only one Unreal-heavy job may run at a time.

## Goal

An NPC that doesn't look like it belongs in the game: super cute, small, child-sized and non-human. No one can kill it, and anyone who hits it dies. It is done when:
- the model is made by the Asset Forge, passes visual review, and the owner signs off;
- in game, the server makes it take no damage and kills whoever hits it;
- a netcheck or automation test proves both behaviours.

## State

Not started.

## Evidence

- Combat code: `GameClient/Source/ShatteredFrontier/{Public,Private}/Combat/SFCombatPilot.*` and `ASFE1Character`.
- Docs: `docs/dependency-map/gameplay.md`, `docs/gameplay-combat-pilot.md`.

## Blocked on

The model is blocked on `art-greenlands-boar.md`. The gameplay rule is not blocked.

## Next step

Build the rule in a sandbox map, `SB_OutOfPlaceNpc`:
1. A tagged actor rejects all damage on the server.
2. It applies lethal damage to the instigator.

Add a test that covers both, then place the NPC in Greenlands.

## Rules for this task

- Build in the sandbox map first and place it in Greenlands later.
- Prove the rule with `/test <filter>` or `/netcheck <name>`, running only the targeted tests.
- Write only under `/Game/Frontier/`.
- Tuning values go in typed configuration, not hard-coded.

## Outcome
