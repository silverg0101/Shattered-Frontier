#pragma once

#include "CoreMinimal.h"

/** Server simulation clock for the sandbox health resource. No client input affects timing or amount. */
struct FSFOutOfCombatRegen
{
    bool bInCombat = false;
    double LastHostileAt = -1.0;
    double OutOfCombatAt = -1.0;
    double NextRegenTickAt = -1.0;
    double FractionalHealth = 0.0;

    void Hostile(double Now)
    {
        bInCombat = true;
        LastHostileAt = Now;
        OutOfCombatAt = -1.0;
        NextRegenTickAt = -1.0;
    }

    int32 Advance(double Now, bool bOnThreatTable, int32 Health, int32 MaxHealth,
        int32 Constitution, double CombatExitSeconds, double RegenDelaySeconds, double HealthPctPerSecond)
    {
        if (Health <= 0 || MaxHealth <= 0) { return Health; }
        if (bInCombat)
        {
            if (bOnThreatTable || LastHostileAt < 0.0 || Now - LastHostileAt < CombatExitSeconds) { return Health; }
            bInCombat = false;
            OutOfCombatAt = Now;
            NextRegenTickAt = FMath::CeilToDouble(OutOfCombatAt + RegenDelaySeconds);
        }
        if (Health >= MaxHealth || OutOfCombatAt < 0.0 || Now < NextRegenTickAt) { return Health; }
        const double PerTick = MaxHealth * HealthPctPerSecond * (1.0 + Constitution / 500.0);
        while (NextRegenTickAt <= Now && Health < MaxHealth)
        {
            FractionalHealth += PerTick;
            const int32 Whole = FMath::FloorToInt(FractionalHealth);
            FractionalHealth -= Whole;
            Health = FMath::Min(MaxHealth, Health + Whole);
            NextRegenTickAt += 1.0;
        }
        return Health;
    }
};
