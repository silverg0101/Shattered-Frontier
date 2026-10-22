# TER-02 — Digging and raising limits, and healing

Status: proposed, 2026-10-22; limits and healing questions both answered by the owner the same day (§9, §10). Conflicts with existing design are listed in §13. Answers the "Digging limits", "Terrain tools in building mode", "Terrain tool preview + sizes" and "Healing rules" lines in [OWNER-2026-10-22](OWNER-2026-10-22-decisions.md). In force: [TER-01](TER-01-mesh-terrain-fit-check.md) ended in a dig-anywhere adoption on 2026-10-22 (TER-01b passed), so these limits apply to the voxel zone ground. Extends [DEC-03](DEC-03-terrain.md)'s T1–T10 order and its healing lifecycle; it changes no storage or codec.

Owner's words: "digging into a mountain to make a house, which would require claiming that area. I am ok with that. But digging down for grief holes shouldnt be allowed. digging a small ridge side down, to give room for a house is allowed." And: "healing needs to be defined fully too. if a player's voxel occupies the space, it cant get healed over it."

## 1. One idea

**Open land is shallow and open to the sky. Claims go deep. Healing puts open land back, but never over anything that is there.**

- **Dig** and **Raise** are two actions in the building menu's Terrain section, each with small/medium/large brushes and a result preview (ghost of the final surface, volume removed or added, clipped and blocked parts). Both obey the same limits, buffers, budget and healing.
- Outside a claim you may cut or raise at most 2 m, every edit must be walkable out of, and nothing may be dug under or raised over a roof of ground. That allows levelling a pad, cutting a ridge side back, banking a mound, ditches and ramps. It cannot make a pit, a shaft, a tunnel, a trench with vertical walls, or a wall ring around someone.
- Inside your own claim the claim's vertical band is the limit: basements, tunnels and a house carved into a mountainside are allowed as long as every changed voxel lies inside the parcel and band and clears the existing protections.
- Nobody digs under anyone's feet, next to a road, or up against someone else's claim or structure.
- Altering land **costs nothing and yields nothing** (owner): digging gives no material, raising uses none, on open land and in claims alike. Resources come only from harvesting and mining nodes.
- Public edits heal after 7 days; claiming the land adopts them. Nothing can be placed on open land without a claim, so nothing pins a hole open. Healing leaves alone every voxel that an online player, a dropped item, a creature or a mount occupies, with a 1 m margin; a player who logged out there is moved to the nearest spawn on login (§8).

## 2. Rules by location (dig and raise)

| Where | Dig below original surface | Raise above original surface | Shape | Buffers | Budget |
| --- | --- | --- | --- | --- | --- |
| Open land, outside any claim (zone allows public edits) | ≤ 2.0 m (`public_dig_depth_m`) | ≤ 2.0 m (`public_raise_m`) | Exit rule and open-sky rule (§3) | 3 m from roads, paths, others' claims and others' structures (`public_buffer_m`) | DEC-03 rate keys; outstanding public changed volume ≤ 1,000 m³ per character per realm, dig and raise combined (`public_outstanding_cells` = 8,000) |
| Your own claim (terrain right in that parcel and band) | Down to `max(band_bottom, anchor_base_z − 8 m, zone.min_dig_z)` | To the band ceiling, within T7's 12 m | Tunnels and rooms allowed; no change within 1 m of the parcel boundary (`claim_shell_m`) | Existing T5: 1 m to foreign structures; others' parcels are off limits by T4 | DEC-03 rate keys; no outstanding cap (claimed edits persist) |
| Within 3 m of a road, path, respawn, others' claim boundary or others' structure | No dig | No raise, except `flatten`/`smooth` that add ≤ 0.5 m to repair a surface | — | T5 extension | — |
| Under any character or NPC capsule (1 m radius cylinder, 2 m down) | No dig, anywhere, including your own feet | T8 already blocks raise into a capsule | — | — | — |
| Mountain interior, open land | 2 m measured along the column, so a horizontal cut into a 45° face stops after about 2 m | 2 m | Open-sky rule stops undercutting a lip or tunnelling | — | — |
| Mountain interior, own claim | Band rule: anchored to the claim anchor's base elevation, not the column surface, so a 30 m personal band covers a hillside home with rooms behind it | Band ceiling | Tunnels inside parcel + band; T5 protections (landmarks, watercourses, routes, nodes) still reject | — | — |
| Cities, safe areas, roads, spawns, region borders, bedrock | No dig (T5/T6/T7) | No raise | — | — | — |

