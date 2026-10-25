#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

// The server supplies an attacker's five draws even when an outcome is unavailable.
// Optional combat snapshots are explicit: absent skills, ratings, level or ability
// data do not silently become a made-up chance or mitigation value.
namespace SFCombatResolution
{
    enum class EOutcome : uint8 { Hit, Crit, Miss, Resist, Dodge, Parry, Block, CritBlock, Absorb };

    struct FDraws
    {
        double Hit = 0, Avoid = 0, Crit = 0, Block = 0, Spread = 0;
    };

    inline FDraws Draw(FRandomStream& Stream)
    {
        return { Stream.FRand(), Stream.FRand(), Stream.FRand(), Stream.FRand(), Stream.FRand() };
    }

    struct FAttack
    {
        double RawDamage = 0;
        bool bSpell = false, bPhysical = true, bFrontArc = false;
        bool bCanMiss = false, bCanDodge = false, bCanParry = false;
        bool bCanCrit = false, bCanBlock = false;
        bool bHasArmorSnapshot = false, bHasResistanceSnapshot = false;
        bool bHasSpread = false;
        double MissChance = 0, DodgeChance = 0, ParryChance = 0;
        double CritChance = 0, CritDamage = 1, BlockChance = 0, BlockReduction = 0;
        double MissCap = 0.30, CritCap = 0.60, CritDamageCap = 3.0;
        double BlockCap = 0.75, BlockReductionCap = 0.40;
        double AvoidanceCap = 0.5, Armor = 0, Resistance = 0;
        double ArmorKLevel = 100, ArmorReductionCap = 0.85;
        double ResistK = 5, ResistReductionCap = 0.75;
        double ArmorPenetrationFraction = 0, MagicPenetrationFraction = 0;
        double ArmorPenetrationCap = 0.60, MagicPenetrationCap = 0.60;
        int32 AttackerLevel = 0;
        double DamageTakenMultiplier = 1;
        double Absorb = 0;
        double PilotFlatArmor = 0; // Existing starter gear until levelled armor snapshots are available.
        double SpreadWeaponDamage = 0; // Weapon portion of RawDamage at its average sample.
        double SpreadMinimum = 1, SpreadMaximum = 1;
    };

    struct FResult
    {
        EOutcome Outcome = EOutcome::Hit;
        int32 Damage = 0;
    };

    inline FResult Resolve(const FAttack& Attack, const FDraws& Draws)
    {
        if (Attack.bCanMiss && Draws.Hit < FMath::Clamp(Attack.MissChance, 0.0, Attack.MissCap))
        { return { Attack.bSpell ? EOutcome::Resist : EOutcome::Miss, 0 }; }

        if (Attack.bPhysical && Attack.bFrontArc)
        {
            double Dodge = Attack.bCanDodge ? FMath::Max(0.0, Attack.DodgeChance) : 0.0;
            double Parry = Attack.bCanParry ? FMath::Max(0.0, Attack.ParryChance) : 0.0;
            const double Total = Dodge + Parry;
            if (Total > Attack.AvoidanceCap && Total > 0)
            { Dodge *= Attack.AvoidanceCap / Total; Parry *= Attack.AvoidanceCap / Total; }
            if (Draws.Avoid < Dodge) { return { EOutcome::Dodge, 0 }; }
            if (Draws.Avoid < Dodge + Parry) { return { EOutcome::Parry, 0 }; }
        }

        double Damage = Attack.RawDamage;
        if (Attack.bHasSpread)
        { Damage += Attack.SpreadWeaponDamage *
            (FMath::Lerp(Attack.SpreadMinimum, Attack.SpreadMaximum, Draws.Spread) - 1.0); }
        Damage = FMath::Max(0.0, Damage);
        const bool bCrit = Attack.bCanCrit && Draws.Crit < FMath::Clamp(Attack.CritChance, 0.0, Attack.CritCap);
        if (bCrit) { Damage *= FMath::Clamp(Attack.CritDamage, 1.0, Attack.CritDamageCap); }

        if (Attack.AttackerLevel > 0)
        {
            if (Attack.bPhysical && Attack.bHasArmorSnapshot)
            {
                const double Armor = FMath::Max(0.0, Attack.Armor) *
                    (1.0 - FMath::Clamp(Attack.ArmorPenetrationFraction, 0.0, Attack.ArmorPenetrationCap));
                const double Reduction = Armor / (Armor + Attack.ArmorKLevel * Attack.AttackerLevel);
                Damage *= 1.0 - FMath::Min(Attack.ArmorReductionCap, Reduction);
            }
            else if (!Attack.bPhysical && Attack.bHasResistanceSnapshot)
            {
                const double Resistance = FMath::Max(0.0, Attack.Resistance) *
                    (1.0 - FMath::Clamp(Attack.MagicPenetrationFraction, 0.0, Attack.MagicPenetrationCap));
                const double Reduction = Resistance / (Resistance + Attack.ResistK * Attack.AttackerLevel);
                Damage *= 1.0 - FMath::Min(Attack.ResistReductionCap, Reduction);
            }
        }

        if (Attack.bPhysical && !Attack.bHasArmorSnapshot && Damage > 0 && Attack.PilotFlatArmor > 0)
        { Damage = FMath::Max(1.0, Damage - Attack.PilotFlatArmor); }
        const bool bBlock = Attack.bCanBlock && (!Attack.bPhysical || Attack.bFrontArc) &&
            Draws.Block < FMath::Clamp(Attack.BlockChance, 0.0, Attack.BlockCap);
        if (bBlock) { Damage *= 1.0 - FMath::Clamp(Attack.BlockReduction, 0.0, Attack.BlockReductionCap); }
        Damage *= FMath::Clamp(Attack.DamageTakenMultiplier, 0.10, 2.0);
        const bool bFullyAbsorbed = Damage > 0 && Attack.Absorb >= Damage;
        Damage = FMath::Max(0.0, Damage - FMath::Max(0.0, Attack.Absorb));
        const int32 Rounded = bFullyAbsorbed ? 0 : (Damage > 0 ? FMath::Max(1, FMath::FloorToInt(Damage + 0.5)) : 0);
        return { bFullyAbsorbed ? EOutcome::Absorb : bBlock ? (bCrit ? EOutcome::CritBlock : EOutcome::Block) :
            bCrit ? EOutcome::Crit : EOutcome::Hit, Rounded };
    }

    inline const TCHAR* Name(EOutcome Outcome)
    {
        switch (Outcome)
        {
        case EOutcome::Hit: return TEXT("hit");
        case EOutcome::Crit: return TEXT("crit");
        case EOutcome::Miss: return TEXT("miss");
        case EOutcome::Resist: return TEXT("resist");
        case EOutcome::Dodge: return TEXT("dodge");
        case EOutcome::Parry: return TEXT("parry");
        case EOutcome::Block: return TEXT("block");
        case EOutcome::CritBlock: return TEXT("crit_block");
        case EOutcome::Absorb: return TEXT("absorb");
        }
        return TEXT("unknown");
    }
}
