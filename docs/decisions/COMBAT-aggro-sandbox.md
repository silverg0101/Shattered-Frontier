# Open-world boar aggro sandbox

Scope: a server-owned sight acquisition rule on the existing common, level-2 boar, exercised in `/Game/Frontier/Maps/Sandbox/SB_Aggro`. Detection adds initial threat and uses the existing replicated target. Damage still detects its source regardless of sight. The map and scripted positions are development fixtures.

Design: *AI: NPC Behavior and Memory* §§3.3–3.4 gives common base radius 12 m, `R = clamp(R_tier − (L_character − L_npc), 4 m, R_tier + 6 m)`, beast field of view 160°, a 1.5 m all-facing proximity exception, two head/chest sight rays with either unobstructed, and a 200 ms sight cache. The existing Silverbrook boar is level 2. Sight is evaluated only for a new threat entry; current threats remain subject to combat and leash rules.

Release-authored pilot settings expose the base, minimum, bonus, proximity and field of view with units and bounds, adopted at zone start. No owner decision is needed for this bounded mechanic. Stealth, hearing, faction stance and general NPC perception remain separate work.
