#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatPilotRulesTest,
    "ShatteredFrontier.Combat.Pilot.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatPilotRulesTest::RunTest(const FString& Parameters)
{
    const auto* Settings = GetDefault<USFCombatPilotSettings>();
    FString Error;
    TestTrue(TEXT("Shipped combat pilot settings are bounded"), Settings->Validate(Error));
    TestTrue(TEXT("Front target inside range and off cooldown"), SFCombatPilotRules::CanMelee(220, 0.9, 2, 1, 280));
    TestFalse(TEXT("Out of reach"), SFCombatPilotRules::CanMelee(281, 0.9, 2, 1, 280));
    TestFalse(TEXT("Cooldown enforced"), SFCombatPilotRules::CanMelee(220, 0.9, 0.9, 1, 280));
    TestFalse(TEXT("Behind attacker"), SFCombatPilotRules::CanMelee(220, -0.2, 2, 1, 280));
    TestEqual(TEXT("Overkill clamps at zero"), SFCombatPilotRules::RemainingHealth(8, 20), 0);
    TestEqual(TEXT("Negative damage cannot heal"), SFCombatPilotRules::RemainingHealth(8, -2), 8);

    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    auto* Mode = Fixture.GetTestWorld()->SpawnActor<ASFCombatPilotGameMode>();
    if (!Mode) return false;
    const TArray<FVector2D> Town = {{-4,-4},{4,-4},{4,4},{-4,4}};
    TestTrue(TEXT("Town polygon contains its centre"), Mode->IsInsideTerritory(FVector::ZeroVector, Town));
    TestTrue(TEXT("Town boundary is protected"), Mode->IsInsideTerritory(FVector(400,0,0), Town));
    TestFalse(TEXT("Town boundary excludes countryside"), Mode->IsInsideTerritory(FVector(500,0,0), Town));
    return true;
}
#endif
