#pragma once
#include "CoreMinimal.h"

namespace SFLootGeometry
{
    // Engine 5.8 HALF_WORLD_MAX1, mirrored by WorldLoot.ValidateSource; structural geometry limit, not balance.
    constexpr double MaximumCoordinateCm = 4398046511103.0;
    SHATTEREDFRONTIER_API bool ValidGroundBounds(const FVector& Origin, const FVector& Extent, const FTransform& Transform);
}
