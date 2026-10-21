#pragma once

#include "CoreMinimal.h"
#include "Construction/SFConstructionGeometry.h"

// BLD-02: terrain-anchored construction. Cell addresses and cell-local solids stay claim-local (BLD-01); the anchor
// only translates the claim into the world. Smallest version: Z-only, whole 50 cm cells, fixed 40x40x20 claim extent.
namespace SFConstructionAnchor
{
    inline FVector OffsetCm(const FIntVector& AnchorCell) { return FVector(AnchorCell) * SFConstructionGeometry::CellCentimetres; }
    // X/Y stay at the claim origin until claim territory supplies a plot origin.
    inline bool IsValid(const FIntVector& AnchorCell) { return AnchorCell.X == 0 && AnchorCell.Y == 0 && FMath::Abs(AnchorCell.Z) <= 100000; }
    // The one claim extent check: a cell outside it is InvalidCell, so no contact can cross a claim edge.
    inline bool Contains(const FIntVector& Cell)
    { return Cell.X >= -20 && Cell.X < 20 && Cell.Y >= -20 && Cell.Y < 20 && Cell.Z >= 0 && Cell.Z < 20; }
}