"Original surface" is DEC-03's shipped base surface per column (`base_content_version`); repeated edits and healing never move it, so a player cannot dig 2 m, wait for healing, and dig 2 m deeper, nor stack cuts or mounds by smoothing.

## 3. Shape rules (open land only)

Both run on the final surface of the brush footprint plus a one-voxel ring, at most 22 × 22 columns: one small 2D pass per candidate.

- **Exit rule.** Every column in the footprint and ring must connect, through adjacent columns, to a column outside the footprint by a path whose steps *up* are each ≤ `public_exit_step_m` = 1.0 m (steps down are free; the 2 m bound keeps them harmless). A cut into a hillside passes because its downhill side has zero step. A pit, a shaft, a vertical-walled trench or a raised ring around a player fails because every way out is a wall taller than 1 m. The step value must stay at or below the character's mantle/jump height declared by Character movement.
- **Open-sky rule.** A removed voxel must have no surviving solid voxel above it; an added voxel must have a solid voxel below it after the edit. No undercut lips, no tunnels, no floating fill, no "raise a roof over the path".

Levelling a ridge for a house on open land is cut ≤ 2 m on the high side, raise ≤ 2 m on the low side, with a sloped or stepped edge the player can walk out of. It needs no claim (owner, §9). Permanent structures on open land are already impossible ([Construction] §13), so the house requires the claim, and claiming adopts the pad.

## 4. Fall and trap safety

- Open land: the exit rule bounds every climb at 1.0 m and every drop at 2.0 m, below any fall-damage threshold, and guarantees a walk-out. No auto-ramp is generated; a dig or raise that would leave no exit is refused with a red preview so the player slopes it.
- Everywhere: no dig under a capsule (T8.1), no raise into a capsule (T8). Healing and claim-archive restoration refuse to entomb (§8).
- Claims: tunnels can enclose a visitor behind a door or a later fill. Doors follow Construction's door/permission rules; for sealed terrain the **Unstuck** command (owner, §9) returns the character to the nearest spawn with their items. Unstuck is a prerequisite of claim tunnelling.

## 5. Typed configuration

All under `tune.world.*`, realm-scoped, operator permission, effective at the next terrain commit or nightly run, declared through DOC-05 as the Tuning architecture requires. Shape rules, the band formula, the healing algorithm and reject codes are release-bound code.

| Key | Unit | Default | Safe bounds | Step |
| --- | --- | --- | --- | --- |
| `public_dig_depth_m` | m | 2.0 | 1.0–3.0 | 0.5 |
| `public_raise_m` | m | 2.0 | 1.0–3.0 | 0.5 |
| `public_exit_step_m` | m | 1.0 | 0.5–1.5 (≤ Character mantle height) | 0.5 |
| `public_buffer_m` | m | 3.0 | 2.0–8.0 | 0.5 |
| `public_outstanding_cells` | cells | 8,000 (= 1,000 m³) | 1,000–20,000 | 500 |
| `claim_dig_below_anchor_m` | m | 8.0 | 4.0–8.0 (T7 ceiling) | 0.5 |
| `claim_shell_m` | m | 1.0 | 0.5–2.0 | 0.5 |
| `heal_age_days` | days | 7 | 3–30 | 1 |
| `heal_clearance_m` | m | 1.0 | 0.5–2.0 | 0.5 |
| `heal_warn_hours` | h | 24 | 6–72 | 6 |
| `heal_window_min` | min | 60 | 15–180 | 15 |
| `heal_ops_per_s` | ops/s per zone | 100 | 20–200 (≤ half `terrain_zone_ops_per_s`) | 10 |

