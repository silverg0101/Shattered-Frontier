# Out-of-place cute NPC that kills attackers

- **Area:** gameplay
- **Owner of the next step:** agent
- **Updated:** 2026-10-25

## Goal

An NPC that doesn't look like it belongs in the game: super cute, small, child-sized and non-human. No one can kill it, and anyone who hits it dies.

Done means:
- the model is made by the Asset Forge and passes visual review and owner sign-off;
- in game it takes no damage and kills the attacker, with server authority;
- a netcheck or automation test proves both.

## State

Not started.

## Evidence

- Combat lives in `GameClient/Source/ShatteredFrontier/{Public,Private}/Combat/SFCombatPilot.*` and `ASFE1Character`.
- See `docs/dependency-map/gameplay.md` and `docs/gameplay-combat-pilot.md`.

## Blocked on

The model needs the creature pipeline from `art-greenlands-boar.md`. The gameplay rule is not blocked.

## Next step

Build the rule in a sandbox map `SB_OutOfPlaceNpc` first:
1. A tagged actor rejects all damage on the server.
2. It applies lethal damage to the instigator.

Add a test covering both, then place it in Greenlands.

## Rules for this task

- Write only under `/Game/Frontier/`.
- Keep the tuning as typed configuration, not hard-coded.

## Outcome
