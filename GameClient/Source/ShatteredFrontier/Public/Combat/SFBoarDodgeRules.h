#pragma once

#include "CoreMinimal.h"

namespace SFBoarDodgeRules
{
    inline bool IsInFrontArc(const FVector& DefenderForward, const FVector& ToAttacker)
    {
        return FVector::DotProduct(DefenderForward.GetSafeNormal2D(), ToAttacker.GetSafeNormal2D()) >= 0.0;
    }

    inline bool Dodges(const FVector& DefenderForward, const FVector& ToAttacker, double AvoidanceDraw, double Chance)
    {
        return IsInFrontArc(DefenderForward, ToAttacker) && AvoidanceDraw >= 0.0 && AvoidanceDraw < Chance;
    }
}
