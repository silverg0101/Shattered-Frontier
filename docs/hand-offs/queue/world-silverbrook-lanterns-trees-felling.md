# Silverbrook door-lantern light, realistic trees, felling art for every species

**Area:** world · **Worktree:** SF-lane-a · **Owner decisions:** 2026-10-26 (answers to `docs/decisions/WORLD-silverbrook-water-and-swim.md` 1-3)

## Goal
1. **House fronts lit by their door lanterns.** Each house's front-door lantern becomes a real light source (warm, small radius, no or cheap shadows), so shaded fronts stop reading near-black. No global lighting change; the lantern is the light.
2. **Realistic trees.** Replace the zone's low-poly `SM_Oak_01` dressing with realistic broadleaf trees from content already in the project (Megascans/Fab is read-only but usable; PCG pines already look right). Harvest nodes keep showing the same mesh as the tree they replace.
3. **Felling art for every harvestable species.** Cut notches while chopping and a falling-tree animation on the last hit, for every tree type a node can show (pines, broadleaf, young trees), and the matching break-up for every rock type. Prefer one generic method that works on any mesh (e.g. notch decal/mask at the hit height plus a physics or authored topple of the node's own mesh) over per-species hand art.

## Rules
- Every change goes in the builder/import scripts (`build_silverbrook.py`, `silverbrook_props.py`, `pcg_dressing.py`, gathering code) so a map rebuild keeps it.
- Performance: lantern lights capped in radius and count; check frame time in the main street before/after.
- Visual work: captures before/after to `Artifacts/Evidence/silverbrook-lanterns-trees/`, fresh Opus reviewer per `docs/visual-review.md`, max 3 rounds, then owner.

## Checks
- `ShatteredFrontier.World.SilverbrookIntegrity` and the gathering felling tests stay green; add one test that every node mesh type has a felling path.

## State
Started 2026-10-26 by a Sonnet agent.
