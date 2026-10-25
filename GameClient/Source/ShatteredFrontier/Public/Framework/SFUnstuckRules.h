#pragma once

#include "CoreMinimal.h"

// World Cells, Movement and Handoffs § Teleport tokens / Unstuck.
namespace SFUnstuckRules
{
constexpr double CooldownSeconds = 15.0 * 60.0;
constexpr float MinimumMetres = 2.0f;
constexpr float MaximumMetres = 30.0f;

inline bool CanRequest(bool bAlive, bool bInCombat, bool bInInstance, double Now, double LastAccepted)
{
    return bAlive && !bInCombat && !bInInstance &&
        (LastAccepted < 0.0 || Now - LastAccepted >= CooldownSeconds);
}

inline bool IsNearbyDestination(const FVector& From, const FVector& To)
{
    const float Distance = FVector::Dist2D(From, To);
    return Distance >= MinimumMetres * 100.0f && Distance <= MaximumMetres * 100.0f;
}
}
