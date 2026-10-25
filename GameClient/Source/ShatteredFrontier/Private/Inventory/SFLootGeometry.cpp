#include "Inventory/SFLootGeometry.h"
#include "EngineDefines.h"

static_assert(SFLootGeometry::MaximumCoordinateCm == HALF_WORLD_MAX1, "Update WorldLoot geometry validation with the engine world envelope");

bool SFLootGeometry::ValidGroundBounds(const FVector& Origin, const FVector& Extent, const FTransform& Transform)
{
    const auto InWorld = [](const FVector& V) { return !V.ContainsNaN() && V.GetAbsMax() <= MaximumCoordinateCm; };
    const FVector Scale = Transform.GetScale3D();
    if (!InWorld(Origin) || !InWorld(Extent) || Extent.GetMin() < 0 || Extent.GetMax() <= 0 ||
        !Transform.IsValid() || Scale.X == 0 || Scale.Y == 0 || Scale.Z == 0 || !InWorld(Transform.GetLocation()) ||
        !InWorld(Extent * Scale.GetAbs()) || !InWorld(Origin * Scale)) return false;
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector Offset((Corner & 1) ? Extent.X : -Extent.X, (Corner & 2) ? Extent.Y : -Extent.Y, (Corner & 4) ? Extent.Z : -Extent.Z);
        if (!InWorld(Transform.TransformPosition(Origin + Offset))) return false;
    }
    return true;
}
