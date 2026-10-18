#pragma once

#include "CoreMinimal.h"

// Release-bound rough-timber-v1 support profile. Lengths are centimetres;
// weight and capacity use the pilot's abstract load unit.
struct SHATTEREDFRONTIER_API FSFConstructionSupportProfile
{
    FString ProfileId = TEXT("rough-timber-v1");
    double WeightPerFullCell = 1.0;
    double Capacity = 64.0;
    double VerticalReachCm = 800.0;
    double HorizontalReachCm = 300.0;
};

struct SHATTEREDFRONTIER_API FSFConstructionSupportMember
{
    FIntVector Address = FIntVector::ZeroValue;
    int32 Shape = INDEX_NONE;
    int32 Rotation = INDEX_NONE;
    double CombatHealth = 1.0;
    bool bAnchored = false; // Trusted synthetic pilot foundation boundary.
    double AttachedLoad = 0.0; // Authority-supplied functional-object shares in abstract load units.
};

enum class ESFConstructionSupportState : uint8 { Stable, Strained, Unsupported };
enum class ESFConstructionSupportReason : uint8
{
    None, InvalidInput, NoAnchorPath, ZeroHealth, AggregateCapacity,
    ContactCapacity, Reach, FailedParent
};

struct SHATTEREDFRONTIER_API FSFConstructionSupportResult
{
    FIntVector Address = FIntVector::ZeroValue;
    ESFConstructionSupportState State = ESFConstructionSupportState::Unsupported;
    ESFConstructionSupportReason Reason = ESFConstructionSupportReason::None;
    FIntVector LimitingParent = FIntVector::ZeroValue;
    bool bHasLimitingParent = false;
    double LoadRatio = 0.0;
    double ReachRatio = 0.0;
    int32 AnchorDistance = INDEX_NONE;
};

namespace SFConstructionSupport
{
    // Invalid input preserves Out. Successful output is sorted by X, then Y, then Z.
    SHATTEREDFRONTIER_API bool Evaluate(const TArray<FSFConstructionSupportMember>& Members,
        const FSFConstructionSupportProfile& Profile, TArray<FSFConstructionSupportResult>& Out);
}
