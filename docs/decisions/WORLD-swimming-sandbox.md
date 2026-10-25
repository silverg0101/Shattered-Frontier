# Swimming sandbox scope

Source: [World Cells, Movement and Handoffs §Movement modes and base speeds](../Game-Design/World-Simulation/Legends_Shattered_Frontier_World_Cells_Movement_and_Handoffs_v1.0.md#movement-modes-and-base-speeds) and its mode transitions. The server selects `swim` when authored water depth at the character is at least 1.3 m (`tune.world.swim_depth_m`); the client cannot select it. Swim base speed is 3.0 m/s (`tune.world.swim_speed_mps`), versus the default 7.0 m/s run. The release-controlled E1 fixture settings carry these bounded defaults. No owner decision is required for this scope.

`SB_Swimming` contains one deep, invisible physics volume and a flat floor. Only this map enables the server depth gate. The two-client check drives one owned pawn across the water boundary and back, and checks the server transition plus both clients' movement modes and swimming speed. This does not introduce water art, drowning, currents, mounts, or a Greenlands water placement.
