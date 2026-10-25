#include "Tests/SFLootWorkflowNetcheck.h"
#if WITH_EDITOR
#include "Inventory/SFLootComponent.h"
#include "Inventory/SFLootSourceActor.h"
#include "Inventory/SFInventory.h"
#include "Framework/SFPlayerController.h"
#include "Framework/SFGameMode.h"
#include "Characters/SFE1Character.h"
#include "Combat/SFCombatPilot.h"
#include "Interaction/SFActionTargetComponent.h"
#include "Services/SFBackpackComponent.h"
#include "UI/SFInventoryWidget.h"
#include "UI/SFWindowStack.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Input/Events.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
struct FWorkflowState
{
    int32 Stage = 0;
    double NextAt = 0;
    TWeakObjectPtr<ASFLootSourceActor> Source;
    FVector BodyAt = FVector::ZeroVector;
    FString Item, Instance;
    int32 Quantity = 0, Before = 0;
    bool bSeenCorpse = false;
};
TMap<TWeakObjectPtr<USFLootComponent>, FWorkflowState> States;
int32 CountItem(ASFPlayerController* Player, const FString& Id)
{
    auto* Bag = Player->FindComponentByClass<USFInventoryComponent>();
    int32 Total = 0;
    for (int32 I = 0; Bag && I < Bag->NumSlots; ++I)
    {
        TSoftObjectPtr<USFItemDefinition> Item; int32 Quantity = 0;
        if (Bag->GetSlot(I, Item, Quantity))
        { if (const auto* Definition = Item.LoadSynchronous(); Definition && Definition->UniqueId == Id) Total += Quantity; }
    }
    return Total;
}
FPointerEvent Pointer(bool bShift, const FVector2D& Position = FVector2D(-10000, -10000))
{
    return FPointerEvent(0, Position, Position, TSet<FKey>{EKeys::LeftMouseButton},
        EKeys::LeftMouseButton, 0, FModifierKeysState(bShift, false, false, false, false, false, false, false, false));
}
}

