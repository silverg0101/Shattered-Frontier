# Resurrection acceptance sandbox scope

Combat, *Damage, Mitigation and NPCs* v1.0, “Death” §4 specifies that a `player_ability` resurrection restores the ability-authored fraction, default **35%**, and that the dead player must accept a request within **60 seconds** (`tune.combat.res_accept_s`). Combat, *Abilities, Effects and Runtime* v1.0, “Combat state machine” allows resurrection-accept while dead and returns the character to out-of-combat on resurrection.

`SB_ResurrectionAccept` exercises an authority-issued, development-only offer after a real boar defeat of the player. The dead owning client accepts; the server checks ownership, death, offer identity and deadline, then restores 35% of the pilot's 80 maximum health (28). The second client observes death and restoration. The sandbox suppresses the pilot's four-second automatic respawn while the 60-second offer is live and uses the existing respawn at expiry. The offer source is a fixture; a published player resurrection ability, its range/cast/cost, telemetry, durable recovery and teleport-token integration are later production work.

No owner value is required for this isolated acceptance rule. No ability range, cast time or cost is inferred here.
