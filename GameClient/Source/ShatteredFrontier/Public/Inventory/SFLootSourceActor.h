#pragma once

#include "GameFramework/Actor.h"
#include "Inventory/SFLootTypes.h"
#include "SFLootSourceActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class SHATTEREDFRONTIER_API ASFLootSourceActor : public AActor
{
    GENERATED_BODY()
public:
    ASFLootSourceActor();
    void Initialize(const FSFLootSourceSnapshot& Snapshot);
    const FSFLootSourceSnapshot& GetSnapshot() const { return ServerSnapshot; }
    bool IsGround() const { return bGround; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> InteractionBounds;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(Replicated) bool bGround = false;
    FSFLootSourceSnapshot ServerSnapshot;
};
