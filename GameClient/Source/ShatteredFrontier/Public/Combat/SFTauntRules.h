#pragma once

#include "CoreMinimal.h"

/** Combat threat §3 and abilities §crowd control; server clocks are in seconds. */
namespace SFTauntRules
{
    inline double ForcedDuration(int32 PriorApplications, double SinceLastApplication,
        double ForceSeconds, double DrWindowSeconds)
    {
        const int32 Step = SinceLastApplication >= DrWindowSeconds ? 0 : PriorApplications;
        return Step >= 3 ? 0.0 : ForceSeconds / (1 << FMath::Max(0, Step));
    }

    inline double RaisedThreat(double Own, double Top, double ThreatMultiplier)
    { return FMath::Max(Own, Top * ThreatMultiplier); }
}
