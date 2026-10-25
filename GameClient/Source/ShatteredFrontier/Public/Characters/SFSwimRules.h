#pragma once

#include "CoreMinimal.h"

namespace SFSwimRules
{
inline bool IsDeepEnough(float SurfaceZCm, float FeetZCm, float MinimumDepthMetres)
{
    return FMath::IsFinite(SurfaceZCm) && FMath::IsFinite(FeetZCm) &&
        SurfaceZCm - FeetZCm >= MinimumDepthMetres * 100.0f;
}
}
