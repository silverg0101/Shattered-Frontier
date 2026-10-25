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
        FString Id, Slot; int32 Armor = -1, Damage = -1;
        if (!Item || !Item->TryGetStringField(TEXT("definition_id"), Id) || !Item->TryGetStringField(TEXT("slot"), Slot)
            || !Item->TryGetNumberField(TEXT("armor"), Armor) || !Item->TryGetNumberField(TEXT("damage"), Damage)
            || !Id.StartsWith(TEXT("item/")) || Slot.IsEmpty() || Armor < 0 || Armor > 50 || Damage < 0 || Damage > 50 || Parsed.Contains(Id))
        { Error = FString::Printf(TEXT("invalid gear entry %s"), *Id); return false; }
        Parsed.Add(Id, FSFGearStats{FName(*Slot), Armor, Damage});
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
        if (const FSFGearStats* Stats = Find(Id)) { Result.Armor += Stats->Armor * PilotArmorUnit; }
        if (Id == TEXT("item/iron_buckler")) { Result.bShield = true; }
        if (Id == TEXT("item/iron_shortsword")) { Result.Weapon = {3.64, 2.0, true}; }
        else if (Id == TEXT("item/cudgel")) { Result.Weapon = {7.0, 2.0, true}; }
        else if (Id == TEXT("item/brigand_blade")) { Result.Weapon = {10.0, 2.0, true}; }
        else if (Id == TEXT("item/fangtooth_dagger")) { Result.Weapon = {12.4, 1.4, true}; }
        else if (Id == TEXT("item/captains_longsword")) { Result.Weapon = {12.4, 2.0, true}; }
    }
    return Result;
}

int32 SFGear::MitigatedDamage(int32 Damage, int32 Armor)
{
    return FMath::Max(1, Damage - FMath::Max(0, Armor));
}
