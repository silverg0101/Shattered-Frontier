#pragma once

#include "CoreMinimal.h"
#include "Construction/SFConstructionGeometry.h"

// BLD-02: terrain-anchored construction. Cell addresses and cell-local solids stay claim-local (BLD-01); the anchor
// only translates the claim into the world. Whole 50 cm cells, yaw 0, fixed 40x40x20 claim extent (a 20 m plot).
namespace SFConstructionAnchor
{
    inline FVector OffsetCm(const FIntVector& AnchorCell) { return FVector(AnchorCell) * SFConstructionGeometry::CellCentimetres; }
    // The anchor is the claim plot centre (X/Y) and its level (Z), in whole cells.
    inline bool IsValid(const FIntVector& AnchorCell) { return FMath::Abs(AnchorCell.X) <= 1000000 && FMath::Abs(AnchorCell.Y) <= 1000000 && FMath::Abs(AnchorCell.Z) <= 100000; }
    // The one claim extent check: a cell outside it is InvalidCell, so no contact can cross a claim edge.
    inline bool Contains(const FIntVector& Cell)
    { return Cell.X >= -20 && Cell.X < 20 && Cell.Y >= -20 && Cell.Y < 20 && Cell.Z >= 0 && Cell.Z < 20; }
}
