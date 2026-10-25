#pragma once

#include "CoreMinimal.h"

struct FSFSpiritReleaseRules
{
    static bool CanRelease(bool bSandbox, bool bAuthority, bool bOwnedPawn, int32 Health, bool bHasRespawnPoint)
    {
        return bSandbox && bAuthority && bOwnedPawn && Health == 0 && bHasRespawnPoint;
    }

    static int32 RestoredHealth(int32 Maximum, float Fraction)
    {
        return Maximum > 0 ? FMath::Clamp(FMath::RoundToInt(Maximum * Fraction), 1, Maximum) : 0;
    }
};
