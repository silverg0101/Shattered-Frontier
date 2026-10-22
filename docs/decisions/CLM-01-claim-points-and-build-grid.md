# CLM-01 — Claim points and the build grid

Status: proposed, 2026-10-22, from the owner's idea in the Task Runner session. Extends [Building and Territory] §2 and [BLD-02](BLD-02-terrain-anchored-construction.md); the digging half is [TER-02](TER-02-digging-limits.md). Nothing here is implemented.

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

Unchanged: 5 m clear spacing between unrelated claims, the protected town plus 5 m clearance, first-winner reservation, upkeep/grace/delinquent/notice/archive lifecycle (§2.2, §4), permissions and presets (§3), the §15 cell/object ceilings, 7-day public healing and claim adoption (DEC-03, TER-02), siege rules (personal claims are never capturable), the `cba_` action protocol (DEC-02).

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
| Wilderness | > `middle_m` | `wild_cost` 0.25 pt/m² | none below the §15 ceilings | 60 m (the existing guild band) | Castle, mine, tower |

Worked examples at the shipped progression: a new character (400 pt) buys 200 m² near town (a 10 × 10 house with a 2 m yard) or 1,600 m² in the wilderness. At level 50 (2,400 pt) the wilderness buys 9,600 m² (about 98 × 98 m, castle scale) and the near band is still capped at 400 m². Alpha (1,200 pt) reaches 4,800 m² in the wilderness, 1,200 m² in the middle band and the near cap.

| Key | Unit | Default | Safe bounds | Step | Effective |
| --- | ---: | ---: | ---: | ---: | --- |
| `near_m` / `middle_m` | m | 150 / 500 | 50–400 / 200–1,500, `near_m < middle_m` | 25 | New cells |
| `near_cost` / `middle_cost` / `wild_cost` | pt/m² | 2.0 / 1.0 / 0.25 | 0.1–10, non-increasing with distance | 0.05 | New cells |
| `near_cap_m2` / `middle_cap_m2` | m² | 400 / 1,600 | 100–1,600 / 400–10,000, `near ≤ middle` | 100 | New cells |
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

- **Start.** A claim begins with the first foundation placed on unblocked, unclaimed ground while the character has a free claim slot. The foundation's base level becomes `anchor_base_z`; its band is read once. The BLD-02 integer anchor is the foundation's minimum cell.
- **Grow.** Every placed piece claims the 1 m cells under its footprint plus every cell within `reach_m` (4 m) of that footprint, at the cell's band price, skipping cells already owned. A piece may only be placed inside the claim or inside that reach ring of the claim. Cells that cannot be claimed (blocked, over budget, over cap, outside the band) refuse the placement with the first failing reason; the grid already showed them red or faded.
- **Cap.** Near and middle bands cap a claim's total footprint; the wilderness is bounded only by points and the §15 ceilings (50,000 structural cells, 100,000 altered terrain cells). The cell under a piece is always part of the footprint, so a cap also caps the building.
- **Shrink.** Removing a piece releases every cell that is no longer within `reach_m` of any remaining piece, returns its points, and hands its committed terrain edits back to the public 7-day healing clock, starting at the release. A removal that would strand another piece's support follows Construction §3.4. A claim may become two parts after removals; it stays one record (one id, one band, one anchor).
- **Empty claim.** A claim whose last piece is removed archives after `empty_claim_hours` = 24 h (bounds 1–168) through the ordinary archive path, releasing its land and points. Until then the owner can place again on it.
- **Multiple claims.** A foundation placed outside every own claim's reach starts a new claim if a slot is free (`max_claims`); inside the reach of an own claim it extends it. Two own claims never overlap; one placement within reach of two own claims extends the nearer and the other is unchanged.
- **Leaving and inactivity.** Unchanged from §2.2 and §4: funded reserve, 7-day grace, delinquent at 30 days, 7-day notice, archive to the owner's recovery store, then release. Points return at archive. The empty-land minimum upkeep (1 worked timber + 1 stone per day) applies per claim. Alpha publishes upkeep at 0% (owner question 3); the lifecycle code ships either way.
- **Healing and adoption.** Each growth adopts committed public edits under the new cells at the same `cba_` action (DEC-03 "legal claim creation atomically adopts", now per growth), with the point and allowance accounting of §3.3. Public edits outside the footprint keep their own 7-day deadline.

## 5. The build grid

### 5.1 What it shows

