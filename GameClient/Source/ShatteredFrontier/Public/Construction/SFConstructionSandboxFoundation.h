#pragma once

#include "CoreMinimal.h"

class UWorld;

// Synthetic development foundation only. Never interprets a generic terrain,
// furniture or collision hit as World-owned compact-ground evidence.
namespace SFConstructionSandboxFoundation
{
    SHATTEREDFRONTIER_API bool HasKnownPlane(const UWorld* World);
    SHATTEREDFRONTIER_API bool TouchesPlane(const FIntVector& Cell, int32 Shape, int32 Rotation);
    SHATTEREDFRONTIER_API bool Anchors(const UWorld* World, const FIntVector& Cell, int32 Shape, int32 Rotation);
}
