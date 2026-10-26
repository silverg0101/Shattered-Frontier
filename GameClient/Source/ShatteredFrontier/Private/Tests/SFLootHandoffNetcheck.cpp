#include "Tests/SFLootWorkflowNetcheck.h"
#if WITH_EDITOR
#include "Inventory/SFLootComponent.h"
#include "Inventory/SFLootSourceActor.h"
#include "Inventory/SFInventory.h"
#include "Framework/SFPlayerController.h"
#include "Characters/SFE1Character.h"
#include "Interaction/SFActionTargetComponent.h"
#include "Services/SFBackpackComponent.h"
#include "UI/SFWindowStack.h"
#include "UI/SFInventoryWidget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Headless two-client handoff. Alpha opens its own corpse through the shared
// action dispatch, takes one row and drops it with the same RPCs the bag widget
// sends; beta collects the public ground source with the same dispatch the F
// action uses. The server half (one real boar defeat) lives in Tick().
namespace
{
struct FHandoffState
{
    int32 Stage = 0;
    double NextAt = 0, FirstAt = -1, Deadline = 0;
    FString Item;
    int32 Quantity = 0, Before = 0;
    bool bGroundSeen = false;
};
TMap<TWeakObjectPtr<USFLootComponent>, FHandoffState> HandoffStates;

int32 BagCount(ASFPlayerController* Player, const FString& Id)
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
}


