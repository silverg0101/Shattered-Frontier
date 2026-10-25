#pragma once

#include "CoreMinimal.h"

namespace SFTravelMountRules
{
    inline bool CanBegin(bool bOwned, bool bAlive, bool bInCombat, bool bSwimming, bool bIndoors, bool bAlreadyMounted, bool bAlreadyCasting)
    {
        return bOwned && bAlive && !bInCombat && !bSwimming && !bIndoors && !bAlreadyMounted && !bAlreadyCasting;
    }

    inline float MountedSpeedCm(float MetresPerSecond, float CapMetresPerSecond)
    {
        return FMath::Min(FMath::Max(0.0f, MetresPerSecond), FMath::Clamp(CapMetresPerSecond, 1.0f, 20.0f)) * 100.0f;
    }
}
