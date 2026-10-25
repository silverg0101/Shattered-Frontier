# NPC profiles

`profiles.json` is immutable, schema-versioned combat content keyed by `definition_id`.
`SFNpcProfiles` validates the complete catalogue once per process. Invalid content
rejects the catalogue; an unknown ID cannot configure a spawn. Existing actors use
the process snapshot until restart. Publication and permission belong to reviewed
content releases, not a live console. No formulas or new balance values are added.

Loot stays authoritative in `loot_rules/<loot_rule>.json`: its creature ID must
match, and it supplies the table and shared-drop flag. Reward RPCs, durable defeat
IDs, eligibility and protected-zone checks retain their existing authority.

| Field | Type / units | Permitted values |
| --- | --- | --- |
| definition_id | ID | `npc/` plus lower-case letters, digits, underscores; unique |
| level | integer, levels | 1–7 |
| health | integer, HP | 1–3558 |
| skill | integer, skill points | 0–194 |
| swing_seconds | number, seconds | 1.6–2.0 |
| swing_damage | number, raw damage | 0–95.5 |
| armor | number, armor points | 0–378 |
| resistance | number, resistance points | 0–18.9 |
| crit_chance | number, probability | 0–1 |
| crit_damage | number, multiplier | 1–1.5 |
| attack_kind | enum | `melee`, `ranged` |
| abilities | unique enum array | `mend` |
| loot_rule | filename stem | lower-case letters, digits, underscores |
| local_hides_fixture | boolean | existing development-only hide fallback eligibility |

All numeric fields must be finite. Bounds conservatively admit the existing six
profiles; expanding their numerical envelope requires a compatible reviewed release.
Shipped defaults are the explicit six records, preserved from the previous C++ data.
Range, kiting, perception, mend costs and thresholds continue using the existing
typed realm settings and algorithms. `attack_kind` and `abilities` select those
algorithms, without species-ID checks.

The seventh ranged profile lives only in `tests/fixtures/npc-profiles`; it is not
staged game content. `ShatteredFrontier.Combat.Pilot.NpcProfileCatalogue` loads it
alongside the six profiles, resolves a ranged hit and tests rejection/atomicity.