void FSFLootWorkflowNetcheck::TickHandoffClient(USFLootComponent& Loot)
{
    auto* Player = Cast<ASFPlayerController>(Loot.GetOwner());
    auto* Pawn = Player ? Cast<ASFE1Character>(Player->GetPawn()) : nullptr;
    auto* Target = Player ? Player->FindComponentByClass<USFActionTargetComponent>() : nullptr;
    if (!Pawn || !Target) return;
    UWorld* World = Player->GetWorld();
    auto& State = HandoffStates.FindOrAdd(&Loot);
    const auto FindSource = [](UWorld* In, bool bGround) -> ASFLootSourceActor*
    {
        for (TActorIterator<ASFLootSourceActor> It(In); It; ++It)
            if (It->IsGround() == bGround && It->Mesh->GetStaticMesh()) return *It;
        return nullptr;
    };
    const double Now = World->GetTimeSeconds();
    if (State.Stage == 99 || Now < State.NextAt) return;
    State.NextAt = Now + .2;
    if (State.FirstAt < 0) State.FirstAt = Now;
    FString Role; FParse::Value(FCommandLine::Get(), TEXT("LootHandoffRole="), Role);
    const FString Item = TEXT("item/boar_hide");
    const auto Fail = [&State](const TCHAR* Reason) { UE_LOG(LogTemp, Error, TEXT("LOOT_HANDOFF_FAIL %s"), Reason); State.Stage = 99; };
    // Personal corpse loot is owner-only: beta must never receive one, in any phase.
    if (Role == TEXT("Beta") && HasPersonalSource(World)) { Fail(TEXT("BetaSawPersonalCorpse")); return; }
    const auto Aim = [Player, Target](ASFLootSourceActor* Source)
    {
        FVector Start; FRotator Rotation; Player->GetPlayerViewPoint(Start, Rotation);
        Target->RefreshFromRay(Start, (Source->InteractionBounds->GetComponentLocation() - Start).GetSafeNormal());
        if (Target->GetTarget() != Source) return false;
        Player->DispatchSelectedAction(Target->GetSelection()); // Same dispatch as the F/click action.
        return true;
    };

    if (FParse::Param(FCommandLine::Get(), TEXT("LootHandoffRestore")))
    {
        // After a server restart: report the reconnected bag once it has loaded.
        const int32 Count = BagCount(Player, Item);
        if (Now < State.FirstAt + 10 || (Role == TEXT("Beta") && Count == 0 && Now < State.FirstAt + 40)) return;
        UE_LOG(LogTemp, Display, TEXT("LOOT_HANDOFF_RESTORED Role=%s Item=%s Count=%d Ground=%d Personal=%d"),
            *Role, *Item, Count, FindSource(World, true) != nullptr, HasPersonalSource(World));
        State.Stage = 99; return;
    }

    if (Role == TEXT("Alpha"))
    {
        if (State.Stage == 0)
        {
            ASFLootSourceActor* Corpse = FindSource(World, false);
            if (!Corpse || !Aim(Corpse)) return;
            State.Stage = 1; State.Deadline = Now + 15;
            UE_LOG(LogTemp, Display, TEXT("LOOT_HANDOFF_ALPHA_OPEN Corpse=1"));
        }
        else if (State.Stage == 1)
        {
            const auto* Row = Loot.ExternalItems.FindByPredicate([&Item](const FSFLootEntry& E) { return E.DefinitionId == Item; });
            if (!Row) { if (Now > State.Deadline) Fail(TEXT("AlphaCorpseHasNoHideRow")); return; }
            State.Item = Row->DefinitionId; State.Quantity = Row->Quantity; State.Before = BagCount(Player, State.Item);
            Loot.RequestTake(Row->DefinitionId, Row->Quantity, Row->InstanceId); // Bag widget Shift/drag RPC.
            State.Stage = 2; State.Deadline = Now + 20;
        }
        else if (State.Stage == 2)
        {
            if (BagCount(Player, State.Item) != State.Before + State.Quantity) { if (Now > State.Deadline) Fail(TEXT("AlphaTakeDidNotCommit")); return; }
            UE_LOG(LogTemp, Display, TEXT("LOOT_HANDOFF_ALPHA_TAKEN Item=%s Quantity=%d Bag=%d"), *State.Item, State.Quantity, State.Before + State.Quantity);
            if (Loot.ExternalWidget) Player->GetWindowStack()->Close(Loot.ExternalWidget);
            Loot.RequestDrop(State.Item, State.Quantity, FString()); // Bag widget world-drop RPC.
            State.Stage = 3; State.Deadline = Now + 20;
        }
        else if (State.Stage == 3)
        {
            if (BagCount(Player, State.Item) != State.Before || !FindSource(World, true))
            { if (Now > State.Deadline) Fail(TEXT("AlphaDropDidNotCommit")); return; }
            UE_LOG(LogTemp, Display, TEXT("LOOT_HANDOFF_ALPHA_DROPPED Item=%s Quantity=%d Bag=%d Ground=1"), *State.Item, State.Quantity, State.Before);
            State.Stage = 4; // Keep ticking: report if the item ever comes back to alpha.
        }
        else if (State.Stage == 4 && BagCount(Player, State.Item) != State.Before) Fail(TEXT("AlphaBagChangedAfterDrop"));
        return;
    }
    if (Role != TEXT("Beta")) return;
    ASFLootSourceActor* Ground = FindSource(World, true);
    if (State.Stage == 0)
    {
        if (!Ground) return;
        State.Before = BagCount(Player, Item); State.Deadline = Now + 45; State.Stage = 1;
        UE_LOG(LogTemp, Display, TEXT("LOOT_HANDOFF_BETA_SEES_GROUND Mesh=1"));
    }
    const int32 Gained = BagCount(Player, Item) - State.Before;
    if (State.Stage == 1)
    {
        if (Gained > 0 && !Ground)
        {
            UE_LOG(LogTemp, Display, TEXT("LOOT_HANDOFF_BETA_COLLECTED Item=%s Quantity=%d Widget=%d"), *Item, Gained,
                Loot.ExternalWidget && Player->GetWindowStack()->IsOpen(Loot.ExternalWidget));
            State.Stage = 2;
        }
        else if (Now > State.Deadline) Fail(TEXT("BetaPickupDidNotCommit"));
        else if (Ground && Gained == 0 && Aim(Ground))
        { State.NextAt = Now + 2; UE_LOG(LogTemp, Display, TEXT("LOOT_HANDOFF_BETA_ACTION Target=Ground")); }
    }
    else if (State.Stage == 2 && Ground) Fail(TEXT("GroundSourceReappeared"));
}
#endif