Existing keys unchanged: reach 8 m, brush 0.5–5 m, 100 m³ per commit, 8 m below / 12 m above (T7 remains the outer ceiling for claims), border 8 m, 1 commit / 1.5 s, 2,000 m³ per hour, 200 ops/s per zone.

## 6. What the player sees

Dig and Raise share the build-mode palette (CLM-01). The ghost tints **green** when the full brush passes, **amber** when the tool clips the brush to a legal shape (depth or raise limit reached; the clipped result is what the ghost shows and what is submitted), **red** when nothing legal remains, with one line, the first failing check in T order:

| Reject code | Message |
| --- | --- |
| `dig_too_deep` | "Too deep for open land. You can dig 2 m below the original ground here. Claim this land to dig deeper." |
| `raise_too_high` | "Too high for open land. You can raise the ground 2 m here. Claim this land to build higher." |
| `dig_no_exit` | "This would leave a hole or wall you can't climb out of. Slope one side so it can be walked out of." |
| `dig_overhang` | "You can't dig under solid ground or build ground in the air on open land. Tunnels and overhangs need a claim." |
| `dig_buffer` | "Too close to a road, path or someone else's land. Keep 3 m clear." |
| `dig_under_character` | "You can't dig under someone's feet." |
| `dig_outside_band` | "Below your claim's limit. Your claim lets you dig 8 m below its ground level." |
| `dig_claim_shell` | "This would break through the edge of your claim." |
| `rate_limited` (budget) | "You have 1,000 m³ of open-land digging and raising at a time. Yours heals over the next N days, or claim it to keep it." |
| existing `interact_state` reasons | unchanged wording for cities, borders, spawns, protections |

The preview also states "Heals on <date> unless claimed" for open land and the volume this edit adds to the player's outstanding budget. It never shows a material cost or a yield, because there is none (§8.1).

## 7. Server enforcement in DEC-03's order

Insert as sub-steps; no existing step moves. Public rules apply when T4 passed under "zone policy allows edits on unclaimed land"; claim rules apply when T4 passed under a parcel terrain right. The 32-op flush ceiling, prepare/activate staging and expected-head rechecks are unchanged.

| Step | Check | Reject |
| --- | --- | --- |
| T4 | Unchanged. Its outcome selects the public or claim rule set below. | `interact_state` |
| T5 | Unchanged, plus on open land the 3 m `public_buffer_m` around road and path splines, others' claim boundaries and others' structures for both dig and raise. | `dig_buffer` |
| T7 | Unchanged outer bound (8 m / 12 m / `min_dig_z`). | `interact_state` |
| T7.1 | Open land: final surface ≥ original − `public_dig_depth_m` and ≤ original + `public_raise_m` in every touched column. Claim: every changed voxel inside the parcel, z ≥ `max(band_bottom, anchor_base_z − claim_dig_below_anchor_m, min_dig_z)` and ≤ band ceiling; changed voxels within `claim_shell_m` of the parcel boundary reject. | `dig_too_deep` / `raise_too_high` / `dig_outside_band` / `dig_claim_shell` |
| T7.2 | Open land: open-sky rule. | `dig_overhang` |
| T7.3 | Open land: exit rule. | `dig_no_exit` |
| T8 | Unchanged (raise versus capsules). | `interact_state` |
| T8.1 | Any location: no density removal inside the 1 m × 2 m cylinder under any character or NPC capsule. | `dig_under_character` |
| T9 | Unchanged, plus open land: outstanding public changed cells after this edit ≤ `public_outstanding_cells`. | `rate_limited` |

Clipping (amber) is computed client-side for preview only; the server never clips, it rejects, and the client submits the clipped brush. Rollups to anticheat use the new codes with the existing T1–T10 counters.

## 8. Healing

### 8.1 What heals

