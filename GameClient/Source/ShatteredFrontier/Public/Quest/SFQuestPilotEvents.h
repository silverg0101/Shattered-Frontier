#pragma once

#include "CoreMinimal.h"
#include "Gathering/SFGatheringPilot.h"

class APlayerController;

// Server-side broadcasts from the pilots' accepted-outcome paths; the quest pilot is the only listener.
namespace SFQuestPilotEvents
{
    // Output is the committed item definition (the recipe output for a station); Operation is the ledger or craft
    // operation id behind it, empty in session-only mode.
    DECLARE_MULTICAST_DELEGATE_FourParams(FGathered, APlayerController*, ESFGatheringPilotKind, const FString& /*output*/, const FString& /*operation*/);
    DECLARE_MULTICAST_DELEGATE_ThreeParams(FStructurePlaced, APlayerController*, uint64 /*construction revision*/, const FString& /*housing operation, empty when session-only*/);
    DECLARE_MULTICAST_DELEGATE_TwoParams(FBoarDefeated, APlayerController*, const FString& /*boar death id*/);

    /** Harvest or refine committed to the owner's bag. */
    SHATTEREDFRONTIER_API FGathered& OnGathered();
    /** Construction edit accepted (placement, not removal). */
    SHATTEREDFRONTIER_API FStructurePlaced& OnStructurePlaced();
    /** First credit of one boar death outside protected areas. */
    SHATTEREDFRONTIER_API FBoarDefeated& OnBoarDefeated();
}
