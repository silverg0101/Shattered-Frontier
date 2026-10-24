# Open-world boar evade sandbox

AI NPC Behavior and Memory §8.1 sets the common-creature spawn leash at 60 m and requires immediate evade on entry into a safe zone. §8.2 defines evade: discard the target, become immune and untargetable, return at 1.5× chase speed, then restore full health, spawn position and facing. The existing pilot settings supply the 60 m leash and 3.5 m/s chase speed, so evade return is 5.25 m/s. These are shipped values, with no new owner decision.

`SB_Evade` isolates one server-owned boar and a network probe. The existing Silverbrook pilot shares the corrected evade transition. This scope does not implement the separate §8.1 unreachable-target timer, threat tables, or §8.3 anti-abuse signal.
