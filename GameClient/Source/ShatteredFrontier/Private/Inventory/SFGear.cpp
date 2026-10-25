#include "Inventory/SFGear.h"

#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Inventory/SFInventory.h"

bool SFGear::Parse(const FString& Json, TMap<FString, FSFGearStats>& Out, FString& Error)
{
    TSharedPtr<FJsonObject> Root; const TArray<TSharedPtr<FJsonValue>>* Gear = nullptr;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root || !Root->TryGetArrayField(TEXT("gear"), Gear))
    { Error = TEXT("missing gear array"); return false; }
    TMap<FString, FSFGearStats> Parsed;
    for (const TSharedPtr<FJsonValue>& Value : *Gear)
    {
        const TSharedPtr<FJsonObject> Item = Value ? Value->AsObject() : nullptr;
        FString Id, Slot, Category; int32 Armor = -1, Damage = -1;
        if (!Item || !Item->TryGetStringField(TEXT("definition_id"), Id) || !Item->TryGetStringField(TEXT("slot"), Slot)
            || !Item->TryGetNumberField(TEXT("armor"), Armor) || !Item->TryGetNumberField(TEXT("damage"), Damage)
            || !Item->TryGetStringField(TEXT("category"), Category)
            || !Id.StartsWith(TEXT("item/")) || Slot.IsEmpty() || Armor < 0 || Armor > 50 || Damage < 0 || Damage > 50 || Parsed.Contains(Id))
        { Error = FString::Printf(TEXT("invalid gear entry %s"), *Id); return false; }
        const TMap<FString, ESFEquipmentCategory> Categories = {
            {TEXT("other"), ESFEquipmentCategory::Other}, {TEXT("one_hand"), ESFEquipmentCategory::OneHand},
            {TEXT("two_hand"), ESFEquipmentCategory::TwoHand}, {TEXT("bow"), ESFEquipmentCategory::Bow},
            {TEXT("staff"), ESFEquipmentCategory::Staff}, {TEXT("dagger"), ESFEquipmentCategory::Dagger},
            {TEXT("shield"), ESFEquipmentCategory::Shield}, {TEXT("ring"), ESFEquipmentCategory::Ring}};
        const ESFEquipmentCategory* Kind = Categories.Find(Category);
        double Dps = 0, Speed = 0;
        Item->TryGetNumberField(TEXT("weapon_dps"), Dps);
        Item->TryGetNumberField(TEXT("speed_s"), Speed);
        const bool bWeapon = Kind && (*Kind == ESFEquipmentCategory::OneHand || *Kind == ESFEquipmentCategory::TwoHand ||
            *Kind == ESFEquipmentCategory::Bow || *Kind == ESFEquipmentCategory::Staff || *Kind == ESFEquipmentCategory::Dagger);
        const bool bOneHand=Kind && (*Kind == ESFEquipmentCategory::OneHand || *Kind == ESFEquipmentCategory::Dagger);
        if (!Kind || (bWeapon && (Dps <= 0 || Speed < 1.4 || Speed > 3.8 ||
            (bOneHand ? Slot != TEXT("MainHand") && Slot != TEXT("OffHand") : Slot != TEXT("MainHand"))))
            || (*Kind == ESFEquipmentCategory::Shield && Slot != TEXT("OffHand"))
            || (*Kind == ESFEquipmentCategory::Ring && Slot != TEXT("Ring"))
            || (!bWeapon && (Dps != 0 || Speed != 0)))
        { Error = FString::Printf(TEXT("invalid gear category/weapon line %s"), *Id); return false; }
        Parsed.Add(Id, FSFGearStats{FName(*Slot), Armor, Damage, *Kind, Dps, Speed});
    }
    Out = MoveTemp(Parsed);
    return true;
}

const TMap<FString, FSFGearStats>& SFGear::Shipped()
{
    static const TMap<FString, FSFGearStats> Gear = []()
    {
        TMap<FString, FSFGearStats> Loaded; FString Text, Error;
        const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/items/gear_greenlands_starter.json")));
        if (!FFileHelper::LoadFileToString(Text, *Path) || !Parse(Text, Loaded, Error))
        { UE_LOG(LogTemp, Warning, TEXT("GEAR_STATS_UNAVAILABLE %s %s"), *Path, *Error); Loaded.Reset(); }
        return Loaded;
    }();
    return Gear;
}

const FSFGearStats* SFGear::Find(const FString& DefinitionId)
{
    FString Base = DefinitionId;
    if (DefinitionId.StartsWith(TEXT("item/"))) { FString Slug, Grade; if (DefinitionId.Mid(5).Split(TEXT("/"), &Slug, &Grade)) { Base = TEXT("item/") + Slug; } }
    return Shipped().Find(Base);
}

ESFEquipmentCategory SFGear::Category(const USFItemDefinition* Definition)
{
    if (!Definition) { return ESFEquipmentCategory::Other; }
    if (const FSFGearStats* Stats = Find(Definition->UniqueId)) { return Stats->Category; }
    return Definition->EquipmentCategory;
}

FSFGearStats SFGear::Equipped(const AActor* Owner)
{
    FSFGearStats Total;
    const auto* Inventory = Owner ? Owner->FindComponentByClass<USFInventoryComponent>() : nullptr;
    for (const FSFEquippedSlot& Slot : Inventory ? Inventory->EquippedItems : TArray<FSFEquippedSlot>())
    {
        const USFItemDefinition* Definition = Slot.Item.LoadSynchronous();
        if (const FSFGearStats* Stats = Definition ? Find(Definition->UniqueId) : nullptr)
        {
            if (Slot.DurabilityPercent > 0) { Total.Armor += Stats->Armor; Total.Damage += Stats->Damage; }
        }
    }
    return Total;
}

SFGear::FCombatEquipment SFGear::CombatEquipment(const AActor* Owner, double PilotArmorUnit)
{
    FCombatEquipment Result;
    const auto* Inventory = Owner ? Owner->FindComponentByClass<USFInventoryComponent>() : nullptr;
    for (const FSFEquippedSlot& Slot : Inventory ? Inventory->EquippedItems : TArray<FSFEquippedSlot>())
    {
        if (Slot.DurabilityPercent <= 0) { continue; }
        const USFItemDefinition* Definition = Slot.Item.LoadSynchronous();
        if (!Definition) { continue; }
        FString Id = Definition->UniqueId;
        FString Slug, Grade;
        if (Id.StartsWith(TEXT("item/")) && Id.Mid(5).Split(TEXT("/"), &Slug, &Grade))
        { Id = TEXT("item/") + Slug; }
        if (const FSFGearStats* Stats = Find(Id))
        {
            Result.Armor += Stats->Armor * PilotArmorUnit;
            if (Slot.SlotName == TEXT("OffHand") && Stats->Category == ESFEquipmentCategory::Shield) { Result.bShield = true; }
            if (Slot.SlotName == TEXT("MainHand") && Stats->WeaponDps > 0)
            {
                Result.Weapon = {Stats->WeaponDps, Stats->SpeedSeconds, true};
                Result.bRangedWeapon = Stats->Category == ESFEquipmentCategory::Bow;
            }
        }
    }
    return Result;
}

int32 SFGear::MitigatedDamage(int32 Damage, int32 Armor)
{
    return FMath::Max(1, Damage - FMath::Max(0, Armor));
}
