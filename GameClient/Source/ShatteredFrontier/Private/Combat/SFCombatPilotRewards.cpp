#include "Combat/SFCombatPilotRewards.h"
#include "Gathering/SFGatheringPilot.h"
#include "GameFramework/PlayerController.h"
#if SF_WITH_MINIMAL_INVENTORY
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryTransactionLibrary.h"
#endif

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

bool SFCombatPilotRewards::GrantHides(AActor* Owner, int32 Count)
{
#if SF_WITH_MINIMAL_INVENTORY
    auto* Inventory = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
    if (!Inventory || Count <= 0) { return false; }
    FInventoryItemQuantity Hides; Hides.Item = TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(HidePath())); Hides.Quantity = Count;
    return UInventoryTransactionLibrary::ExchangeInventoryItems(Inventory, {}, {Hides}) == EInventoryTransactionResult::Success;
#else
    return false;
#endif
}

int32 SFCombatPilotRewards::CountHides(const AActor* Owner)
{
    int32 Total = 0;
#if SF_WITH_MINIMAL_INVENTORY
    const auto* Inventory = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
    for (int32 I = 0; Inventory && I < FMath::Clamp(Inventory->NumSlots, 0, 64); ++I)
    {
        TSoftObjectPtr<UInventoryItemDefinition> Item; int32 Quantity = 0;
        if (Inventory->GetSlot(I, Item, Quantity) && Item.ToSoftObjectPath() == FSoftObjectPath(HidePath())) { Total += Quantity; }
    }
#endif
    return Total;
}

void SFCombatPilotRewards::AddPilotBag(APlayerController* Controller)
{
    if (!Controller || !Controller->HasAuthority()) { return; }
#if SF_WITH_MINIMAL_INVENTORY
    if (!Controller->FindComponentByClass<UInventoryComponent>())
    {
        // The gathering pilot's bag settings: one development bag, no saves, no built-in UI or interaction.
        auto* Inventory = NewObject<UInventoryComponent>(Controller, TEXT("CombatPilotInventory"));
        Inventory->bUseBuiltInInteraction = false; Inventory->bEnableLookPrompt = false;
        Inventory->bUseExternalInventoryUI = true; Inventory->bGrantStartupItems = false;
        Inventory->bAutoLoadOnBeginPlay = false; Inventory->bAutoSaveOnInventoryChanged = false;
        Inventory->bAutoSaveLoadInNetworkedGames = false;
        Inventory->bAutoCreateHotbarOnOwningClient = false; Inventory->bAutoShowUIOnOwningClient = false;
        Inventory->NumSlots = GetDefault<USFGatheringPilotSettings>()->InventorySlots;
        Inventory->MaxWeight = GetDefault<USFGatheringPilotSettings>()->InventoryWeightKg;
        Inventory->SetIsReplicated(true);
        Inventory->RegisterComponent();
    }
#endif
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
