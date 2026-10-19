# GAT-01 — Growing resource nodes

Status: accepted by the owner 2026-10-19 (design). Not implemented. This record extends the [gathering node lifecycle](../Game-Design/Crafting/Legends_Shattered_Frontier_Crafting_Gathering_and_Resources_v1.0.md) (§3) so that every harvestable node visibly grows back, and so that a young node can be harvested for less. Asset authoring for growth stages is evaluated in [VEG-01](VEG-01-procedural-vegetation-editor-fit-check.md).

## Decisions

| # | Question | Decision |
| --- | --- | --- |
| 1 | Regrow in place, or at the pool's next point? | **At the pool's next free point**, chosen as today (§2.2). Routes keep varying and camping stays hard, with no stump state left behind. For a node that should come back where it stood, give it a **one-point pool**, which makes in-place regrowth a content choice rather than a code path. |
| 2 | Can young nodes be harvested? | **Yes, for less.** This applies to **every growing node kind**, not just trees. Rocks and ores push up out of the ground. |
| 3 | Herbalism | Not built yet, but the model must already cover herbs and flowers. The checklist below records the common kinds and what each needs, so adding one is a content step. |
| 4 | Stumps | **None.** A felled tree falls and is gone until something grows back at a pool point. A sapling never grows from a stump. The felling presentation's torn stump is removed with the fallen tree, or dropped entirely if keeping it costs real work ([felling WIP](../gameplay-harvest-felling-wip.md)). |

## Model

The states in §3.1 are unchanged (`pending_spawn`, `available`, `depleted`, `retired`). Growth is a separate attribute of an `available` node.

```text
respawn_at  = World's respawn time (§5.3, unchanged; scarcity still slows it)   → the node is MATURE at respawn_at
emerged_at  = respawn_at − grow_s                                               → the node APPEARS at stage 0 here
stage(now)  = last stage whose start_s ≤ now − emerged_at                       → derived, never stored separately
```

- **Appearance:** at `emerged_at`, zone-server spawns the node at the chosen free pool point in its first stage, and the node is `available`. A harvest intent against a stage that is not harvestable is rejected with a new reason, `node_immature`.
- **Economy:** because maturity still lands on World's `respawn_at`, node yield per hour (§5.5) and the scarcity maths (§5.3) are unchanged for players who wait.
- **Early harvest:** a young stage gives fewer charges: `charges_stage = max(1, round(charges × yield_fraction))`. The per-charge quantity formula (§5.2) is untouched. Harvesting the last charge depletes the node, it disappears, and World schedules the next growth as today.
- **No early-harvest advantage:** harvesting early must never out-earn waiting. The full cycle runs from depletion to maturity. For every harvestable stage, the validator requires `yield_fraction ≤ (respawn_s − grow_s + start_fraction × grow_s) / respawn_s`, using the unscaled `respawn_s` as the worst case.
- **Persistence:** only `respawn_at` is persisted, as today. `emerged_at` and the stage are derived, so restarts converge.
- **Replication:** the stage and its start time replicate. Clients interpolate rising and scaling locally, as presentation only.
- **Collision safety:** a stage that adds or enlarges blocking collision (a tree past sapling, any rising rock) advances only when no pawn, construction member or other node overlaps its next-stage bounds. If something does, the server retries on the next tick, so nothing grows into a player or a wall. Non-blocking stages (sprouts, buried rock tips) never wait.
- **Placement:** growth never happens on roads, build plots or protected areas. The pipeline's gameplay-candidates validator ([world pipeline](../settlement-world-pipeline.md)) already excludes these.

### `growth_profile` (content, on `gathering_node`, typed configuration)

```json
"growth_profile": {
  "style": "rise",
  "grow_s": "derived",
  "stages": [
    { "name": "emerging", "start_fraction": 0.0, "harvestable": false, "yield_fraction": 0.0, "blocking": false, "visual": { "rise": 0.15 } },
    { "name": "young",    "start_fraction": 0.5, "harvestable": true,  "yield_fraction": 0.4, "blocking": true,  "visual": { "rise": 0.6 } },
    { "name": "mature",   "start_fraction": 1.0, "harvestable": true,  "yield_fraction": 1.0, "blocking": true,  "visual": { "rise": 1.0 } }
  ]
}
```

