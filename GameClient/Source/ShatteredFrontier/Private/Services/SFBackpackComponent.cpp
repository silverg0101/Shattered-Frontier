#include "Services/SFBackpackComponent.h"

#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"
#include "Gathering/SFGatheringPilot.h"
#include "Gathering/SFStationRecipes.h"
#include "Inventory/SFInventoryLedgerClient.h"
#include "Services/SFGameplayServicesClient.h"
#if SF_WITH_MINIMAL_INVENTORY
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryTransactionLibrary.h"
#endif

USFBackpackComponent* USFBackpackComponent::FindOrCreate(APlayerController* Controller)
{
    if (!IsValid(Controller) || !Controller->HasAuthority()) { return nullptr; }
    if (auto* Existing = Controller->FindComponentByClass<USFBackpackComponent>()) { return Existing->Ledger ? Existing : nullptr; }
    if (!GetDefault<USFInventoryLedgerSettings>()->bEnabled) { return nullptr; }
    auto* Backpack = NewObject<USFBackpackComponent>(Controller, TEXT("Backpack"));
    Backpack->RegisterComponent();
    return Backpack->Initialize(Controller) ? Backpack : nullptr;
}

TSharedPtr<FSFGameplayServicesClient> USFBackpackComponent::Services()
{
    // One client per server process; it holds the shared transport (one channel per endpoint).
    static TSharedPtr<FSFGameplayServicesClient> Client = FSFGameplayServicesClient::Create(*GetDefault<USFGameplayServicesSettings>());
    return Client;
}

bool USFBackpackComponent::Initialize(APlayerController* Controller)
{
    Ledger = FSFInventoryLedgerClient::Create(*GetDefault<USFInventoryLedgerSettings>());
    if (!Ledger) { return false; }  // fail closed: an enabled ledger never falls back to session-only grants
    BindIdentity();
    const auto* Settings = GetDefault<USFGatheringPilotSettings>();
    Capacity = Settings->InventorySlots * Settings->MaterialStackUnits;
    return true;
}

bool USFBackpackComponent::IsValidDevName(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len() > 24) { return false; }
    for (TCHAR C : Name) { if (!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '_')) { return false; } }
    return true;
}

void USFBackpackComponent::AssignDevIdentity(APlayerController* Controller, const FString& Name)
{
    USFBackpackComponent* Backpack = IsValidDevName(Name) ? FindOrCreate(Controller) : nullptr;
    if (!Backpack || Backpack->bIdentityUsed) { return; }
    Backpack->DevName = Name;
    Backpack->BindIdentity();
    UE_LOG(LogTemp, Display, TEXT("DEV_IDENTITY_BOUND Character=%s Container=%s"), *Backpack->CharacterId, *Backpack->ContainerId);
}

void USFBackpackComponent::BindIdentity()
{
    // Development key per server connection (INT-03 6.3); character identity arrives with DEC-04. A controller gets its
    // connection after its components begin play, so the key is (re)bound until the first mutation uses it.
    if (bIdentityUsed) { return; }
    const auto* Controller = Cast<APlayerController>(GetOwner());
    const UNetConnection* Connection = Controller ? Controller->GetNetConnection() : nullptr;
    ContainerId = !DevName.IsEmpty() ? TEXT("ctn_dev_") + DevName
        : Connection ? FString::Printf(TEXT("ctn_dev_%u"), Connection->GetConnectionHandle().GetParentConnectionId()) : FString(TEXT("ctn_dev_local"));
    CharacterId = TEXT("chr_dev_") + ContainerId.RightChop(8);
}

void USFBackpackComponent::Enqueue(FSFBackpackMutation Mutation)
{
    Queue.Add(MoveTemp(Mutation));
    Pump();
}

