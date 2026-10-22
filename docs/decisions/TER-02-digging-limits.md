# TER-02 — Digging limits: build yes, grief holes no

Status: proposed, 2026-10-22. Answers the owner's "Digging limits" line in [OWNER-2026-10-22](OWNER-2026-10-22-decisions.md). Applies only if [TER-01](TER-01-mesh-terrain-fit-check.md) ends in a dig-anywhere adoption; otherwise the owner's terrain choice rule drops digging entirely and this packet is moot. Extends [DEC-03](DEC-03-terrain.md)'s T1–T10 order; it changes no storage, codec or healing algorithm.

Owner's words: "digging into a mountain to make a house, which would require claiming that area. I am ok with that. But digging down for grief holes shouldnt be allowed. digging a small ridge side down, to give room for a house is allowed."

## 1. One idea

**Public land is shallow and open to the sky. Claims go deep.** Everything below follows from that.

- Outside a claim you may cut or fill at most 2 m, every cut must be walkable out of, and nothing may be dug under a roof of original ground. That allows levelling a pad, cutting a ridge side back, ditches and ramps. It cannot make a pit, a shaft, a tunnel or a trench with vertical walls.
- Inside your own claim the claim's vertical band is the limit: basements, tunnels and a house carved into a mountainside are allowed as long as every removed voxel lies inside the parcel and band and clears the existing protections.
- Nobody digs under anyone's feet, next to a road, or up against someone else's claim or structure.
- Public edits still heal after 7 days; claiming the land adopts them (DEC-03 lifecycle, unchanged).

## 2. Rules by location

