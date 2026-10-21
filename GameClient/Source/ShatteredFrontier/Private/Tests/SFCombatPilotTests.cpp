#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Combat/SFCombatPilotRewards.h"
#include "GameFramework/PlayerController.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatPilotRewardTest,
    "ShatteredFrontier.Combat.Pilot.Reward",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatPilotRewardTest::RunTest(const FString& Parameters)
{
    const int32 PerBoar = GetDefault<USFCombatPilotSettings>()->HidesPerBoar;
    TSet<FString> Granted;
    TestEqual(TEXT("First death rewards"), SFCombatPilotRewards::HidesFor(Granted, TEXT("boar_farm_edge:0:1"), false, PerBoar), PerBoar);
    TestEqual(TEXT("Duplicate death event rewards nothing"), SFCombatPilotRewards::HidesFor(Granted, TEXT("boar_farm_edge:0:1"), false, PerBoar), 0);
    TestEqual(TEXT("Respawned boar is a new death"), SFCombatPilotRewards::HidesFor(Granted, TEXT("boar_farm_edge:0:2"), false, PerBoar), PerBoar);
    TestEqual(TEXT("Protected-town kill rewards nothing"), SFCombatPilotRewards::HidesFor(Granted, TEXT("boar_farm_edge:1:3"), true, PerBoar), 0);
    TestEqual(TEXT("...and stays spent once the boar leaves town"), SFCombatPilotRewards::HidesFor(Granted, TEXT("boar_farm_edge:1:3"), false, PerBoar), 0);

#if SF_WITH_MINIMAL_INVENTORY
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    auto* Controller = Fixture.GetTestWorld()->SpawnActor<APlayerController>();
    if (!Controller) return false;
    SFCombatPilotRewards::AddPilotBag(Controller);
    TSet<FString> Deaths;
    for (int32 Event = 0; Event < 2; ++Event)  // the same death reported twice
    {
        const int32 Hides = SFCombatPilotRewards::HidesFor(Deaths, TEXT("boar_forest_edge:2:7"), false, PerBoar);
        if (Hides > 0) { TestTrue(TEXT("Hides fit the pilot bag"), SFCombatPilotRewards::GrantHides(Controller, Hides)); }
    }
    TestEqual(TEXT("Bag holds one death's hides"), SFCombatPilotRewards::CountHides(Controller), PerBoar);
    TestFalse(TEXT("No bag, no grant"), SFCombatPilotRewards::GrantHides(nullptr, PerBoar));
#endif
    return true;
}
#endif