void USFBackpackComponent::Pump()
{
    if (bBusy || Queue.IsEmpty() || !Ledger) { return; }
    BindIdentity(); bIdentityUsed = true;
    bBusy = true;
    if (Revision >= 0) { Execute(0); return; }
    Refresh([Weak = TWeakObjectPtr<USFBackpackComponent>(this)](bool bOk)
    {
        USFBackpackComponent* Self = Weak.Get();
        if (!Self) { return; }
        if (bOk) { Self->Execute(0); return; }
        // The ledger is unreachable: this mutation fails without touching the world; later ones try again.
        FSFBackpackMutation Failed = MoveTemp(Self->Queue[0]);
        Self->Queue.RemoveAt(0);
        Self->bBusy = false;
        UE_LOG(LogTemp, Display, TEXT("BACKPACK_OUTCOME Label=%s Op=%s Outcome=Unavailable Container=%s Stage=Read"), *Failed.Label, *Failed.OperationId, *Self->ContainerId);
        if (Failed.Finished) { Failed.Finished(ESFBackpackOutcome::Unavailable); }
        Self->Pump();
    });
}

void USFBackpackComponent::Execute(int32 Attempt)
{
    const FSFBackpackMutation& Front = Queue[0];
    const FString Operation = Attempt == 0 ? Front.OperationId : Front.OperationId + TEXT(":r1");
    Front.Run(Operation, Revision, [Weak = TWeakObjectPtr<USFBackpackComponent>(this), Attempt, Operation](ESFBackpackOutcome Outcome, int64 After)
    {
        USFBackpackComponent* Self = Weak.Get();
        if (!Self || Self->Queue.IsEmpty()) { return; }
        if (Outcome == ESFBackpackOutcome::Stale && Attempt == 0)
        {
            Self->Revision = -1;
            Self->Refresh([Weak](bool bOk)
            {
                USFBackpackComponent* Again = Weak.Get();
                if (!Again || Again->Queue.IsEmpty()) { return; }
                if (bOk) { Again->Execute(1); return; }
                FSFBackpackMutation Failed = MoveTemp(Again->Queue[0]);
                Again->Queue.RemoveAt(0); Again->bBusy = false;
                if (Failed.Finished) { Failed.Finished(ESFBackpackOutcome::Unavailable); }
                Again->Pump();
            });
            return;
        }
        // Anything but a commit with a known revision leaves the cache unknown, so the next mutation re-reads.
        Self->Revision = Outcome == ESFBackpackOutcome::Committed ? After : -1;
        FSFBackpackMutation Done = MoveTemp(Self->Queue[0]);
        Self->Queue.RemoveAt(0);
        Self->bBusy = false;
        UE_LOG(LogTemp, Display, TEXT("BACKPACK_OUTCOME Label=%s Op=%s Outcome=%d Container=%s Revision=%lld"), *Done.Label, *Operation, int32(Outcome), *Self->ContainerId, Self->Revision);
        if (Done.Finished) { Done.Finished(Outcome); }
        Self->Pump();
    });
}

void USFBackpackComponent::Refresh(TFunction<void(bool)> Done)
{
    // EnsureContainer is idempotent: it creates the backpack once and otherwise returns the current view.
    FSFLedgerRequest Ensure; Ensure.Rpc = ESFLedgerRpc::EnsureContainer; Ensure.ContainerId = ContainerId;
    Ensure.OwnerId = CharacterId; Ensure.Capacity = Capacity;
    Ledger->Call(Ensure, [Weak = TWeakObjectPtr<USFBackpackComponent>(this), Done = MoveTemp(Done)](const FSFLedgerResponse& Response)
    {
        USFBackpackComponent* Self = Weak.Get();
        if (!Self) { return; }
        const bool bOk = Response.Status == ESFLedgerStatus::Ok;
        if (bOk) { Self->Revision = Response.Revision; }
        Done(bOk);
    });
}

