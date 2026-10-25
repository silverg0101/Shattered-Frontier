#include "Combat/SFPlayerInterruptRules.h"
#include "Combat/SFCastRules.h"
#include "Combat/SFStarterAbilities.h"
#include "Combat/SFCastingComponent.h"
#include "Combat/SFCombatEnergy.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Characters/SFE1Character.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#endif

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

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNpcMendCapabilityTest, "ShatteredFrontier.Combat.NpcCasting.ProfileAdmissionAndCompletion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFNpcMendCapabilityTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectDir(), TEXT("../tests/fixtures/npc-mend"));
    const auto* Mend = SFStarterAbilities::Find(TEXT("mend"));
    if (!TestNotNull(TEXT("Published Mend"), Mend)) return false;
    const auto* Settings = GetDefault<USFCastingSettings>();
    for (const bool bCanMend : {true, false})
    {
        TMap<FString, SFNpcProfiles::FProfile> Profiles;
        FString Error;
        if (!TestTrue(FString::Printf(TEXT("Validated fixture loads: %s"), *Error), SFNpcProfiles::Load(
            Directory / (bCanMend ? TEXT("enabled.json") : TEXT("disabled.json")),
            Directory / TEXT("loot_rules"), Profiles, Error))) return false;
        SFNpcProfiles::FScopedTestCatalogue Catalogue(Profiles);
        FTestWorldWrapper Fixture;
        if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
        UWorld* World = Fixture.GetTestWorld();
        ASFCombatNpc* Npc = World->SpawnActor<ASFCombatNpc>();
        ASFE1Character* Attacker = World->SpawnActor<ASFE1Character>(FVector(1000, 0, 0), FRotator::ZeroRotator);
        if (!TestNotNull(TEXT("NPC actor"), Npc) || !TestNotNull(TEXT("Damage source"), Attacker)) return false;
        if (!TestTrue(TEXT("Non-captain profile configures actor"), Npc->ConfigureCreature(TEXT("npc/mend_reuse_fixture")))) return false;
        Npc->Initialize(nullptr, TEXT("mend_reuse_test"), 0, {}, 100, 1);
        Npc->ReceivePilotDamage(Attacker, 75);
        USFCastingComponent* Casting = Npc->GetCasting();
        if (!TestNotNull(TEXT("Real casting component"), Casting)) return false;
        const int32 ManaBefore = Casting->GetNpcMana();
        const int32 HealthBefore = Npc->Health();
        TestEqual(TEXT("Actor capability matches authored fixture"), Npc->CanCastMend(), bCanMend);
        TestEqual(TEXT("Actual cast admission follows capability"), Casting->TryBeginNpcMend(), bCanMend);
        TestEqual(TEXT("Heal withheld at admission"), Npc->Health(), HealthBefore);
        if (bCanMend)
        {
            TestTrue(TEXT("Actual component entered casting phase"), Casting->GetCastView().Phase == ESFCastPhase::Casting);
            TestEqual(TEXT("Published mana cost paid"), Casting->GetNpcMana(),
                ManaBefore - FMath::RoundToInt(Settings->CaptainManaMaximum * Mend->StaminaCostFraction));
        }
        const double StartedAt = World->GetTimeSeconds();
        for (float Elapsed = 0; Elapsed <= Mend->CastSeconds + .1f; Elapsed += .05f)
        {
            World->Tick(LEVELTICK_All, .05f);
            // Synchronous test worlds do not schedule ordinary component ticks across engine frames.
            Casting->TickComponent(.05f, LEVELTICK_All, nullptr);
        }
        TestTrue(TEXT("Server world clock advanced through cast duration"), World->GetTimeSeconds() - StartedAt >= Mend->CastSeconds);
        TestTrue(TEXT("Actual component returns idle after cast window"), Casting->GetCastView().Phase == ESFCastPhase::Idle);
        TestEqual(TEXT("Only authored Mend capability heals through completion callback"), Npc->Health(),
            HealthBefore + (bCanMend ? FMath::RoundToInt(Npc->MaximumHealth() * Settings->CaptainMendHealFraction) : 0));
        if (!bCanMend) TestEqual(TEXT("Rejected cast never spends mana"), Casting->GetNpcMana(), ManaBefore);
    }
    TestNotNull(TEXT("Scoped fixture restores production catalogue"), SFNpcProfiles::Find(TEXT("npc/bandit_captain")));
    TestNull(TEXT("Scoped fixture leaves no production test ID"), SFNpcProfiles::Find(TEXT("npc/mend_reuse_fixture")));
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
