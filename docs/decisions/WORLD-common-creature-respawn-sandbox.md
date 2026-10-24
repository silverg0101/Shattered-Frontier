# Common creature respawn sandbox

`SB_Respawn` exercises one authored common boar spawn slot on a dedicated server. On death, the slot becomes respawning; the server schedules its next cycle, then creates a fresh boar at an available authored point. The boar and its death/return are visible to both clients. The slot is session-only, as specified for common NPCs; the existing Silverbrook WorldContent path is untouched.

Design: *World Terrain, Resources and Population* § Spawn groups (point positions/facing, active slot, NPC-killed transition), § NPC spawn and population simulation (common base 300 s; `D(n) = max(0.35, 1/(1+0.15 max(0,n-1)))`; jitter `J` ±10%; server population bounds; common slots memory-only). `n` is the players inside the authored sandbox area at death. The map authors two points for one active slot. Timer and point selection run on authority only. Values are bounded configuration, adopted at zone start.

Acceptance: two clients observe the same boar, a client kills it through ordinary combat, both see absence throughout the computed delay, then both see a fresh full-health boar at the alternate point. No owner decision is needed for this common NPC slice.
