# BLD-02 — Terrain-anchored construction

Status: design approved 2026-10-21 with reviewer amendments; the smallest subset below plus the X/Y anchor (step 2, INT-04) is implemented for the development claim. Extends [BLD-01](BLD-01-voxel-geometry-interface.md), which keeps owning cell geometry; this record only adds how a claim sits in the world.

## Problem

Construction was sandbox-only: `MatchesKnownPlaneGeometry` required one foundation slab at world (0,0,-50), the grid was ±20 cells around the world origin, and every cell-to-world conversion assumed that origin. Silverbrook terrain is thousands of centimetres above z=0, so the quest chain's wall could never be placed in the zone.

## Decision

- **A1 Integer anchor.** A claim has a whole-cell anchor (50 cm cells; world offset = anchor × 50 cm). BLD-01 cell addresses and cell-local solids stay claim-local. The anchor is applied by moving the network state actor, so every projected component translates with it, and by adding the offset in world queries (reach, line of sight, clearance overlaps, previews). It is never added to vertices. Minimum-corner pivots and the exact `Equals` checks follow the actor transform instead of assuming small magnitudes.
- **A2 Claim scope (later).** One anchor per claim and network state. RPCs gain a claim ID; the server resolves anchor and bounds from its own claim record and clients only send cells. Until ClaimTerritory is wired in, the claim is hard-coded to the development claim.
- **A3 Immutable anchor.** `ConfigureAnchor` is server-only and refused once the snapshot has members or doors.
- **A4 Claim edges.** One `SFConstructionAnchor::Contains(Cell)` bounds check; a cell outside the claim extent is `InvalidCell`, so no contact crosses a claim edge.
- **A5 Replication.** `AnchorCell` replicates on the state. Previews and debug draws use it. The extent is a fixed 40 × 40 × 20 cells in this version.
- **A6 Rotation.** Yaw 0 only.
- **Step 2: X/Y anchor (done with INT-04).** The anchor is the claim plot centre plus its level, in whole cells. The dev claim is the zone record's first build plot (area purpose `claims`, 20 m square at 430, 215 m), outside the protected town; `Housing.ClaimPlot` accepts it, and the dev pad no longer sits in the market square.
- **A7 Save (not yet).** The network path has no persistence (`Persistence=0`); the sandbox save codec is a separate standalone format. When saves reach claims, the snapshot header stores claim ID and anchor, a mismatch against the claim record is a hard reject, and the codec `FormatVersion` and magic change.
- **A8 Bounds.** Hard-coded bounds in the network state and preview now use `Contains`. Still literal until the extent follows the claim: the wire packing in `FSFConstructionNetworkSnapshot::NetSerialize` (`uint8(X+20)`), `Activate`, the sandbox save codec, and the standalone sandbox.

## Foundation fixture

The slab check now takes the anchor offset: one tagged cube at (0,0,-50)+offset, scale (20,20,1), identity rotation, blocking pawn and visibility. In the Silverbrook zone the server spawns it as a replicated dev claim pad: a level slab above the highest hit on a 1 m grid over the 20 × 20 m plot at the world origin, so no terrain or prop overlaps a cell. Real terrain levelling and the claim's plot origin belong to DEC-03 and ClaimTerritory.

## Sandbox compatibility

The sandbox keeps the identity anchor and its map-placed slab, so all existing construction and sandbox behaviour and tests are unchanged. Only the zone world injects a non-identity anchor.

## Not covered by the smallest version

- Doors in anchored claims: `PlaceDoor` returns `InvalidContext`, because door envelopes, motion and approach volumes are claim-local and need translating first.
- X/Y anchors, multiple claims, claim-scoped RPCs, save header, claim-derived extent and wire packing.

## Evidence

`ShatteredFrontier.Construction.Anchor` covers the offset, validity and claim-edge bounds. `tools/Test-QuestPilot.ps1 -Leg Full` places the Silverbrook wall through the real placement RPC on the claim pad.