void USFBackpackComponent::Reconcile()
{
    FSFBackpackMutation Read; Read.Label = TEXT("reconcile"); Read.OperationId = TEXT("read");
    Read.Run = [Weak = TWeakObjectPtr<USFBackpackComponent>(this)](const FString&, int64, TFunction<void(ESFBackpackOutcome, int64)> Report)
    {
        USFBackpackComponent* Self = Weak.Get();
        if (!Self) { Report(ESFBackpackOutcome::Unavailable, -1); return; }
        FSFLedgerRequest Get; Get.Rpc = ESFLedgerRpc::GetContainer; Get.ContainerId = Self->ContainerId;
        Self->Ledger->Call(Get, [Weak, Report](const FSFLedgerResponse& View)
        {
            USFBackpackComponent* Again = Weak.Get();
            if (!Again || View.Status != ESFLedgerStatus::Ok) { Report(ESFBackpackOutcome::Unavailable, -1); return; }
#if SF_WITH_MINIMAL_INVENTORY
            // Every economy item the pilot knows, plus whatever the ledger holds; tools are not ledger items.
            TSet<FString> Items; View.Quantities.GetKeys(Items);
            for (const FSFStationRecipe& Recipe : SFStationRecipes::Shipped())
            { for (const FSFItemStack& Stack : Recipe.Inputs) { Items.Add(Stack.DefinitionId); } for (const FSFItemStack& Stack : Recipe.Outputs) { Items.Add(Stack.DefinitionId); } }
            Items.Append({TEXT("item/oak_log"), TEXT("item/iron_ore"), TEXT("item/rough_stone"), TEXT("item/boar_hide")});
            const auto* Inventory = Again->GetOwner() ? Again->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
            TMap<FString, int64> Deltas;
            for (const FString& Item : Items)
            {
                const int64 Bag = Inventory ? UInventoryTransactionLibrary::GetItemQuantity(const_cast<UInventoryComponent*>(Inventory),
                    TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(SFStationRecipes::ItemAssetPath(Item)))) : 0;
                if (View.Quantities.FindRef(Item) != Bag) { Deltas.Add(Item, View.Quantities.FindRef(Item) - Bag); }
            }
            UE_LOG(LogTemp, Display, TEXT("BACKPACK_RECONCILED Container=%s Revision=%lld Corrections=%d"), *Again->ContainerId, View.Revision, Deltas.Num());
            Again->ApplyToBag(Deltas);
#endif
            Report(ESFBackpackOutcome::Committed, View.Revision);
        });
    };
    Enqueue(MoveTemp(Read));
}

void USFBackpackComponent::ApplyToBag(const TMap<FString, int64>& Deltas) const
{
#if SF_WITH_MINIMAL_INVENTORY
    auto* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
    if (!Inventory) { return; }
    FInventoryTransactionChange Change; Change.Inventory = Inventory;
    for (const TPair<FString, int64>& Delta : Deltas)
    {
        const FString Path = SFStationRecipes::ItemAssetPath(Delta.Key);
        if (Delta.Value == 0) { continue; }
        if (!LoadObject<UInventoryItemDefinition>(nullptr, *Path))
        { UE_LOG(LogTemp, Warning, TEXT("BACKPACK_PROJECTION_UNKNOWN_ITEM Item=%s Delta=%lld"), *Delta.Key, Delta.Value); continue; }
        FInventoryItemQuantity Item; Item.Item = TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(Path)); Item.Quantity = int32(FMath::Abs(Delta.Value));
        (Delta.Value > 0 ? Change.AddItems : Change.RemoveItems).Add(Item);
    }
    if ((Change.AddItems.Num() || Change.RemoveItems.Num()) && UInventoryTransactionLibrary::ExecuteInventoryTransaction({Change}) != EInventoryTransactionResult::Success)
    { UE_LOG(LogTemp, Error, TEXT("BACKPACK_PROJECTION_MISMATCH Container=%s"), *ContainerId); }
#endif
}

#if WITH_DEV_AUTOMATION_TESTS
USFBackpackComponent* USFBackpackComponent::CreateForTests(APlayerController* Controller, TSharedPtr<FSFInventoryLedgerClient> InLedger)
{
    auto* Backpack = NewObject<USFBackpackComponent>(Controller, TEXT("Backpack"));
    Backpack->RegisterComponent();
    Backpack->Ledger = InLedger;
    Backpack->ContainerId = TEXT("ctn_dev_local"); Backpack->CharacterId = TEXT("chr_dev_local");
    const auto* Settings = GetDefault<USFGatheringPilotSettings>();
    Backpack->Capacity = Settings->InventorySlots * Settings->MaterialStackUnits;
    return Backpack;
}
#endif
