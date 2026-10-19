# Mounts and Companions — Abilities and Animation

Version 0.1 — Design baseline (2026-10-19)

Status: Design only; not implemented, animated or balance-validated. [Entry](../Legends_Shattered_Frontier_Mounts_and_Companions_v0.1.md) · [Gameplay details](Legends_Shattered_Frontier_Companion_Gameplay_Details_v0.1.md) · [Integration/delivery](Legends_Shattered_Frontier_Companion_Integration_and_Delivery_v0.1.md) · [Combat runtime](../Combat/Legends_Shattered_Frontier_Combat_Abilities_Effects_and_Runtime_v1.0.md) · [World movement](../World-Simulation/Legends_Shattered_Frontier_World_Cells_Movement_and_Handoffs_v1.0.md) · [Configuration policy](../Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md). This part closes Gameplay Details §3's "publish animation support" requirement as a content contract. Clip IDs, content fields and config keys below are **proposals** pending CMP-01 schema integration.

## 1. Where abilities come from

| Layer | Supplies | Animation consequence |
| --- | --- | --- |
| Family (Canine, Equine, Ursine, Feline, Reptile) | Kit: basic attack, commands, signature A (10), signature B (30), passive choice (50), upgrade (70); handling and riding rules. | Owns the family **clip set** and skeleton/retarget profile. Every family clip ID must resolve before the family ships. |
| Capabilities | Trainable, combat-capable, rideable flags. | Rideable adds the mount clip set (§4b); combat-capable adds combat clips (§4a). Trainable only needs §4c. |
| Species | Anatomy, natural weapon presentation, scale, seat geometry. | May **override** a family clip with a species clip of the same ID suffix and identical phase markers; never changes server timing. |
| Rarity | One active individual trait (Common → Legendary). | Usually none. An epic/legendary technique **variant** brings its own clips and contract (§5). |
| Individual trait | Utility, tradeoff or variant replacing one technique. | A variant inherits the replaced technique's slot, cooldown class and budget; it must ship its complete clip set or the trait is not releasable. |

Rarity never adds stat slots, active pets, encounter immunity or combat budget (Gameplay §3, §6). Coefficients and kits are release-bound content, not console values.

## 2. Ability-to-animation contract

Each companion ability definition (Combat Part 3 model) gains a proposed `presentation` block. Server timing is authoritative; clips conform to it, never the reverse.

| Field (proposed) | Rule |
| --- | --- |
| `clips.unmounted` / `clips.mounted` | Clip IDs per variant. A technique tagged usable while ridden needs a mounted clip plus a rider sync clip; otherwise it is rejected while ridden. |
| `phases` | `windup_ms`, `active_ms`, `recovery_ms`. Windup = `cast_s` (haste-scaled) or an authored instant windup; resolution happens at `server_start_ms + windup_eff`. Clip markers must match within one server tick. |
| `hit_marker` | Presentation only. Damage/CC resolve on the server tick from ability timing (`CombatResult`); the client aligns the impact frame to that result, never reports hits. |
| `play_rate_bounds` | Client may time-warp a clip 0.8–1.25× to absorb haste/latency; beyond that it uses a loopable hold segment (casts/channels) rather than distorting. |
| `motion_mode` | `in_place` (default) or `root_motion_authored`. Runtime motion is always server-driven capsule movement; root motion is extracted offline and must equal the authored displacement (§2.1). |
| `interrupt_window` | Phases in which Combat interrupt/stun/knockback cancels the ability (casts: whole windup; channels: per tick). Cancel → blend to hit-react or idle within 150 ms; refunds follow Combat `cancel_refund_pct`. |
| `cancel_window` | Owner `CancelCast`/new command accepted only during windup; active/recovery are committed. |
| `rider_sync` | For mounted clips: `brace`, `impact`, `settle` markers the rider layer keys to; rider clips use the same timing. |
| `events` | Named VFX/SFX/camera notifies (`vfx.*`, `sfx.*`, `cam.shake`). Cosmetic only; never gate gameplay. Relevance-filtered like `EffectUpdate`. |
| `telegraph` | Required when a technique deals a heavy hit (`dmg_coef`-equivalent ≥ 0.25 of the companion profile) or applies hard CC: windup ≥ 1500 ms or a ground telegraph (Dungeon readability rule applied to pets in PvP/PvE alike). |

