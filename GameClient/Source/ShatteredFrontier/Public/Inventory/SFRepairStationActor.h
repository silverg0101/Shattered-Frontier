#pragma once

#include "GameFramework/Actor.h"
#include "SFRepairStationActor.generated.h"

// Authoritative interaction point at the Silverbrook blacksmith entrance.
// The smithy itself supplies the world landmark; menu layout stays in Widget Blueprints.
UCLASS()
class SHATTEREDFRONTIER_API ASFRepairStationActor : public AActor
{
    GENERATED_BODY()
public:
    ASFRepairStationActor();
};