| Where | Depth below original surface | Raise above | Shape | Buffers | Rate / volume |
| --- | --- | --- | --- | --- | --- |
| Public land, outside any claim (zone allows public edits) | ≤ 2.0 m (`public_dig_depth_m`) | ≤ 2.0 m (`public_raise_m`) | Exit rule and open-sky rule (§3) | 3 m from roads, paths, others' claims and others' structures (`public_buffer_m`) | DEC-03 rate keys; outstanding public cells cut to 4,000 per character per realm (§5) |
| Your own claim (terrain right in that parcel and band) | Down to the band floor: `max(band_bottom, anchor_base_z − 8 m, zone.min_dig_z)` | To the band ceiling, within T7's 12 m | Tunnels and rooms allowed; the dig may not open a hole in the parcel boundary wall thinner than 1 m (`claim_shell_m`) | Existing T5: 1 m to foreign structures; others' parcels are off limits by T4 | DEC-03 rate keys; no outstanding-cell cap (claimed edits persist) |
| Within 3 m of a road, path, respawn, others' claim boundary or others' structure | No dig (`raise`/`flatten`/`smooth` that only add density within 0.5 m of the current surface are allowed, so a path can be repaired but not trenched) | — | — | T5 extension | — |
| Under any character or NPC capsule (1 m radius cylinder, 2 m down) | No dig, any location, including your own feet | T8 already blocks `raise` | — | — | — |
| Mountain interior, public | Same as public: 2 m measured along the column, so a horizontal cut into a slope stops after about 2 m on a 45° face and the open-sky rule stops undercutting a lip | — | No tunnels (open-sky rule) | — | — |
| Mountain interior, own claim | Band rule above: the band is anchored to the claim anchor's base elevation, not to the column surface, so a 30 m personal band covers a hillside home with rooms behind it | — | Tunnels allowed inside parcel + band; T5 protections (landmarks, watercourses, routes, nodes) still reject | — | — |
| Cities, safe areas, roads, spawns, region borders, bedrock | No dig (existing T5/T6/T7, owner's "valid limits") | — | — | — | — |

"Original surface" is DEC-03's shipped base surface per column (`base_content_version`); repeated edits never move it, so a player cannot dig 2 m, wait for healing, and dig 2 m deeper, nor stack 2 m cuts by smoothing.

## 3. Shape rules (public land only)

These two checks turn "no grief holes" into geometry. Both run on the final surface of the brush footprint plus a one-voxel ring, at most 22 × 22 columns, so they cost one small 2D pass per candidate.

- **Exit rule.** Every column whose final surface is below its original surface must connect, through adjacent columns, to a column that is not lowered (or lowered less than one step), with no single step up greater than `public_exit_step_m` = 1.0 m. A cut into a hillside passes because its open downhill side has zero step. A pit, a shaft or a vertical-walled trench fails because every way out is a 2 m wall. The step value must stay at or below the character's mantle/jump height declared by Character movement; the bound below keeps it there.
- **Open-sky rule.** A removed voxel must have no original-solid voxel above it that survives the edit. No undercut lips, no tunnels, no "dig sideways under the path".

Levelling a ridge for a house on public land therefore works as cut ≤ 2 m on the high side, fill ≤ 2 m on the low side, with a sloped or stepped edge the player can walk out of. It needs no claim. Structures on public land are already impossible ([Construction] §13), so the house itself requires the claim, and claiming adopts the pad.

## 4. Fall and trap safety

- Public: the exit rule bounds every drop at 1.0 m per step and 2.0 m total, below any fall-damage threshold, and guarantees a walk-out. No auto-ramp is generated; a dig that would leave no exit is refused with a preview tint so the player ramps it themselves.
- Everywhere: no dig under a capsule (T8.1), no raise into a capsule (T8). Healing and claim-archive restoration already refuse to entomb (DEC-03 lifecycle, TER-D03-08).
- Claims: tunnels can enclose a visitor behind a door or a later fill. Construction's door/permission rules cover doors; for sealed terrain the server's existing unstuck path (respawn at the nearest spawn, no item loss) is the exit. If no unstuck command exists yet, it is a prerequisite of claim tunnelling, not a new terrain rule. See owner question 6.

## 5. Typed configuration

All under `tune.world.*`, realm-scoped, operator permission, effective at the next terrain commit, declared through DOC-05 as the Tuning architecture requires. Shape rules, the band formula and reject codes are release-bound code.

| Key | Unit | Default | Safe bounds | Step |
| --- | --- | --- | --- | --- |
| `public_dig_depth_m` | m | 2.0 | 1.0–3.0 | 0.5 |
| `public_raise_m` | m | 2.0 | 1.0–3.0 | 0.5 |
| `public_exit_step_m` | m | 1.0 | 0.5–1.5 (≤ Character mantle height) | 0.5 |
| `public_buffer_m` | m | 3.0 | 2.0–8.0 | 0.5 |
| `public_outstanding_cells` | cells | 4,000 (= 500 m³) | 1,000–20,000 | 500 |
| `claim_dig_below_anchor_m` | m | 8.0 | 4.0–8.0 (T7 ceiling) | 0.5 |
| `claim_shell_m` | m | 1.0 | 0.5–2.0 | 0.5 |

Existing keys unchanged: reach 8 m, brush 0.5–5 m, 100 m³ per commit, 8 m below / 12 m above (T7 remains the outer ceiling for claims), border 8 m, 1 commit / 1.5 s, 2,000 m³ per hour, 200 ops/s per zone.

## 6. What the player sees

The preview ghost tints **green** when the full brush passes, **amber** when the server would clip the brush to a legal shape (depth or raise limit reached: the tool offers the clipped brush, like the existing smaller-brush offer for volume), **red** when nothing legal remains. Red shows one line, the first failing check in T order:

| Reject code | Message |
| --- | --- |
| `dig_too_deep` | "Too deep for open land. You can dig 2 m below the original ground here. Claim this land to dig deeper." |
| `dig_no_exit` | "This would leave a hole you can't climb out of. Slope one side so it can be walked out of." |
| `dig_overhang` | "You can't dig under solid ground on open land. Tunnels need a claim." |
| `dig_buffer` | "Too close to a road, path or someone else's land. Keep 3 m clear." |
| `dig_under_character` | "You can't dig under someone's feet." |
| `dig_outside_band` | "Below your claim's limit. Your claim lets you dig 8 m below its ground level." |
| `dig_claim_shell` | "This would break through the edge of your claim." |
| existing `interact_state` reasons | unchanged wording for cities, borders, spawns, protections |

## 7. Server enforcement in DEC-03's order

Insert as sub-steps; no existing step moves. Public rules apply when T4 passed under "zone policy allows edits on unclaimed land"; claim rules apply when T4 passed under a parcel terrain right. The 32-op flush ceiling, prepare/activate staging and expected-head rechecks are unchanged.

| Step | Check | Reject |
| --- | --- | --- |
| T4 | Unchanged. Its outcome selects the public or claim rule set below. | `interact_state` |
| T5 | Unchanged, plus on public land the 3 m `public_buffer_m` around road and path splines, others' claim boundaries and others' structures for density removal. | `dig_buffer` |
| T7 | Unchanged outer bound (8 m / 12 m / `min_dig_z`). | `interact_state` |
| T7.1 | Public: final surface ≥ original − `public_dig_depth_m` and ≤ original + `public_raise_m` in every touched column. Claim: every removed voxel z ≥ `max(band_bottom, anchor_base_z − claim_dig_below_anchor_m, min_dig_z)` and inside the parcel; removed voxels within `claim_shell_m` of the parcel boundary reject. | `dig_too_deep` / `dig_outside_band` / `dig_claim_shell` |
| T7.2 | Public: open-sky rule. | `dig_overhang` |
| T7.3 | Public: exit rule. | `dig_no_exit` |
| T8 | Unchanged (`raise` versus capsules). | `interact_state` |
| T8.1 | Any location: no density removal inside the 1 m × 2 m cylinder under any character or NPC capsule. | `dig_under_character` |
| T9 | Unchanged, plus public: outstanding public cells after this edit ≤ `public_outstanding_cells`. | `rate_limited` |

Clipping (amber) is computed client-side for preview only; the server never clips, it rejects, and the client resubmits the clipped brush. Rollups to anticheat use the new codes with the existing T1–T10 counters.

## 8. Acceptance tests

Fixtures join DEC-03's TER-D03 set as TER-D03-10 "digging limits"; each is a committed op sequence against a golden chunk set with the expected accept/reject code.

1. Public pad: cut 2.0 m and fill 2.0 m on a 20° slope with a 1 m stepped edge → accepted; stepping stones at 1.0 m pass, 1.5 m fails `dig_no_exit`.
2. Public pit: 2 m sphere straight down on flat ground → `dig_no_exit`; same with one side ramped → accepted.
3. Public depth: 2.0 m accepted, 2.5 m `dig_too_deep`; after a 2 m cut heals, a second 2 m cut from the healed surface is accepted and a 2.5 m cut is still `dig_too_deep` (baseline never drifts).
4. Public ridge: horizontal cut into a 45° face stops at about 2 m along the column → accepted up to there, next brush `dig_too_deep`; an undercut lip → `dig_overhang`.
5. Trench across a path: 2 m deep with vertical walls crossing a `critical` link → T5 reject; same trench 3 m beside the road → `dig_buffer`; 4 m beside with sloped walls → accepted.
6. Under feet: dig touching the cylinder under a standing stranger, an NPC, and the actor's own capsule → `dig_under_character` each time; the same brush 1.5 m away → accepted.
7. Claim basement: owner digs 8 m below the anchor base in-parcel → accepted; 8.5 m → `dig_outside_band`; a stranger with no terrain right → T4.
8. Mountain home: owner carves a 6 m deep, 3 m high room into a hillside inside the band → accepted; the same room reaching within 1 m of the parcel edge → `dig_claim_shell`; the same room on public land → `dig_overhang`.
9. Claim adoption: public pad from test 1, claim created over it on day 5 → adopted, no healing on day 7, yields not refreshed (reuses TER-D03-08).
10. Outstanding cap: 3,900 public cells outstanding, next 200-cell dig → `rate_limited`; after nightly healing the count drops and the same dig is accepted.
11. Preview parity: for every case above the client preview tint (green/amber/red) and message match the server's first failing code; a stale preview after a neighbouring claim appears is rejected with a fresh quote (TER-D03-03 pattern).
12. Config bounds: `public_exit_step_m` above the Character mantle height is refused at publication; each key outside its bounds fails closed.

## 9. Owner questions

1. **How deep may anyone dig on open land?** (a) 1 m: safe, but too shallow to level a pad on a real slope. (b) 2 m: levels a house pad on most hillsides, still walkable out of. (c) 3 m: roomier, but a 3 m back wall is a cliff. **Recommend (b) 2 m.**
2. **Does levelling a spot for a house on open land need a claim first?** (a) No claim needed; the pad heals in 7 days unless claimed, and claiming keeps it. (b) Claim first, always. (c) No claim up to 1 m, claim beyond. **Recommend (a):** the house already needs the claim, so the pad gets adopted anyway, and players can try a spot before paying for it.
3. **How far from roads, paths and other people's land must a dig stay?** (a) 2 m. (b) 3 m. (c) 5 m. **Recommend (b) 3 m:** wide enough that a 2 m cut can't undermine a foundation, narrow enough to build beside a road.
4. **How much open-land digging may one player leave unhealed at once?** (a) Today's 2,500 m³, about a 35 m × 35 m pad 2 m deep. (b) 500 m³, a 16 m × 16 m pad 2 m deep. (c) 1,000 m³. **Recommend (b) 500 m³:** enough for a house pad and a ditch, not enough to scar a hillside.
5. **In your own claim, how far below the claim's ground level may you dig?** (a) 8 m, the current world limit: a two-storey basement or a deep cellar. (b) 4 m: one basement. (c) The whole vertical band (30 m on a personal plot): deep mines. **Recommend (a) 8 m:** matches the existing rule and the mountain home still works, since rooms behind a hillside sit above the anchor, not below it.
6. **If a visitor gets sealed inside someone's tunnel, what happens?** (a) An Unstuck command returns them to the nearest spawn with their items. (b) The server refuses any claim edit that would seal a person in. (c) Both. **Recommend (a):** simple, covers every way of getting stuck, and refusing seals needs a slow flood-fill on every claim edit.

## 10. Not in this packet

No terrain support solver (terrain never collapses; structures still do under Construction §3.4), no auto-generated ramps, no GM or land-owner override of the depth limits, no siege-time digging (Building's), no dig zones.

[Construction]: ../Game-Design/Legends_Shattered_Frontier_Construction_System_Architecture_v0.1.md
