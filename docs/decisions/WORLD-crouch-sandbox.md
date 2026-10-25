# Crouch sandbox scope

Source: `docs/Game-Design/World-Simulation/Legends_Shattered_Frontier_World_Cells_Movement_and_Handoffs_v1.0.md`, § Movement modes and base speeds / Mode transitions. Crouch has base speed **2.0 m/s** (`tune.world.crouch_speed_mps`), run defaults to **7.0 m/s** (`tune.world.run_speed_mps`), and a player may request crouch at any time. The server computes the legal speed from its movement state; a client sends movement intent.

`SB_Crouch` exercises a grounded crouch, forward travel, and return to run using built-in server-authoritative CharacterMovement and replicated stance. The sandbox uses release-controlled `USFE1Settings` for the 2.0 m/s cap. Production's shared configuration workflow and full movement modifiers remain separate. No owner decision is needed for this bounded mechanic.
