# Silverbrook water, swimming and bridge ends

**Area:** world · **Worktree:** SF-lane-a (after the landscape-collision task lands) · **Owner report:** 2026-10-26 live playtest

## Goal
River and ponds read as real water, the player swims in them, and bridge approaches have no flat placeholder patches.

## Defects seen in the packaged client
1. Bridge ends: flat yellow-green untextured patches where the bridge deck meets the bank (both Silverbrook bridges).
2. Water is a flat skin: `M_GN_River` on flat planes (`import_ground.py`, `build_silverbrook.py`) has no depth colour, shoreline, foam or flow.
3. Camera below the water plane: the surface vanishes (single-sided plane, no underwater post-process).
4. No swim animation: the character walks in water; `SFSwimRules` / `Water/Swim` PhysicsVolumes exist but the anim graph shows no swim pose.

5. Town props (same build pass): hay bales are blocky cubes with a flat noise texture and stiff straw cards sticking out; replace them with a proper bale mesh or fix the material and drop the cards. Several house fronts in the main street read near-black in shade.

6. Harvest swap (from 9538fcb7): near players, pines and other trees turn into the blobby low-poly harvest oak. The harvest node must show the same mesh as the tree it replaces (see `docs/decisions/WORLD-silverbrook-trees-rocks-gathering.md` items 1-2).

## Route (decide in step 1, record in docs/decisions/)
- Prefer the engine Water plugin (Water Body River/Lake + Water Zone): depth colour, shoreline, underwater post-process and buoyancy come with it. Keep the plane route only if the plugin can't fit the server/dedicated build, and then add two-sided surface + underwater post-process volume.
- Swim: drive a swim locomotion state from the existing swim flag (no new rules); use a licensed swim clip already in the project or the creature/character motion library.
- Bridge ends: fix the material or mesh the patches come from in the builder script, not by hand.

## Checks
- Captures from the three owner views (bridge top, river level, under-surface) before/after in `Artifacts/Evidence/silverbrook-water/`, then the fresh-reviewer visual review (`docs/visual-review.md`), max 3 rounds.
- Swim netcheck or automation test: enter Eastwood Pond, swim state replicates.

## State
Implemented and landed on main 2026-10-26; visual review stopped after 3 rounds (round 3: 6 PASS, 8 FAIL), so it is **not signed off**. Open list: `docs/evidence/graphics/2026-10-26-silverbrook-water/REVIEW-r3.md`; owner questions: `docs/decisions/WORLD-silverbrook-water-and-swim.md`.

## Outcome
Water (SLW flow/depth/foam, underwater grade), swim clip and pose, bridge-end fill, node shows its own tree, new hay bales, props out of water. Test: `tools/Test-SwimPose.ps1` PASS; `SilverbrookIntegrity` PASS. Not run: Silverbrook environment netcheck (WorldContent bridge failed here).
