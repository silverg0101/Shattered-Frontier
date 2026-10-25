#pragma once

#include "CoreMinimal.h"
#include "Combat/SFCombatPart6.h"
#include "Inventory/SFInventory.h"

class AActor;

// One wearable's combat stats (content/items/gear_greenlands_starter.json). Armour lowers each incoming hit, damage adds
// to each melee hit; both are summed over the equipped items.
struct FSFGearStats
{
    FName Slot;
    int32 Armor = 0;
    int32 Damage = 0;
    ESFEquipmentCategory Category = ESFEquipmentCategory::Other;
    double WeaponDps = 0;
    double SpeedSeconds = 0;
};

namespace SFGear
{
    struct FCombatEquipment
    {
        SFCombatPart6::FWeapon Weapon;
        double Armor = 0;
        bool bShield = false;
        bool bRangedWeapon = false; // An equipped MainHand bow supplies ranged abilities.
    };
    // Server equipment snapshot. The five authored weapon lines are release content;
    // legacy armor points use Part 6's development conversion until design armor ships.
    SHATTEREDFRONTIER_API FCombatEquipment CombatEquipment(const AActor* Owner, double PilotArmorUnit);
    // Base definition id -> stats, loaded once from content/items/gear_greenlands_starter.json.
    SHATTEREDFRONTIER_API const TMap<FString, FSFGearStats>& Shipped();
    // Graded ids ("item/hide_cap/fine") share their base item's stats. Null for non-gear (tools, materials).
    SHATTEREDFRONTIER_API const FSFGearStats* Find(const FString& DefinitionId);
    SHATTEREDFRONTIER_API ESFEquipmentCategory Category(const USFItemDefinition* Definition);
    SHATTEREDFRONTIER_API bool Parse(const FString& Json, TMap<FString, FSFGearStats>& Out, FString& Error);
    // Totals over the equipment of Owner's inventory (the player controller); zero without one.
    SHATTEREDFRONTIER_API FSFGearStats Equipped(const AActor* Owner);
    // Melee damage after armour; every landed hit does at least 1.
    SHATTEREDFRONTIER_API int32 MitigatedDamage(int32 Damage, int32 Armor);
}