Entering build mode (B) projects a 1 m grid on the ground within `grid_radius_m` = 24 m of the player. Each cell has one of five states, computed on the client from replicated claim cells, the remaining budget, the band map and the blocked mask, and confirmed by the server on every placement:

| State | When | Look (v5 palette, `docs/blueprints/ui-hud.md` §3) |
| --- | --- | --- |
| Owned | cell in one of my claims | ivory `#E8E1CE` lines at 35% alpha, 3 cm wide; 4 m major lines at 5 cm, 50% |
| Affordable | cell within reach of my claim (or, with no claim yet, within the disc the budget buys around the player, radius `sqrt(points / (π × cost))`), and claiming it fits points and cap | moss `#688267` lines 55%, 3 cm, plus 8% moss fill |
| Limit | the first `fade_m` = 3 m of affordable cells past the budget or cap | same lines, alpha falls linearly to 0 over 3 m; the last affordable cell carries a 6 cm ivory rim |
| Blocked | §3.4 reasons | crimson `#A34842` fill 25% with a 45° hatch at 0.25 m pitch, lines 60%; never colour alone: the status line names the reason ("Too close to the road") |
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

Owner, 2026-10-22: Dig and Raise Ground are two actions in a **Terrain** section of the build catalogue, beside Foundation, Wall, Door and Roof (the v5 `build-mode.png` already shows the Terrain tab), not a held tool. Selecting one shows the TER-02 brush instead of a piece ghost.

