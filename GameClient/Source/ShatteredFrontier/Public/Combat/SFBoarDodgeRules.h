#pragma once

#include "CoreMinimal.h"

namespace SFBoarDodgeRules
{
    inline bool IsInFrontArc(const FVector& DefenderForward, const FVector& ToAttacker, double ArcDegrees = 180.0)
    {
        const double Threshold = FMath::Cos(FMath::DegreesToRadians(ArcDegrees * 0.5));
        return FVector::DotProduct(DefenderForward.GetSafeNormal2D(), ToAttacker.GetSafeNormal2D()) >= Threshold - 1e-6;
    }

    inline bool Dodges(const FVector& DefenderForward, const FVector& ToAttacker, double AvoidanceDraw, double Chance)
    {
        return IsInFrontArc(DefenderForward, ToAttacker) && AvoidanceDraw >= 0.0 && AvoidanceDraw < Chance;
    }
}
