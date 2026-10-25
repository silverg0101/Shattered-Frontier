# Deep snow travel sandbox scope

World Simulation, *Cells, Movement, Handoffs and World Clock* §Authoritative movement speed specifies 7.0 m/s run, deep snow `M_zone = 0.80`, terrain multiplier bounds [0.5, 1.2], and the 12.0 m/s on-foot cap. On level snow, the expected run speed is 5.6 m/s. The same section requires server-decided speed from authored area volumes; clients predict from the same content.

`SB_DeepSnow` contains one authored deep-snow volume. The server and two clients check a real normal-ground → deep-snow → normal-ground crossing. The bounded `DeepSnowSpeedMultiplier` is a shipped release setting (default 0.80, effective on server/client startup), not a console expression. This sandbox adds no Greenlands placement, weather simulation, or snow art. No owner decision is needed for this mechanic.
