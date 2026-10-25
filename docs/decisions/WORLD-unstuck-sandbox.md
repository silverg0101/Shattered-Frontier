# Unstuck sandbox scope

Source: World Simulation, *World Cells, Movement and Handoffs* v1.0, §Teleport tokens / Unstuck. The player command is limited to once per 15 minutes. Authority selects the nearest navigable point at least 2 m and at most 30 m away; if none exists, it uses the nearest authored respawn point. Dead characters, characters in combat and instance use are denied. The 15-minute and 2–30 m limits are shipped rule bounds, with no operator override in this pilot.

`SB_Unstuck` provides a nav area and a tagged fallback point. `SFUnstuck` sends only intent; the server searches outward in 0.5 m rings and 32 directions, then applies the first navigable destination. This bounded search approximates the nearest point. The sandbox uses an in-memory cooldown on the owning controller. World teleport tokens, an exact nearest-point search and a durable per-character rate limit are required before rollout beyond the sandbox. No owner decision is needed for the isolated sequence.