- Only **public, unclaimed** edits: dig, raise, flatten, smooth and paint committed under T4's "unclaimed land" permission. Healing restores each voxel to the shipped base density **and** base material.
- **No cost, no yield, anywhere.** Owner: "altering land shouldnt cost anything or gain material from". Dig produces no soil, stone or ore; raise consumes no fill; healing returns nothing. This replaces DEC-03's terrain yield and paid-fill accounting (§13): the terrain action carries no stock or yield participant, the yield-provenance fields stay at zero, and the Alpha Starter Kit is not drawn on by terrain. Resource gathering stays with harvesting and mining nodes (GAT-01).
- **Claimed** land never heals. A legal claim adopts every committed intersecting public edit at the claim transaction (DEC-03), and adopted edits leave the editor's outstanding budget. Claim release or archive restores through the archive-and-safe-restoration workflow using the same §8.3 occupancy rules.
- Event and GM edits follow their declared policy, never this process.

### 8.2 When

- Each edit carries its own deadline: `commit_time + heal_age_days` (7 real days). Overlapping, adjacent or newer edits **do not reset** it; nor does standing on it, re-painting it, or logging out on it. The oldest part of a worked area heals first.
- The nightly safe-restoration run (realm-local 04:00, inside `heal_window_min`) heals every edit whose deadline has passed, oldest first. An expired edit is dropped from the chunk's op list and the remaining, younger ops replay over the base (World's "replay without expired ops"), so a younger overlapping edit keeps its own shape until its own deadline.
- **Re-editing a healed spot** is a new edit: new deadline, counted against the budget again, measured against the same original surface.

### 8.3 Occupied space is never filled

**Occupants:** online player characters, dropped items (until their own despawn timer), NPCs and creatures, vehicles and mounts. Placed structures, pieces, blocks and props are not on the list because they cannot exist on open land (§8.4). Logged-out characters are not on the list either: the run heals normally, and a character whose capsule is inside solid ground on login is moved to the nearest spawn with their items (owner). For each occupant the server takes its collision bounds and grows them by `heal_clearance_m` = 1.0 m in every direction: the **occupant mask**.

The heal of an edit is computed as the base state masked by:

1. **No fill into the mask.** A voxel inside any occupant mask never gains density. The pocket around an occupant stays exactly as the player left it.
2. **No removal under an occupant.** Healing a raise never removes a voxel in the column beneath an occupant's footprint plus the 1 m margin, down to the base surface. A mount parked on a mound keeps its mound until it moves.
3. **No floating, no undercut.** Columns heal bottom-up: a voxel gains density only if the voxel below it is solid after this run; a voxel loses density only if no solid voxel remains above it. If rule 1 or 2 blocks a voxel, every voxel above it in that column stays as it is.
4. **Walkable edge.** Where healed ground meets a masked pocket, the final surface may not step more than `public_exit_step_m` (1.0 m) per column. Instead of building a wall around the pocket the run widens the untouched zone outward, column by column, until the step rule holds: healing fills less, never more.
5. **Exit check on the pocket.** The residual pocket (masked columns plus the widening) must pass the §3 exit rule for a character standing in it. If healing around the occupant would leave a well a player can't climb out of, nothing in that edit's footprint within 3 m of the pocket heals this run.

Voxels left unhealed by rules 1–5 remain a **residual edit** with the same identity, already past its deadline. It is retried every night and heals the first night its occupants are gone (the player walks away or logs out, the item despawns, the creature wanders, the mount is ridden off). Nothing an occupant can do holds a residual for longer than its own presence. Residual, expired voxels **do not count** against the original editor's outstanding budget.

Healing never entombs anyone who is present: rule 1 keeps 1 m of air around every online capsule; rule 3 never seals a pocket from above; rule 5 keeps the pocket walkable. A character who logged out in the area and finds solid ground where they stood is moved to the nearest spawn on login, keeping their inventory; the login check is the same capsule-overlap test as T8.

### 8.4 Ownership

- **Anything placed on open land needs a claim** (owner). No structure, piece, block, prop, tent, campfire, scaffold or cache can be placed outside a claim parcel. The rule lives in Construction's placement validation, the Building-rights step that mirrors terrain T4: a placement whose footprint is not inside a parcel where the actor holds the build right rejects `interact_state` with "You need a claim to place this here", and the CLM-01 grid shows it as crimson hatch. Dropping an item is not placement; dropped items keep their own despawn timer.
- So nothing placed can pin a public hole open, and no structure ever stands in one. Only a claim keeps an edit; the editor keeps its outstanding budget until it heals or is adopted. Expired residual voxels blocked by an occupant count against nobody.
- When a claim adopts an edit, the claim owns it from then on: the original editor's budget is released and the World provenance records both.

