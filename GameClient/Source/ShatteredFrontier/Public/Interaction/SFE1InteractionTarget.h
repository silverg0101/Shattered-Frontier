#pragma once

#include "GameFramework/Actor.h"
#include "SFE1InteractionTarget.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

// Versioned, non-economic fixture. Acceptance changes no world or economic state.
UCLASS()
class SHATTEREDFRONTIER_API ASFE1InteractionTarget : public AActor
{
    GENERATED_BODY()

public:
    ASFE1InteractionTarget();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    bool IsInteractionEnabled() const { return bInteractionEnabled; }
    uint32 GetInteractionRevision() const { return InteractionRevision; }
    FVector GetInteractionAnchor() const;
    // Authority-owned lifecycle change, never a client RPC or a configurable expression.
    void SetInteractionEnabled(bool bEnabled);

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> FixtureCollision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FixtureMesh;
    UPROPERTY(Replicated) bool bInteractionEnabled = true;
    UPROPERTY(Replicated) uint32 InteractionRevision = 1;
};
