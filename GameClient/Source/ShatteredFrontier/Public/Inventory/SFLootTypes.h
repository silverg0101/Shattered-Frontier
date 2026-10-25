#pragma once

#include "CoreMinimal.h"
#include "SFLootTypes.generated.h"

USTRUCT()
struct FSFLootEntry
{
    GENERATED_BODY()
    UPROPERTY() FString DefinitionId;
    UPROPERTY() int32 Quantity = 0;
    UPROPERTY() FString InstanceId;
    UPROPERTY() int32 DurabilityPercent = 100;
};

// Server transport/cache only. Eligibility and item lists are never replicated
// on the public world actor; authorized rows travel only to the owning player.
struct FSFLootSourceSnapshot
{
    FString Id, ZoneId, CreatureId, MeshPath;
    FVector Position = FVector::ZeroVector;
    FVector MeshScale = FVector::OneVector;
    FRotator MeshRotation = FRotator::ZeroRotator;
    bool bGround = false, bShared = false;
    int64 Revision = 0;
    TArray<FString> EligibleCharacters;
    TArray<FSFLootEntry> Items;
};