### 2.1 Displacement and root motion

- Charges/leaps/rift moves request a World teleport token (`charge` or `teleport_spell`) **before** movement; destination is the server-computed landing point. Ridden: issue a token for the mount and for the rider character (distinct actors) with the same destination and timing.
- Content publish extracts the clip's root travel and checks it equals authored displacement within `teleport_dest_tolerance_m`; mismatch fails publication.
- Locomotion clips are in-place with authored stride speed; runtime speed-warps to authoritative `v_eff` (and the 7 m/s mounted combat cap). No clip can move an actor faster than World permits.
- Knockback/pull received by a creature uses Combat displacement with a generic airborne/landing reaction (§4c).

### 2.2 Missing clip rule

A technique, variant or capability whose required clip (per §4 tables at its priority) is missing, unresolved or fails marker checks is **not releasable**: content validation rejects it and the feature catalog does not admit it. No generic fallback in production. Development fixtures may use a placeholder clip flagged `non_releasable`; that flag blocks publication to any non-dev realm.

## 3. Clip ID convention

`<family|species>.<category>.<name>[.m]` where `.m` = mounted variant and species IDs override family IDs with the same suffix (e.g. `voidreign.atk.basic` overrides `equine.atk.basic`). Rider clips: `rider.<seat_class>.<name>`. Loop clips carry `_loop`. Turn clips are left/right pairs unless mirrored at import.

## 4. Required clip sets

Priority: **P0** first playable packet, **P1** before family release, **P2** polish/after release.

### (a) Any combat pet

| Clip | Purpose | Loop | Motion | Pri |
| --- | --- | --- | --- | --- |
| `idle_loop`, `combat_idle_loop` | Neutral and engaged stance | loop | in place | P0 |
| `walk_loop`, `run_loop` | Follow, engage path, recall | loop | in place (stride speed) | P0 |
| `turn_l/r` (in place 90°) | Facing to target before frontal techniques | one-shot | in place | P1 |
| `atk.basic` (2 variants P1) | Natural-weapon auto/basic attack | one-shot | in place | P0 |
| `react.hit_front`, `react.hit_back` | Damage reaction (additive where possible) | one-shot | in place | P0 / P1 |
| `react.stun_loop`, `react.knockdown` + `getup` | CC states and knockback landing | loop / one-shot | in place / root extracted | P1 |
| `defeat` + `defeat_hold_loop` | Defeat → Injured | one-shot / loop | in place | P0 |
| `cmd.acknowledge` | Command accepted (optional head/ear gesture, additive) | one-shot | in place | P2 |

### (b) Any rideable mount (adds to (a) when combat-capable)

