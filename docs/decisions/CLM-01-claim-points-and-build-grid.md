# CLM-01 — Claim points and the build grid

Status: proposed, 2026-10-22, from the owner's idea in the Task Runner session; every owner question in the closing section is answered and applied. Extends [Building and Territory] §2 and [BLD-02](BLD-02-terrain-anchored-construction.md); the digging half is [TER-02](TER-02-digging-limits.md). Nothing here is implemented.

Owner, 2026-10-22: "Closer to a town you are limited on size, but as you adventure away your available claim points can be spent. This would allow higher population close to established towns and then larger houses or even someone wanting to build a castle out in the middle of nowhere. We could put a grid on the ground showing the available area based on where you are and your budget ... once an object is placed, it would extend from the placed objects ... a little more defined with a fade effect once it reaches the limit or show red in blocked areas."

Reference: [building-claim-grid-reference.webp](../Game-Design/concept-art/UI/building-claim-grid-reference.webp) (owner-supplied building-mode concept: faint ground grid around a timber foundation, green valid-placement ghost, construction cost panel) and the v5 [build-mode.png](../Game-Design/concept-art/UI/v5/build-mode.png).

## 1. One idea

**Land costs claim points, and points go further the farther you are from a town.** A claim is not a square you pick; it is the ground under and around what you build, paid for one square metre at a time out of a per-character budget. Near a town a metre is dear and a claim is capped small, so many players fit. In the wilderness a metre is cheap and the cap is gone, so one budget buys a castle. The grid on the ground shows what you own, what you can still afford from where you stand, and what is blocked.

## 2. What changes in the existing claim design

| Existing rule | CLM-01 | Why |
| --- | --- | --- |
| Personal plot is a fixed 40 × 40 × 30 m square ([Building and Territory] §2.1; `ClaimTerritory` centre + side; BLD-02 20 m authored plots) | The footprint is a set of 1 m cells grown from placed pieces (§4), bounded by points, band cap and the hard cell ceilings. The 40 m side becomes the **middle-band cap** and stays the shipped ceiling near town; wilderness goes larger. | Owner's idea. |
| Claim is created by a crafted claim anchor (20 rough timber + 10 granite) | The first placed **foundation** is the anchor; its base level is `anchor_base_z` for TER-02. The anchor recipe is kept in the Crafting catalogue for guild claims and is not on the alpha personal path. | One fewer item to learn before the first wall; TER-02 already keys the band to a foundation base. |
| One personal claim per account per world | Up to `max_claims` (3) disjoint personal claims per character, sharing one point budget. | Owner wants a home near town and a keep far away. |
| BLD-02 authored `claims` plots are the only claimable land | Plots stay as the development fixture until J4 ships; afterwards any unblocked cell in a claimable zone is claimable and the plots become ordinary ground. | Delivery order. |
| Claim extent fixed 40 × 40 × 20 cells in the network state (BLD-02 A5/A8) | Extent = claim bounding box, replicated with the cell mask (§7). | BLD-02 already lists this as the pending follow-up. |
| Expansion through the claim map (§2.1) | Expansion is placing a piece; shrinking is removing one (§4). The map stays the view, not the tool. | Owner: "it would always show the limits and update each placed item." |
| `Construction` §13: "a larger claim vertical band permits taller structures, not deeper excavation" | Unchanged for height. Depth is TER-02's `claim_dig_below_anchor_m` (8 m under the anchor base). | TER-02. |

| Terrain excavation can yield materials (World fractional-yield provenance) and World charges material for fill | **Dig and Raise Ground cost nothing and yield nothing** (owner's answer): no materials, no points, no ore or stone from the hole; resources come from gathering nodes only. The preview readout shows volume and remaining budget, never materials. | Owner's answer. |
| Claim anchor maintenance reserve, hourly charges on elapsed real time, 7-day grace → 30-day delinquency → 7-day notice → archive (§2.2, §4) | The **claim storehouse** is the reserve (§9). Charges accrue only while the owner is offline, at a rate derived from the materials actually in that claim; a depleted storehouse starts a 30-day offline countdown (7 days without decay, then decay), then the existing 7-day final notice and archive. Off in alpha behind `tune.building.upkeep.enabled`. | Owner's answer 4. |

Unchanged: 5 m clear spacing between unrelated claims, the protected town plus 5 m clearance, first-winner reservation, the archive/recovery-store path (§2.2), permissions and presets (§3), the §15 cell/object ceilings, 7-day public healing and claim adoption (DEC-03, TER-02), siege rules (personal claims are never capturable), the `cba_` action protocol (DEC-02).

## 3. Claim points

### 3.1 The budget

Each character has a point budget `claim_points_total`. Points are **allocated**, not spent: a cell holds its points while it is claimed and returns them when it is released or archived. The alpha budget is fixed and granted with the Alpha Starter Kit; the shipped progression is level-based. Quests may grant bounded "deed" points later (owner question 1).

| Key (`tune.building.claim.*`) | Unit | Default | Safe bounds | Step | Effective |
| --- | ---: | ---: | ---: | ---: | --- |
| `points_base` | points | 400 | 100–2,000 | 50 | New characters; existing characters on next login |
| `points_per_level` | points/level | 40 | 0–200 | 10 | Next login |
| `alpha_fixed_points` | points | 1,200 (alpha only; 0 = off) | 0–4,000 | 100 | Next login; a development grant, not a feature |
| `max_claims` | claims | 3 | 1–5 | 1 | New claims |

All keys are realm-scoped, economy tier 3 publishing, declared through DOC-05 like the existing `tune.building.*` keys. The cost formula, band lookup and cell rules below are release-bound code.

### 3.2 Cost by distance from an established town

A cell's cost is fixed by the **band** it lies in. Band distance is the shortest horizontal distance from the cell centre to the boundary polygon of the nearest **established town** (not its centre, since Silverbrook's wall is 800 × 1,060 m). A cell's band and price are pinned when it is claimed; later changes grandfather existing cells, as §7 already does for ceilings.

| Band | Distance from town boundary | Cost | Per-claim cap | Height above anchor base | Intent |
| --- | ---: | ---: | ---: | ---: | --- |
| Near | 0–`near_m` (150 m) | `near_cost` 2.0 pt/m² | `near_cap_m2` 400 m² (20 × 20) | 20 m | Dense suburb; a house and yard |
| Middle | `near_m`–`middle_m` (150–500 m) | `middle_cost` 1.0 pt/m² | `middle_cap_m2` 1,600 m² (40 × 40, the existing entitlement) | 30 m (the existing personal band) | Farmstead, workshop |
| Wilderness | > `middle_m` | `wild_cost` 0.75 pt/m² | `wild_cap_m2` 3,600 m² (60 × 60, owner's answer) | 60 m (the existing guild band) | Keep, mine, tower |

Worked examples at the shipped progression (owner's answer: 400 + 40 per level; the level cap is 100, so 4,400 pt at cap): a new character (400 pt) buys 200 m² near town (a 10 × 10 house with a 2 m yard) or 533 m² in the wilderness. At level 100 the full 60 × 60 m keep costs 2,700 pt and the remaining 1,700 pt still buys the whole 40 × 40 farm; the near band is capped at 400 m² at every level. Alpha (1,200 pt fixed, owner's answer) reaches 1,600 m² (40 × 40) in the wilderness, 1,200 m² in the middle band and the near cap.

| Key | Unit | Default | Safe bounds | Step | Effective |
| --- | ---: | ---: | ---: | ---: | --- |
| `near_m` / `middle_m` | m | 150 / 500 | 50–400 / 200–1,500, `near_m < middle_m` | 25 | New cells |
| `near_cost` / `middle_cost` / `wild_cost` | pt/m² | 2.0 / 1.0 / 0.75 | 0.1–10, non-increasing with distance | 0.05 | New cells |
| `near_cap_m2` / `middle_cap_m2` / `wild_cap_m2` | m² | 400 / 1,600 / 3,600 | 100–1,600 / 400–10,000 / 1,600–10,000, non-decreasing | 100 | New cells |
| `band_height_m` near / middle / wild | m | 20 / 30 / 60 | 10–60 each, non-decreasing | 5 | New claims (an existing claim keeps its band) |
| `reach_m` | m | 4 | 2–8 | 1 | New placements |
| `spacing_m` | m | 5 (existing `SpacingMetres`) | 5–20 | 1 | New cells |

Silverbrook today: the three BLD-02 plots are 62–141 m from the wall (near band); the zone's east edge is about 850 m from the wall, so the middle band runs to x ≈ 900 m and the wilderness is the outer 350 m. A larger zone or a second settlement moves the bands with no code change.

### 3.3 Vertical extent and digging

A claim's volume is its footprint × its band: from `anchor_base_z − claim_dig_below_anchor_m` (TER-02 and the owner's answer, 8 m below the claim anchor) up to `anchor_base_z + band_height_m`. The anchor base is the first foundation's base level, so the band is fixed by where the first slab goes, not by each column's surface: a hillside home's rooms behind the slab are inside the band as long as their cells are claimed. Digging a house into a mountainside is paid for like any other land: the cells over the rooms must be claimed at the band price, and TER-02's parcel, band, 1 m shell and protection rules decide what may be removed. Height and depth do not cost points; only footprint does. T7's 12 m above the original surface still bounds raised ground, not walls. A visitor sealed in by a door or a later fill uses the Unstuck command (TER-02 §4, owner's answer); it is a prerequisite of claim tunnelling, not a claim rule.