**Field rules:**
- **`stages`:** 2–4 stages. `start_fraction` is a fraction of `grow_s` and increases with each stage. The first stage starts at 0 and the last at 1.0 (mature, `yield_fraction` 1.0).
- **`grow_s`:** `"derived"` maps to `tune.crafting.grow_fraction` × `respawn_s`, with a default fraction of 0.5. The growth window must fit inside the respawn time.
- **`style`:** one of the visual styles below.

Defaults per kind come from tuning, so most nodes need no explicit profile.

### Visual styles

| Style | What the player sees | Asset cost |
| --- | --- | --- |
| `swap` | A different mesh per stage, such as sapling, young tree and full tree | One asset per stage. PVE can grow a matching set from one seed (VEG-01). |
| `scale` | The same mesh grows from small to full size | None beyond the mature mesh |
| `rise` | The same mesh starts mostly buried and pushes up out of the ground; optional disturbed-soil decal | None beyond the mature mesh |

Styles can combine, for example `rise` + `scale` for crystals.

## Kind checklist (coverage today, and the path for Herbalism)

| Kind | Skill | Style | Stages | Harvestable from | Blocking from | Asset source | Today |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Tree | Logging | `swap` | sapling, young, mature | young | young | PVE stage set or hand-made; felling uses the mature mesh | Pilot wood node and felling WIP; growth not built |
| Stone outcrop | Mining | `rise` | emerging, young, mature | young | young | Existing rock meshes | Not started |
| Ore vein | Mining | `rise` | emerging, young, mature | young | young | Existing rock meshes plus an ore material | Pilot ore node; growth not built |
| Crystal | Mining (higher tiers) | `rise` + `scale` | emerging, young, mature | young | mature | Hand-made cluster | Not started |
| Bush / berries | Herbalism or foraging | `scale`, berries appear at mature | sprout, leafy, fruiting | leafy (leaves or fibre), fruiting (berries) | none | Existing shrub meshes or PVE | Not started |
| Herb / flower | Herbalism | `scale`, flowering at mature | sprout, leafy, flowering | leafy | none | CC0 flowering clumps already in the biome library | Not started (Herbalism is future) |
| Fibre / reeds | Herbalism or farming | `scale` | shoot, mature | mature | none | Grass and reed meshes | Not started |
| Mushroom | Herbalism | `rise` + `scale` | button, cap | cap | none | Hand-made | Not started |

**Not growth nodes:** these keep their own lifecycles.
- Fishing spots
- Carcasses (`retired`, never regrow)
- Farming plots (player-planted, §6.4)

### Adding a new kind

1. Pick the skill, tool and tier, and confirm the resource exists (§1).
2. Choose a style. Use `scale` or `rise` unless a stage really needs a different silhouette.
3. Use the tuned default stages, or set a `growth_profile` that passes the no-early-harvest-advantage rule.
4. Say which stages block movement, and keep sprouts non-blocking.
5. Provide the mature asset, plus stage assets only for `swap`.
6. Add the kind to a biome PCG graph's gameplay-candidates output with spacing and exclusions (world pipeline, stage 3).
7. Add a pool and its weights (§2.2). Use a one-point pool only where in-place regrowth matters.
8. Run the targeted check: stages advance on the server clock, early harvest gives the reduced charges, `node_immature` rejects, and growth is blocked while a pawn overlaps.

## Not decided here

- **Exact stage fractions and tuning defaults:** tuning owners decide these per the tuning architecture.
- **Respawn while growing:** whether a node can be `despawn_idle_s`-relocated while growing. The default is no; relocation applies only to mature nodes.
- **Rendering cost:** the cost of many interpolating nodes. The world pipeline's stage 3 measurement covers it.