| Clip | Purpose | Loop | Motion | Mounted | Pri |
| --- | --- | --- | --- | --- | --- |
| `idle_loop`, `idle_fidget` | Standing; fidget breaks loop | loop / one-shot | in place | both | P0 / P2 |
| `walk_loop`, `trot_loop`, `canter_loop`, `gallop_loop` | Gaits by speed band; gallop = travel only | loop | in place | both | P0 (walk, run gait), P1 (rest) |
| `turn_l/r_walk`, `turn_l/r_run` | Leaning turns (additive/blendspace) | loop | in place | both | P1 |
| `turn_l/r_inplace` | Pivot 90/180° | one-shot | in place | both | P1 |
| `start_walk/run`, `stop_walk/run` | Accel/decel transitions | one-shot | root extracted | both | P1 |
| `idle_loop.m` | Mounted stance (weight-shift for rider) | loop | in place | mounted | P0 |
| `rider.mount`, `rider.dismount` (+ mount reaction) | Riding lesson / out-of-combat mount; validated dismount point | one-shot | root extracted (rider) | — | P0 |
| `rider.dismount_forced` + `mount.rear_throw` | Explicit dismount effect / prohibited volume / defeat | one-shot | in place; rider placement from server | — | P0 |
| `combat_idle_loop.m`, `combat_move_loop.m` (≤ 7 m/s) | Engaged mounted stance/movement | loop | in place | mounted | P0 |
| `react.hit.m` | Mount hit while ridden (rider additive sync) | one-shot | in place | mounted | P1 |
| `rider.<seat>.melee_l/r`, `rider.<seat>.bow_fire` | Rider mounted-compatible basics, keyed to seat class | one-shot | in place | mounted | P0 (one), P1 |
| `jump`/`fall_loop`/`land` | Terrain traversal | one-shot/loop | root extracted | both | P2 |
| `swim_loop` | Only if species is swim-capable (none initially) | loop | in place | — | n/a |

### (c) Lifecycle

| Clip | Purpose | Loop | Pri |
| --- | --- | --- | --- |
| `wild.exhausted_loop` / `wild.calm_loop` | Capture opening (Gameplay §2) | loop | P0 |
| `capture.bind_loop` + `capture.secured` / `capture.break` | 8 s Calm and Bind channel and outcome | loop / one-shot | P0 |
| `train.approach`, `train.follow` (= walk), `train.hold_loop`, `train.saddle` (rideable) | Lessons; projection uses same clips, no collision | mixed | P1 (P0 for rideable saddle) |
| `deploy.arrive` / `store.depart` | 6 s channel presentation (owner channel + creature arrival/fade) | one-shot | P0 |
| `injured_idle_loop` (inspect/Stable UI) | Injury state | loop | P1 |
| `recovery.rest_loop` + `recovery.rise` | 30 s recovery channel | loop / one-shot | P1 |

### (d) Family signature techniques

| Family | Required technique clips (beyond (a)/(b)) |
| --- | --- |
| Canine (CMP-02) | `sig_a.pursuit` (lunge, `charge` token), `sig_b.interrupt` (snap/howl), passive presentation, upgrade variant. |
| Ursine | `sig_a.swipe`, `sig_b.taunt_roar`; mounted variants only for rideable species. |
| Feline | `sig_a.positional_strike` (flank pounce), `sig_b.slow` (rake). |
| Reptile | `sig_a.bite_dot`, `sig_b.defend_loop` (short defensive). |

**Equine kit in detail** (CMP-05). Coefficients remain release content; techniques usable ridden or unridden unless noted.

| Clip | Technique / purpose | Phases (proposal) | Loop | Motion | Variant | Pri |
| --- | --- | --- | --- | --- | --- | --- |
| `equine.atk.basic` | Basic: front-hoof strike | 350 / 150 / 400 ms | one-shot | in place | both | P0 |
| `equine.atk.basic_rear` | Basic alt: rear kick (target behind) | 400 / 150 / 450 | one-shot | in place | unmounted | P1 |
| `equine.sig_a.charge_windup` | Signature A Short charge: gather/paw | 500 windup (telegraph) | one-shot | in place | both | P0 |
| `equine.sig_a.charge_run` | Charge travel ≤ authored distance | active | loop | root authored (`charge` token) | both | P0 |
| `equine.sig_a.charge_impact` | Impact + stagger presentation; CC via canonical DR | 150 / 500 recovery | one-shot | root extracted | both | P0 |
| `rider.<seat>.charge_brace` | Rider lean/brace synced to `brace/impact/settle` | — | one-shot | — | mounted | P0 |
| `equine.sig_b.rally` | Signature B owner movement aid (stamp + call); speed effect flagged `applies_mounted` | 300 / 0 / 300 | one-shot | in place | both | P1 |
| `equine.passive.<id>` | Level-50 passive (pool §4e): no new body clip; additive overlay (mane/head carriage, gait accent) or `vfx.*` only on proc | — | loop overlay | in place | both | P1 |
| `equine.up70.<id>` | Level-70 upgrade (pool §4e): if it alters motion (e.g. wider charge impact), new impact clip with same marker layout; magnitude-only upgrades reuse clips | as replaced | one-shot | as replaced | both | P1 |

