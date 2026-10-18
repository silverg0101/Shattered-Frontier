#pragma once

#include "CoreMinimal.h"

class UWorld;
class ASFConstructionSandboxDoorProjection;
class ASFConstructionSandboxState;
class UPrimitiveComponent;

// Only State can populate this exact, revision-bound component replacement set.
// Default/unowned/stale contexts fail closed. No actor-wide collision exclusion.
class SHATTEREDFRONTIER_API FSFConstructionReplacementSet
{
public:
    bool Validate(UWorld* World) const;
    bool Contains(const UPrimitiveComponent* Component) const;
private:
    friend class ASFConstructionSandboxState;
    TWeakObjectPtr<ASFConstructionSandboxState> Owner;
    uint64 Revision = 0;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> Components;
};

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
    // Trusted released reservation geometry only; not an intent geometry route.
    SHATTEREDFRONTIER_API EResult EvaluateBox(UWorld* World, const FBox& Bounds,
        const ASFConstructionSandboxDoorProjection* OwnOldDoor = nullptr);
    SHATTEREDFRONTIER_API EResult IntersectsReservation(int32 Shape, int32 Rotation,
        const FVector& CellCorner, const FBox& Reservation);
    SHATTEREDFRONTIER_API EResult EvaluateReplacing(UWorld* World, int32 Shape, int32 Rotation,
        const FVector& CellCorner, const FSFConstructionReplacementSet& Replacement);
    SHATTEREDFRONTIER_API EResult EvaluateBoxReplacing(UWorld* World, const FBox& Bounds,
        const FSFConstructionReplacementSet& Replacement);
}