### 8.5 Player-facing

- **Marker.** In build mode, every public edit shows an earth-coloured hatch tint with "Heals in N days" on hover; within `heal_warn_hours` (24 h) the tint turns orange (owner); a residual edit waiting on an occupant shows a striped tint with "Waiting: someone or something is in the way". Outside build mode nothing is drawn.
- **Notification.** The commit preview states the healing date. The editor's notification inbox gets one message 24 h before the first of their edits heals ("Your digging near Silverbrook north ridge heals tonight unless you claim it") and one summary after each run ("3 edits healed; 1 is waiting because a player is standing in it"). One message per run, not per edit. A character moved to a spawn on login sees "The ground where you logged out has healed; you were moved to <spawn>".
- **Preview colours** stay the build palette: green/amber/red for the candidate; hatch/orange/striped for healing age on existing edits.

### 8.6 Server enforcement

Healing is not a T1–T10 player edit; it is a world action by `world-service` through Construction's lifecycle, after all T-validated commits of the day and before any claim action on the same chunks (pending claim/chunk actions fence it, DEC-03). Each edit heals under its own DEC-02 `cba_` decision:

| Step | Check | Outcome if it fails |
| --- | --- | --- |
| H1 | Edit still public: not adopted, no pending claim action or chunk action on its chunks, deadline passed. | Skip (adopted) or defer to next run (pending). |
| H2 | Build the occupant mask from authoritative positions (online characters, dropped items, NPCs, mounts; logged-out characters are not masked); apply §8.3 rules 1–2. | Masked voxels become residual. |
| H3 | Support: no removal beneath occupant footprints; Construction re-evaluates support for any claim structure whose anchor touches healed columns across the parcel edge, with the same solver as placement. | Columns that would fail support stay residual. |
| H4 | Shape: §8.3 rules 3–5 on the result; protected volumes, routes and watercourses are restored first and never left partial. | Widen the untouched zone or defer the 3 m neighbourhood. |
| H5 | Prepare: expected heads for every touched chunk under the current zone-owner epoch, 100 m³ and 8-chunk caps per decision (a larger edit heals as several decisions, oldest voxels first), immutable decision and receipt recorded. | Head conflict: revalidate and retry inside the window. |
| H6 | Apply, install collision/support projection, activate with the completion certificate; update the editor's outstanding budget and provenance (`healed`, or `residual` with reason). | Crash after decision: recovery completes it; never a half-healed chunk. |

**Batching and limits.** Per realm, one run per night, oldest edits first, at most `heal_ops_per_s` = 100 ops/s per zone (half of the 200 ops/s zone cap, leaving room for players), stopping at `heal_window_min` = 60 min; everything left is already expired and continues the next night. A zone with a live siege or an active world event skips its chunks that night. Metrics: edits healed, residual count and reasons, ops, time used; alert when the backlog grows three nights running.

**Crash safety.** Every heal is idempotent by edit id and expected head: a repeated decision after recovery returns the original receipt, and a chunk is never activated twice. Undecided work recovers through the coordinator; decided work completes; no timeout refund, no partial geometry visible.

## 9. Owner answers, 2026-10-22 (limits)

Open-land depth **2 m**; levelling a house pad on open land needs **no claim**; buffer **3 m**; unhealed open-land volume per player **1,000 m³** (about 23 m × 23 m × 2 m), dig and raise combined; in your own claim dig to **8 m** below the claim anchor; trapped visitors use **Unstuck**. Dig and Raise are separate building-menu actions with brush sizes and a result preview; raising follows the same limits. The tables above carry these values.

## 10. Owner answers, 2026-10-22 (healing)

