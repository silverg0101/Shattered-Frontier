#include "Combat/SFCombatPilotRewards.h"
#include "Gathering/SFGatheringPilot.h"
#include "GameFramework/PlayerController.h"
#include "Services/SFGameplayServicesClient.h"
#include "Inventory/SFInventory.h"

const TCHAR* SFCombatPilotRewards::HidePath()
{
    return TEXT("/Game/Frontier/Gameplay/Pilot/DA_BoarHide.DA_BoarHide");
}

int32 SFCombatPilotRewards::HidesFor(TSet<FString>& Granted, const FString& DeathId, bool bProtected, int32 HidesPerBoar)
{
    bool bSeen = false;
    Granted.Add(DeathId, &bSeen);
    return bSeen || bProtected || DeathId.IsEmpty() ? 0 : FMath::Max(0, HidesPerBoar);
}

FSFGameplayRequest SFCombatPilotRewards::BuildDefeatRequest(const FString& EventId, const FString& KillerId, const FString& CharacterId,
    const FString& ContainerId, int64 ExpectedRevision, bool bKillerProtected, bool bBoarProtected)
{
    FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::GrantDefeatReward;
    Request.OperationId = EventId; Request.KillerCharacterId = KillerId; Request.CharacterId = CharacterId; Request.ContainerId = ContainerId;
    Request.ExpectedContainerRevision = ExpectedRevision;
    Request.bKillerProtected = bKillerProtected; Request.bBoarProtected = bBoarProtected;
    Request.TableId = TEXT("loot/greenlands_silverbrook_boar");
    return Request;
}

bool SFCombatPilotRewards::GrantHides(AActor* Owner, int32 Count)
{
    auto* Inventory = Owner ? Owner->FindComponentByClass<USFInventoryComponent>() : nullptr;
    if (!Inventory || Count <= 0) { return false; }
    FSFItemQuantity Hides; Hides.Item = TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(HidePath())); Hides.Quantity = Count;
    return SFInventoryTransactions::ExchangeInventoryItems(Inventory, {}, {Hides}) == ESFInventoryResult::Success;
}

int32 SFCombatPilotRewards::CountHides(const AActor* Owner)
{
    int32 Total = 0;
    const auto* Inventory = Owner ? Owner->FindComponentByClass<USFInventoryComponent>() : nullptr;
    for (int32 I = 0; Inventory && I < FMath::Clamp(Inventory->NumSlots, 0, 64); ++I)
    {
        TSoftObjectPtr<USFItemDefinition> Item; int32 Quantity = 0;
        if (Inventory->GetSlot(I, Item, Quantity) && Item.ToSoftObjectPath() == FSoftObjectPath(HidePath())) { Total += Quantity; }
    }
    return Total;
}

void SFCombatPilotRewards::AddPilotBag(APlayerController* Controller)
{
    if (!Controller || !Controller->HasAuthority()) { return; }
    if (!Controller->FindComponentByClass<USFInventoryComponent>())
    {
        // The gathering pilot's bag settings: one development bag, no saves.
        auto* Inventory = NewObject<USFInventoryComponent>(Controller, TEXT("CombatPilotInventory"));
        Inventory->NumSlots = GetDefault<USFGatheringPilotSettings>()->InventorySlots;
        Inventory->MaxWeight = GetDefault<USFGatheringPilotSettings>()->InventoryWeightKg;
                Inventory->RegisterComponent();
    }
    if (!Controller->FindComponentByClass<USFCombatPilotHideLog>())
    {
        auto* Log = NewObject<USFCombatPilotHideLog>(Controller, TEXT("CombatPilotHideLog"));
        Log->SetIsReplicated(true);
        Log->RegisterComponent();
    }
}

USFCombatPilotHideLog::USFCombatPilotHideLog()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.5f;
}

void USFCombatPilotHideLog::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const auto* Controller = Cast<APlayerController>(GetOwner());
    if (!Controller || !Controller->IsLocalController() || Controller->HasAuthority()) { return; }
    const int32 Hides = SFCombatPilotRewards::CountHides(Controller);
    if (Hides != Logged) { Logged = Hides; UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_CLIENT_HIDES Count=%d"), Hides); }
}
