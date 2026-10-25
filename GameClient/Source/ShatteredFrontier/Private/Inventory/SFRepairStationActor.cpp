#include "Inventory/SFRepairStationActor.h"
#include "Components/SceneComponent.h"

ASFRepairStationActor::ASFRepairStationActor()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RepairStationRoot"));
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
}
