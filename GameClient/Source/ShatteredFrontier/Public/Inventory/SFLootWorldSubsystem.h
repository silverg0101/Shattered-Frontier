#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Inventory/SFLootTypes.h"
#include "Services/SFGameplayServicesClient.h"
#include "SFLootWorldSubsystem.generated.h"

// Immutable server observation from the combat authority. Content decides the roll;
// the client never supplies eligibility, source position or an item list.
struct FSFLootDefeatSource
{
    FString EventId, CreatureId, TableId, KillerCharacterId, RaidGroupId;
    TArray<FString> EligibleCharacters;
    FVector Position = FVector::ZeroVector;
    FString MeshPath;
    FTransform MeshTransform = FTransform::Identity;
    FVector MeshBoundsOriginCm = FVector::ZeroVector, MeshBoundsExtentCm = FVector::ZeroVector;
    bool bHasMeshBounds = false;
    bool bShared = false;
    bool bProtected = false;
};

class ASFLootSourceActor;

UCLASS()
class SHATTEREDFRONTIER_API USFLootWorldSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    // True transfers payout responsibility to the durable loot route. A false
    // result leaves the existing non-service fallback responsible.
    bool RegisterDefeat(AActor* Corpse, const FSFLootDefeatSource& Source);
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    float GetReconcileSeconds() const { return ReconcileSeconds; }
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    static FString ZoneKey(const UWorld* World);
    void Apply(const FSFLootSourceSnapshot& Snapshot);
private:
    void SendDefeat(const FString& EventId);
    void BindOwner(ASFLootSourceActor* Actor);
    TMap<FString, FSFGameplayRequest> Pending;
    TSet<FString> InFlight;
    TMap<FString, TWeakObjectPtr<ASFLootSourceActor>> Actors;
    TMap<FString, int64> LatestRevision;
    double NextSyncAt = 0;
    bool bSyncInFlight = false;
    float ReconcileSeconds = 5;
};
