#pragma once

#include "CoreMinimal.h"

/** World common-NPC respawn clock. Unit is sampled once by authority at death. */
struct FSFCommonRespawn
{
    static double DelaySeconds(double BaseSeconds, int32 PlayersInArea, double PlayerSlope,
        double PlayerFloor, double Jitter, double Unit)
    {
        const double Population = FMath::Max(PlayerFloor,
            1.0 / (1.0 + PlayerSlope * FMath::Max(0, PlayersInArea - 1)));
        return BaseSeconds * Population * (1.0 + Jitter * (2.0 * FMath::Clamp(Unit, 0.0, 1.0) - 1.0));
    }

    static bool IsDue(double Now, double RespawnAt) { return RespawnAt >= 0.0 && Now >= RespawnAt; }
};
