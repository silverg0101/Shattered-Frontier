#pragma once

#include "Components/ActorComponent.h"
#include "Camera/CameraModifier.h"
#include "Components/PrimitiveComponent.h"
#include "SFActionTargetComponent.generated.h"

class UMeshComponent;
class UMaterialInterface;
class USFActionTargetComponent;

// Per-owning-camera post process, layered through CameraManager's PP cache.
UCLASS(Transient)
class USFActionTargetCameraModifier : public UCameraModifier
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<USFActionTargetComponent> Selection;
protected:
    virtual void ModifyPostProcess(float DeltaTime, float& Weight, FPostProcessSettings& Settings) override;
};

struct FSFActionTargetSelection
{
    TWeakObjectPtr<AActor> Target;
    FVector RayStart = FVector::ZeroVector;
    FVector RayDirection = FVector::ForwardVector;
    FHitResult Hit;
};

// One local selection/highlight owner for every action target. No item data or
// authority is inferred from this presentation selection.
UCLASS()
class SHATTEREDFRONTIER_API USFActionTargetComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFActionTargetComponent();
    AActor* GetTarget() const { return Selected.Get(); }
    const FSFActionTargetSelection& GetSelection() const { return Selection; }
    void Refresh();
    void RefreshFromRay(const FVector& Start, const FVector& Direction);
    // Bit7 is reserved for this local renderer; existing FX retain bits0-6.
    static constexpr int32 SelectionStencilBit = 128;
    bool IsMeshHighlighted(const UMeshComponent* Mesh) const;
    bool HasSelectionOutline() const;
    UMaterialInterface* GetOutlineMaterial() const { return HighlightMaterial.Get(); }
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    friend class FSFActionTargetHighlightTest;
    void Select(AActor* Target);
    bool EnsureOutlinePass();
    void RestoreHighlight();
    TWeakObjectPtr<AActor> Selected;
    FSFActionTargetSelection Selection;
    struct FPreviousDepth
    {
        bool bRender;
        int32 Stencil;
        ERendererStencilMask WriteMask;
        int32 AppliedStencil;
    };
    TMap<TWeakObjectPtr<UMeshComponent>, FPreviousDepth> PreviousDepth;
    TWeakObjectPtr<APlayerCameraManager> OutlineCamera;
    UPROPERTY(Transient) TObjectPtr<USFActionTargetCameraModifier> OutlineModifier;
    UPROPERTY() TObjectPtr<UMaterialInterface> HighlightMaterial;
};
