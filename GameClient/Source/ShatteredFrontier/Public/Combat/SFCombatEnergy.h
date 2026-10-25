#pragma once

#include "Engine/DeveloperSettings.h"
#include "SFCombatEnergy.generated.h"

/** Realm combat controls. Defaults and bounds ship with the server; effective values apply on the next simulation tick. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Combat Energy"))
class SHATTEREDFRONTIER_API USFCombatEnergySettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Regeneration", meta=(ClampMin="0", ClampMax="15", Units="s", ToolTip="Pause after combat before out-of-combat energy regeneration; realm, immediate, combat.tuning.publish.")) float RegenDelaySeconds = 5.f;
    UPROPERTY(Config, EditAnywhere, Category="Regeneration", meta=(ClampMin="0", ClampMax="0.02", ToolTip="In-combat mana fraction per second; realm, immediate, combat.tuning.publish.")) float ManaCombatBase = .003f;
    UPROPERTY(Config, EditAnywhere, Category="Regeneration", meta=(ClampMin="0", ClampMax="0.0001", ToolTip="Additional in-combat mana fraction per Wisdom; realm, immediate, combat.tuning.publish.")) float ManaWisdomCoefficient = .00002f;
    UPROPERTY(Config, EditAnywhere, Category="Regeneration", meta=(ClampMin="0.01", ClampMax="0.25", ToolTip="Out-of-combat mana fraction per second; realm, immediate, combat.tuning.publish.")) float ManaOutOfCombatFraction = .05f;
    UPROPERTY(Config, EditAnywhere, Category="Regeneration", meta=(ClampMin="0", ClampMax="40", ToolTip="In-combat Stamina points per second before Dexterity; realm, immediate, combat.tuning.publish.")) float StaminaCombatPerSecond = 10.f;
    UPROPERTY(Config, EditAnywhere, Category="Regeneration", meta=(ClampMin="5", ClampMax="100", ToolTip="Out-of-combat Stamina points per second before Dexterity; realm, immediate, combat.tuning.publish.")) float StaminaOutOfCombatPerSecond = 25.f;
    UPROPERTY(Config, EditAnywhere, Category="Costs", meta=(ClampMin="0.5", ClampMax="2", ToolTip="Ability energy cost multiplier; realm, immediate, combat.tuning.publish.")) float AbilityEnergyCostMultiplier = 1.f;
    UPROPERTY(Config, EditAnywhere, Category="Abilities", meta=(ClampMin="2", ClampMax="5", Units="m", ToolTip="Melee ability reach; realm, restart, combat.tuning.publish.")) float MeleeReachMetres = 3.f;
    UPROPERTY(Config, EditAnywhere, Category="Sprint", meta=(ClampMin="0.01", ClampMax="0.20", ToolTip="Sprint stamina drain as fraction of maximum per second; realm, immediate, combat.tuning.publish.")) float SprintDrainFractionPerSecond = .04f;
    UPROPERTY(Config, EditAnywhere, Category="Sprint", meta=(ClampMin="0", ClampMax="10", Units="s", ToolTip="Stamina regeneration pause after sprint; realm, immediate, combat.tuning.publish.")) float SprintRegenDelaySeconds = 2.f;
    UPROPERTY(Config, EditAnywhere, Category="Sprint", meta=(ClampMin="0", ClampMax="0.5", ToolTip="Minimum Stamina fraction required to start sprint; realm, immediate, combat.tuning.publish.")) float SprintMinStartFraction = .10f;
    UPROPERTY(Config, EditAnywhere, Category="Rank 1", meta=(ClampMin="0.01", ClampMax="0.20")) float ChallengeStaminaCostFraction = .05f;
    UPROPERTY(Config, EditAnywhere, Category="Rank 1", meta=(ClampMin="1", ClampMax="20", Units="m")) float ChallengeRangeMetres = 10.f;
    UPROPERTY(Config, EditAnywhere, Category="Rank 1", meta=(ClampMin="1", ClampMax="60", Units="s")) float ChallengeCooldownSeconds = 8.f;
    UPROPERTY(Config, EditAnywhere, Category="Rank 1", meta=(ClampMin="0.01", ClampMax="0.20")) float PommelStaminaCostFraction = .05f;
    UPROPERTY(Config, EditAnywhere, Category="Rank 1", meta=(ClampMin="1", ClampMax="60", Units="s")) float PommelCooldownSeconds = 15.f;
    UPROPERTY(Config, EditAnywhere, Category="Rank 1", meta=(ClampMin="0.1", ClampMax="1", ToolTip="Pommel physical damage as a fraction of the normal strike; release-authored.")) float PommelPhysicalModifier = .3f;
    UPROPERTY(Config, EditAnywhere, Category="Rank 1", meta=(ClampMin="0.01", ClampMax="0.20")) float CounterspellManaCostFraction = .04f;
    UPROPERTY(Config, EditAnywhere, Category="Rank 1", meta=(ClampMin="1", ClampMax="40", Units="m")) float CounterspellRangeMetres = 30.f;
    UPROPERTY(Config, EditAnywhere, Category="Rank 1", meta=(ClampMin="1", ClampMax="60", Units="s")) float CounterspellCooldownSeconds = 24.f;
    bool Validate(FString& Error) const
    {
        const auto Within = [](float Value, float Min, float Max) { return FMath::IsFinite(Value) && Value >= Min && Value <= Max; };
        if (!Within(RegenDelaySeconds, 0, 15) || !Within(ManaCombatBase, 0, .02f) ||
            !Within(ManaWisdomCoefficient, 0, .0001f) || !Within(ManaOutOfCombatFraction, .01f, .25f) ||
            !Within(StaminaCombatPerSecond, 0, 40) || !Within(StaminaOutOfCombatPerSecond, 5, 100) ||
            !Within(AbilityEnergyCostMultiplier, .5f, 2) || !Within(MeleeReachMetres, 2, 5) ||
            !Within(SprintDrainFractionPerSecond, .01f, .20f) ||
            !Within(SprintRegenDelaySeconds, 0, 10) || !Within(SprintMinStartFraction, 0, .5f) ||
            !Within(ChallengeStaminaCostFraction, .01f, .20f) || !Within(ChallengeRangeMetres, 1, 20) ||
            !Within(ChallengeCooldownSeconds, 1, 60) || !Within(PommelStaminaCostFraction, .01f, .20f) ||
            !Within(PommelCooldownSeconds, 1, 60) || !Within(PommelPhysicalModifier, .1f, 1.f) ||
            !Within(CounterspellManaCostFraction, .01f, .20f) ||
            !Within(CounterspellRangeMetres, 1, 40) || !Within(CounterspellCooldownSeconds, 1, 60))
        { Error = TEXT("Combat energy configuration is outside release bounds"); return false; }
        return true;
    }
};

namespace SFCombatEnergy
{
inline float MaxStamina(float Constitution, float Dexterity) { return 100.f + 5.f * Constitution + 3.f * Dexterity; }
inline float MaxMana(float Intelligence, int32 Level) { return 100.f + 20.f * Intelligence + 10.f * Level; }
inline float AbilityCost(float Maximum, float Fraction, float Multiplier) { return Maximum * Fraction * Multiplier; }
inline bool TrySpend(float& Current, float Maximum, float Fraction, float Multiplier)
{
    const float Cost = AbilityCost(Maximum, Fraction, Multiplier);
    if (Maximum <= 0 || Fraction <= 0 || Current + KINDA_SMALL_NUMBER < Cost) return false;
    Current = FMath::Max(0.f, Current - Cost);
    return true;
}
inline bool CanStartSprint(float Current, float Maximum, float MinimumFraction, bool bInCombat)
{ return !bInCombat && Maximum > 0 && Current >= Maximum * MinimumFraction && Current > 0; }
inline float SprintDrain(float Current, float Maximum, float FractionPerSecond, float Seconds)
{ return FMath::Max(0.f, Current - Maximum * FractionPerSecond * Seconds); }
inline bool CanUseRankOne(int32 Level, bool bAlive, bool bValidTarget, bool bCasting, bool bInterrupt)
{ return Level >= 10 && bAlive && bValidTarget && (!bInterrupt || bCasting); }
}
