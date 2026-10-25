#pragma once

#include "CoreMinimal.h"

namespace SFFallDamage
{
    // Impact speed is authoritative CharacterMovement downward velocity in metres/second.
    inline float Coefficient(float ImpactMetresPerSecond, float ThresholdMetresPerSecond)
    {
        return FMath::Clamp((ImpactMetresPerSecond - ThresholdMetresPerSecond) / 20.0f, 0.0f, 1.2f);
    }

    inline int32 Amount(int32 MaxHealth, float ImpactMetresPerSecond, float ThresholdMetresPerSecond)
    {
        return FMath::RoundToInt(MaxHealth * Coefficient(ImpactMetresPerSecond, ThresholdMetresPerSecond));
    }
}
