#include "Inventory/SFInventory.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

namespace
{
    float WeightOf(const TArray<FSFItemQuantity>& Stacks)
    {
        float Total = 0.f;
        for (const FSFItemQuantity& Stack : Stacks)
        {
            if (const USFItemDefinition* Definition = Stack.Item.LoadSynchronous()) { Total += Definition->Weight * Stack.Quantity; }
        }
        return Total;
    }

    // Takes from the last matching stacks first; false (Stacks untouched) when fewer than Quantity are held.
    bool Remove(TArray<FSFItemQuantity>& Stacks, const TSoftObjectPtr<USFItemDefinition>& Item, int32 Quantity)
    {
        int64 Held = 0;
        for (const FSFItemQuantity& Stack : Stacks) { if (Stack.Item == Item) { Held += Stack.Quantity; } }
        if (Held < Quantity) { return false; }
        for (int32 I = Stacks.Num() - 1; I >= 0 && Quantity > 0; --I)
        {
            if (Stacks[I].Item != Item) { continue; }
            const int32 Taken = FMath::Min(Quantity, Stacks[I].Quantity);
            Stacks[I].Quantity -= Taken; Quantity -= Taken;
            if (Stacks[I].Quantity <= 0) { Stacks.RemoveAt(I); }
        }
        return true;
    }

    // Tops up existing stacks, then appends new ones; false when that needs more than NumSlots stacks.
    bool Add(TArray<FSFItemQuantity>& Stacks, const USFItemDefinition& Definition, const TSoftObjectPtr<USFItemDefinition>& Item, int32 Quantity, int32 NumSlots)
    {
        const int32 MaxStack = FMath::Max(1, Definition.MaxStackSize);
        for (FSFItemQuantity& Stack : Stacks)
        {
            if (Quantity <= 0) { break; }
            if (Stack.Item != Item || Stack.Quantity >= MaxStack) { continue; }
            const int32 Added = FMath::Min(Quantity, MaxStack - Stack.Quantity);
            Stack.Quantity += Added; Quantity -= Added;
        }
        while (Quantity > 0)
        {
            FSFItemQuantity& Stack = Stacks.AddDefaulted_GetRef();
            Stack.Item = Item; Stack.Quantity = FMath::Min(Quantity, MaxStack); Quantity -= Stack.Quantity;
        }
        return Stacks.Num() <= FMath::Max(0, NumSlots);
    }

    // Weight is refused only when the change adds weight past the limit (a bag already over a lowered limit can still shed items).
    bool WithinWeight(const USFInventoryComponent& Inventory, const TArray<FSFItemQuantity>& Before, const TArray<FSFItemQuantity>& After)
    {
        if (Inventory.MaxWeight <= 0.f) { return true; }
        const float New = WeightOf(After);
        return New <= Inventory.MaxWeight + KINDA_SMALL_NUMBER || New <= WeightOf(Before) + KINDA_SMALL_NUMBER;
    }

    bool HasAuthority(const USFInventoryComponent* Inventory)
    {
        return Inventory && Inventory->GetOwner() && Inventory->GetOwner()->HasAuthority();
    }
}

struct FSFInventoryTransactionAccess
{
    static TArray<FSFItemQuantity>& Stacks(USFInventoryComponent& Inventory) { return Inventory.Stacks; }
    static const TArray<FSFItemQuantity>& Stacks(const USFInventoryComponent& Inventory) { return Inventory.Stacks; }

    static ESFInventoryResult Evaluate(const TArray<FSFInventoryChange>& Changes, bool bCommit)
    {
        if (Changes.IsEmpty()) { return ESFInventoryResult::InvalidRequest; }
        TMap<USFInventoryComponent*, TArray<FSFItemQuantity>> Working;
        for (const FSFInventoryChange& Change : Changes)
        {
            if (!Change.Inventory || (Change.RemoveItems.IsEmpty() && Change.AddItems.IsEmpty())) { return ESFInventoryResult::InvalidRequest; }
            if (bCommit && !HasAuthority(Change.Inventory)) { return ESFInventoryResult::NotAuthority; }
            TArray<FSFItemQuantity>& Copy = Working.Contains(Change.Inventory) ? Working[Change.Inventory] : Working.Add(Change.Inventory, Change.Inventory->Stacks);
            for (const FSFItemQuantity& Cost : Change.RemoveItems)
            {
                if (Cost.Item.IsNull() || Cost.Quantity <= 0) { return ESFInventoryResult::InvalidRequest; }
                if (!Remove(Copy, Cost.Item, Cost.Quantity)) { return ESFInventoryResult::InsufficientItems; }
            }
            for (const FSFItemQuantity& Reward : Change.AddItems)
            {
                if (Reward.Item.IsNull() || Reward.Quantity <= 0) { return ESFInventoryResult::InvalidRequest; }
                const USFItemDefinition* Definition = Reward.Item.LoadSynchronous();
                if (!Definition) { return ESFInventoryResult::MissingItemDefinition; }
                if (!Add(Copy, *Definition, Reward.Item, Reward.Quantity, Change.Inventory->NumSlots)) { return ESFInventoryResult::InsufficientSpace; }
            }
        }
        for (const TPair<USFInventoryComponent*, TArray<FSFItemQuantity>>& Pair : Working)
        {
            if (!WithinWeight(*Pair.Key, Pair.Key->Stacks, Pair.Value)) { return ESFInventoryResult::WeightLimitExceeded; }
        }
        if (bCommit) { for (TPair<USFInventoryComponent*, TArray<FSFItemQuantity>>& Pair : Working) { Pair.Key->Stacks = MoveTemp(Pair.Value); } }
        return ESFInventoryResult::Success;
    }
};