### (e) Milestone choice pools

At levels 50 and 70 the player chooses the active option from the family pool; the choice changes only at a rest area, out of combat (Gameplay §3). Every option in a pool fits the same combat budget: magnitudes are calibrated so a fully used option lands on the family reference output, and none grants immunity, heals, shields or group damage buffs. CC uses canonical DR; displacement uses World tokens. Each option ships its clips/VFX or is not offered.

**Equine level 50 — passive (pick one)**

| Option | Effect | Presentation |
| --- | --- | --- |
| `equine.passive.gathering_stride` — Gathering Stride | Continuous movement in combat builds Momentum (up to `momentum_max_stacks` over `momentum_build_ms`); the next basic attack consumes it for bonus damage and a minor stagger (DR). Base basic-attack coefficient is lowered so a full-stack hit stays on budget. | Additive overlay: head lowers and tail flags at full stacks; `vfx.momentum_spark` on the empowered hit. |
| `equine.passive.surefooted` — Surefooted | While ridden, knockback distance on mount and rider is reduced by `surefooted_knockback_bp`, and slows on the mount by `surefooted_slow_bp` (after canonical DR). Forced dismount, deep water and prohibited volumes still apply. | New additive clip `equine.react.brace` (hooves plant, neck braces) on reduced knockback; `vfx.ground_crack`. |
| `equine.passive.flight_instinct` — Flight Instinct (alternate) | Once per `flight_instinct_cooldown_ms`, dropping below 30% health grants the mount (and rider if ridden) a `flight_instinct_ms` sprint surge. Not a heal or effect removal. | Snort/toss overlay at trigger; gallop accent overlay during surge. |

**Equine level 70 — upgrade (pick one)**

| Option | Replaces | Effect | Presentation |
| --- | --- | --- | --- |
| `equine.up70.trampling_charge` — Trampling Charge | Short charge (sig A) | Charge runs its full validated distance, hitting up to 3 enemies once each with the same coefficient split and CC (DR per target). | New clip `equine.up70.charge_through` with mid-stride hit markers instead of an impact stop. |
| `equine.up70.bolt` — Bolt | Rally (sig B) | Re-press within `bolt_window_ms` surges mount and owner forward `bolt_distance_m` along a `charge`-token path. | Reuses `equine.start_run` + rally overlay. |

**Voidreign** (legendary; Rift Recall replaces sig A, so its level-70 pool is its own):

- Level 50 uses the Equine passive pool with void presentation only (`vfx.void_momentum`, rift-light ground crack). No stat difference.
- Level 70 options:

| Option | Replaces | Effect | Presentation |
| --- | --- | --- | --- |
| `voidreign.up70.rift_collapse` — Rift Collapse (recommended) | Rift Recall | After a successful Recall, the rift at the impact point implodes `rift_collapse_delay_ms` later, applying a `rift_collapse_snare_bp` snare for `rift_collapse_snare_ms` (DR). No damage. | Horn-flare overlay on `recall_exit`; `vfx.rift_collapse`. |
| `voidreign.up70.long_shadow` — Long Shadow | Rift Recall | Return window extended to `long_shadow_window_ms`, and the anchor may be held up to `long_shadow_hold_ms` for a delayed return. Same validation; no power gain. | Anchor VFX pulse while held; no new body clip. |
| `equine.up70.bolt` — Bolt | Rally (sig B) | As Equine. | As Equine. |

Proposed controls (`tune.combat.companion_` prefix; Combat-owned, LiveOps tier 2; Gameplay §11 publication rules; adoption on next deployment, never mid-effect):