1. **Placed objects:** anything placed on open land needs a claim; a tent, block or campfire cannot be placed on unclaimed land, so nothing can pin a public hole open (§8.4).
2. **Logged-out players:** heal normally, then move the player to the nearest spawn on login (§8.3).
3. **Warnings:** build-mode tint with days left, orange on the last day, plus an inbox note the day before (§8.5).
4. **No cost, no yield:** "altering land shouldnt cost anything or gain material from". Digging gives no material and raising consumes none, on open land and in claims alike; resource gathering stays with harvesting and mining nodes (§8.1, §13).

## 11. Acceptance tests

Fixtures join DEC-03's TER-D03 set as TER-D03-10 "digging and raising limits" and extend TER-D03-08 "lifecycle"; each is a committed op sequence against a golden chunk set with the expected accept/reject code or healed/residual state.

Limits (TER-D03-10):

1. Open pad: cut 2.0 m and raise 2.0 m on a 20° slope with a 1 m stepped edge → accepted; steps at 1.0 m pass, 1.5 m fails `dig_no_exit`.
2. Open pit: 2 m sphere straight down on flat ground → `dig_no_exit`; same with one side ramped → accepted. Raised 2 m ring around a standing stranger → `dig_no_exit`; the same ring with a 1 m gap → accepted.
3. Depth and height: 2.0 m accepted, 2.5 m `dig_too_deep` / `raise_too_high`; after a 2 m cut heals, a second 2 m cut from the healed surface is accepted and 2.5 m is still `dig_too_deep` (baseline never drifts).
4. Open ridge: horizontal cut into a 45° face stops at about 2 m along the column, next brush `dig_too_deep`; an undercut lip or a raised overhang → `dig_overhang`.
5. Trench across a path: 2 m deep crossing a `critical` link → T5; the same trench, or a 2 m bank, 3 m beside the road → `dig_buffer`; 4 m beside with sloped walls → accepted.
6. Under feet: dig touching the cylinder under a stranger, an NPC and the actor's own capsule → `dig_under_character`; 1.5 m away → accepted.
7. Claim basement: owner digs 8 m below the anchor base in-parcel → accepted; 8.5 m → `dig_outside_band`; raise to the band ceiling → accepted, above → `interact_state` (T7); a stranger with no terrain right → T4.
8. Mountain home: owner carves a 6 m deep, 3 m high room into a hillside inside the band → accepted; the same room within 1 m of the parcel edge → `dig_claim_shell`; the same room on open land → `dig_overhang`.
9. Budget: 7,900 cells outstanding (dig and raise mixed), next 200-cell edit → `rate_limited`; after nightly healing the count drops and the same edit is accepted.
10. Preview parity: for every case above the client tint and message match the server's first failing code; a stale preview after a neighbouring claim appears is rejected with a fresh quote.
11. Config bounds: `public_exit_step_m` above the Character mantle height and `heal_ops_per_s` above half the zone cap are refused at publication; each key outside its bounds fails closed.

Healing (TER-D03-08 extension):

