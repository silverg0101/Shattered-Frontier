#pragma once

#include "CoreMinimal.h"
#include "Combat/SFCombatResolution.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Release-bound Part 6 formulas and content. Only the scalar inputs in
// USFCombatPilotSettings are operator tuning; no formula is an editable control.
namespace SFCombatPart6
{
    inline bool Active(const UWorld* World)
    {
        if (!World || World->GetOutermost()->GetName() !=
            TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook")) return false;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        return !FParse::Param(FCommandLine::Get(), TEXT("CombatPilotFixture")) &&
            !FParse::Param(FCommandLine::Get(), TEXT("GreenlandsBoarNetcheck")) &&
            !FParse::Param(FCommandLine::Get(), TEXT("GreenlandsEnergyNetcheck"));
#else
        return true;
#endif
    }
    enum class EKit : uint8 { Fighter, Guardian, Archer, Mage, Healer, Shadow };

    struct FStats
    {
        int32 Level = 1, Strength = 10, Dexterity = 10, Constitution = 10;
        int32 Intelligence = 10, Wisdom = 10, Willpower = 10;
        int32 Health = 0, Mana = 0, Stamina = 0;
        int32 Skill = 100, Tactics = 50;
        double SkillMultiplier = 1, CritChance = 0, Resistance = 0;
    };

    inline int32 SkillReference(int32 Level)
    { return FMath::RoundToInt(900.0 * FMath::Pow(FMath::Clamp(Level, 1, 20) / 100.0, 0.85)); }

    inline double Throughput(int32 Level)
    {
        const double L = FMath::Clamp(Level, 1, 20);
        return 0.04 * (400.0 + 44.5 * L) / (38.0 + 5.882 * L);
    }

    inline FStats Player(EKit Kit, int32 Level, double CritBase = 0.05, double SkillFloor = 0.80)
    {
        FStats S; S.Level = FMath::Clamp(Level, 1, 20);
        const double Points = (S.Level <= 10 ? 120.0 + 30.0 * (S.Level - 1) / 9.0 :
            150.0 + 60.0 * (S.Level - 10) / 15.0) - 80.0;
        double Str = 0, Dex = 0, Con = 0, Int = 0, Wis = 0, Wil = 0;
        switch (Kit)
        {
        case EKit::Fighter: Str = .5; Con = .375; Dex = .125; break;
        case EKit::Guardian: Con = .5; Str = .375; Wil = .125; break;
        case EKit::Archer: Dex = .75; Con = .25; break;
        case EKit::Mage: Int = .75; Wis = .25; break;
        case EKit::Healer: Wis = .75; Int = .25; break;
        case EKit::Shadow: Dex = .75; Str = .25; break;
        }
        S.Strength += FMath::RoundToInt(Points * Str);
        S.Dexterity += FMath::RoundToInt(Points * Dex);
        S.Constitution += FMath::RoundToInt(Points * Con);
        S.Intelligence += FMath::RoundToInt(Points * Int);
        S.Wisdom += FMath::RoundToInt(Points * Wis);
        S.Willpower += FMath::RoundToInt(Points * Wil);
        S.Health = 100 + 15 * S.Constitution + 25 * S.Level;
        S.Mana = 100 + 20 * S.Intelligence + 10 * S.Level;
        S.Stamina = 100 + 5 * S.Constitution + 3 * S.Dexterity;
        const int32 Reference = SkillReference(S.Level);
        S.Skill = FMath::Max(100, Reference);
        S.Tactics = FMath::Max(50, FMath::RoundToInt(.75 * Reference));
        S.SkillMultiplier = SkillFloor + (1.0 - SkillFloor) *
            FMath::Min(1.10, double(S.Skill) / FMath::Max(1, Reference));
        S.CritChance = CritBase + S.Dexterity / 5000.0;
        S.Resistance = FMath::FloorToInt(S.Wisdom / 2.0);
        return S;
    }

    struct FWeapon
    {
        double Dps = 0, SpeedSeconds = 2;
        bool bEquipped = false;
    };

    inline double Budget(int32 ItemLevel) { return 1.0 + 0.075 * ItemLevel; }
    inline FWeapon ReferenceWeapon(int32 Level, double SpeedSeconds = 2.0,
        double Rarity = .70, double HandFactor = 1.0, double WeaponDpsK = 4.0)
    { return { WeaponDpsK * Budget(4 * FMath::Clamp(Level, 1, 20)) * Rarity * HandFactor,
        SpeedSeconds, true }; }

    inline double UnarmedDps(int32 Level, double Factor = .40, double WeaponDpsK = 4.0)
    { return Factor * WeaponDpsK * Budget(4 * FMath::Clamp(Level, 1, 20)); }

    // Part 6 §5 release content. Utility abilities have no power coefficient;
    // second_wind is 15% maximum health rather than a weapon/power formula.
    struct FAbilityCoefficient
    {
        const TCHAR* Id;
        double Modifier, ReferenceSeconds;
        bool bPhysical = true, bHeal = false, bAbsorb = false, bPhysicalPower = false;
    };
    inline const FAbilityCoefficient* Ability(const FString& Id)
    {
        static const FAbilityCoefficient Values[] = {
            {TEXT("strike"), .7, 1.5}, {TEXT("heavy_blow"), 1.5, 1.5},
            {TEXT("cleave"), .42, 1.5}, {TEXT("pommel_strike"), .3, .5},
            {TEXT("rend"), 1.8, 1.5}, {TEXT("charge"), .5, .5},
            {TEXT("shield_bash"), 1.0, 1.5},
            {TEXT("quick_shot"), .7, 1.5}, {TEXT("aimed_shot"), 1.3, 2.0},
            {TEXT("crippling_shot"), .6, 1.5}, {TEXT("multi_shot"), .42, 1.5},
            {TEXT("serpent_sting"), 1.6, 1.5, false, false, false, true},
            {TEXT("fire_bolt"), 1.4, 2.0, false}, {TEXT("frost_shard"), .7, 1.5, false},
            {TEXT("arcane_ward"), 2.0, 1.5, false, false, true},
            {TEXT("flame_burst"), .9, 1.5, false},
            {TEXT("mend"), 1.5, 2.0, false, true}, {TEXT("smite"), 1.0, 1.5, false},
            {TEXT("renew"), 1.2, 1.5, false, true},
            {TEXT("flash_mend"), 1.0, 1.5, false, true},
            {TEXT("healing_circle"), .9, 2.5, false, true},
            {TEXT("backstab"), 2.4, 1.5}, {TEXT("effect/weak_poison"), .6, 1.5, false, false, false, true}
        };
        for (const FAbilityCoefficient& Value : Values)
        { if (Id == Value.Id) return &Value; }
        return nullptr;
    }

    inline SFCombatResolution::FAttack PlayerAbility(const FStats& Source, const FWeapon& Weapon,
        double ReferenceSeconds, double Modifier, bool bPhysical, double Spread,
        double PowerSpeedReference = 2.4, double UnarmedFactor = .40,
        double WeaponDpsK = 4.0, bool bPhysicalPower = false)
    {
        SFCombatResolution::FAttack A;
        A.AttackerLevel = Source.Level;
        const double Dps = Weapon.bEquipped ? Weapon.Dps : UnarmedDps(Source.Level, UnarmedFactor, WeaponDpsK);
        const double Time = ReferenceSeconds;
        const double Scale = Throughput(Source.Level) * Source.SkillMultiplier;
        A.SpreadWeaponDamage = Dps * Time * Modifier * Scale;
        A.RawDamage = A.SpreadWeaponDamage +
            2.0 * ((bPhysical || bPhysicalPower) ? Source.Strength : Source.Intelligence) *
            Time / PowerSpeedReference * Modifier * Scale;
        A.bPhysical = bPhysical; A.bSpell = !bPhysical;
        A.bHasSpread = true; // Unarmed also samples spread.
        A.SpreadMinimum = 1.0 - Spread; A.SpreadMaximum = 1.0 + Spread;
        A.bCanCrit = true; A.CritChance = Source.CritChance; A.CritDamage = 2.0;
        return A;
    }

    inline SFCombatResolution::FAttack AuthoredAbility(const FStats& Source, const FWeapon& Weapon,
        const FAbilityCoefficient& Coefficient, double Spread, double PowerSpeedReference = 2.4,
        double UnarmedFactor = .40, double WeaponDpsK = 4.0)
    {
        auto A = PlayerAbility(Source, Weapon, Coefficient.ReferenceSeconds, Coefficient.Modifier,
            Coefficient.bPhysical, Spread, PowerSpeedReference, UnarmedFactor,
            WeaponDpsK, Coefficient.bPhysicalPower);
        if (Coefficient.bHeal || Coefficient.bAbsorb)
        {
            A.RawDamage *= 1.0 + Source.Wisdom / 1000.0;
            A.SpreadWeaponDamage *= 1.0 + Source.Wisdom / 1000.0;
        }
        if (Coefficient.bAbsorb) { A.bCanCrit = false; }
        return A;
    }

    inline SFCombatResolution::FAttack PlayerAuto(const FStats& Source, const FWeapon& Weapon,
        double Spread, double PowerSpeedReference = 2.4, double UnarmedFactor = .40,
        double WeaponDpsK = 4.0)
    { return PlayerAbility(Source, Weapon, Weapon.bEquipped ? Weapon.SpeedSeconds : 2.0,
        1.0, true, Spread, PowerSpeedReference, UnarmedFactor, WeaponDpsK); }

    struct FNpc
    {
        int32 Level = 2, Health = 489, Skill = 32;
        double SwingSeconds = 1.6, SwingDamage = 23.5, Armor = 108,
            Resistance = 5.4, CritChance = .056, CritDamage = 1.5;
    };

    inline FNpc Npc(const FString& Id)
    {
        if (Id == TEXT("npc/wolf")) return {6, 667, 82, 1.6, 32.0, 324, 16.2, .057, 1.5};
        if (Id == TEXT("npc/bandit_thug")) return {5, 623, 71, 2.0, 37.4, 270, 13.5, .053, 1.5};
        if (Id == TEXT("npc/bandit_archer")) return {5, 623, 71, 2.0, 34.9, 270, 13.5, .058, 1.5};
        if (Id == TEXT("npc/wolf_alpha")) return {7, 1779, 144, 1.6, 54.0, 378, 18.9, .057, 1.5};
        if (Id == TEXT("npc/bandit_captain")) return {7, 3558, 194, 2.0, 95.5, 378, 18.9, .053, 1.5};
        return {};
    }
}
