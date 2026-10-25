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
    if (Selected == Target) { return; }
    for (const auto& Previous : PreviousOverlays)
    { if (UMeshComponent* Mesh = Previous.Key.Get()) { Mesh->SetOverlayMaterial(Previous.Value.Get()); } }
    PreviousOverlays.Reset(); Selected = Target;
    if (!Target) { return; }
    if (!HighlightMaterial) { HighlightMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Frontier/Gameplay/Interaction/M_ActionTargetRim.M_ActionTargetRim")); }
    if (!HighlightMaterial) { return; }
    TArray<UMeshComponent*> Meshes; Target->GetComponents(Meshes, true);
    for (UMeshComponent* Mesh : Meshes)
    { if (Mesh && Mesh->IsVisible()) { PreviousOverlays.Add(Mesh, Mesh->GetOverlayMaterial()); Mesh->SetOverlayMaterial(HighlightMaterial); } }
}

void USFActionTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{ Select(nullptr); Super::EndPlay(EndPlayReason); }
