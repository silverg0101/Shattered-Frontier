#include "Characters/SFRoadSpeedVolume.h"
#include "Framework/SFE1Settings.h"

ASFRoadSpeedVolume::ASFRoadSpeedVolume()
{
    Priority = 1;
}

ASFDeepSnowVolume::ASFDeepSnowVolume()
{
    SpeedMultiplier = FMath::Clamp(GetDefault<USFE1Settings>()->DeepSnowSpeedMultiplier, 0.5f, 1.2f);
}
