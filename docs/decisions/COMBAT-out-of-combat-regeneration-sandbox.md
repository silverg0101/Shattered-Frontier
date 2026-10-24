# Out of combat health regeneration sandbox

Scope: `/Game/Frontier/Maps/Sandbox/SB_Regeneration` only. The Silverbrook pilot has a fixed health resource but no character attribute snapshot, so this sandbox uses the Combat Part 1 `ATTRIBUTE_MIN` Constitution 10 as a development fixture. It does not change Greenlands combat.

Design values:

- `docs/Game-Design/Combat/Legends_Shattered_Frontier_Combat_Abilities_Effects_and_Runtime_v1.0.md`, combat state machine: hostile action or threat enters combat; exit requires 5 seconds without a hostile action and no threat table.
- `docs/Game-Design/Combat/Legends_Shattered_Frontier_Combat_Attributes_and_Ratings_v1.0.md`, Regeneration and attribute sources: health has zero in-combat regeneration; out of combat regeneration starts after 5 seconds, ticks every 1 second at `MaxHealth × 0.03 × (1 + con_total / 500)`. Constitution minimum is 10.
- `docs/Game-Design/AI/Legends_Shattered_Frontier_AI_NPC_Behavior_and_Memory_v1.0.md`, §8.2: evade clears threat. The existing boar evade supplies that transition.

The server owns combat state, the simulation clock, threat checks and health. The isolated map uses the existing pilot maximum health of 80. Both clients observe the damaged pawn, combat exit, delay and replicated healing. Production attribute binding, mana/stamina resources and rest-area modifiers remain separate work; no missing design value or owner decision is needed for this sandbox slice.
