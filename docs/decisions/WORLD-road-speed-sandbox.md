# Road speed sandbox scope

World Simulation, *Cells, Movement, Handoffs and World Clock* §Authoritative movement speed sets run to 7.0 m/s, road `M_zone` to 1.10, the zone modifier clamp to [0.5, 1.2], and the on-foot hard cap to 12.0 m/s. The server derives legal speed from the authored volume; clients predict using the same map content. The existing 0.6 backpedal factor still applies after the road modifier.

`SB_RoadSpeed` contains one authored road volume. The sandbox checks an actual normal-ground → road → normal-ground crossing on an authoritative server with two clients. This adds no Greenlands placement, weather, terrain system, or console formula.

Owner decision for a later mechanic: sprint Stamina drain rate. World requires Stamina drain, but the consulted Combat attributes and World movement sections do not specify its rate. Road speed needs no owner decision.
