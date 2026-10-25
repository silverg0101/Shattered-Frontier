#include "Combat/SFPlayerInterruptRules.h"
#include "Combat/SFCastRules.h"
#include "Combat/SFStarterAbilities.h"
#include "Combat/SFCastingComponent.h"
#include "Combat/SFCombatEnergy.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPlayerInterruptRulesTest, "ShatteredFrontier.Combat.PlayerInterrupt.Admission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPlayerInterruptRulesTest::RunTest(const FString& Parameters)
{
    using SFPlayerInterruptRules::CanInterrupt;
    TestTrue(TEXT("live caster at reach boundary"), CanInterrupt(true, true, true, 280, 280, true));
    TestFalse(TEXT("outside reach"), CanInterrupt(true, true, true, 280.01, 280, true));
    TestFalse(TEXT("blocked sight"), CanInterrupt(true, true, true, 100, 280, false));
    TestFalse(TEXT("idle target"), CanInterrupt(true, true, false, 100, 280, true));
    TestFalse(TEXT("dead caster"), CanInterrupt(true, false, true, 100, 280, true));
    TestFalse(TEXT("dead requester"), CanInterrupt(false, true, true, 100, 280, true));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNpcCastingContractTest, "ShatteredFrontier.Combat.NpcCasting.Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFNpcCastingContractTest::RunTest(const FString& Parameters)
{
    using namespace SFCastRules;
    const auto* Mend = SFStarterAbilities::Find(TEXT("mend"));
    if (!TestNotNull(TEXT("captain uses published Mend"), Mend)) return false;
    const auto* Settings = GetDefault<USFCastingSettings>();
    FString Error;
    TestTrue(TEXT("bounded NPC casting tuning"), Settings->Validate(Error));
    FAbility Ability; Ability.Id = 3; Ability.School = 2; Ability.Cast = Mend->CastSeconds;
    Ability.Cooldown = Settings->CaptainMendCooldownSeconds;
    FState Complete;
    TestTrue(TEXT("NPC starts a two-second server cast"), Complete.Request(Ability, 10, 0, Settings->Tuning()) == EStart::Started);
    bool bCompleted = false;
    Complete.Advance(11.99, Ability, bCompleted);
    TestFalse(TEXT("heal withheld before cast end"), bCompleted);
    Complete.Advance(12, Ability, bCompleted);
    TestTrue(TEXT("heal becomes due at cast end"), bCompleted);
    for (const auto Kind : {TEXT("pommel_strike"), TEXT("counterspell")})
    {
        FState Interrupted;
        Interrupted.Request(Ability, 20, 0, Settings->Tuning());
        int32 Mana = 100 - FMath::RoundToInt(100 * Mend->StaminaCostFraction);
        if (Interrupted.Phase != EPhase::Idle)
        {
            Mana += Refund(100 - Mana, true, Settings->CancelRefundPercent);
            Interrupted.Interrupt(21, Settings->NpcSchoolLockoutSeconds, Ability.School);
        }
        TestEqual(FString::Printf(TEXT("%s refunds once"), Kind), Mana, 100);
        TestTrue(FString::Printf(TEXT("%s locks holy for three seconds"), Kind),
            Interrupted.Request(Ability, 23.99, 0, Settings->Tuning()) == EStart::Locked);
        TestEqual(FString::Printf(TEXT("%s duplicate idle interrupt has no refund"), Kind), Mana, 100);
    }
    TestTrue(TEXT("counterspell reaches thirty metres"), SFPlayerInterruptRules::CanInterrupt(true, true, true,
        GetDefault<USFCombatEnergySettings>()->CounterspellRangeMetres * 100.0,
        GetDefault<USFCombatEnergySettings>()->CounterspellRangeMetres * 100.0, true));
    return true;
}
#endif
