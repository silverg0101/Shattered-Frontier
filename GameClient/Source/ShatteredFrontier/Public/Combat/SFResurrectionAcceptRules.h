#pragma once

#include "CoreMinimal.h"

struct FSFResurrectionAcceptRules
{
    static bool CanAccept(bool bSandbox, bool bAuthority, bool bOwned, int32 Health,
        uint32 PendingOffer, uint32 RequestedOffer, double Now, double ExpiresAt)
    {
        return bSandbox && bAuthority && bOwned && Health == 0 && PendingOffer != 0 &&
            PendingOffer == RequestedOffer && FMath::IsFinite(Now) && Now < ExpiresAt;
    }

    static int32 RestoredHealth(int32 Maximum, float Fraction)
    {
        return Maximum > 0 ? FMath::Clamp(FMath::RoundToInt(Maximum * Fraction), 1, Maximum) : 0;
    }
};
