# Rest-area health regeneration sandbox

`SB_RestRegen` exercises the Combat Part 1 *Regeneration* rule in `docs/Game-Design/Combat/Legends_Shattered_Frontier_Combat_Attributes_and_Ratings_v1.0.md`: a `rest_area` doubles out-of-combat health and mana regeneration. The existing health clock uses maximum health × 0.03 × (1 + Constitution/500) per one-second tick, after five clear seconds to leave combat and five more seconds of regeneration delay. The Silverbrook pilot's existing 80-health, Constitution-10 fixture supplies the health resource; it has no mana resource yet.

The server checks the authored `ASFRestAreaVolume` at each tick. A boar damages the driver, who leaves threat range, heals outside the volume, then enters it. Two clients must observe damage, combat exit, ordinary healing and faster healing in the rest area. The bonus is sandbox-only until production resources and placement are integrated. No owner decision is needed for this health slice.
