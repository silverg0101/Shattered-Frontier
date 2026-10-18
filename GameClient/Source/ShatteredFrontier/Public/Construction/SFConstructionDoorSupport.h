#pragma once

#include "CoreMinimal.h"
#include "Construction/SFConstructionSupport.h"

struct SHATTEREDFRONTIER_API FSFConstructionDoorSupportInput
{
    FIntVector OriginCell = FIntVector::ZeroValue;
    int32 YawQuarterTurns = 0;
};

struct SHATTEREDFRONTIER_API FSFConstructionDoorSupportProfile
{
    FString ProfileId = TEXT("sandbox-rough-timber-door-v1");
    double Weight = 1.0;
};

struct SHATTEREDFRONTIER_API FSFConstructionDoorSupportResult
{
    FIntVector OriginCell = FIntVector::ZeroValue;
    ESFConstructionSupportState State = ESFConstructionSupportState::Unsupported;
    bool bFoundationAttached = false;
    TArray<FIntVector> Receivers;
};

namespace SFConstructionDoorSupport
{
    // Invalid input preserves both outputs. Successful outputs use X/Y/Z ordering.
    SHATTEREDFRONTIER_API bool Evaluate(
        const TArray<FSFConstructionSupportMember>& StructuralMembers,
        const TArray<FSFConstructionDoorSupportInput>& Doors,
        const FSFConstructionDoorSupportProfile& Profile,
        bool bKnownFoundation,
        TArray<FSFConstructionSupportResult>& OutStructuralResults,
        TArray<FSFConstructionDoorSupportResult>& OutDoorResults);
}
