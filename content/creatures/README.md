# NPC profiles

`profiles.json` schema 3 is immutable, versioned NPC content keyed by `definition_id`.
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
| behavior_preset | ID reference | declared typed `behavior_presets` row |
| presentation_preset | ID reference | declared typed `presentation_presets` row |
| attack_kind | enum | `melee`, `ranged` |
| respawn_tier | enum | `common`, `elite`, `mini_boss`; selects existing World tuning |
| abilities | unique enum array | `mend` |
| loot_rule | filename stem | lower-case letters, digits, underscores |
| local_hides_fixture | boolean | existing development-only hide fallback eligibility |

All numeric fields must be finite. Bounds conservatively admit the existing six
profiles; expanding their numerical envelope requires a compatible reviewed release.
Shipped defaults are the explicit six records, preserved from the previous C++ data.
Each shipped species references the same `common` behavior and `placeholder`
presentation preset. All defaults preserve the previous settings/body exactly.
NPC-specific range/kiting/perception/movement/leash/assist settings now resolve
through that reference, including hidden-movement practice and real attack admission.
The retired Boar/Archer AI config fields have no competing runtime source. Player
reach, threat algorithms, defense/formula caps and Mend costs/thresholds stay global.
`attack_kind` and `abilities` select shared algorithms without species-ID checks.
Preset references and values adopt together on process restart; profile/preset changes
use the same reviewed release permission/workflow as stats, never console expressions.
Schemas 1/2 are rejected so older consumers cannot silently ignore required preset/bounds semantics.

| Behavior field | Units | Bounds | Shipped default |
| --- | --- | --- | --- |
| aggro_radius_metres | m | 4–18 | 12 |
| near_notice_radius_metres | m | 1–4, at most aggro minimum | 1.5 |
| aggro_minimum_metres | m | 1–4 | 4 |
| aggro_maximum_bonus_metres | m | 0–6 | 6 |
| field_of_view_degrees | degrees | 60–360 | 160 |
| assist_delay_seconds | s | 0.5–5 | 2 |
| assist_radius_metres | m | 2–15 | 8 |
| leash_radius_metres | m | 20–100 | 60 |
| wander_radius_metres | m | 2–40 | 18 |
| wander_speed_metres_per_second | m/s | 0.5–5 | 1.3 |
| chase_speed_metres_per_second | m/s | 1–10 | 3.5 |
| wander_min_seconds | s | 1–10 | 3 |
| wander_max_seconds | s | minimum–20 | 6 |
| melee_range_metres | m | 1–5 | 2.8 |
| ranged_range_metres | m | 5–30 | 30 |
| retreat_range_metres | m | 2–15 | 6 |
| preferred_range_metres | m | retreat–20, below ranged maximum | 10 |
| ranged_swing_seconds | s | 1–4 | 2.8 |

Presentation references use a valid `/Game/` or `/Engine/` static-mesh soft path
without subobjects or traversal. Editor/client catalogue admission also resolves the
reference as a StaticMesh and rejects missing/wrong-type assets transactionally.
Dedicated cooks may strip render assets, so they validate the descriptor without
loading the mesh; renderer asset closure is a required release gate. Actual actor
configuration also resolves before changing any state, preserving the previous
profile/body on failure. The shipped mesh remains `/Engine/BasicShapes/Cube.Cube`.
`body_scale` is a finite three-number array, each 0.01–1.3, default `[1.3,0.7,0.7]`.
`mesh_bounds_origin_cm` and `mesh_bounds_extent_cm` are finite three-number arrays
in asset-local centimetres, before any body/world scale. Origin is bounded -50–50
(default `[0,0,0]`); positive extent is 0.01–50 (default `[50,50,50]`). Editor/client
asset closure also requires these values to match the actual mesh bounds within
0.01 cm. This supplies renderless authorities with the same geometry; it adds no
appearance change. Schema 3 makes the required bounds explicit.
Capsule radius is 1–55 cm (55 default); half-height is radius–65 cm (65 default).
Label height is 0–130 cm (130 default); world size is 1–34 cm (34 default).
Those conservative geometry bounds require a reviewed compatible release to widen.
`animation_profile` is a typed reference with only `none` currently supported: the
existing static placeholder consumes no animation assets. No skeletal/animation
framework or appearance change is introduced. Live/editor construction, authority
configuration and client definition replication consume the same presentation.
Serialized component names and constructor defaults remain compatible.

`ShatteredFrontier.Combat.NpcReuse.BehaviorAndPresentationPresets` uses scoped,
automatically restored test content to compare real perception, chase, leash, shot
reach and assist behavior plus structural mesh/transform application. The alternate
preset is isolated under `tests/fixtures/npc-profiles/preset-variation.json`; it is
never published as gameplay balance or staged for players.

Zone NPC groups are selected by `spawn_kind`, and each definition must exist in this
catalogue. `Combat/SFNpcZoneContent` indexes approved zone files by `map_id`. NPC
point `npc_defensive` is a boolean (default false), effective on spawn from the
restart-bound content snapshot. Lower-case ASCII letters, digits and underscores
are required for NPC group/point tokens. Existing boar point choices are authored
by the zone generator; runtime no longer interprets their names or ordinals.

The seventh ranged profile lives only in `tests/fixtures/npc-profiles`; it is not
staged game content. `ShatteredFrontier.Combat.Pilot.NpcProfileCatalogue` loads it
alongside the six profiles, resolves a ranged hit and tests rejection/atomicity.
`ShatteredFrontier.Combat.NpcReuse.ContentSpawnAndShot` then loads the single-group
fixture zone, drives the real slot spawn and authoritative ranged shot, and checks
a melee control at the same distance. Fixtures are never staged game content.

Defeat identity includes a per-actor spawn GUID in addition to the existing process
nonce and group/slot/serial. Retries for the same spawn retain the same opaque ID,
while different worlds cannot collide by reusing group names and counters. The
legacy sequence key remains the combat random seed; identity scoping does not
change the combat draw sequence. Wire field names are unchanged.

`SFNpcZoneContent::IsProductionMap` is the common admission rule for approved
production maps; the Sandbox directory retains explicit fixture policy. Service
slot replies must cover the selected authored census with unique group/slot keys
and valid active definitions/points before any row spawns. The production parity
regression uses a validated one-slot test map through the real services callback,
front/rear defense and durable death/release/acceptance paths; no extra map or NPC
is staged for players.
