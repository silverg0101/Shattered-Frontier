#pragma once

#include "Components/ActorComponent.h"
#include "SFActionTargetComponent.generated.h"

class UMeshComponent;
class UMaterialInterface;

// One local selection/highlight owner for every action target. No item data or
// authority is inferred from this presentation selection.
UCLASS()
class SHATTEREDFRONTIER_API USFActionTargetComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFActionTargetComponent();
    AActor* GetTarget() const { return Selected.Get(); }
    void Refresh();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    void Select(AActor* Target);
    TWeakObjectPtr<AActor> Selected;
    TMap<TWeakObjectPtr<UMeshComponent>, TWeakObjectPtr<UMaterialInterface>> PreviousOverlays;
    UPROPERTY() TObjectPtr<UMaterialInterface> HighlightMaterial;
};