12. Age: an edit committed at day 0 is untouched on the day-7 run before its commit hour and healed on the first run after 7 × 24 h; a day-3 overlapping edit does not extend it and keeps its own shape after the older one heals.
13. Restore and no economy: healed voxels equal base density and material byte-for-byte; the editor's budget drops by the healed volume; no item, stock or yield receipt is produced by the dig, the raise or the heal; a dig and a raise commit with an empty inventory succeed and change no inventory.
14. Adoption: a claim over a day-5 pad adopts it; day-7 run skips it; the editor's budget is released; releasing the claim later archives and restores with the same occupancy rules.
15. Occupied by a character: a player standing in a 2 m hole at run time → the run heals the rest, leaves 1 m of air around the capsule, widens until no step exceeds 1 m, and the pocket passes the exit rule; the residual heals the night after the player leaves. A player standing where the partial heal would fail the exit rule → the 3 m neighbourhood is deferred whole. A player logged out in the same hole → the run heals it fully; on login the capsule-overlap check moves them to the nearest spawn with their inventory and shows the notice.
16. Placement on open land: placing a tent, block, campfire or scaffold outside any parcel → `interact_state` "You need a claim to place this here", crimson hatch in the grid, nothing charged; the same placement inside the actor's parcel → accepted. A dropped item in a pit keeps its 1 m pocket until it despawns; the residual counts against nobody's budget.
17. Occupied by an NPC or mount: a mount parked in a trench keeps its pocket; the surrounding trench heals; no voxel is added inside its bounds plus 1 m.
18. Never floating, never undercut: healing a raise under which a later dig left a cavity removes the raise only where no solid remains above; healing a dig beside a surviving raised bank never adds a voxel over air.
19. Support: a public mound raised beside a parcel edge (outside the 3 m buffer) that a claim structure's anchor later came to rest on across the boundary → healing leaves the columns under the anchor plus 1 m residual; Construction re-evaluation reports no instability.
20. Batching and crash: 10,000 expired edits with a 15 min window → oldest first, the run stops at the window, the rest heal the next night, metrics match; a crash after decision on edit N → recovery completes N with the original receipt, N+1 is untouched, no chunk is activated twice.
21. Player-facing: build-mode tint shows days left, turns orange within 24 h, striped for residual; one inbox note the day before and one summary after the run, regardless of edit count.

## 12. Not in this packet

No terrain support solver (terrain never collapses; structures still do under Construction §3.4), no auto-generated ramps, no GM or land-owner override of the limits, no siege-time digging (Building's), no dig zones, no placement of any kind on open land, no terrain yield or fill cost.

## 13. Conflicts with existing design

The owner's "no cost, no yield" and "nothing placed on open land" decisions override the former passages below; the cited documents are now synchronized.

| Source | Current text | Now |
| --- | --- | --- |
| [DEC-03] "Proposed adaptor sequence" step 3, "Lifecycle" and fixtures TER-D03-02/04/08 (synced in c29677266f77c86155f88f9d91dda6d8dbd86710) | Terrain edits prepare "paid stock/yield" under the `cba_` action; "fractional yield provenance" persists through excavation, fill, revert and healing; "renewed yield" and "paid fill" are checked before healing. | No stock or yield participant on a terrain action. Provenance fields stay but are always zero; the "paid fill" and "renewed yield" rechecks become no-ops. Fixture steps that assert a yield or a charge assert zero instead. |
| World terrain spec, "Durability and material conservation" and the cost tables (synced in c29677266f77c86155f88f9d91dda6d8dbd86710) | "Resource-yielding excavation"; "healing cannot refresh an already harvested baseline cell". | Excavation yields nothing, so there is no harvested baseline to protect. The sentence stays true vacuously; mining nodes own ore. |
| [Construction] §3.1 (synced in c29677266f77c86155f88f9d91dda6d8dbd86710) | "Terrain removal yields the authored common soil/stone yield once; placed fill consumes matching material, and neither action creates ore nodes." | Terrain removal yields nothing; placed fill consumes nothing. Compact/fill provenance for structure anchors (DEC-03 owner request 2) remains a support question, not a cost question. |
| [Construction] §13 and §3.1 (synced in c29677266f77c86155f88f9d91dda6d8dbd86710) | "Public digging cannot place permanent unclaimed structures, caches, or automation" and scaffolding as a reclaimable structure imply temporary unclaimed placeables. | No placeable of any kind outside a claim parcel, scaffolding included. Placement validation rejects outside a parcel (§8.4). |
| CLM-01 grid colours (synced in c29677266f77c86155f88f9d91dda6d8dbd86710) | Crimson hatch marks blocked cells. | Also marks every cell outside the actor's parcels for placement. |
| GAT-01 growing resource nodes (synced in c29677266f77c86155f88f9d91dda6d8dbd86710) | Unchanged. | Confirmed as the only source of soil, stone and ore. |

Unaffected: DEC-02 staging/certificate, the terrain codec, chunk keys, T1–T10 order, 7-day healing, claim adoption, the 8 m / 12 m outer bounds.

[DEC-03]: DEC-03-terrain.md

[Construction]: ../Game-Design/Legends_Shattered_Frontier_Construction_System_Architecture_v0.1.md
