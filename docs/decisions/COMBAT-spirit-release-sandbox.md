# Spirit release sandbox scope

Combat, **Damage, Mitigation and NPCs** § Death specifies that a dead player can return at a spirit healer/respawn point with `tune.combat.spirit_res_fraction = 0.50` of maximum health and mana. Combat, **Abilities, Effects, State Machine and Zone-Server Runtime** § Combat state machine accepts release-spirit intent while dead. These are the only gameplay values used here. The current pilot has health but no mana resource.

`SB_SpiritRelease` exercises boar damage through the existing authoritative death path with Gameplay Services disabled. A dead owning client may request spirit release; the server alone checks death, chooses the authored respawn point and restores 50% health. Living and repeated requests are denied. The sandbox holds death until the request, replacing its pilot auto-respawn only on this map. When Gameplay Services are enabled, the existing durable death path remains in force and sandbox release is denied. This does not change Greenlands or the durable death/World teleport-token rollout.

The netcheck gives its first player 35 current health under a development-only flag so three real 14-point boar hits reach zero before the boar disengages. Maximum health remains 80; revival must therefore be 40. This fixture value is not a gameplay balance setting.

No owner decision is needed for this isolated health-only mechanic. Production placement still needs the existing durable death, wear, event and teleport-token integration.