USFInventoryComponent::USFInventoryComponent()
{
    SetIsReplicatedByDefault(true);
}

void USFInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(USFInventoryComponent, NumSlots, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFInventoryComponent, MaxWeight, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFInventoryComponent, EquippedItems, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFInventoryComponent, Stacks, COND_OwnerOnly);
}

bool USFInventoryComponent::GetSlot(int32 SlotIndex, TSoftObjectPtr<USFItemDefinition>& OutItem, int32& OutQuantity) const
{
    if (!Stacks.IsValidIndex(SlotIndex) || SlotIndex >= NumSlots) { return false; }
    OutItem = Stacks[SlotIndex].Item; OutQuantity = Stacks[SlotIndex].Quantity;
    return true;
}

bool USFInventoryComponent::GetEquippedItem(FName EquipmentSlot, TSoftObjectPtr<USFItemDefinition>& OutItem, int32& OutQuantity) const
{
    const FSFEquippedSlot* Worn = EquippedItems.FindByPredicate([EquipmentSlot](const FSFEquippedSlot& S) { return S.SlotName == EquipmentSlot; });
    if (!Worn) { return false; }
    OutItem = Worn->Item; OutQuantity = Worn->Quantity;
    return true;
}

int32 USFInventoryComponent::GetTotalItemCount() const
{
    int32 Total = 0;
    for (const FSFItemQuantity& Stack : Stacks) { Total += Stack.Quantity; }
    return Total;
}

float USFInventoryComponent::GetTotalWeight() const
{
    return WeightOf(Stacks);
}

bool USFInventoryComponent::EquipFromSlot(int32 SlotIndex)
{
    if (!HasAuthority(this) || !Stacks.IsValidIndex(SlotIndex)) { return false; }
    const TSoftObjectPtr<USFItemDefinition> Item = Stacks[SlotIndex].Item;
    const USFItemDefinition* Definition = Item.LoadSynchronous();
    if (!Definition || Definition->UseType != ESFItemUseType::Equip || Definition->EquipmentSlot.IsNone()
        || (!AllowedEquipmentSlots.IsEmpty() && !AllowedEquipmentSlots.Contains(Definition->EquipmentSlot))) { return false; }
    TArray<FSFItemQuantity> Copy = Stacks;
    if (--Copy[SlotIndex].Quantity <= 0) { Copy.RemoveAt(SlotIndex); }
    FSFEquippedSlot* Worn = EquippedItems.FindByPredicate([Definition](const FSFEquippedSlot& S) { return S.SlotName == Definition->EquipmentSlot; });
    if (Worn)
    {
        const USFItemDefinition* Old = Worn->Item.LoadSynchronous();
        if (!Old || !Add(Copy, *Old, Worn->Item, Worn->Quantity, NumSlots) || !WithinWeight(*this, Stacks, Copy)) { return false; }
    }
    else
    {
        Worn = &EquippedItems.AddDefaulted_GetRef(); Worn->SlotName = Definition->EquipmentSlot;
    }
    Worn->Item = Item; Worn->Quantity = 1; Worn->InstanceId.Reset(); Worn->DurabilityPercent = 100;
    Stacks = MoveTemp(Copy);
    return true;
}

bool USFInventoryComponent::UnequipSlot(FName EquipmentSlot)
{
    const int32 Index = EquippedItems.IndexOfByPredicate([EquipmentSlot](const FSFEquippedSlot& S) { return S.SlotName == EquipmentSlot; });
    if (!HasAuthority(this) || Index == INDEX_NONE) { return false; }
    const FSFEquippedSlot& Worn = EquippedItems[Index];
    const USFItemDefinition* Definition = Worn.Item.LoadSynchronous();
    TArray<FSFItemQuantity> Copy = Stacks;
    if (!Definition || !Add(Copy, *Definition, Worn.Item, Worn.Quantity, NumSlots) || !WithinWeight(*this, Stacks, Copy)) { return false; }
    Stacks = MoveTemp(Copy);
    EquippedItems.RemoveAt(Index);
    return true;
}

int64 SFInventoryTransactions::GetItemQuantity(const USFInventoryComponent* Inventory, const TSoftObjectPtr<USFItemDefinition>& Item)
{
    int64 Total = 0;
    if (Inventory) { for (const FSFItemQuantity& Stack : FSFInventoryTransactionAccess::Stacks(*Inventory)) { if (Stack.Item == Item) { Total += Stack.Quantity; } } }
    return Total;
}

ESFInventoryResult SFInventoryTransactions::CanExecuteInventoryTransaction(const TArray<FSFInventoryChange>& Changes)
{
    return FSFInventoryTransactionAccess::Evaluate(Changes, false);
}

ESFInventoryResult SFInventoryTransactions::ExecuteInventoryTransaction(const TArray<FSFInventoryChange>& Changes)
{
    return FSFInventoryTransactionAccess::Evaluate(Changes, true);
}

ESFInventoryResult SFInventoryTransactions::ExchangeInventoryItems(USFInventoryComponent* Inventory, const TArray<FSFItemQuantity>& Costs, const TArray<FSFItemQuantity>& Rewards)
{
    FSFInventoryChange Change; Change.Inventory = Inventory; Change.RemoveItems = Costs; Change.AddItems = Rewards;
    return FSFInventoryTransactionAccess::Evaluate({Change}, true);
}