**Levelled pads and claim adoption.** Per TER-02 and the owner's answers, a house pad can be levelled on open land without a claim: 2 m cut or raise, 3 m from roads and others' land, at most 1,000 m³ unhealed per player, healing after 7 days. The first foundation then claims the pad's cells like any others, at the band price, and the growth adopts the committed edits under them in the same `cba_` action. Adoption costs no extra points and moves nothing between budgets: the cells are paid once, the adopted volume leaves the player's 1,000 m³ public allowance and counts against the claim's 100,000 altered-terrain-cell ceiling instead. A pad wider than the budget buys stays partly public and that part heals on schedule; the grid shows it as the limit fade so the player sees before placing which part the claim will keep. Releasing cells (§4 Shrink) hands their adopted edits back to the public clock, and they count against the public allowance again; if that would exceed 1,000 m³ the release still happens (the allowance only gates new digs).

### 3.4 Spacing, protected zones and established towns

A cell is **blocked** (red on the grid, `claim_blocked` on the server) when it is:

- inside another owner's claim or within `spacing_m` (5 m) of it; own claims within reach merge instead (§4);
- inside a protected area (zone `areas[].safety = protected`, the Silverbrook wall) or its 5 m clearance (existing `HitsProtected`);
- within `road_clear_m` = 6 m of a road or path centreline (`silverbrook-town-layout.json` roads, future zone `routes`), or inside an authored public corridor (TER-02's 3 m dig buffer is measured from the road edge; the claim clearance is wider so a claim's digging never reaches it);
- within `spawn_clear_m` = 25 m of a `respawn_points` entry or 50 m of `default_spawn`;
- within `border_clear_m` = 16 m of the zone extent (twice World's 8 m edit exclusion, so every claimed edit stays legal);
- on a water volume, a gathering node plus 3 m, an instance entrance, or an authored landmark;
- steeper than `max_slope_deg` = 35° measured over the cell (the piece could not be founded there anyway; TER-02 levelling is public land work the player does first);
- part of a growth that would **engulf** another claim or corridor: after the growth, every other claim within 200 m must still touch unclaimed, unblocked ground (bounded flood fill on the 1 m zone grid).

Clearances are `tune.building.claim.*` keys, metres, bounds 2–50, step 1, effective for new cells.

**Established towns.** In alpha and at launch these are the authored settlements: every zone area with `purpose = settlement` and `safety = protected` (Silverbrook). A player settlement becomes established when a guild claim reaches the Village stage of [Building and Territory] §5 and the zone record is republished with its boundary as a `settlement` area through the normal content release; cells already claimed keep their band.

## 4. Claim growth from placed objects

- **Start.** Nothing can be placed on open land (owner's answer: anything placed needs a claim; Construction §13 already forbids public structures). A claim begins with the first foundation placed on unblocked, unclaimed ground while the character has a free claim slot; creating the claim and placing the foundation are one action, so there is never a free-standing piece. Every later placeable, the storehouse included, must land inside a claim or its reach ring. The foundation's base level becomes `anchor_base_z`; its band is read once. The BLD-02 integer anchor is the foundation's minimum cell.
- **Grow.** Every placed piece claims the 1 m cells under its footprint plus every cell within `reach_m` (4 m) of that footprint, at the cell's band price, skipping cells already owned. A piece may only be placed inside the claim or inside that reach ring of the claim. Cells that cannot be claimed (blocked, over budget, over cap, outside the band) refuse the placement with the first failing reason; the grid already showed them red or faded.
- **Cap.** Every band caps a claim's total footprint (400 / 1,600 / 3,600 m²), and the §15 ceilings (50,000 structural cells, 100,000 altered terrain cells) bind as well. The cell under a piece is always part of the footprint, so a cap also caps the building.
- **Shrink.** Removing a piece releases every cell that is no longer within `reach_m` of any remaining piece, returns its points, and hands its committed terrain edits back to the public 7-day healing clock, starting at the release. A removal that would strand another piece's support follows Construction §3.4. A claim may become two parts after removals; it stays one record (one id, one band, one anchor).
- **Empty claim.** A claim whose last piece is removed archives after `empty_claim_hours` = 24 h (bounds 1–168) through the ordinary archive path, releasing its land and points. Until then the owner can place again on it.
- **Multiple claims.** A foundation placed outside every own claim's reach starts a new claim if a slot is free (`max_claims`); inside the reach of an own claim it extends it. Two own claims never overlap; one placement within reach of two own claims extends the nearer and the other is unchanged.
- **Leaving and inactivity.** Upkeep is §9: nothing in alpha; later the claim storehouse pays a daily charge derived from the materials in the claim while the owner is offline, and a depleted storehouse starts a 30-day countdown to archive. Points return at archive.
- **Healing and adoption.** Each growth adopts committed public edits under the new cells at the same `cba_` action (DEC-03 "legal claim creation atomically adopts", now per growth), with the point and allowance accounting of §3.3. Public edits outside the footprint keep their own 7-day deadline.

## 5. The build grid

### 5.1 What it shows

Entering build mode (B) projects a 1 m grid on the ground within `grid_radius_m` = 24 m of the player. Each cell has one of five states, computed on the client from replicated claim cells, the remaining budget, the band map and the blocked mask, and confirmed by the server on every placement:

| State | When | Look (v5 palette, `docs/blueprints/ui-hud.md` §3) |
| --- | --- | --- |
| Owned | cell in one of my claims | ivory `#E8E1CE` lines at 35% alpha, 3 cm wide; 4 m major lines at 5 cm, 50% |
| Affordable | cell within reach of my claim (or, with no claim yet, within the disc the budget buys around the player, radius `sqrt(points / (π × cost))`), and claiming it fits points and cap | moss `#688267` lines 55%, 3 cm, plus 8% moss fill |
| Limit | the first `fade_m` = 3 m of affordable cells past the budget or cap | same lines, alpha falls linearly to 0 over 3 m; the last affordable cell carries a 6 cm ivory rim |
| Blocked | §3.4 reasons and every cell outside the actor's claim parcels for placement (TER-02 §13) | crimson `#A34842` fill 25% with a 45° hatch at 0.25 m pitch, lines 60%; never colour alone: the status line names the reason ("Too close to the road") |
| Out of band | the cell's band differs from the claim's | amber `#B69A55` lines 40%, no fill, status "Outside this claim's band" |

Beyond `fade_m`, cells past the limit are not drawn, so an empty budget shows only the owned area. The whole grid fades with distance from the player from 16 m to 24 m. Lines keep a 1.5 px screen-space minimum so they read at 20 m, matching the faint white lines of the reference at about a quarter of the ghost's brightness; the ghost and its corner studs stay the brightest green element on screen.

### 5.2 Live updates

The grid is a 64 × 64 R8 **state mask** (one byte per cell, 4 KB) centred on the player, rebuilt on the client when any of these change: player cell, placement or removal receipt, claim replication, remaining points. Rebuild is one pass over 4,096 cells and uploads one texture region; a placement therefore extends the moss ring from the new piece on the frame its receipt arrives. A refused placement refreshes the mask from the server reply so the client never disagrees with authority for more than one round trip.

The build HUD cost panel (reference: "Construction Cost") gains one row under the materials: "Claim  +36 pt  (820 / 1,200)" in the have/need style, crimson underline when short, and the status line reuses the existing "Valid placement" / "Invalid placement" wording with the claim reason appended.

### 5.3 Implementation in UE 5.8

| Option | Cost | Fit |
| --- | --- | --- |
| **Deferred decal (DBuffer)**, one `UDecalComponent` 48 × 48 m × 20 m tall on the player, material reads the state mask texture and draws lines from world XY | ~0.15–0.3 ms at 1080p for one 48 m decal; no asset or ground-material changes; one 4 KB texture update per change | Works on the current `SM_Silverbrook_Terrain` mesh, the GFX-02 Landscape, TER-01 Mesh Terrain and DEC-03 voxel meshes alike. Stretches past 35°, which is blocked anyway |
| RVT write (grid drawn into the GFX-02 runtime virtual texture) | Near zero per frame after the write | Needs the GFX-02 Landscape + RVT (not built yet) and a second path for mesh/voxel ground; ties a UI element to the ground material |
| Procedural mesh snapped to the ground | Exact lines, no stretch | 2,500 line traces per rebuild at 1 m over 48 m, z-fighting on slopes, a second mesh per client |

**Pick the decal.** `MI_ClaimGrid` under `/Game/Frontier/UI/Build/`, parameters: `StateMask` (R8 64 × 64, nearest sampling), `GridOriginCm` (vector), `CellCm` 100, `LineCm` 3, `MajorEvery` 4, `MajorLineCm` 5, `FadeStartCm` 1600, `FadeEndCm` 2400, `LimitFadeCm` 300, the five colours as vector parameters. Sort order above the ground, `DBuffer Translucent Color` blend so it does not change normals or roughness. The decal exists only while build mode is active. Recipe: `docs/blueprints/building-mode.md`.

### 5.4 Dig and Raise Ground in the build menu

Owner, 2026-10-22: Dig and Raise Ground are actions in a **Terrain** section of the build catalogue, beside Foundation, Wall, Door and Roof (the v5 `build-mode.png` already shows the Terrain tab), not a held tool; §5.5 grows the section to six tools (Dig, Raise, Level, Smooth, Ramp, Restore) under the same limits. Selecting one shows the TER-02 brush instead of a piece ghost.

- **Same limits for both.** Raising follows exactly the digging limits: 2 m above the original ground on open land (`public_raise_m`), the claim band inside a claim (to `anchor_base_z + band_height_m`, within T7's 12 m), the 3 m buffers, the shared 1,000 m³ open-land budget per player (one counter for cut and fill, 8,000 cells), and 7-day healing of public edits.
- **Shared grid and preview.** Both actions draw the same claim grid of §5.1 and tint the brush ghost green / amber / red per TER-02 §6: green when the whole brush passes, amber when the server would clip it to the legal depth, height or shape (the tool offers the clipped brush), red with the first failing reason. Blocked grid cells (§3.4) are red for terrain actions too; a terrain action on open land ignores the budget states (it claims nothing), so owned, affordable and limit cells draw at their normal alpha but the ghost tint alone decides validity.
- **Points.** Terrain actions never grow or shrink a claim. Inside a claim the brush must lie within claimed cells (TER-02 parcel rule); on open land it needs no claim and no points. Claiming later adopts the edits per §3.3.
- **Cost panel.** Terrain actions have no material rows at all (owner's answer: digging and raising cost nothing and yield nothing). The panel shows "Volume  36 / 100 m³" (brush changed volume against the 100 m³ per-commit cap) and, on open land, "Open-land budget  620 / 1,000 m³"; both in the have/need style with the crimson underline when over. No inventory changes on commit, and no items are produced.

**TER-02 wording that needs a one-line update** so that raising is covered explicitly (TER-02 owns these; CLM-01 only lists them):

1. §1 "every cut must be walkable out of": add "and every raised edge must be walkable up to (same 1.0 m step), so a 2 m fill cannot wall off a road or a neighbour".
2. §3 exit rule: state that it runs on raised columns too (a column whose final surface is above its original surface must connect through steps of at most `public_exit_step_m` to an unchanged column).
3. §5 `public_outstanding_cells`: default 8,000 (= 1,000 m³ per the owner's answer), and note that cut and fill share it.
4. §6 reject table: add `raise_too_high` — "Too high for open land. You can raise the ground 2 m above the original here. Claim this land to build higher." and `raise_no_step` — "This would leave a wall you can't climb. Slope one side so it can be walked up."
5. §2 "Your own claim" row: "To the band ceiling" should name `anchor_base_z + band_height_m` from CLM-01 §3.3.
6. §1 and §2: name all six tools of §5.5.1; Level, Smooth and Ramp are cut-and-fill strokes under the same limits per column; Restore moves only toward the original surface and releases that volume from the budget; inside a buffer Smooth and Level may add at most 0.5 m and remove nothing.
7. §6 reject table: `raise_no_step` is raised by Raise, Level and Ramp alike; the client may append the advisory "Try Ramp" / "Try Smooth" after a `dig_no_exit` or `raise_no_step` message.
8. §8.1: Restore and undo (inverse commits, §5.5.4) are public edits like any other: a column returned to the baseline leaves the healing list and the editor's outstanding count at once.

### 5.5 Terrain tools, brushes and the result preview

Owner, 2026-10-22: the Terrain section "needs a visual guide on how it's going to affect the ground too, with different size tools to use", and "the terrain editor should be full featured. size, flatten, etc. research other games terrain editors and pick the best tools from them". The research is in §5.6; this section is the chosen set. Everything here is a client tool over the same server action: every stroke is one DEC-03 terrain commit measured against the original surface, clipped by the client and validated by the server in T order, with no cost and no yield.

#### 5.5.1 The six tools

| # | Tile | What a stroke does | Taken from | Why it is in |
| --- | --- | --- | --- | --- |
| 1 | **Dig** | Lowers every column in the brush by the strength step, to at most the depth limit. | Valheim pickaxe, No Man's Sky Subtract | The basic cut. |
| 2 | **Raise** | Lifts every column by the step, to at most the height limit. | Valheim hoe Raise Ground, No Man's Sky Create | The basic fill. |
| 3 | **Level** | Sets every column to one **target height**: the ground under the cursor at the first click, or the locked height (§5.5.4). Cut and fill in one stroke. | Valheim hoe Level + Shift lock, Cities: Skylines 2 Level Terrain, Astroneer Flatten | The most used tool in every game surveyed; a house pad in one stroke. |
| 4 | **Smooth** | Replaces each column by the mean of its 3 × 3 neighbours, `smooth_passes` times. Never crosses the limits; adds at most 0.5 m inside a buffer and removes nothing there (TER-02 §2 row 3). | WorldEdit `//smooth`, Cities: Skylines 2 Soften, Landmark Smooth | Fixes the stepped edges every voxel tool leaves and turns a clipped cut into something walkable. |
| 5 | **Ramp** | Two clicks: low end, then high end. Draws a straight slope of the brush's width between the two heights, at most `ramp_max_m` long. | Cities: Skylines 2 Slope Terrain, Medieval Engineers stakes-and-rope plane, Space Engineers Ramp shape | The exit rule (TER-02 §3) refuses any cut you can't walk out of; Ramp is the one-stroke way to satisfy it. |
| 6 | **Restore** | Returns every column in the brush to the shipped original surface and material, and frees that volume from the open-land budget. Never clipped: it only ever moves toward the baseline. | No Man's Sky Restore, Landmark Heal | Free, unlimited "put it back"; the only undo that works after the 60 s window. |

`1`–`6` pick the tiles. Level, Smooth and Ramp are the "flatten, etc." the owner asked for; Restore is the safety net that an MMO with a 7-day heal and a shared budget needs.

#### 5.5.2 Brushes: shape, size, strength

| Control | Values | Default | Key (`tune.world.brush.*`) | Bounds | Step | Effective |
| --- | --- | --- | --- | --- | --- | --- |
| Shape | round, square | round | — (fixed set) | — | — | — |
| Size (footprint across) | presets 0.5 / 2 / 4 / 7 m; fine steps between in 0.5 m | 4 m | `precise_m` / `small_m` / `medium_m` / `large_m` (radius; square uses it as half-side) | 0.25–5.0 m, non-decreasing (World's 5 m ceiling) | 0.25 | Next brush selection |
| Strength (height per stroke) | ½ m, 1 m, 2 m | 1 m (Precise: ½ m only) | `step_m` | 0.5–2.0 | 0.5 | Next stroke |
| Smooth passes | 1–4 | 2 | `smooth_passes` | 1–4 | 1 | Next stroke |
| Ramp length | — | 10 m | `ramp_max_m` | 4–10 (the 22-column shape pass of TER-02 §3) | 1 | Next stroke |
| Preview rate | — | 20 Hz | `preview_hz` | 5–60 | 5 | Client only |
| Undo window | — | 60 s, 8 strokes | `undo_window_s` / `undo_depth` | 15–120 s / 1–16 | 15 / 1 | Next stroke |

| Size preset | Shape | Footprint | Strength default | Full 2 m cut volume | Use |
| --- | --- | ---: | ---: | ---: | --- |
| Precise | square only | 0.5 m (one column) | ½ m | 0.1 m³ | Trim a step, fix a seam |
| Small | round or square | 2 m | ½ m | 6 m³ | Ditch, post hole, path edge |
| Medium | round or square | 4 m | 1 m | 25 m³ | House pad, ramp |
| Large | round or square | 7 m | 1 m | 77 m³ | Whole pad, terrace; under the 100 m³ per-commit cap at a full 2 m cut |

No hardness or falloff slider (Planet Coaster): on a 0.5 m grid with ½–2 m steps a soft edge only makes the clipped outline unpredictable; Smooth does the same job visibly. No continuous strength (Cities: Skylines, Planet Coaster intensity 0.1–1.0): three steps on the voxel grid are exact and read out as metres. World's 100 m³ per-commit cap, 0.5–5 m brush bounds and rate keys are unchanged; a stroke over 100 m³ is offered clipped (Construction §13).

#### 5.5.3 Controls

| Action | Mouse + keyboard | Gamepad |
| --- | --- | --- |
| Stroke (Dig, Raise, Level, Smooth, Restore) | LMB; hold to repeat at the commit rate | RT; hold to repeat |
| Ramp points | LMB low end, LMB high end; Esc cancels the first point | RT, RT; B cancels |
| Sample and lock a height (Level, Raise, Dig) | Shift + LMB | LT |
| Clear the height lock | `L` | hold LT 0.5 s |
| Tool | `1`–`6`; `Q` / `E` previous / next | LB / RB |
| Size | `[` / `]` presets; Ctrl + wheel fine 0.5 m steps | D-pad left / right presets; hold + D-pad fine |
| Strength | `-` / `=` | D-pad down / up |
| Shape round / square | `T` | Y |
| Undo / redo | Ctrl + Z / Ctrl + Y | X / hold X 0.5 s |
| Leave the Terrain section | Esc or `B` | B |

Build mode's camera, movement, `E`/`F` and placement keys are untouched (`IMC_Build`); the plain wheel keeps its camera role. Every binding is remappable through the existing input settings. There is no console command and no typed command line (WorldEdit, Axiom): those tools exist for unlimited editors and would not survive the gamepad or the limits.

#### 5.5.4 Modifiers

- **Height lock** (Valheim Shift). Shift-click samples the ground under the cursor and pins the target: Level flattens to it, Raise never lifts above it, Dig never cuts below it. The chip reads "Lock 412.5 m (+1.5 m)" with the offset from the original ground at the cursor; a locked target outside the limits at the cursor shows amber and the stroke clips there. The chip also accepts a typed value: click it, type a signed offset in metres (−2.0 … +2.0 on open land, −8.0 … the band height in a claim), Enter; the field rounds to 0.5 m. The lock survives tool changes and clears with `L`, on leaving build mode, or when the player moves more than 24 m from the sample.
- **Grid snap** is always on: the brush centre snaps to the 0.5 m column, the square brush aligns to the 1 m claim grid when its size is a whole metre, heights to 0.5 m. There is no free-aim or off-grid mode (No Man's Sky, Astroneer); the server stores columns, so the preview must show columns.
- **Undo / redo.** Ctrl + Z submits the inverse of the last stroke within `undo_window_s`, up to `undo_depth` strokes back; Ctrl + Y re-submits it. Each is an ordinary commit: measured against the original surface, validated in T order, counted in the rate limit, and the budget counts the net. Undo is refused with "Nothing to undo" when the window has passed or another player's commit touched the columns; Restore covers everything older. The panel shows "Undo 3" with the count available. (Cities: Skylines 2 has no undo and every guide warns about it; Planet Coaster 2 batches eight edits per undo; Axiom keeps a history window.)
- **Repeat on hold.** Holding the stroke button repeats at the commit rate; the ghost shows the next stroke from the result of the last. A toggle in Accessibility makes a single press repeat until pressed again.
- **No symmetry, no stamps, no noise** (Axiom Distort/Roughen, World Painter shapes, Mountools): nothing in a 2 m, 100 m³ edit benefits, and they would not survive the exit rule.

#### 5.5.5 The Terrain section in the build menu

Left Build Catalogue panel, Terrain tab, top to bottom, panel width as `build-mode.png`:

1. **Tool row**: six 100 px catalogue tiles in two rows of three: Dig, Raise, Level / Smooth, Ramp, Restore (`slot_catalogue_*` from the kit; the selected tile carries the brass edge of `ui-hud.md` §3; key caps `1`–`6` bottom-right of each tile).
2. **Size row**: four 72 px tiles labelled "0.5 m", "2 m", "4 m", "7 m"; a fine size between presets shows on the nearest tile's label ("3.5 m") with no preset selected; `[` `]` key caps at the row ends.
3. **Shape and strength row**: a two-tab round / square toggle (`tab_*`) left; three tabs "½ m", "1 m", "2 m" right (Precise greys "1 m" and "2 m"; a strength above the remaining limit greys too and shows the reason on hover: "Only 0.5 m left before the 2 m limit").
4. **Modifier row**: the height-lock chip ("Lock off" / "Lock 412.5 m (+1.5 m)", `L` key cap) and the undo chip ("Undo 3 · Redo 1", Ctrl + Z / Ctrl + Y caps). Smooth replaces the lock chip with "Passes 2" (`-` / `=` adjust it while Smooth is selected); Ramp replaces it with "Max 10 m".

Bottom-right cost panel (reference "Construction Cost"), no material rows ever:

1. "Cut 36 m³ · Fill 0 m³" and "This stroke 36 / 100 m³".
2. "Open-land budget 620 / 1,000 m³" or, inside a claim, "Claim terrain 1,240 / 100,000 cells"; have/need style, crimson underline when over.
3. Tool line: Level "Target 412.5 m (+1.5 m)"; Ramp "Ramp 8.0 m · rise 2.0 m · 1:4 · 14°"; Restore "Restores 12 m³ · frees 12 m³"; Smooth "2 passes"; Dig/Raise "Step 1.0 m".
4. "Heals on 29 Oct unless claimed" on open land (TER-02 §6).
5. The depth/height gauge: 24 × 120 px at the panel's right edge, kit bar frame, moss fill to the stroke's deepest cut or highest fill against the active limit (2.0 m open land; the band distance inside a claim), amber past it, label "1.5 / 2.0 m".
6. Status line, one of: "Valid dig" / "Valid raise" / "Valid level" / "Valid smooth" / "Valid ramp" / "Valid restore"; "Clipped to 2 m"; "Clipped to 100 m³"; "Clipped to the lock"; "Pick the ramp's low end" / "Pick the high end"; "Ramp too long: 10 m per stroke"; "Nothing to restore here"; "Nothing to undo"; "Undone (2 left)"; or the first TER-02 §6 reject message. A refused stroke that Ramp or Smooth could fix appends "Try Ramp" or "Try Smooth".

#### 5.5.6 Result preview

While a tool is selected the player sees, before clicking:

1. **Surface ghost.** A translucent ghost of the final surface over the footprint plus a one-voxel ring (Ramp: the slope plus ring, at most 22 × 22 columns), a moss `#688267` surface at 30% with the 0.5 m sub-grid at 45%. Dig, Level-down and Restore-down sit below the ground with the removed volume as 3 cm ivory wire boxes per column, so the hole reads through the turf; Raise, Level-up and Restore-up rise above it. Columns the server would clip (depth, height, lock, 100 m³) draw amber `#B69A55`; columns it would refuse (buffers, under a capsule, protected, outside parcel or band, open-sky, exit) draw crimson `#A34842` with the §5.1 hatch. The outline carries the TER-02 §6 tint for the whole stroke; the per-column colours say where. Ramp shows a 3 cm ivory centre line from the first click to the cursor before the second.
2. **Readouts and gauge** of §5.5.5, from the same preview pass.
3. **Existing public edits** under the brush keep their TER-02 §8.5 healing tint (earth hatch, orange on the last day, striped when residual).

Everything is a client preview from the replicated voxel density and the zone masks; the server's prepare step (DEC-03 step 2) recomputes it and a mismatch returns the clipped or refused receipt, which refreshes the ghost. Clipping is computed client-side only; the server rejects, never clips, and the client submits the clipped stroke (TER-02 §7).

#### 5.5.7 How each tool meets the TER-02 limits

Per column, in T order; amber = clipped (the stroke still goes, at the clipped shape), red = refused (nothing goes) with the first failing code:

| Tool | Clip (amber) | Refuse (red) |
| --- | --- | --- |
| Dig | Stops at original − 2 m (open) or the band floor (claim); at the lock; at 100 m³ | `dig_buffer`, `dig_under_character`, `dig_claim_shell`, `dig_outside_band`; `dig_no_exit` when even the clipped cut has no 1 m walk-out ("Try Ramp"); `dig_overhang` on any undercut |
| Raise | Stops at original + 2 m or the band ceiling; at the lock; at 100 m³ | `dig_buffer`, T8 capsule, `dig_claim_shell`; `raise_no_step` when a raised edge exceeds 1 m against an unchanged neighbour ("Try Ramp"); `dig_overhang` if fill would float |
| Level | Target clipped into [original − 2, original + 2] per column, so a pad across a slope levels as far as the limit allows and shows amber where it could not reach; 100 m³ | As Dig for cut columns and Raise for fill columns; inside a buffer only fill ≤ 0.5 m is allowed, cut columns there are red |
| Smooth | Result clipped to the limits; inside a buffer the add is capped at 0.5 m and removal is dropped | Capsule, parcel and band codes only; Smooth can never create an exit or overhang fault because it only moves toward neighbours |
| Ramp | Each column clipped to the limits; a clipped ramp changes grade and the tool line turns amber ("1:4 → 1:6, clipped") | Any red column refuses the whole ramp; a ramp whose steps exceed 1 m per column (`ramp_max_m` too short for the rise) is `dig_no_exit` |
| Restore | Never clipped | `dig_under_character` when restoring a raise under a capsule; T8 when filling into one; T4 outside the actor's rights; nothing else, because the baseline is always legal |

Inside a claim the parcel rule holds for all six (TER-02 §2) and open-land tools ignore the grid's budget states (§5.4). The 1,000 m³ open-land budget counts cut and fill net of Restore, so a player can always undo their own work.

#### 5.5.8 Accessibility

- Never colour alone: amber columns carry a dotted outline, red ones the hatch, and the status line names the reason; the gauge has the number next to the bar (`ui-hud.md` colour-blind rule 3).
- Three distinct short sounds for pass, clipped and refused on hover change, and one on commit; off in the audio settings.
- Hold-to-repeat can be a toggle (§5.5.4); every key remaps; gamepad has full parity (§5.5.3) and the tiles are focusable with the stick.
- Reduce-motion setting: the ghost does not pulse and the wire boxes are static.
- The panel text follows the HUD scale; the height-lock field accepts typed values so a player who cannot aim precisely can still level to an exact height.
- A help line under the tool row on first use: "Hold to repeat · Shift-click locks a height · Ctrl+Z undoes" (dismissed per tool, remembered on the client).

#### 5.5.9 Dropped, and why

| Idea (source) | Why not |
| --- | --- |
| Free-aim spheres in the air, tunnels on open land (No Man's Sky, Astroneer, Space Engineers Voxel Hands) | Open-sky rule and the exit rule forbid overhangs and pits on open land; inside a claim Dig with a square brush carves rooms within the band. |
| Placed terrain shapes: cubes, ramps, capsules dropped as blocks (Enshrouded hammer terrain row, Space Engineers Box/Capsule) | A floating or unsupported fill is `dig_overhang`; Raise with the square brush and a height lock gives the same block on the ground. |
| Command line, masks, scripted brushes, heightmap stamps, history window (WorldEdit, Axiom) | Unlimited-editor tools; no gamepad path; nothing under a 2 m / 100 m³ limit needs them. |
| Hardness / falloff and continuous intensity (Planet Coaster, Cities: Skylines) | Unpredictable clipped outlines on a 0.5 m grid; three metre steps plus Smooth cover it. |
| Material cost per stroke (Valheim stone, Astroneer soil canisters, Enshrouded dirt) | Owner: altering land costs nothing and yields nothing. |
| Planet-curvature alignment (Astroneer Alignment Mod) | Flat world; the most complained-about tool in the survey. |
| Symmetry, stamps, noise, distort (Axiom, World Painter) | No use under the limits. |
| Surface paint: path, cobble, soil (Valheim Pathen/Paved, Enshrouded Dirt Road) | Not in J9. Material writes need TER-01's material byte and the ground material's blend; owner question 1 below. |
| Unbounded undo history (Axiom) | Server-authoritative commits; 60 s / 8 strokes of inverse commits plus Restore cover it without server history. |

### 5.6 Research: terrain editors in other games

Surveyed 2026-10-22; player opinions are from forum threads and are dated where the thread is.

| Game | Tools and modes | Brush | Controls, preview | Praise / complaints |
| --- | --- | --- | --- | --- |
| Valheim | Hoe: Level ground, Raise ground, Pathen, Paved road; pickaxe lowers; cultivator plants. Shift-click locks the hoe to the height under the cursor; otherwise it uses the player's feet. | One size; Level smooths toward the sample rather than cutting; raise costs stone. | No preview; each strike is a persistent modifier. | Level "doesn't do it all at once"; 100 modifiers per 10 × 10 m caused lag until the 0.150.3 terrain rewrite ([wiki](https://breezewiki.discard.no/valheim/wiki/Hoe), [guide](https://www.corrosionhour.com/how-does-the-hoe-work-in-valheim/), [PC Gamer](https://www.pcgamer.com/au/valheim-patch-overhauls-terrain-buffs-hoe), [BetterTerrain README](https://github.com/74oshua/BetterTerrain/blob/master/README.md)). |
| Enshrouded | Pickaxe digs; Rake levels toward the surroundings; the Construction Hammer's terrain row (hold Alt / Tab) places 1 / 2 / 4 m dirt cubes and other shapes; the hammer's remove action clears them. | Fixed shapes, three sizes. | Ghost of the shape. | Praised: fast fill-and-rake workflow; complaints: rake recipe bugs, shapes rather than brushes ([Steam](https://steamcommunity.com/app/1203620/discussions/0/4361247379738471743), [GamesFuze](https://gamesfuze.com/guides/enshrouded-how-to-fill-up-holes-in-the-ground/), [Gameluster](https://gameluster.com/enshrouded-basic-custom-base-building-guide/)). |
| Minecraft + WorldEdit | `/brush sphere`, `cylinder`, `smooth [radius] [iterations] [mask]`, bound to a held item. | Radius typed; hollow flag. | None; `//undo`. | Power-user; command syntax is the barrier ([docs](https://worldedit.readthedocs.io/en/latest/usage/tools/brushes/)). |
| Minecraft + Axiom | Editor mode (Right Shift): Elevation, Slope; Smooth, Distort, Roughen, Shatter, Extrude; Sculpt Draw, Melt, Stamp, Path; Floodfill. | Sphere / cube / octahedron; Radius slider, Ctrl + middle button. | Live preview; Ctrl + Z / Y with a History window. | Praised for live preview and history; mouse-and-keyboard only ([docs](https://axiomdocs.moulberry.com/tools/intro.html)). |
| No Man's Sky | Terrain Manipulator: Create, Subtract, Flatten, Restore; mode cycles on middle mouse. | Size steps. | Cursor sphere; no result ghost. | Edits revert when the save-wide edit cache overflows, especially near bases; the top complaint ([Steam](https://steamcommunity.com/app/275850/discussions/0/1762482479185377839), [TheGamer](https://www.thegamer.com/nms-guide-terrain-manipulator-blueprint-location/)). |
| Astroneer | Terrain Tool: dig, Raise (spends soil), Flatten (samples the grade under the centre dot); Alignment Mod locks the plane to gravity at the first click. | Size augments. | No ghost; continuous flow. | Flatten drifts over distance; the Alignment Mod is widely called a "lie" for bases ([wiki](https://astroneer.wiki.gg/wiki/Terrain_Tool), [Alignment Mod](https://astroneer.wiki.gg/wiki/Alignment_Mod), [Steam](https://steamcommunity.com/app/361420/discussions/0/2647504242058531931)). |
| Space Engineers / Medieval Engineers | SE Voxel Hands (creative): Box, Capsule, Ramp, Sphere, Terrain Auto Level; K opens size 1–100 per axis. ME survival: stakes and rope define a plane, the shovel cuts above and fills below it. | Shape sizes typed. | Shape ghost. | ME stakes called cumbersome; players asked for a plain flatten ([SE wiki](https://spaceengineers.wiki.gg/wiki/Voxel_Hands), [ME devblog](https://www.medievalengineers.com/devblog-002-shovel-tool-tools-update/), [feedback](https://support.keenswh.com/medievalengineers/topic/better-terraforming-street-building-tools-for-survival-mode)). |
| 7 Days to Die | Shovel and auger remove blocks; no flatten; players level by placing frames that conform to terrain. | Block. | None. | "Terrain smoothing" leaves uneven tops and spikes; auger noisy and gas-hungry ([wiki](https://wiki.7d2d.net/hosting/community/terrain-leveling-and-construction-techniques/), [Steam](https://steamcommunity.com/app/251570/discussions/0/558751364210721867)). |
| Vintage Story | Shovel digs soil blocks; placement is block-by-block; land claims by command. | Block. | None. | Claims are command-only; no terrain tool beyond the block ([wiki shovel](https://wiki.vintagestory.at/index.php/Shovel), [claims](https://wiki.vintagestory.at/Special:MyLanguage/List_of_server_commands/land)). |
| Landmark (EverQuest Next) | Add, Delete, Heal, Select, Smooth, Paint, Line; tools earned by crafting. | Scalable; copy/paste, undo. | Ghost. | Smooth would not bevel hard edges well; tool gating by crafting frustrated builders ([PC Gamer](https://pcgamer.com/everquest-next-landmark-hands-on), [Allakhazam](https://everquest.allakhazam.com/news/tags/MakingOurMark)). |
| Dual Universe | Nanoformer digs and contours; later a line/selection tool. | Shape. | Ghost. | Little documented; voxel precision for ships, not terrain ([wiki](https://dualuniverse.fandom.com/wiki/Voxel_Technology?oldid=970), [MassivelyOP](https://massivelyop.com/?p=162499)). |
| Satisfactory | No terrain editing by design; foundations instead. | — | — | Static hand-made world; players bury foundations ([Steam](https://steamcommunity.com/app/526870/discussions/0/5241649843389545572)). The owner's fallback if TER-01 fails. |
| Cities: Skylines 2 | Shift (raise LMB / lower RMB), Level (RMB sets the grade), Soften, Slope between a target height and the ground. | Three sizes, three strengths. | None, and no undo: every guide warns to start weak. | Level called the most useful tool; no undo the top complaint ([GGRecon](https://www.ggrecon.com/guides/cities-skylines-2-how-to-terraform), [Prima](https://primagames.com/tips/how-to-terraform-your-city-in-cities-skylines-2), [Steam](https://steamcommunity.com/app/949230/discussions/0/3877095833483358195)). |
| Planet Coaster 2 | Sculpt push/pull, flatten, smooth, paint; `[` `]` size, `-` `=` intensity. | Size to 60 m, intensity 0.1–1.0, no hardness slider; undo batches 8 edits. | Live. | "Invisible ceilings" on size and intensity; paint lost by sculpting ([wiki](https://planetcoaster.fandom.com/wiki/Terrain), [Nexus mod notes](https://www.nexusmods.com/planetcoaster2/mods/44)). |

What carried over: Valheim's Shift height lock and the level-to-sample; Cities: Skylines 2's four verbs and its right-click target; Astroneer's sample-under-the-dot flatten (without the curvature); No Man's Sky's Restore; Medieval Engineers' two-point plane as Ramp; Axiom's live preview and undo; Planet Coaster's `[` `]` `-` `=` keys; Enshrouded's fill-then-rake as Raise-then-Smooth.

## 6. Server authority and durable owner

- **Durable owner.** ClaimTerritory keeps claims; its record changes from centre + side to `cells` (a sparse set of 1 m cell keys with pinned band and price), `anchor_base_z`, `band`, `points_allocated`, plus the per-character `points_total` and `points_allocated` sums. `Reserve` becomes `Grow(op, claimId | new, cells[], expectedRevision)` and `Shrink`. Settings move from `ClaimSettings` to the `tune.building.claim.*` snapshot. `Check`, `Grant`, `Revoke` and `Recover` are unchanged; `Pay` becomes a storehouse deposit and the lifecycle follows §8 (offline-attributed ticks, `depleted_at`, stages).
- **One action.** Placement is one Building `cba_` action per DEC-02: Housing validates the piece (BLD-01 geometry, support, reach), asks ClaimTerritory to grow with the exact cells, debits materials through Inventory, adopts public terrain edits (World), records the immutable decision, then activates. A denial consumes nothing. The client sends the piece and cell list; the server recomputes band, price, cap, blocked mask and engulfment from its own zone record and never trusts the client's grid.
- **Replication.** The claim's cell mask and bounding box replicate on `ASFConstructionNetworkState` next to `AnchorCell`; remaining points replicate on the player state. The blocked mask and band map are derived on both sides from the zone record shipped with the content version.
- **Anti-abuse.**
  - Squatting near town: the near cap (400 m²), one near-band claim per character (`max_near_claims` = 1, bounds 1–3), the engulf rule and the 24 h empty-claim archive. Foundations cost real materials (DEC-02), so a ring of empty slabs is not free, and the slab's own reach costs points.
  - Griefing: road, spawn, protected-area and border clearances; no claim may touch another's spacing; a claim cannot enclose another claim or a corridor; digging under a claim is TER-02's job.
  - Point farming: points are allocated, never minted by release; refunds of points and materials are the same receipt; a repeated operation id returns the original receipt.
  - Rate: the existing two commits per second with a burst of four; the engulf flood fill is bounded to 200 × 200 cells and runs only when a growth touches the ring 5 m around another claim.

## 7. Acceptance tests

1. **Band pricing.** Cells at 100 m, 300 m and 700 m from the Silverbrook wall cost 2.0, 1.0 and 0.25 pt/m²; the boundary cells at exactly `near_m` and `middle_m` land in the nearer band.
2. **Growth and reach.** A 4 × 4 m foundation claims a 12 × 12 m square (144 cells); a wall placed at its edge adds only the new ring cells; a placement 5 m beyond the edge is refused `outside_reach`.
3. **Budget and cap.** With 1,200 pt, a near-band claim stops at 400 m² with `claim_cap`; a wilderness claim stops at 4,800 m² with `claim_points`; neither debits materials.
4. **Shrink.** Removing the only wall at the edge releases exactly the cells beyond reach of the slab, refunds their points, and their adopted edits reappear on the healing list with a fresh 7-day deadline.
5. **Blocked.** Cells within 6 m of a road, 25 m of a respawn, 5 m of another claim, 16 m of the zone edge, inside the wall, or over 35° are `claim_blocked` with that reason, and the grid state mask marks them 3 (red) on the client.
6. **Engulf.** Growing a ring around another claim fails at the cell that would close it; the other claim's ground path is preserved.
7. **Two claims.** A foundation 20 m from an own claim starts claim 2; a third 20 m away starts claim 3; a fourth is refused `claim_slots`; a foundation 3 m from claim 1 extends claim 1.
8. **Restart.** As in `tools/Test-ClaimHousing.ps1`: after a server restart the cell mask, points and anchor restore; a mask whose cells disagree with the structures is a hard reject (`CONSTRUCTION_NETWORK_RESTORE_REJECTED`).
9. **Empty archive.** A claim with every piece removed archives after 24 h and its points return; placing again inside 24 h keeps it.
10. **Levelled pad adoption.** A 2 m public cut of 200 m³ is levelled without a claim; a foundation on it claims the pad cells at the band price with no extra points, the 200 m³ leaves the player's 1,000 m³ public allowance and the edits leave the healing list; removing the foundation puts them back with a fresh 7-day deadline.
11. **Grid visual.** Fresh Fable review of `build-grid-<view>-compare.png` against the reference and `build-mode.png` per `docs/visual-review.md`: owned ivory, affordable moss, limit fade over 3 m, crimson hatch by the road, no grid past the limit.
12. **Terrain preview.** Captures per `docs/blueprints/building-mode.md` §C: each brush size on flat ground, each of the six tools at Medium, the Medium brush on a slope, at a road buffer and at the claim edge; the ghost, readout and gauge match the Codex concept and the preview equals the server result (cut volume, clipped columns, refused columns) within one voxel.
13. **Upkeep charge and pause.** With `enabled`, a claim listing 320 timber and 80 ingot at 1% debits 3.2 timber and 0.8 ingot per offline day from the storehouse, nothing while the owner is logged in, and whole units only once the fractions reach one.
14. **Depletion stages.** An empty storehouse pins `depleted_at`; at offline day 7 condition starts falling 1 per day with a floor of 1 and no collapse; at day 30 the final notice is sent; at day 37 the claim archives, its stock, structures and edits land in the recovery store, and its cells and points are released. A deposit on day 20 returns it to Provisioned and resumes growth.
15. **Storehouse rules.** A second storehouse is refused; removing a stocked one is refused; a visitor can deposit and cannot withdraw; a non-listed item is refused. With `enabled` false none of 13–14 happens and the plaque reads "Provisioned".
16. **Level.** On a 20° slope with Medium, the first click samples the ground under the cursor and the pad levels to it, amber where the 2 m limit stops it; Shift-click then a typed +1.0 lock levels to original + 1.0 m at the sample column; the result equals the server receipt within one voxel.
17. **Smooth.** A 2 m stepped cut smoothed with 2 passes has no step over 1.0 m along its edge and passes the exit rule; Smooth 3 m from the road adds at most 0.5 m and removes nothing inside the buffer.
18. **Ramp.** Low end on a 2 m pad floor, high end on original ground 8 m away → accepted, tool line "Ramp 8.0 m · rise 2.0 m · 1:4 · 14°"; 12 m requested → "Ramp too long: 10 m per stroke"; a 2 m rise over 1.5 m → `dig_no_exit`.
19. **Restore.** After a 25 m³ cut, Restore over it returns every column to base density and material byte-for-byte, the open-land budget drops by 25 m³ and no item is produced; Restore of a raise under a standing player → `dig_under_character`.
20. **Undo.** Ctrl + Z within 60 s inverts the last stroke and the budget shows the net; a ninth undo, or one after 60 s, reads "Nothing to undo"; Ctrl + Y re-applies; another player's commit on the same columns refuses the undo with a fresh quote.
21. **Controls and accessibility.** Every §5.5.3 binding fires its action on keyboard and on gamepad (`tools/Test-BuildModeInput.ps1`-style injection, `INPUT_ACTION` log lines); in the colour-blind capture amber columns keep the dotted outline and red ones the hatch; the hold-to-repeat toggle repeats a Dig stroke at the commit rate without the button held.

## 8. Upkeep: the claim storehouse

Owner, 2026-10-22: "nothing during alpha, but planned upkeep later. a special container can be used to keep materials in that would prevent degrading after the defined time. materials required will be based on materials used for that specific claim. if you are online, it doesnt degrade. offline degrades and reduced stored materials. once materials are depleted, 30 days."

### 8.1 The storehouse

- **What it is.** A placeable functional object, one per claim, that replaces the old claim anchor's maintenance reserve. Recipe content default: 30 rough timber, 10 iron ingot (Crafting owns the definition). It occupies 2 × 2 × 2 BLD-01 cells (1 × 1 × 1 m), origin bottom centre, and must stand inside the claim; it is the only container that upkeep draws from. A second storehouse on the same claim is refused (`storehouse_exists`); removing a stocked storehouse is refused until it is emptied (`storehouse_not_empty`); archive moves its stock to the recovery store with everything else.
- **Look.** An oak strongbox with iron straps, a hinged lid and a brass plaque on the front bearing the deer crest and the claim's state text (§8.5); weathered-brown plank material and forged iron from the Silverbrook kit, built like any prop from a Codex sheet (`docs/blueprints/props-and-dressing.md` §A).
- **Capacity.** `storehouse_slots` = 24 stacks; it accepts only materials that appear on the claim's upkeep list (§8.2) and refuses everything else with "The storehouse keeps only this claim's building materials." Stack sizes are Itemization's.
- **Who may deposit and withdraw.** Deposit: anyone with Visitor rights or above (the existing "Deposit maintenance materials: Yes" row of [Building and Territory] §3), so friends can keep a claim provisioned. Withdraw: the owner and Stewards. Upkeep debits need no rights. Container grants (`UseContainers`) do not apply to it.

### 8.2 The charge

The claim's **upkeep list** is the sum of the recipe material bundles of every structural piece and functional object placed in it, from the same Placement receipts that priced them; decorative furnishings and the storehouse itself are excluded (existing §4 rule). The daily charge per material is `list quantity × upkeep_rate_pct_per_day / 100`. Example: a house of 40 walls at 8 rough timber and 2 iron ingot each lists 320 timber and 80 ingot; at 1% per day that is 3.2 timber and 0.8 ingot per offline day, so 30 offline days need 96 timber and 24 ingot. The §4 rounding rule stays: fractions accumulate per material and are debited whole, so splitting pieces cannot lower the charge; quantities come from authored recipes, never market prices.

### 8.3 Online and offline time

- An owner's claims are **paused** while the owner is logged in: no charge, no countdown, no decay. The server attributes each hourly tick's seconds to online or offline from the character's session records; the charge for the tick is the daily charge × offline seconds / 86,400. Logging in for a minute pauses only that minute.
- While offline, each tick debits every material on the list from the storehouse. While every material is covered the claim is **Provisioned** and nothing changes in the world.
- The first tick in which any required material is short pins `depleted_at` and the claim becomes **Depleted**; the tick takes no partial payment for that material (existing rule). All later deadlines count **offline days** from `depleted_at`.

### 8.4 Stages after depletion

| Offline days since `depleted_at` | Stage (ClaimTerritory state) | What happens |
| ---: | --- | --- |
| 0–`depleted_grace_days` (7) | Grace | Intact. Growth and new placement paused (`claim_unprovisioned`); repair, removal, doors, containers and automation work. |
| 7–`depleted_days` (30) | Delinquent | Condition of every listed piece falls `decay_condition_per_day` (1) per offline day, prorated per tick, floor 1; never collapse (existing neglect rule). Shelter and devices may fail with the visible cause. |
| 30–30 + `final_notice_days` (7) | Notice | Final notice. Decay continues. |
| 37 | Archived | Existing archive: terrain delta, structures, contents and storehouse stock to the owner's recovery store; land, cells and points released; adopted edits archived, then the baseline restored safely. |

Restocking the storehouse so that every material is covered at the next tick returns the claim to Provisioned at any stage before archive, clears `depleted_at` and resumes growth; damaged condition stays until repaired. Payment of the old `Pay` RPC becomes a deposit.

| Key (`tune.building.upkeep.*`) | Unit | Default | Safe bounds | Step | Effective |
| --- | ---: | ---: | ---: | ---: | --- |
| `enabled` | bool | false (alpha) | — | — | Next hourly tick; switching on gives every existing claim `switch_on_provision_days` of free cover |
| `upkeep_rate_pct_per_day` | % of the upkeep list per day | 1.0 | 0–5 | 0.25 | Next tick; 72-hour notice for increases (existing §7 rule) |
| `depleted_grace_days` | offline days | 7 | 0–30 | 1 | New depletion episodes; existing deadlines stay pinned |
| `depleted_days` | offline days | 30 | 7–90, ≥ grace | 1 | New depletion episodes |
| `final_notice_days` | offline days | 7 | 7–30 | 1 | New depletion episodes |
| `decay_condition_per_day` | condition/offline day | 1 | 0–5 | 1 | Next tick |
| `storehouse_slots` | stacks | 24 | 8–64 | 4 | New storehouses |
| `warn_days` | days of cover | 7 | 1–30 | 1 | Next login |
| `switch_on_provision_days` | days | 30 | 0–90 | 1 | At switch-on only |

Realm-scoped, economy tier 3, declared through DOC-05; these replace ClaimTerritory's `GraceDays` / `ReclamationDays` / `FinalNoticeDays` and the existing `tune.building.*` keys of the same names. The charge algorithm, online attribution, stage order, floor of 1 and archive-before-release stay release-bound code.

### 8.5 What other players see

- The storehouse plaque reads the claim's state: "Provisioned", "Provisioned · 12 days of cover", "Depleted · day 9 of 30", "Final notice · 5 days". Anyone can read it and deposit.
- Decayed pieces show Construction's condition weathering; nothing collapses and nobody can enter or take anything that the permissions did not already allow. A Notice-stage claim shows a "lapsing" marker on the world map and in the build grid's status line when another player's grid touches its spacing ring ("Someone's lapsing claim"), so neighbours know the land may free up; the cells stay red until the archive actually releases them.
- Nothing is visible on open land after archive except the restored baseline.

### 8.6 Notifications

Claim panel plus the account notification inbox (existing §2.2 channels, no external messaging): on login when any claim has less than `warn_days` of cover ("Riverside claim: 5 days of upkeep left"); at depletion; at the end of grace (decay starting); at the final notice; at archive, with the recovery-store contents. Each is sent once per episode.

### 8.7 Healing, adoption and alpha

- Adopted terrain edits stay claimed through every stage; healing never touches a live claim. Growth is paused from depletion, so no new adoption happens until the claim is Provisioned again. Archive archives the edits before restoring the baseline, as DEC-03 and TER-02 already require, and never entombs or overwrites a surviving structure.
- Points stay allocated until archive; release is the same receipt as the land release.
- In alpha `enabled` is false: no charge, no stages, no decay, claims last until the test wipe. The storehouse can still be crafted, placed and filled so the UI and deposit permissions get tested. When the flag turns on, every existing claim starts Provisioned with `switch_on_provision_days` of free cover, and the login notice tells the owner to stock the storehouse.

## 9. Jobs, in order

| Job | Size | Scope |
| --- | --- | --- |
| J1 Typed config and band map | S | `tune.building.claim.*` declarations; band distance from zone settlement polygons; road/spawn/border clearance masks from the zone record and town layout; a `tools/claim_bands.py` that prints a cell's band for a coordinate |
| J2 ClaimTerritory cells and points | M | Replace centre + side with the cell set, pinned prices, `Grow`/`Shrink`, points ledger, caps, engulf check, empty-claim archive; `ClaimTerritory.Tests` for tests 1–7 and 9 |
| J3 Housing and Placement action | M | `cba_` growth in `HousingGrpcService`/`PlacementService`: cell list in the place RPC, adoption hook, refusal reasons, `ListStructures` restore with the mask; test 8 |
| J4 Client claim mask and gate | M | `SFConstructionNetwork` replicates the mask and box; `Contains` uses the mask; previews refuse outside reach; the BLD-02 plots become ordinary ground when `alpha_fixed_points > 0` |
| J5 Grid decal | M | `MI_ClaimGrid`, state-mask build, decal lifetime with build mode, HUD cost row; blueprint `docs/blueprints/building-mode.md`; capture and Fable review (test 11) |
| J6 TER-02 hook | S | `anchor_base_z`, band height and footprint cells feed TER-02's parcel/band checks |
| J7 Two-client runner | M | `tools/Test-ClaimGrid.ps1` on owned PostgreSQL covering tests 2–9 |
| J8 Dependency map, Building and Territory v1.1 note | S | Record §2's changes in the design doc's §2.1 and the map |
| J9a Terrain tools and result preview | M | Dig / Raise / Level / Smooth / Ramp / Restore tiles, size / shape / strength rows, lock and undo chips (§5.5.5), `UDynamicMeshComponent` ghost with `MI_TerrainGhost`, readouts, gauge and status from one preview pass, per-tool clip/refuse of §5.5.7; Codex concept and Fable review (tests 12, 16–19); needs TER-01/DEC-03 digging to exist on the server |
| J9b Terrain input and accessibility | S | §5.5.3 bindings in `IMC_Build` for keyboard and gamepad, remap, hold-to-repeat toggle, the three sounds, reduce-motion, help line; test 21 |
| J9c Undo and Restore | S | Inverse-commit undo under `undo_window_s` / `undo_depth`, Restore tool and budget release, "Nothing to undo" states; tests 19–20; after J9a |
| J10 Upkeep and storehouse | M | Storehouse object and Codex sheet, upkeep list from Placement receipts, hourly offline-attributed charge, stages, notifications, plaque, map marker, `enabled` flag and switch-on cover; `ClaimTerritory.Tests` for tests 13–15; after J2 and only when `enabled` is planned for a release |

## Owner answers and open questions

Answered 2026-10-22 (recorded in [OWNER-2026-10-22-decisions.md](OWNER-2026-10-22-decisions.md)):

1. Claim points: fixed 1,200 in alpha, then 400 + 40 per level (§3.1).
2. Three claims per character, sharing one budget (§3.1, §4).
3. Largest wilderness claim about 60 × 60 m: `wild_cost` 0.75 pt/m² and `wild_cap_m2` 3,600 m² (§3.2).
4. Upkeep: nothing in alpha, then the claim storehouse, charges from the materials used in that claim, no decay while online, a 30-day countdown once depleted (§8).
5. Grid colours: option (a), ivory owned / moss affordable / fade at the limit / crimson hatch blocked (§5.1).
6. Open-land placement: anything placed needs a claim (§4 Start).
7. Terrain edits: Dig and Raise Ground cost nothing and yield nothing; the readout shows volume and remaining budget only (§2, §5.4, §5.5).

Open after the terrain-tool revision of §5.5 (2026-10-22):

1. **Should players be able to paint the ground surface (dirt path, cobbles, tilled soil) as a seventh Terrain tool?** Options: (a) not now, revisit after TER-01 proves the material byte and the ground material's blend; (b) ship one "Path" material in J9a so roads between claims look made; (c) a full material set in a later packet. Recommendation: (a); nothing else in J9 depends on it, and a half-working paint tool is the most visible failure in the survey (Planet Coaster loses paint when you sculpt).
2. **How should undo work?** Options: (a) 60 s / 8 strokes of inverse commits on the client plus the Restore tool for anything older; (b) no undo, Restore only; (c) server-side per-player history. Recommendation: (a); Cities: Skylines 2's missing undo is the top complaint found, and (c) adds server state for no player benefit over (a) + Restore.
3. **How fast may strokes repeat while the button is held?** Options: (a) the existing terrain rate, one commit per 1.5 s; (b) the placement rate, two per second with a burst of four, still under the 2,000 m³ per hour cap; (c) batch the held strokes into one commit per second. Recommendation: (b); at (a) levelling a 7 m pad in 1 m steps feels like Valheim's "pad the same spot several times".
4. **Where does Level take its target height from?** Options: (a) the ground under the cursor at the first click, with Shift-click or a typed value to lock another; (b) always the player's feet, Valheim style, unless locked; (c) lock required before Level works. Recommendation: (a); it matches Cities: Skylines 2 and Astroneer and needs no extra step for the common pad.
5. **Largest brush?** Options: (a) keep 7 m; (b) allow 10 m (World's 5 m radius ceiling), which at a full 2 m cut is 157 m³ and is always clipped to the 100 m³ commit cap; (c) raise the commit cap to 160 m³ for Large. Recommendation: (a); a brush that is always clipped teaches players to distrust the ghost.

[Building and Territory]: ../Game-Design/Legends_Shattered_Frontier_Building_and_Territory_System_v1.0.md
[Construction]: ../Game-Design/Legends_Shattered_Frontier_Construction_System_Architecture_v0.1.md
