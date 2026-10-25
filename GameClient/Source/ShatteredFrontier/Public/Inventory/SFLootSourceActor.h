#pragma once

#include "GameFramework/Actor.h"
#include "Inventory/SFLootTypes.h"
#include "SFLootSourceActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;

USTRUCT()
struct FSFLootHitShape
{
    GENERATED_BODY()
    UPROPERTY() FVector Center = FVector::ZeroVector;
    UPROPERTY() FVector Extent = FVector(25);
};

UCLASS()
class SHATTEREDFRONTIER_API ASFLootSourceActor : public AActor
{
    GENERATED_BODY()
public:
    ASFLootSourceActor();
    void Initialize(const FSFLootSourceSnapshot& Snapshot);
    const FSFLootSourceSnapshot& GetSnapshot() const { return ServerSnapshot; }
    bool IsGround() const { return bGround; }
    bool SharesCorpseBody(const ASFLootSourceActor* Other) const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    friend class FSFLootSourceClientHitTest;
    UFUNCTION() void OnRep_HitShape();
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SourceRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> InteractionBounds;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(Replicated) bool bGround = false;
    UPROPERTY(ReplicatedUsing=OnRep_HitShape) FSFLootHitShape HitShape;
    FSFLootSourceSnapshot ServerSnapshot;
};
