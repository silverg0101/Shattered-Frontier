#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace SFConstructionClearance
{
    enum class EResult : uint8 { Clear, Blocked, Unavailable };

    // Synchronous game-thread query of approved canonical pilot occupancy.
    // Shared boundary contact is legal; strictly positive narrowphase penetration
    // blocks. No component/actor is created and no body or world state is changed.
    // Admitted scene: simple collision bodies. An encountered complex-only or
    // skeletal body fails closed; this does not certify triangle-shell interiors
    // as occupied volumes or replace the future World terrain density adapter.
    SHATTEREDFRONTIER_API EResult Evaluate(UWorld* World, int32 Shape, int32 Rotation, const FVector& CellCorner);
}
