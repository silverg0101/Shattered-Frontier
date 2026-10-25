#pragma once

#include "CoreMinimal.h"

// Release-bound Part 5 rank-I content. The server consumes this table; input and
// future hotbars use the same IDs. Time is in seconds, distance in metres.
namespace SFStarterAbilities
{
enum class EKit : uint8 { Weapon, Magic };
enum class ETarget : uint8 { Enemy, Cone, Self, EnemyCasting, Charge };
enum class EEffect : uint8 { Hit, Interrupt, HealOverTime, DamageOverTime, DisplaceHit, Absorb };
struct FDefinition
{
    const TCHAR* Id;
    int32 Level;
    float StaminaCostFraction, CooldownSeconds, GcdSeconds;
    float MinimumRangeMetres, MaximumRangeMetres;
    ETarget Target;
    EEffect Effect;
    bool bHasteCooldown = false;
    int32 MaximumTargets = 1;
    EKit Kit = EKit::Weapon;
};

inline const FDefinition* Find(const FString& Id)
{
    static const FDefinition Values[] = {
        {TEXT("strike"), 1, .05f, 0, 1.5f, 0, 3, ETarget::Enemy, EEffect::Hit},
        {TEXT("heavy_blow"), 4, .08f, 8, 1.5f, 0, 3, ETarget::Enemy, EEffect::Hit, true},
        {TEXT("cleave"), 7, .07f, 0, 1.5f, 0, 3, ETarget::Cone, EEffect::Hit, false, 3},
        {TEXT("pommel_strike"), 10, .05f, 15, 0, 0, 3, ETarget::EnemyCasting, EEffect::Interrupt},
        {TEXT("second_wind"), 14, .10f, 60, 1, 0, 0, ETarget::Self, EEffect::HealOverTime},
        {TEXT("rend"), 18, .08f, 12, 1.5f, 0, 3, ETarget::Enemy, EEffect::DamageOverTime, true},
        {TEXT("charge"), 20, .08f, 20, 0, 8, 20, ETarget::Charge, EEffect::DisplaceHit},
        {TEXT("arcane_ward"), 7, .08f, 30, 1.5f, 0, 0, ETarget::Self, EEffect::Absorb,
            false, 1, EKit::Magic}
    };
    for (const FDefinition& Value : Values) if (Id == Value.Id) return &Value;
    return nullptr;
}

inline TArray<FName> BindingIds(EKit Kit, int32 Level)
{
    TArray<FName> Out;
    for (const TCHAR* Id : {TEXT("strike"), TEXT("heavy_blow"), TEXT("cleave"),
        TEXT("pommel_strike"), TEXT("second_wind"), TEXT("rend"), TEXT("charge")})
    { if (const FDefinition* Definition = Find(Id); Definition && Kit == Definition->Kit && Level >= Definition->Level) Out.Add(FName(Id)); }
    if (Kit == EKit::Magic && Level >= 7) Out.Add(TEXT("arcane_ward"));
    return Out;
}

inline bool CanStart(const FDefinition& Ability, int32 Level, bool bAlive, double Now,
    double GcdEndsAt, double CooldownEndsAt, float CurrentStamina, float MaximumStamina,
    float CostMultiplier)
{
    return bAlive && Level >= Ability.Level && Now >= GcdEndsAt && Now >= CooldownEndsAt &&
        MaximumStamina > 0 && CurrentStamina + KINDA_SMALL_NUMBER >=
        MaximumStamina * Ability.StaminaCostFraction * CostMultiplier;
}
inline double Cooldown(const FDefinition& Ability, double HasteFraction)
{ return Ability.bHasteCooldown ? Ability.CooldownSeconds / (1.0 + FMath::Clamp(HasteFraction, 0.0, .75)) : Ability.CooldownSeconds; }
}
