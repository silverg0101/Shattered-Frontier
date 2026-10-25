# Greenlands starter creatures and a hybrid

- **Area:** art
- **Owner of the next step:** agent
- **Updated:** 2026-10-25

## Goal

Starting-area creatures for players to kill, all made by the Asset Forge creature job with idle, walk, run, attack and death clips:
- Deer
- Stag
- Rabbit
- Wolf
- Bear
- one harder hybrid, "Bramble Stag": a deer with thorny antlers and a lizard tail

Each passes its own fresh Opus visual review, then owner sign-off.

## State

Not started. Species come from the Greenlands wildlife table. Planned closest Zoo-300K animals:
- Deer and Stag: DEER
- Rabbit: Hamster
- Wolf: Coyote
- Bear: BEAR

The boar run showed these packs have no clean walk or run loops, so gaits will come from UniMate or the procedural fallback.

## Evidence

- Forge API: `tools/asset-manager/API.md` (Creature jobs).
- Pipeline: `docs/dependency-map/art-pipeline.md`.
- Live Forge: http://192.168.50.133:8090.

## Blocked on

`art-greenlands-boar.md`: the pipeline fixes must pass review and be deployed first.

## Next step

Submit the Deer as a creature job on the live Forge and wait for it to finish. Then send its sheets to a fresh Opus visual review. Run one creature at a time.

## Rules for this task

- Run one creature job at a time, because RA-SIEMS disk is about 4.6 GB.
- At most 3 visual review rounds per creature, then it goes to the owner.
- If an asset is still stuck after a couple of rounds, hand it whole to a Fable agent.

## Outcome
