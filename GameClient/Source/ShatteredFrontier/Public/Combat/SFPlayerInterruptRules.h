#pragma once

#include "CoreMinimal.h"

namespace SFPlayerInterruptRules
{
inline bool CanInterrupt(bool bRequesterAlive, bool bCasterAlive, bool bCasting,
    double DistanceCm, double ReachCm, bool bSightClear)
{
    return bRequesterAlive && bCasterAlive && bCasting && bSightClear &&
        DistanceCm <= ReachCm;
}
}
