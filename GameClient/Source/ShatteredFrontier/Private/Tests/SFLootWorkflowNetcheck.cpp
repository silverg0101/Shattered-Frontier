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
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UObject/StrongObjectPtr.h"

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
    bool bGroundSeen = false, bObserverReported = false, bGroundBarrierWritten = false;
    double BarrierAt = -1;
    TStrongObjectPtr<USFItemDragOperation> PendingDrag;
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
FPointerEvent Pointer(bool bShift, const FVector2D& Position = FVector2D(1, 1), bool bDown = true)
{
    return FPointerEvent(0, Position, Position, bDown ? TSet<FKey>{EKeys::LeftMouseButton} : TSet<FKey>{},
        EKeys::LeftMouseButton, 0, FModifierKeysState(bShift, false, false, false, false, false, false, false, false));
}
}

int32 FSFLootWorkflowNetcheck::FindAuthorizedRow(const TArray<FSFInventoryEntry>& Rows, const TArray<FSFLootEntry>& Items)
{
    return Rows.IndexOfByPredicate([&Items](const FSFInventoryEntry& Row)
    {
        return Items.ContainsByPredicate([&Row](const FSFLootEntry& Item)
        { return Item.DefinitionId == Row.DefinitionId && Item.InstanceId == Row.InstanceId && Item.Quantity == Row.Count; });
    });
}
bool FSFLootWorkflowNetcheck::HasPersonalSource(UWorld* World)
{
    bool bPersonal = false;
    for (TActorIterator<ASFLootSourceActor> It(World); It; ++It) bPersonal |= !It->IsGround();
    return bPersonal;
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
    FString GroundBarrier; FParse::Value(FCommandLine::Get(), TEXT("LootWorkflowGroundSeen="), GroundBarrier);
    if (Role == TEXT("Observer"))
    {
        const bool bPersonal = HasPersonalSource(World);
        for (TActorIterator<ASFLootSourceActor> It(World); It; ++It)
            if (It->IsGround() && It->Mesh->GetStaticMesh()) State.bGroundSeen = true;
        if (bPersonal) { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL ObserverSawPersonalCorpse")); State.Stage = 99; return; }
        if (State.bGroundSeen && !State.bGroundBarrierWritten && !GroundBarrier.IsEmpty())
            State.bGroundBarrierWritten = FFileHelper::SaveStringToFile(TEXT("Observer received the meshed ground source."), *GroundBarrier);
        FString Barrier; FParse::Value(FCommandLine::Get(), TEXT("LootWorkflowBarrier="), Barrier);
        if (!Barrier.IsEmpty() && FPaths::FileExists(Barrier) && State.BarrierAt < 0) State.BarrierAt = Now;
        if (State.bGroundSeen && State.BarrierAt >= 0 && Now >= State.BarrierAt + 1 && !State.bObserverReported)
        {
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_OBSERVER PersonalSources=0 GroundMesh=1 DriverComplete=1"));
            State.bObserverReported = true; // Continue monitoring through runner teardown.
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
        if (State.Stage == 1 && !State.Source.IsValid() && Loot.ExternalItems.IsEmpty())
        { Stack->Close(Loot.ExternalWidget); State.Stage = 2; State.NextAt = Now + 1; return; }
        if (!Loot.ExternalWidget || !Player->InventoryWidget || !Stack->IsOpen(Loot.ExternalWidget) || !Stack->IsOpen(Player->InventoryWidget)) return;
        auto* Window = Loot.ExternalWidget.Get();
        // A real NativeTick populates cells from the RPC result, as in normal play.
        if (Window->ShownItems.IsEmpty()) return;
        int32 Index = FindAuthorizedRow(Window->ShownItems, Loot.ExternalItems);
        if (State.Stage == 3)
        {
            Index = Window->ShownItems.IndexOfByPredicate([](const FSFInventoryEntry& Item) { return Item.DefinitionId == TEXT("item/wolf_pelt"); });
            if (Index == INDEX_NONE) return;
        }
        if (Index == INDEX_NONE) return;
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
            State.Stage = 1; // Drain every real rolled row, including bonus recipes.
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
            State.PendingDrag.Reset(Cast<USFItemDragOperation>(Operation)); State.Stage = 14;
        }
    }
    else if (State.Stage == 14 && State.PendingDrag.IsValid())
    {
        auto* Surface = State.PendingDrag->WorldDropTarget.Get(); if (!Surface) return;
        const FGeometry Geometry = Surface->GetCachedGeometry();
        if (Geometry.GetLocalSize().X < 100 || Geometry.GetLocalSize().Y < 100) return;
        for (double X : {.9, .7, .5}) for (double Y : {.4, .6, .2})
        {
            const FVector2D Point = Geometry.LocalToAbsolute(FVector2D(Geometry.GetLocalSize()) * FVector2D(X,Y));
            const auto Release = Pointer(false, Point, false);
            if (Surface->NativeOnDrop(Geometry, FDragDropEvent(Release, nullptr), State.PendingDrag.Get()))
            {
                State.PendingDrag->Drop_Implementation(Release); State.PendingDrag.Reset(); State.Stage = 12;
                UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_DROP_REQUEST WorldRelease=1 Item=%s"), *State.Item); return;
            }
        }
    }
    else if (State.Stage == 12)
    {
        // Keep the actual replicated ground model alive until the observer has
        // received it, then continue monitoring privacy through the end barrier.
        if (GroundBarrier.IsEmpty() || !FPaths::FileExists(GroundBarrier)) return;
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
