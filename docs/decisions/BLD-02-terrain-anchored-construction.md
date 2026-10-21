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
- **Persistence.** A7 is required before the network path gains persistence: the snapshot header must store claim ID and anchor, and a mismatch against the claim record is a hard reject.
- **Doors.** `PlaceDoor` returns `InvalidContext` in anchored claims until door envelopes, motion and approach volumes are translated by the anchor.
