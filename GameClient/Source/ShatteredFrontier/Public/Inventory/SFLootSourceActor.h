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

USTRUCT()
struct FSFLootMeshView
{
    GENERATED_BODY()
    UPROPERTY() FString Path;
    UPROPERTY() FVector Scale = FVector::OneVector;
};

UCLASS()
class SHATTEREDFRONTIER_API ASFLootSourceActor : public AActor
{
    GENERATED_BODY()
    friend class FSFLootWorkflowNetcheck;
public:
    ASFLootSourceActor();
    void Initialize(const FSFLootSourceSnapshot& Snapshot);
    const FSFLootSourceSnapshot& GetSnapshot() const { return ServerSnapshot; }
    bool IsGround() const { return bGround; }
    bool SharesCorpseBody(const ASFLootSourceActor* Other) const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    friend class FSFLootSourceClientHitTest;
    friend class FSFNpcDedicatedCorpseTest;
    UFUNCTION() void OnRep_HitShape();
    UFUNCTION() void OnRep_MeshView();
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SourceRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> InteractionBounds;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(Replicated) bool bGround = false;
    UPROPERTY(ReplicatedUsing=OnRep_HitShape) FSFLootHitShape HitShape;
    UPROPERTY(ReplicatedUsing=OnRep_MeshView) FSFLootMeshView MeshView;
    FSFLootSourceSnapshot ServerSnapshot;
};