## Known gaps and required follow-ups

- **Dev claim location.** Resolved by step 2: the pad now stands on the first build plot. Real terrain levelling remains with DEC-03.
- **Persistence.** Resolved by step 3 for the network path, below.
- **Doors.** `PlaceDoor` returns `InvalidContext` in anchored claims until door envelopes, motion and approach volumes are translated by the anchor.

## Step 3 design: real claims and housing persistence (A2 + A7, implemented 2026-10-21)

- **Claim states.** There is one `ASFConstructionNetworkState` per claim, spawned at its anchor. All existing anchor, bounds and projection code stays per-actor. The server owns a `ClaimId → state` map. Construction RPCs gain a `ClaimId`; the server resolves the state, anchor and extent from its own record, and clients only send cells.
- **Claiming.** The player stands on one of the zone record's build plots (area purpose `claims`) and requests a claim. The server calls `Housing.ClaimPlot` with the plot centre and side and a deterministic id (`clm_<zone>_<plot index>`). The service enforces territory, the protected town and spacing. The reply's owner and permission revision create the claim state, and the anchor Z comes from the level-pad rule.
- **Persistence.** Structures live only in Housing. At server start and on admission, the server reads each known plot's claim (`GetClaim`) and its structures (`ListStructures`, a new RPC), then rebuilds the snapshot. The rebuilt state carries `ClaimId` and `AnchorCell`. A record whose geometry differs from the zone plot or anchor is hard-rejected, logged and not projected (A7 for the network path).
- **Permissions.** Place and Remove send the actor's character id; the service rejects non-owners without a build grant (`PERMISSION_DENIED`). The client shows the refusal text. The owner grants with a new `Housing.Grant(claim, target, rights)` RPC, using the existing `ClaimRight` model and the `SaveClaim` store path.
- **Identity across a restart.** Today a character is `chr_dev_<connection>`, which is not stable across server restarts. Proposed: a dev-only URL option `?SFDevCharacter=<name>` binds `chr_dev_<name>`, so a restarted server reconnects A to A's claim. This is a fixture until DEC-04.
- **Acceptance.** A two-client runner: A claims, places and removes. The server restarts, while the hosts and database stay up. A's structures come back in place, B is refused on A's claim, and after A grants B, B's placement succeeds.

**As built.**
- The zone server spawns one slab and one state per build plot (`CONSTRUCTION_NETWORK_CLAIM_PLOT`). With Gameplay services on, it loads each plot with `GetClaim` and `ListStructures` (`HOUSING_CLAIM_LOADED`) and retries every 3 s while the service is unavailable.
- Each structure stores its BLD-01 shape key and orientation index: index 0 is identity, then the catalog's lexicographic order. The restore rebuilds exact pieces. An unknown key, an out-of-range orientation, a cell outside the extent or a duplicate rejects the whole claim (`CONSTRUCTION_NETWORK_RESTORE_REJECTED`).
- The first placement on an unclaimed plot claims it for the builder; `K` claims explicitly. `SFGrantBuild <name>` grants build rights through `Housing.Grant`.
- `?SFDevCharacter=<name>` (`[a-z0-9_]{1,24}`) is honoured only on a development server started with `-SFDevIdentity` in the pilot world. Otherwise it logs `DEV_IDENTITY_IGNORED`. This is a fixture until DEC-04.
- Services off, the plots still spawn, but claims are unavailable and the sandbox rules apply. Doors stay `InvalidContext` in anchored claims.

**Evidence.** `tools/Test-ClaimHousing.ps1` runs on owned PostgreSQL with both hosts:
1. Alpha claims, places a turned slab and a second piece, then removes the second piece.
2. The server restarts while the hosts and database stay up.
3. Alpha's content digest is unchanged.
4. Bravo is refused (`PERMISSION_DENIED`) until alpha grants build rights, then builds.

`tools/Test-QuestPilot.ps1 -Leg Plain` asserts that a services-off client receives all three plot states and keeps the inventory key.