| Key suffix / label | Type; units; default | Bounds; step |
| --- | --- | --- |
| `momentum_max_stacks` / Momentum stacks | integer; 5 | 3–5; 1 |
| `momentum_build_ms` / Time to full Momentum | integer ms; 3000 | 2000–5000; 250 |
| `surefooted_knockback_bp` / Surefooted knockback reduction | integer bp; 4000 | 0–5000; 100 |
| `surefooted_slow_bp` / Surefooted slow reduction | integer bp; 2500 | 0–3000; 100 |
| `flight_instinct_cooldown_ms` / Flight Instinct recovery | integer ms; 60000 | 45000–120000; 1000 |
| `flight_instinct_ms` / Flight Instinct duration | integer ms; 4000 | 2000–5000; 250 |
| `bolt_window_ms` / Bolt window | integer ms; 2000 | 1000–3000; 250 |
| `bolt_distance_m` / Bolt distance | fixed decimal m; 6 | 4–8; .5 |
| `rift_collapse_delay_ms` / Rift Collapse delay | integer ms; 1000 | 500–1500; 100 |
| `rift_collapse_snare_bp` / Rift Collapse snare | integer bp; 3000 | 1500–4000; 100 |
| `rift_collapse_snare_ms` / Rift Collapse snare time | integer ms; 2000 | 1000–3000; 250 |
| `long_shadow_window_ms` / Long Shadow window | integer ms; 4500 | 3000–5000; 250 |
| `long_shadow_hold_ms` / Long Shadow hold | integer ms; 2000 | 1000–3000; 250 |

Pool membership, option effects, the target cap of 3, the 30% Flight Instinct threshold and damage coefficients are release-bound content, not console values. The Rift Collapse snare is the same effect class as other companion snares and shares their DR.

## 5. Voidreign legendary signature — Rift Recall

Voidreign: legendary equine species (horned, dark, skeletal flank motif, mane/tail and leg feathering; reference `GameClient/ArtSource/Voidreign/ConceptPrototype04/README.md`, motion plan `QualityCompletion-20261018/PrimaryLocomotionPlan-v001`). Its legendary trait **Rift Recall** replaces Equine signature A (Short charge). A horse charges in; a unicorn heals or gores. The Voidreign **tears a rift at its starting point, charges through, and can step back out of the rift to where it began** — a commit-and-retreat tool for hit-and-run mounted combat.

| Rule | Value / constraint |
| --- | --- |
| Slot/budget | Signature A slot, level 10 gate, same cooldown, cost, GCD class, damage coefficient and control as the Short charge it replaces. No extra slot or stat. |
| Leg 1: Rift Charge | Identical to Short charge (telegraphed windup, `charge` token, canonical CC/DR). On ability start it places a **rift anchor** at the validated origin. |
| Leg 2: Recall | Optional re-press (off-GCD, no cost) within `rift_recall_window_ms` after impact. Voidreign (and rider, if ridden) displaces back to the anchor with a `teleport_spell` token. Deals **no damage and no CC**. |
| Validation | Recall requires: anchor within `rift_anchor_max_m`; LOS anchor↔current; nav path ≤ 1.5× straight distance (release-bound security rule); same server/cell and no handoff since anchor; anchor still in a permitted volume; Voidreign not rooted/stunned/feared/incapacitated. Failure → reject, anchor kept until expiry. |
| Not immunity | Both legs remain targetable/damageable; recall does not remove effects, break CC, drop threat or reset combat. Ridden: rider and mount each take their own token; rider abilities are rejected from recall start until arrival plus `rift_rider_lockout_ms`. |
| Unmounted pet | AI never auto-recalls; owner uses the signature control again. Leash/40 m command range still applies. |
| Lifecycle | Anchor is transient server state: lost on handoff, storage, defeat or window expiry; never persisted. |
| Level-70 upgrade | Its own pool (§4e): Rift Collapse or Long Shadow upgrade Rift Recall; Bolt upgrades Rally. A technique-replacing legendary always publishes its own level-70 variants rather than being blocked. |

