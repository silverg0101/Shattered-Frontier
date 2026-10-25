#include "Interaction/SFActionTargetComponent.h"
#include "Interaction/SFE1InteractionTarget.h"
#include "Inventory/SFLootSourceActor.h"
#include "Inventory/SFRepairStationActor.h"
#include "Exchange/SFMarketBoardActor.h"
#include "Dialogue/SFDialogue.h"
#include "Quest/SFQuestBoard.h"
#include "Gathering/SFGatheringPilot.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "Framework/SFE1Settings.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Camera/PlayerCameraManager.h"

void USFActionTargetCameraModifier::ModifyPostProcess(float DeltaTime, float& Weight, FPostProcessSettings& Settings)
{
    if (Selection.IsValid() && Selection->HasSelectionOutline())
    { Settings.AddBlendable(Selection->GetOutlineMaterial(), 1.f); Weight = 1.f; }
}

USFActionTargetComponent::USFActionTargetComponent() { PrimaryComponentTick.bCanEverTick = false; }

void USFActionTargetComponent::Refresh()
{
    auto* Player = Cast<APlayerController>(GetOwner());
    if (!Player || !Player->IsLocalController() || !Player->GetPawn()) { Select(nullptr); return; }
    FVector Start, Direction;
    if (!Player->DeprojectMousePositionToWorld(Start, Direction))
    { FRotator Rotation; Player->GetPlayerViewPoint(Start, Rotation); Direction = Rotation.Vector(); }
    RefreshFromRay(Start, Direction);
}

void USFActionTargetComponent::RefreshFromRay(const FVector& Start, const FVector& Direction)
{
    auto* Player = Cast<APlayerController>(GetOwner());
    if (!Player || !Player->GetPawn()) { Select(nullptr); return; }
    const FVector At = Player->GetPawn()->GetActorLocation();
    const float Reach = GetDefault<USFE1Settings>()->InteractionRangeMetres * 100.f;
    const float TraceLength = FVector::Distance(Start, At) + Reach * 2;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ActionTarget), false, Player->GetPawn()); FHitResult Hit;
    AActor* Target = nullptr;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Direction * TraceLength, ECC_Visibility, Query))
    {
        AActor* Candidate = Hit.GetActor();
        if (Candidate && (Candidate->IsA<ASFLootSourceActor>() || Candidate->IsA<ASFGatheringPilotNode>() ||
            Candidate->IsA<ASFE1InteractionTarget>() || Candidate->IsA<ASFDialogueNpc>() || Candidate->IsA<ASFQuestBoard>() ||
            Candidate->IsA<ASFMarketBoardActor>() || Candidate->IsA<ASFRepairStationActor>() || Candidate->ActorHasTag(TEXT("SFInteractable"))))
        { Target = Candidate; }
    }
    if (Target && FVector::DistSquared(At, Target->GetActorLocation()) > FMath::Square(Reach)) { Target = nullptr; }
    Selection.RayStart = Start; Selection.RayDirection = Direction; Selection.Hit = Hit;
    Select(Target);
}

void USFActionTargetComponent::Select(AActor* Target)
{
    Selection.Target = Target;
    if (Selected.Get() != Target || !Target) { RestoreHighlight(); Selected = Target; }
    if (!Target) { return; }
    // Follow scene attachments, not component ownership: a detached component
    // can retain its owner while no longer belonging to the selected body.
    TSet<UMeshComponent*> CurrentMeshes;
    if (USceneComponent* Root = Target->GetRootComponent())
    {
        TArray<USceneComponent*> Components; Root->GetChildrenComponents(true, Components); Components.Add(Root);
        for (USceneComponent* Component : Components)
            if (auto* Mesh = Cast<UMeshComponent>(Component); Mesh && Mesh->IsVisible()) CurrentMeshes.Add(Mesh);
    }
    for (auto It = PreviousDepth.CreateIterator(); It; ++It)
    {
        if (!CurrentMeshes.Contains(It.Key().Get()))
        {
            if (auto* Mesh = It.Key().Get()) RestoreMesh(Mesh, It.Value());
            It.RemoveCurrent();
        }
    }
    if (!EnsureOutlinePass()) { return; }
    for (UMeshComponent* Mesh : CurrentMeshes)
    {
        if (PreviousDepth.Contains(Mesh)) continue;
        // Preserve the lower bits the old write mask actually rendered,
        // including zero when custom depth was disabled. Do not activate
        // dormant FX bits just because selection enables the depth pass.
        static_assert(int32(ERendererStencilMask::ERSM_128) == 9);
        constexpr int32 WriteBits[] = {255,255,1,2,4,8,16,32,64,128};
        const int32 MaskIndex = int32(Mesh->CustomDepthStencilWriteMask);
        const int32 LowerBits = Mesh->bRenderCustomDepth && MaskIndex >= 0 && MaskIndex < UE_ARRAY_COUNT(WriteBits) ?
            (Mesh->CustomDepthStencilValue & WriteBits[MaskIndex] & ~SelectionStencilBit) : 0;
        const int32 Applied = LowerBits | SelectionStencilBit;
        PreviousDepth.Add(Mesh, {bool(Mesh->bRenderCustomDepth), Mesh->CustomDepthStencilValue, Mesh->CustomDepthStencilWriteMask, Applied});
        Mesh->SetCustomDepthStencilValue(Applied);
        Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_255);
        Mesh->SetRenderCustomDepth(true);
    }
}

