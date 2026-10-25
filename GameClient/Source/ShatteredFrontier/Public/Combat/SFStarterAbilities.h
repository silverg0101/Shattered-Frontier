#pragma once
#include "CoreMinimal.h"

// Release-bound Part 5 rank-I content. Time is seconds, distance metres and
// energy costs are fractions of the named maximum pool.
namespace SFStarterAbilities
{
enum class EKit : uint8 { Weapon, Guardian, Magic, Healing };
enum class EEnergy : uint8 { Stamina, Mana };
enum class ETarget : uint8 { Enemy, Cone, Self, EnemyCasting, Charge, Ally, AllyOrSelf, DeadAlly, GroundAlly };
enum class EEffect : uint8 { Hit, Interrupt, HealOverTime, DamageOverTime, DisplaceHit, Absorb,
    Stance, BlockBuff, Taunt, DamageReduction, Heal, Dispel, Revive, HealingCircle };
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
    EEnergy Energy = EEnergy::Stamina;
    float CastSeconds = 0;
    bool bRequiresShield = false;
};

inline const TArray<FDefinition>& All()
{
    static const TArray<FDefinition> Values = {
        {TEXT("strike"), 1, .05f, 0, 1.5f, 0, 3, ETarget::Enemy, EEffect::Hit},
        {TEXT("heavy_blow"), 4, .08f, 8, 1.5f, 0, 3, ETarget::Enemy, EEffect::Hit, true},
        {TEXT("cleave"), 7, .07f, 0, 1.5f, 0, 3, ETarget::Cone, EEffect::Hit, false, 3},
        {TEXT("pommel_strike"), 10, .05f, 15, 0, 0, 3, ETarget::EnemyCasting, EEffect::Interrupt},
        {TEXT("second_wind"), 14, .10f, 60, 1, 0, 0, ETarget::Self, EEffect::HealOverTime},
        {TEXT("rend"), 18, .08f, 12, 1.5f, 0, 3, ETarget::Enemy, EEffect::DamageOverTime, true},
        {TEXT("charge"), 20, .08f, 20, 0, 8, 20, ETarget::Charge, EEffect::DisplaceHit},
        {TEXT("defensive_stance"), 1, .05f, 1, 1, 0, 0, ETarget::Self, EEffect::Stance,
            false, 1, EKit::Guardian},
        {TEXT("shield_bash"), 4, .06f, 8, 1.5f, 0, 3, ETarget::Enemy, EEffect::Hit,
            true, 1, EKit::Guardian, EEnergy::Stamina, 0, true},
        {TEXT("shield_block"), 7, .05f, 20, 1, 0, 0, ETarget::Self, EEffect::BlockBuff,
            false, 1, EKit::Guardian, EEnergy::Stamina, 0, true},
        {TEXT("challenge"), 10, .05f, 8, 0, 0, 10, ETarget::Enemy, EEffect::Taunt,
            false, 1, EKit::Guardian},
        {TEXT("shield_wall"), 14, .10f, 90, 1, 0, 0, ETarget::Self, EEffect::DamageReduction,
            false, 1, EKit::Guardian, EEnergy::Stamina, 0, true},
        {TEXT("bulwark"), 18, .08f, 30, 1.5f, 0, 10, ETarget::Ally, EEffect::DamageReduction,
            false, 1, EKit::Guardian},
        {TEXT("arcane_ward"), 7, .08f, 30, 1.5f, 0, 0, ETarget::Self, EEffect::Absorb,
            false, 1, EKit::Magic, EEnergy::Mana},
        {TEXT("mend"), 1, .06f, 0, 1.5f, 0, 30, ETarget::AllyOrSelf, EEffect::Heal,
            false, 1, EKit::Healing, EEnergy::Mana, 2},
        {TEXT("smite"), 4, .05f, 0, 1.5f, 0, 30, ETarget::Enemy, EEffect::Hit,
            false, 1, EKit::Healing, EEnergy::Mana, 1.5f},
        {TEXT("renew"), 7, .05f, 0, 1.5f, 0, 30, ETarget::AllyOrSelf, EEffect::HealOverTime,
            false, 1, EKit::Healing, EEnergy::Mana},
        {TEXT("purify"), 10, .05f, 8, 1.5f, 0, 30, ETarget::AllyOrSelf, EEffect::Dispel,
            false, 1, EKit::Healing, EEnergy::Mana},
        {TEXT("revive"), 14, .20f, 0, 1.5f, 0, 30, ETarget::DeadAlly, EEffect::Revive,
            false, 1, EKit::Healing, EEnergy::Mana, 10},
        {TEXT("flash_mend"), 18, .10f, 0, 1.5f, 0, 30, ETarget::AllyOrSelf, EEffect::Heal,
            false, 1, EKit::Healing, EEnergy::Mana, 1},
        {TEXT("healing_circle"), 20, .14f, 10, 1.5f, 0, 30, ETarget::GroundAlly, EEffect::HealingCircle,
            false, 5, EKit::Healing, EEnergy::Mana, 2.5f}
    };
    return Values;
}
inline const FDefinition* Find(const FString& Id)
{ for (const FDefinition& Value : All()) if (Id == Value.Id) return &Value; return nullptr; }
inline bool IsFriendlyTarget(ETarget Target)
{ return Target == ETarget::Ally || Target == ETarget::AllyOrSelf ||
    Target == ETarget::DeadAlly || Target == ETarget::GroundAlly; }
inline double SkillMultiplier(bool bNativeKit, double NativeMultiplier, double SkillFloor)
{ return bNativeKit ? NativeMultiplier : SkillFloor; }
inline double HealingThreatPerNpc(int32 EffectiveHealing, int32 EngagedNpcCount)
{ return EngagedNpcCount > 0 ? .5 * FMath::Max(0, EffectiveHealing) / EngagedNpcCount : 0.0; }
inline bool IsGroundWithinRange(const FVector& Caster, const FVector& Ground, float RangeMetres)
{ return !Ground.ContainsNaN() && FVector::Dist(Caster, Ground) <= RangeMetres * 100.0f; }
inline bool IsInsideHealingCircle(const FVector& Ground, const FVector& Character)
{ return FVector::Dist(Ground, Character) <= 600.0f; }
inline TArray<FName> BindingIds(EKit Kit, int32 Level)
{
    TArray<FName> Out;
    for (const FDefinition& Value : All())
        if (Value.Kit == Kit && Level >= Value.Level) Out.Add(FName(Value.Id));
    return Out;
}
inline bool CanStart(const FDefinition& Ability, int32 Level, bool bAlive, double Now,
    double GcdEndsAt, double CooldownEndsAt, float CurrentEnergy, float MaximumEnergy,
    float CostMultiplier)
{
    return bAlive && Level >= Ability.Level && Now >= GcdEndsAt && Now >= CooldownEndsAt &&
        MaximumEnergy > 0 && CurrentEnergy + KINDA_SMALL_NUMBER >=
        MaximumEnergy * Ability.StaminaCostFraction * CostMultiplier;
}
inline double Cooldown(const FDefinition& Ability, double HasteFraction)
{ return Ability.bHasteCooldown ? Ability.CooldownSeconds / (1.0 + FMath::Clamp(HasteFraction, 0.0, .75)) : Ability.CooldownSeconds; }
}