Proposed controls (`tune.combat.` prefix; Combat-owned, LiveOps tier 2; Gameplay §11 publication rules):

| Key / label | Type; units; default | Bounds; step | Adoption |
| --- | --- | --- | --- |
| `companion_rift_recall_window_ms` / Rift return window | integer ms; 3000 | 1500–4000; 250 | Next Rift Charge; never extends an open window. |
| `companion_rift_anchor_max_m` / Rift return distance | fixed decimal m; 15 | 8–15; .5 | Next anchor; never above the published charge distance + token tolerance. |
| `companion_rift_rider_lockout_ms` / Rider pause after rift return | integer ms; 500 | 250–1000; 50 | Next recall. |

The 1.5× path ratio, no-damage return, token causes and CC rules are release-bound code/content, not console values.

### 5.1 Rift Recall animation needs

| Clip | Purpose | Loop | Motion | Variant | Pri |
| --- | --- | --- | --- | --- | --- |
| `voidreign.sig_a.rift_windup` | Rear, horn lowers, rift tears at hooves (`vfx.rift_open` at origin) | one-shot | in place | both | P0 |
| `voidreign.sig_a.rift_charge_run` | Charge with void trail (`vfx.rift_trail`) | loop | root authored | both | P0 |
| `voidreign.sig_a.rift_impact` | Horn/shoulder impact | one-shot | root extracted | both | P0 |
| `voidreign.sig_a.recall_enter` | Pivot and plunge into rift (dissolve at current position) | one-shot | in place | both | P0 |
| `voidreign.sig_a.recall_exit` | Emerge from anchor, rift collapses (`vfx.rift_close`) | one-shot | in place | both | P0 |
| `voidreign.sig_a.anchor_idle_vfx` | Anchor marker visible to all relevant clients during window | loop (VFX only) | — | — | P0 |
| `rider.<seat>.rift_brace`, `rider.<seat>.recall_hold` | Rider sync for both legs | one-shot | — | mounted | P0 |
| Secondary motion (mane, tail, leg feathering) | Must hold through charge extremes and recall dissolve/emerge (VR-SECONDARY-01) | — | — | both | P1 |

## 6. Animation completeness acceptance

- **Family:** every §4 clip at the target priority resolves for the family skeleton; markers match ability timing within one tick; root-extracted travel matches authored displacement within token tolerance; loop seams and planted-foot slip pass the mount primary-motion checks (`docs/art-templates/mount-primary-motion.md`).
- **Species:** overrides keep identical markers/phase lengths; seat geometry passes rider placement for every mounted clip; anatomy/attack reach and LOS validated in rendered play (Gameplay §7).
- **Variant/trait:** a replacing technique ships its full clip set and contract or the trait is not releasable (§2.2).
- **Runtime evidence:** rendered network play shows server-timed hit and client impact frame within one tick at nominal latency; interrupt/cancel blend within 150 ms; forced dismount places the rider at the validated point; no movement rejection from Security for any technique.
- **Packet gates:** CMP-02 needs canine P0; CMP-05 needs Equine P0 including (b) mounted/rider clips; a Voidreign release additionally needs §5.1 P0.

## 7. Open decisions

1. Resolved: Equine and Voidreign milestone pools in §4(e); final coefficients still need playtest calibration.
2. Resolved: a technique-replacing legendary publishes its own level-70 options (§4e).
3. Rider seat classes and rider skeleton clip ownership (character animation team vs companion content).
4. Base Equine Short charge distance, coefficient and control type (stagger vs short stun) — needed to finalize Rift Recall bounds.
5. Whether gallop is available mounted in combat (cap 7 m/s suggests canter max) and whether trot is shipped or blended.
6. Exact heavy-hit threshold for companion telegraphs (profile-relative definition).