- **Same limits for both.** Raising follows exactly the digging limits: 2 m above the original ground on open land (`public_raise_m`), the claim band inside a claim (to `anchor_base_z + band_height_m`, within T7's 12 m), the 3 m buffers, the shared 1,000 m³ open-land budget per player (one counter for cut and fill, 8,000 cells), and 7-day healing of public edits.
- **Shared grid and preview.** Both actions draw the same claim grid of §5.1 and tint the brush ghost green / amber / red per TER-02 §6: green when the whole brush passes, amber when the server would clip it to the legal depth, height or shape (the tool offers the clipped brush), red with the first failing reason. Blocked grid cells (§3.4) are red for terrain actions too; a terrain action on open land ignores the budget states (it claims nothing), so owned, affordable and limit cells draw at their normal alpha but the ghost tint alone decides validity.
- **Points.** Terrain actions never grow or shrink a claim. Inside a claim the brush must lie within claimed cells (TER-02 parcel rule); on open land it needs no claim and no points. Claiming later adopts the edits per §3.3.
- **Cost panel.** The materials rows are replaced by "Volume  36 / 100 m³" (brush changed volume against the 100 m³ per-commit cap) and, on open land, "Open-land budget  620 / 1,000 m³"; both in the have/need style with the crimson underline when over.

**TER-02 wording that needs a one-line update** so that raising is covered explicitly (TER-02 owns these; CLM-01 only lists them):

1. §1 "every cut must be walkable out of": add "and every raised edge must be walkable up to (same 1.0 m step), so a 2 m fill cannot wall off a road or a neighbour".
2. §3 exit rule: state that it runs on raised columns too (a column whose final surface is above its original surface must connect through steps of at most `public_exit_step_m` to an unchanged column).
3. §5 `public_outstanding_cells`: default 8,000 (= 1,000 m³ per the owner's answer), and note that cut and fill share it.
4. §6 reject table: add `raise_too_high` — "Too high for open land. You can raise the ground 2 m above the original here. Claim this land to build higher." and `raise_no_step` — "This would leave a wall you can't climb. Slope one side so it can be walked up."
5. §2 "Your own claim" row: "To the band ceiling" should name `anchor_base_z + band_height_m` from CLM-01 §3.3.

### 5.5 Terrain brushes and the result preview

Owner, 2026-10-22: the Terrain section "needs a visual guide on how it's going to affect the ground too, with different size tools to use."

**Brush sizes.** Four sizes on the 0.5 m voxel grid, each a round or square footprint, for the three Terrain actions Dig, Raise Ground and Level (World's dig / raise / flatten kinds; Level sets every column in the brush to the height under the cursor at the first click). Depth per stroke is the action's step; a stroke never exceeds the band or open-land limit, and the server clips it (amber) rather than refusing when part of it is legal.

| Tile | Shape | Size (footprint) | Depth or height per stroke | Full 2 m cut volume | Use |
| --- | --- | ---: | ---: | ---: | --- |
| Precise | square | 0.5 m (one voxel column) | 0.5 m | 0.1 m³ | Trim a step, fix a seam |
| Small | round or square | 2 m across (`small_m` = 1.0 radius) | 0.5 m | 6 m³ | Ditch, post hole, path edge |
| Medium | round or square | 4 m across (`medium_m` = 2.0 radius) | 1.0 m | 25 m³ | House pad, ramp |
| Large | round or square | 7 m across (`large_m` = 3.5 radius) | 1.0 m | 77 m³ | Whole pad, terrace; stays under the 100 m³ per-commit cap at a full 2 m cut |

| Key (`tune.world.brush.*`) | Unit | Default | Safe bounds | Step | Effective |
| --- | ---: | ---: | ---: | ---: | --- |
| `precise_m` / `small_m` / `medium_m` / `large_m` (radius; square uses it as half-side) | m | 0.25 / 1.0 / 2.0 / 3.5 | 0.25–5.0 (World's 5 m ceiling), non-decreasing | 0.25 | Next brush selection |
| `step_m` precise / small / medium / large | m | 0.5 / 0.5 / 1.0 / 1.0 | 0.5–2.0 | 0.5 | Next stroke |
| `preview_hz` | Hz | 20 | 5–60 | 5 | Client only |

World's 100 m³ per-commit cap, 0.5–5 m brush bounds, rate keys and TER-02's limits are unchanged and bind every size; a size whose stroke would exceed 100 m³ on the current ground is offered clipped, as Construction §13 already says.

**Switching sizes.** `[` and `]` step the size down and up; `Ctrl` + mouse wheel does the same (the plain wheel keeps its camera role); `T` toggles round/square; `1`–`4` while the Terrain section is open pick a tile directly. The Terrain section shows the three actions as 100 px catalogue tiles (Dig, Raise Ground, Level) and, below them, four 72 px size tiles labelled with their metre size ("0.5 m", "2 m", "4 m", "7 m") and a two-tab round/square toggle; the selected action and size tiles carry the brass edge of `ui-hud.md` §3, and the key caps `[` `]` sit at the row ends in the prompt-bar style.

**Result preview before commit.** While a Terrain action is selected the player sees, before clicking:

1. **Surface ghost.** A translucent ghost of the final ground surface over the brush footprint plus a one-voxel ring (at most 16 × 16 columns for Large), drawn as a moss `#688267` surface at 30% with the same 0.5 m sub-grid lines as the claim grid at 45%; for Dig the ghost sits below the current ground and the removed volume shows as a 3 cm ivory wire box per column so the hole reads through the turf; for Raise and Level the ghost rises above the ground. Columns the server would clip (depth, height, 100 m³, exit step) draw amber `#B69A55`; columns it would refuse (buffers, under a character, protected, outside the claim parcel or band) draw crimson `#A34842` with the §5.1 hatch. The ghost tint of TER-02 §6 (green / amber / red) applies to the whole brush outline; the per-column colours say where.
2. **Cut/fill readout** in the cost panel, replacing the material rows: "Cut 36 m³ · Fill 0 m³", "This stroke 36 / 100 m³", and on open land "Open-land budget 620 / 1,000 m³" (inside a claim: "Claim terrain 1,240 / 100,000 cells"), all in the have/need style with the crimson underline when over.
3. **Depth/height gauge.** A vertical 24 × 120 px gauge at the panel's right edge, bar frame from the kit, filled moss up to the stroke's deepest cut or highest fill against the active limit: 2.0 m on open land, the band distance inside a claim ("1.5 / 2.0 m"); the part past the limit fills amber and the label turns amber, matching the clipped columns.
4. **Status line.** "Valid dig", "Clipped to 2 m" or the first TER-02 reject message, under the gauge.

Everything is a client preview from the replicated voxel density and the zone masks; the server's prepare step (DEC-03 step 2) recomputes it and a mismatch returns the clipped or refused receipt, which refreshes the ghost.

**Implementation.** Three options for the surface ghost:

| Option | Cost | Fit |
| --- | --- | --- |
| **Dynamic mesh ghost**: one `UDynamicMeshComponent` on the controller, vertices on the 0.5 m columns of the footprint + ring (≤ 17 × 17 = 289 vertices, 512 triangles), heights from the client's voxel replica (or a line trace per column until the voxel replica ships), rebuilt at `preview_hz` only when the cursor cell, size, shape or action changes; per-vertex colour carries the state | ~0.05–0.1 ms rebuild, 289 traces at most 20 times a second on the trace fallback; one translucent draw | Shows a real new surface above or below the ground, which a decal cannot; works on mesh, Landscape and voxel ground |
| Decal | as §5.3 | Can only paint the footprint on the existing surface; no new height, no hole |
| Material on a pre-made preview mesh (one static disc/box per size) | Cheapest draw | Cannot follow a slope or show a clipped shape; wrong on every uneven pad |

**Pick the dynamic mesh ghost**, with `MI_TerrainGhost` (translucent, vertex-colour driven, the §5.3 line parameters, depth-test on so the ground occludes the underside of a dig ghost except for the wire boxes, which are a second `MI_TerrainGhostWire` section with depth test off). The claim grid decal of §5.3 stays underneath. Recipe: `docs/blueprints/building-mode.md` §C; the concept for the Fable review is generated by Codex from the prompt there.

## 6. Server authority and durable owner

- **Durable owner.** ClaimTerritory keeps claims; its record changes from centre + side to `cells` (a sparse set of 1 m cell keys with pinned band and price), `anchor_base_z`, `band`, `points_allocated`, plus the per-character `points_total` and `points_allocated` sums. `Reserve` becomes `Grow(op, claimId | new, cells[], expectedRevision)` and `Shrink`. Settings move from `ClaimSettings` to the `tune.building.claim.*` snapshot. `Check`, `Grant`, `Revoke`, `Pay`, `Recover` and the lifecycle are unchanged.
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
12. **Terrain preview.** Captures per `docs/blueprints/building-mode.md` §C: each brush size on flat ground, the Medium brush on a slope, at a road buffer and at the claim edge; the ghost, readout and gauge match the Codex concept and the preview equals the server result (cut volume, clipped columns, refused columns) within one voxel.

## 8. Jobs, in order

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
| J9 Terrain section and result preview | M | Dig / Raise Ground / Level tiles and size tiles in the catalogue, `[`/`]`/`Ctrl`+wheel/`T` input, `UDynamicMeshComponent` ghost with `MI_TerrainGhost`, cut/fill readout, depth gauge, Codex concept and Fable review (test 12); needs TER-01/DEC-03 digging to exist on the server |

## Owner questions

1. **How should claim points grow?**
   - a) Fixed for everyone in alpha (1,200), and later tied to character level (400 at start, +40 per level). **Recommended**: testers can try every band now, and later players earn bigger land by playing.
   - b) Everyone always has the same fixed budget. Simple, but a new player and a veteran get the same land.
   - c) Points come only from quests and deeds. Strong story hook, but a player who skips quests never gets a home.

2. **How many separate claims can one character hold?**
   - a) Three, sharing one budget (a town house, a farm and a far keep). **Recommended.**
   - b) One. Simplest, but a castle in the wilderness means giving up the town house.
   - c) As many as the budget allows. Risks scattered single-slab claims blocking land.

