# WORLD: Silverbrook trees and rocks block, stand on the ground and are harvestable

Date: 2026-10-26. Status: implemented; owner questions open.

## What changed

- `pcg_dressing.py`: the gameplay dressing volume (trees, rocks, saplings) spawns its instances with `BlockAll`; the PCG spawner's default was no collision. The client-only cosmetic volume (grass, ferns, flowers, shrubs, litter) stays `NoCollision`.
- `pcg_dressing.py`: trees and rocks snap to the lowest ground under their base rim (8 rays at the mesh's base radius), not only under their centre, so no side of a base hangs over a slope.
- `build_silverbrook.py`: wall sections fill each run between two gatehouses end to end and reach 1 m into each gatehouse. The old fixed 20 m stride left 11-17 m grass gaps beside the gates.
- `zone_record.py`: every tree and rock outside the town wall is a gathering node (K = N), including bedrock slabs. `NodeModel` pins node i to point i when K = N and starts fresh state when the topology changes.
- Unreal: the server keeps one slot per point and spawns node actors within `ZoneNodeSpawnMetres` (40 m) of a player. It removes idle ones beyond `ZoneNodeDespawnMetres` (55 m). The two values are settings in `[SFGatheringPilotSettings]`. A removed node gives the map's dressing instance back.
- Check: `ShatteredFrontier.World.SilverbrookIntegrity` fails if a tree or rock has no collision, has no gathering point outside the town, or has a base more than 10 cm above the ground. It also fails if the wall ring has a gap wider than 50 cm.

## Owner questions (defaults in use)

1. Pine, young broadleaf and mossy boulder have no gathering definition. They resolve to `oak_tree` (wood) and `stone_outcrop` (rough stone), with the same yields and respawn. Bedrock slabs also use `stone_outcrop`.
2. Within 40 m, a tree or rock is shown with the existing harvest art (MightyOak, HarvestRock) instead of its own mesh. A pine becomes a low-poly oak while it is harvestable. Should species-specific harvest art come next?
3. The 123 rocks inside the town wall are not harvestable, because the town is protected and no-edit. Saplings are not harvestable either. Keep both?
4. The tall pine the owner reported floating near the East Gate was not found. All four pines within 60 m of the gate sat about 8 cm into the ground before this change. The worst float found anywhere was 32 cm on a slope, and the rim snap now fixes it. Please send coordinates if it still floats.
5. The town "looking far away" behind the wall is the layout: the wall is an ellipse with 400 m by 530 m semi-axes. This change does not alter it.
