# Unstuck sandbox scope

Source: World Simulation, *World Cells, Movement and Handoffs* v1.0, §Teleport tokens / Unstuck. The player command is limited to once per 15 minutes. Authority selects the nearest navigable point at least 2 m and at most 30 m away; if none exists, it uses the nearest authored respawn point. Dead characters, characters in combat and instance use are denied. The 15-minute and 2–30 m limits are shipped rule bounds, with no operator override in this pilot.

`SB_Unstuck` provides a nav area and a tagged fallback point. `SFUnstuck` sends only intent. Greenlands now searches Recast nav polygons for the nearest permitted point, falling back to its authored zone respawn points. The zone server keeps a single-use, 2 s displacement token in memory and commits the 15-minute per-character cooldown with the position in CharacterState. The sandbox keeps its isolated transient cooldown. No new owner value was needed for Greenlands rollout.
