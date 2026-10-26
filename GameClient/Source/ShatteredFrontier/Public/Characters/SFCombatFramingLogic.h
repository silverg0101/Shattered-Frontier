#pragma once

#include "CoreMinimal.h"

// Pure CTL-02 transitions. The release settings supply the durations and zoom threshold.
namespace SFCombatFramingLogic
{
    // The rendered viewpoint includes arm length, shoulder offset and collision retraction. Aiming from the
    // high arm pivot instead makes nearby ground-level targets pull the view sharply into the ground.
    inline FRotator TargetAim(const FVector& CameraLocation, const FVector& TargetCentre)
    { return (TargetCentre - CameraLocation).Rotation(); }

    inline bool ShouldEngage(bool bWasEngaged, bool bHasTarget, bool bEnabled, bool bFirstPerson,
        float DesiredZoomCm, float MaxZoomCm)
    {
        return bHasTarget && bEnabled && !bFirstPerson
            && DesiredZoomCm <= MaxZoomCm * (bWasEngaged ? 1.1f : 1.0f);
    }

    inline float AdvanceBlend(float Current, bool bEngaged, float DeltaSeconds, float InSeconds)
    {
        const float Duration = InSeconds * (bEngaged ? 1.0f : 2.0f);
        return FMath::Clamp(Current + (bEngaged ? 1.0f : -1.0f) * DeltaSeconds / Duration, 0.0f, 1.0f);
    }

    inline float AssistAxis(float CurrentDegrees, float DesiredDegrees, float DeadzoneDegrees,
        float SpeedPerSecond, float Blend, float DeltaSeconds)
    {
        const float Error = FRotator::NormalizeAxis(DesiredDegrees - CurrentDegrees);
        if (FMath::Abs(Error) <= DeadzoneDegrees) { return CurrentDegrees; }
        return CurrentDegrees + FMath::FInterpTo(0.0f, Error, DeltaSeconds, SpeedPerSecond) * Blend;
    }
}
