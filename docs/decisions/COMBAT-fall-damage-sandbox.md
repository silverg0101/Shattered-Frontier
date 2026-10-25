# Fall damage sandbox scope

`SB_FallDamage` exercises one server-authoritative environmental hit from a real CharacterMovement landing. Combat Damage, Mitigation and NPCs §Environmental damage defines physical environment damage as `MaxHealth × fall_coef`, ignoring armor, avoidance and block, with `fall_coef = clamp((v_impact − 12) / 20, 0, 1.2)` and `tune.combat.fall_speed_threshold_mps = 12`. World Cells, Movement and Handoffs §Movement modes gives terminal fall speed 50 m/s and gravity 9.81 m/s². The pilot's existing configured player health and death/respawn path are reused. The test platform is fixture geometry, not a product world value.

No design value is missing for this isolated mechanic. Production rollout, other environmental volumes and their content coefficients are outside this scope.
