#include "Combat/SFCombatEnergy.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatEnergyTest, "ShatteredFrontier.Combat.Energy.RankOne",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCombatEnergyTest::RunTest(const FString& Parameters)
{
    const USFCombatEnergySettings* Tune = GetDefault<USFCombatEnergySettings>();
    FString Error;
    TestTrue(TEXT("shipped bounds"), Tune->Validate(Error));
    TestEqual(TEXT("pommel physical modifier"), Tune->PommelPhysicalModifier, .3f);
    TestEqual(TEXT("fighter maximum Stamina"), SFCombatEnergy::MaxStamina(25, 15), 270.f);
    TestEqual(TEXT("fighter level 10 maximum Mana"), SFCombatEnergy::MaxMana(10, 10), 400.f);
    float Stamina = 270.f;
    TestTrue(TEXT("challenge spends five percent of maximum"), SFCombatEnergy::TrySpend(Stamina, 270.f,
        Tune->ChallengeStaminaCostFraction, Tune->AbilityEnergyCostMultiplier));
    TestEqual(TEXT("challenge balance"), Stamina, 256.5f);
    float Mana = 400.f;
    TestTrue(TEXT("counterspell spends four percent of maximum Mana"), SFCombatEnergy::TrySpend(Mana, 400.f,
        Tune->CounterspellManaCostFraction, Tune->AbilityEnergyCostMultiplier));
    TestEqual(TEXT("counterspell balance"), Mana, 384.f);
    float Low = 10.f;
    TestFalse(TEXT("insufficient Stamina refuses"), SFCombatEnergy::TrySpend(Low, 270.f, .05f, 1.f));
    TestEqual(TEXT("refusal does not charge"), Low, 10.f);
    TestFalse(TEXT("level 9 refuses"), SFCombatEnergy::CanUseRankOne(9, true, true, true, true));
    TestTrue(TEXT("level 10 accepts"), SFCombatEnergy::CanUseRankOne(10, true, true, true, true));
    TestFalse(TEXT("interrupt requires cast"), SFCombatEnergy::CanUseRankOne(10, true, true, false, true));
    TestFalse(TEXT("dead target refuses"), SFCombatEnergy::CanUseRankOne(10, true, false, true, true));
    TestTrue(TEXT("sprint starts at ten percent"), SFCombatEnergy::CanStartSprint(27, 270, .1f, false));
    TestFalse(TEXT("sprint below threshold refuses"), SFCombatEnergy::CanStartSprint(26.9f, 270, .1f, false));
    TestFalse(TEXT("combat refuses sprint"), SFCombatEnergy::CanStartSprint(270, 270, .1f, true));
    TestEqual(TEXT("one second drains four percent"), SFCombatEnergy::SprintDrain(270, 270, .04f, 1), 259.2f);
    TestEqual(TEXT("last partial tick clamps to zero"), SFCombatEnergy::SprintDrain(5, 270, .04f, .5f), 0.f);
    return true;
}
#endif