3. **What happens to a claim while a player is away?**
   - a) In alpha, nothing: claims last until the test wipe, and the designed upkeep (materials per day, 7-day grace, 30-day reclaim) switches on later. **Recommended** for a small invited group.
   - b) Upkeep from day one, as the design says. Realistic, but testers who miss a week lose the build they came back to check.
   - c) No upkeep ever; a claim is archived after 30 days without a login. Easier to understand, but it has no cost for holding land near town.

4. **How big should the largest wilderness claim be at max level?**
   - a) About 100 × 100 m (a real castle with walls and a courtyard). **Recommended**: the 2,400-point budget at level 50 reaches it with the quarter-point wilderness price.
   - b) About 60 × 60 m (a keep and a yard). Cheaper to serve, less epic.
   - c) About 150 × 150 m (a small town). Needs raising the 50,000-structural-cell ceiling and a server profile first.

5. **Grid colours.**
   - a) Ivory for owned, moss for "you can still afford this", fade at the limit, crimson hatch for blocked, as in the HUD style. **Recommended**: matches the v5 UI and reads for colour-blind players because blocked is hatched and named.
   - b) Green and red only, like the reference ghost. Fewer states to learn, but "owned" and "affordable" look the same.
   - c) White only, with blocked cells left empty. Cleanest, but no warning before a refused placement.

[Building and Territory]: ../Game-Design/Legends_Shattered_Frontier_Building_and_Territory_System_v1.0.md
[Construction]: ../Game-Design/Legends_Shattered_Frontier_Construction_System_Architecture_v0.1.md
