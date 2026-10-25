# Unarmed auto attack sandbox scope

Combat Abilities, Effects and Runtime §3 (Auto-attack) requires a selected hostile target in range, swings every `speed_s × (1 − h_swing)`, and pauses swings while casting. Combat Damage, Mitigation and NPCs §2 gives an unarmed `speed_s` of 2.0 s. This sandbox uses zero haste and the existing combat pilot's 2.8 m melee reach, 20 damage and 60-health boar fixture; those pilot values are existing development configuration, not new design balance.

`SB_AutoAttack` exercises a client toggle followed by server-only target, range, health, cast-state and timing checks. One defensive boar keeps the two-client stop/resume check isolated. The server continues to own damage and target selection. This does not add equipment, weapon damage formulas or Greenlands placement. No owner decision is needed for this unarmed sandbox slice; production weapon speeds and damage must come from the authored equipment snapshot.
