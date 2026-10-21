#pragma once

#include "CoreMinimal.h"
#include "Gathering/SFGatheringPilot.h"

class APlayerController;

// Server-side broadcasts from the pilots' accepted-outcome paths; the quest pilot is the only listener.
namespace SFQuestPilotEvents
{
    DECLARE_MULTICAST_DELEGATE_TwoParams(FGathered, APlayerController*, ESFGatheringPilotKind);
    DECLARE_MULTICAST_DELEGATE_TwoParams(FStructurePlaced, APlayerController*, uint64 /*construction revision*/);
    DECLARE_MULTICAST_DELEGATE_TwoParams(FBoarDefeated, APlayerController*, const FString& /*boar death id*/);

    /** Harvest or refine committed to the owner's bag. */
    SHATTEREDFRONTIER_API FGathered& OnGathered();
    /** Construction edit accepted (placement, not removal). */
    SHATTEREDFRONTIER_API FStructurePlaced& OnStructurePlaced();
    /** First credit of one boar death outside protected areas. */
    SHATTEREDFRONTIER_API FBoarDefeated& OnBoarDefeated();
}
