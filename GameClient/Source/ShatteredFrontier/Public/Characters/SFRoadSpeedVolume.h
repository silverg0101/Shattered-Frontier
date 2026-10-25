#pragma once

#include "GameFramework/PhysicsVolume.h"
#include "SFRoadSpeedVolume.generated.h"

/** Authored road travel area. CharacterMovement reads it on both authority and predicted clients. */
UCLASS()
class SHATTEREDFRONTIER_API ASFRoadSpeedVolume : public APhysicsVolume
{
    GENERATED_BODY()

public:
    ASFRoadSpeedVolume();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0.5", ClampMax="1.2", Units="x"))
    float SpeedMultiplier = 1.10f;
};

/** Authored deep-snow travel area; uses the same predicted and authoritative terrain speed path. */
UCLASS()
class SHATTEREDFRONTIER_API ASFDeepSnowVolume : public ASFRoadSpeedVolume
{
    GENERATED_BODY()
public:
    ASFDeepSnowVolume();
};