void FSFLootWorkflowNetcheck::Tick(USFLootComponent& Loot)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("LootWorkflowNetcheck"))) return;
    auto* Player = Cast<ASFPlayerController>(Loot.GetOwner());
    auto* Pawn = Player ? Cast<ASFE1Character>(Player->GetPawn()) : nullptr;
    if (!Pawn || !Player->GetWorld()) return;
    auto& State = States.FindOrAdd(&Loot);
    UWorld* World = Player->GetWorld();
    const double Now = World->GetTimeSeconds();
    if (Now < State.NextAt || State.Stage == 99) return;
    State.NextAt = Now + .2;
    if (Player->HasAuthority())
    {
        auto* Identity = Player->FindComponentByClass<USFBackpackComponent>();
        auto* Mode = World->GetAuthGameMode<ASFGameMode>();
        if (!Identity || Identity->GetCharacterId() != TEXT("chr_dev_alpha") || !Mode || Identity->IsBusy() || Now < 12) return;
        ASFPlayerController* Observer = nullptr;
        for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        {
            auto* Other = Cast<ASFPlayerController>(It->Get());
            auto* Pack = Other ? Other->FindComponentByClass<USFBackpackComponent>() : nullptr;
            if (Pack && Pack->GetCharacterId() == TEXT("chr_dev_beta") && Other->GetPawn() && !Pack->IsBusy()) Observer = Other;
        }
        if (!Observer) return;
        // Fixture positioning and a lethal hit use the existing live actor and
        // real RewardKill path. Canonical profile, eligibility and rolls stay intact.
        if (State.Stage == 0 || State.Stage == 2)
        {
            ASFPilotBoar* Victim = nullptr;
            const FString Creature = State.Stage == 0 ? TEXT("npc/boar") : TEXT("npc/wolf");
            for (TActorIterator<ASFPilotBoar> It(World); It; ++It)
                if (It->Health() > 0 && It->GetCreatureId() == Creature && !It->HasSingleDrop() && !Mode->IsSafe(It->GetActorLocation())) { Victim = *It; break; }
            if (!Victim) return;
            if (State.Stage == 0) State.BodyAt = Victim->GetActorLocation();
            else Victim->SetActorLocation(State.BodyAt + FVector(600, 0, 0));
            const FVector At = Victim->GetActorLocation();
            if (Mode->IsSafe(At + FVector(130, 0, 0))) return;
            Pawn->SetActorLocation(At + FVector(130, 0, 20), false, nullptr, ETeleportType::TeleportPhysics);
            Observer->GetPawn()->SetActorLocation(At + FVector(130, 220, 20), false, nullptr, ETeleportType::TeleportPhysics);
            Pawn->GetCharacterMovement()->DisableMovement();
            if (auto* Other = Cast<ASFE1Character>(Observer->GetPawn())) Other->GetCharacterMovement()->DisableMovement();
            Victim->ReceivePilotDamage(Pawn, 1000000);
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_DEFEAT Creature=%s Dead=%d"), *Creature, Victim->Health() <= 0);
            ++State.Stage; State.bSeenCorpse = false;
        }
        else if (State.Stage == 1 || State.Stage == 3)
        {
            bool bCorpse = false;
            for (TActorIterator<ASFLootSourceActor> It(World); It; ++It) if (!It->IsGround()) bCorpse = true;
            if (bCorpse) State.bSeenCorpse = true;
            if (State.bSeenCorpse && !bCorpse) { ++State.Stage; State.NextAt = Now + 1; }
        }
        return;
    }
    if (!Player->IsLocalController()) return;
    FString Role; FParse::Value(FCommandLine::Get(), TEXT("LootWorkflowRole="), Role);
    if (Role == TEXT("Observer"))
    {
        for (TActorIterator<ASFLootSourceActor> It(World); It; ++It)
        {
            if (!It->IsGround()) { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL ObserverSawPersonalCorpse")); State.Stage = 99; return; }
            if (It->Mesh->GetStaticMesh())
            { UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_OBSERVER PersonalSources=0 GroundMesh=1")); State.Stage = 99; return; }
        }
        return;
    }
    if (Role != TEXT("Driver")) return;
    auto* Stack = Player->GetWindowStack();
    auto* Target = Player->FindComponentByClass<USFActionTargetComponent>();
    if (!Stack || !Target) return;
    if (State.Stage == 0 || State.Stage == 2)
    {
        for (TActorIterator<ASFLootSourceActor> It(World); It; ++It)
        {
            if (It->IsGround() || !It->Mesh->GetStaticMesh()) continue;
            if (!It->InteractionBounds->GetUnscaledBoxExtent().Equals(It->HitShape.Extent, .01) ||
                !It->InteractionBounds->GetRelativeLocation().Equals(It->HitShape.Center, .01)) continue;
            // An outer cross-section beyond the constructor's 25 cm box must
            // collide in this actual network client, not just at body centre.
            FHitResult EdgeHit; const auto Transform = It->GetActorTransform();
            const float Edge = It->HitShape.Extent.X * .8f;
            if (Edge <= 25 || !World->LineTraceSingleByChannel(EdgeHit,
                Transform.TransformPosition(It->HitShape.Center + FVector(Edge, -200, 0)),
                Transform.TransformPosition(It->HitShape.Center + FVector(Edge, 200, 0)), ECC_Visibility) || EdgeHit.GetActor() != *It) continue;
            FVector Start; FRotator Rotation; Player->GetPlayerViewPoint(Start, Rotation);
            Target->RefreshFromRay(Start, (It->InteractionBounds->GetComponentLocation() - Start).GetSafeNormal());
            if (Target->GetTarget() != *It || !It->Mesh->GetOverlayMaterial()) continue;
            State.Source = *It;
            Player->DispatchSelectedAction(Target->GetSelection());
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_TARGET Corpse=1 ExactTarget=1 Rim=1 ClientBounds=1"));
            ++State.Stage; return;
        }
    }
    else if (State.Stage == 1 || State.Stage == 3)
    {
        if (!Loot.ExternalWidget || !Player->InventoryWidget || !Stack->IsOpen(Loot.ExternalWidget) || !Stack->IsOpen(Player->InventoryWidget)) return;
        auto* Window = Loot.ExternalWidget.Get();
        // A real NativeTick populates cells from the RPC result, as in normal play.
        if (Window->ShownItems.IsEmpty()) return;
        int32 Index = 0;
        if (State.Stage == 3)
        {
            Index = Window->ShownItems.IndexOfByPredicate([](const FSFInventoryEntry& Item) { return Item.DefinitionId == TEXT("item/wolf_pelt"); });
            if (Index == INDEX_NONE) return;
        }
        const auto Item = Window->ShownItems[Index];
        auto* Cell = Window->GetCell(Index); if (!Cell) return;
        State.Item = Item.DefinitionId; State.Instance = Item.InstanceId; State.Quantity = Item.Count; State.Before = CountItem(Player, State.Item);
        if (State.Stage == 1)
        {
            Cell->NativeOnMouseButtonDown(Cell->GetCachedGeometry(), Pointer(true));
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_SHIFT Windows=2 Item=%s Quantity=%d Instance=%s"), *State.Item, State.Quantity, *State.Instance);
            State.Stage = 10;
        }
        else
        {
            UDragDropOperation* Operation = nullptr;
            Cell->NativeOnDragDetected(Cell->GetCachedGeometry(), Pointer(false), Operation);
            if (!Operation || !Player->InventoryWidget->NativeOnDrop(Player->InventoryWidget->GetCachedGeometry(), FDragDropEvent(Pointer(false), nullptr), Operation))
            { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL NativeDragRejected")); State.Stage = 99; return; }
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_DRAG Windows=2 Item=%s Quantity=%d Instance=%s"), *State.Item, State.Quantity, *State.Instance);
            State.Stage = 11;
        }
    }
    else if (State.Stage == 10 || State.Stage == 11)
    {
        if (CountItem(Player, State.Item) != State.Before + State.Quantity) return;
        if (State.Stage == 10)
        {
            Stack->Close(Loot.ExternalWidget); State.Stage = 2; State.NextAt = Now + 1;
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_SHIFT_COMMITTED"));
        }
        else
        {
            auto* Own = Player->InventoryWidget.Get();
            const int32 Index = Own->ShownItems.IndexOfByPredicate([&State](const FSFInventoryEntry& Item) { return Item.DefinitionId == State.Item && Item.InstanceId == State.Instance; });
            if (Index == INDEX_NONE || !Own->GetCell(Index)) return;
            UDragDropOperation* Operation = nullptr; auto* Cell = Own->GetCell(Index);
            Cell->NativeOnDragDetected(Cell->GetCachedGeometry(), Pointer(false), Operation);
            if (!Operation) return;
            Stack->Close(Loot.ExternalWidget);
            Cell->NativeOnDragCancelled(FDragDropEvent(Pointer(false), nullptr), Operation);
            State.Stage = 12;
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_DROP_REQUEST NativeCancel=1 Item=%s"), *State.Item);
        }
    }
    else if (State.Stage == 12)
    {
        for (TActorIterator<ASFLootSourceActor> It(World); It; ++It)
        {
            if (!It->IsGround() || !It->Mesh->GetStaticMesh() || CountItem(Player, State.Item) != State.Before) continue;
            FVector Start; FRotator Rotation; Player->GetPlayerViewPoint(Start, Rotation);
            Target->RefreshFromRay(Start, (It->InteractionBounds->GetComponentLocation() - Start).GetSafeNormal());
            if (Target->GetTarget() != *It || !It->Mesh->GetOverlayMaterial()) continue;
            State.Source = *It;
            Player->DispatchSelectedAction(Target->GetSelection()); State.Stage = 13;
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_GROUND Mesh=1 ClientBounds=1 Rim=1 DirectAction=1")); return;
        }
    }
    else if (State.Stage == 13 && CountItem(Player, State.Item) == State.Before + State.Quantity && !State.Source.IsValid())
    {
        if (Stack->IsOpen(Loot.ExternalWidget)) { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL PickupOpenedWidget")); State.Stage = 99; return; }
        Target->RefreshFromRay(Pawn->GetActorLocation(), FVector::UpVector);
        if (Target->GetTarget()) return;
        UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_DONE NativeShift=1 NativeDrag=1 GroundModel=1 PickupWidget=0 TargetCleared=1")); State.Stage = 99;
    }
}
#endif
