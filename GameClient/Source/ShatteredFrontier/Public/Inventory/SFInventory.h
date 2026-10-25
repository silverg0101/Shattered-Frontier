#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "SFInventory.generated.h"

class UTexture2D;
class USFInventoryComponent;

// The pilot bag: item definitions (DA_* under /Game/Frontier/Gameplay/Pilot), a slot inventory on the player controller and an
// all-or-nothing transaction helper. The server mutates; the owning client reads the replicated copy.

UENUM(BlueprintType)
enum class ESFItemUseType : uint8
{
    None,
    Usable,
    Equip,
    Eat,
    Consume,
};

UCLASS(BlueprintType)
class SHATTEREDFRONTIER_API USFItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    // Ledger definition id, e.g. "item/oak_log".
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") FString UniqueId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true)) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1")) int32 MaxStackSize = 1;
    // Kilograms per unit.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0.0", Units = "Kilograms")) float Weight = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") TSoftObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") ESFItemUseType UseType = ESFItemUseType::None;
    // Equipment slot name (Head, MainHand, ...) for Equip items.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") FName EquipmentSlot = NAME_None;
};

USTRUCT(BlueprintType)
struct FSFItemQuantity
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory") TSoftObjectPtr<USFItemDefinition> Item;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory") int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct FSFEquippedSlot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Inventory") FName SlotName = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category = "Inventory") TSoftObjectPtr<USFItemDefinition> Item;
    UPROPERTY(BlueprintReadOnly, Category = "Inventory") int32 Quantity = 1;
    UPROPERTY(BlueprintReadOnly, Category = "Inventory") int32 DurabilityPercent = 100;
    UPROPERTY(BlueprintReadOnly, Category = "Inventory") FString InstanceId;
};

UCLASS(ClassGroup = (Frontier), meta = (BlueprintSpawnableComponent))
class SHATTEREDFRONTIER_API USFInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFInventoryComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Inventory", meta = (ClampMin = "0")) int32 NumSlots = 24;
    // Kilograms; 0 is unlimited. Equipped items do not count.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Inventory", meta = (ClampMin = "0.0")) float MaxWeight = 0.f;
    // Empty allows any slot name.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory") TArray<FName> AllowedEquipmentSlots;
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Inventory") TArray<FSFEquippedSlot> EquippedItems;

    // Bag stacks are kept packed (no gaps), so slot I is the I-th stack. False when empty or out of range.
    bool GetSlot(int32 SlotIndex, TSoftObjectPtr<USFItemDefinition>& OutItem, int32& OutQuantity) const;
    bool GetEquippedItem(FName EquipmentSlot, TSoftObjectPtr<USFItemDefinition>& OutItem, int32& OutQuantity) const;
    int32 GetTotalItemCount() const;
    float GetTotalWeight() const;

    // Server only. Wears one unit from a bag slot; a worn item in that slot returns to the bag, refused when it does not fit.
    bool EquipFromSlot(int32 SlotIndex);
    // Server only. Returns the worn item to the bag, refused when it does not fit.
    bool UnequipSlot(FName EquipmentSlot);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    friend struct FSFInventoryTransactionAccess;
    UPROPERTY(Replicated) TArray<FSFItemQuantity> Stacks;
};

UENUM(BlueprintType)
enum class ESFInventoryResult : uint8
{
    Success,
    InvalidRequest,
    NotAuthority,
    MissingItemDefinition,
    InsufficientItems,
    InsufficientSpace,
    WeightLimitExceeded,
};

struct FSFInventoryChange
{
    USFInventoryComponent* Inventory = nullptr;
    TArray<FSFItemQuantity> RemoveItems;
    TArray<FSFItemQuantity> AddItems;
};

// Validates every change against copies first; commits all of them or none.
namespace SFInventoryTransactions
{
    SHATTEREDFRONTIER_API int64 GetItemQuantity(const USFInventoryComponent* Inventory, const TSoftObjectPtr<USFItemDefinition>& Item);
    SHATTEREDFRONTIER_API ESFInventoryResult CanExecuteInventoryTransaction(const TArray<FSFInventoryChange>& Changes);
    SHATTEREDFRONTIER_API ESFInventoryResult ExecuteInventoryTransaction(const TArray<FSFInventoryChange>& Changes);
    SHATTEREDFRONTIER_API ESFInventoryResult ExchangeInventoryItems(USFInventoryComponent* Inventory, const TArray<FSFItemQuantity>& Costs, const TArray<FSFItemQuantity>& Rewards);
}