bool USFActionTargetComponent::EnsureOutlinePass()
{
    auto* Player = Cast<APlayerController>(GetOwner());
    if (!Player || !Player->IsLocalController() || !Player->PlayerCameraManager) return false;
    APlayerCameraManager* Camera = Player->PlayerCameraManager;
    if (!HighlightMaterial) HighlightMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Frontier/Gameplay/Interaction/M_ActionTargetOutline.M_ActionTargetOutline"));
    if (!HighlightMaterial) return false;
    if (OutlineCamera.Get() != Camera || !OutlineModifier)
    {
        if (OutlineCamera.IsValid() && OutlineModifier) OutlineCamera->RemoveCameraModifier(OutlineModifier);
        OutlineCamera = Camera;
        OutlineModifier = Cast<USFActionTargetCameraModifier>(OutlineCamera->AddNewCameraModifier(USFActionTargetCameraModifier::StaticClass()));
        if (OutlineModifier) OutlineModifier->Selection = this;
    }
    return OutlineModifier != nullptr;
}

bool USFActionTargetComponent::HasSelectionOutline() const
{ return Selected.IsValid() && !PreviousDepth.IsEmpty() && OutlineCamera.IsValid() && OutlineModifier && HighlightMaterial; }

bool USFActionTargetComponent::IsMeshHighlighted(const UMeshComponent* Mesh) const
{
    return Mesh && HasSelectionOutline() && PreviousDepth.Contains(TWeakObjectPtr<UMeshComponent>(const_cast<UMeshComponent*>(Mesh))) &&
        Mesh->bRenderCustomDepth && (Mesh->CustomDepthStencilValue & SelectionStencilBit) != 0;
}

void USFActionTargetComponent::RestoreMesh(UMeshComponent* Mesh, const FPreviousDepth& Saved)
{
    const bool bUnchanged = Mesh->bRenderCustomDepth && Mesh->CustomDepthStencilValue == Saved.AppliedStencil &&
        Mesh->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_255;
    if (bUnchanged)
    {
        Mesh->SetCustomDepthStencilValue(Saved.Stencil);
        Mesh->SetCustomDepthStencilWriteMask(Saved.WriteMask);
        Mesh->SetRenderCustomDepth(Saved.bRender);
    }
    else
    {
        // Another FX writer owns this effective state now. Remove only our
        // bit; restoring an older restrictive mask would suppress its new FX.
        Mesh->SetCustomDepthStencilValue(Mesh->CustomDepthStencilValue & ~SelectionStencilBit);
    }
}

void USFActionTargetComponent::RestoreHighlight()
{
    for (const auto& Previous : PreviousDepth)
        if (UMeshComponent* Mesh = Previous.Key.Get()) RestoreMesh(Mesh, Previous.Value);
    PreviousDepth.Reset();
}

void USFActionTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Select(nullptr);
    if (OutlineCamera.IsValid() && OutlineModifier) OutlineCamera->RemoveCameraModifier(OutlineModifier);
    OutlineModifier = nullptr; OutlineCamera.Reset();
    Super::EndPlay(EndPlayReason);
}
