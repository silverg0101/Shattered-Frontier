#include "Characters/SFE1VitalsHud.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFE1VitalsHudModelTest,
    "ShatteredFrontier.Characters.VitalsHud",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFE1VitalsHudModelTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Full health"),FSFE1VitalsModel::HealthFraction(80,80),1.f);
    TestEqual(TEXT("Boar damage lowers the bar"),FSFE1VitalsModel::HealthFraction(60,80),0.75f);
    TestEqual(TEXT("Overkill clamps to zero"),FSFE1VitalsModel::HealthFraction(-5,80),0.f);
    TestEqual(TEXT("Missing max is empty"),FSFE1VitalsModel::HealthFraction(10,0),0.f);
    TestEqual(TEXT("Countdown mid-delay"),FSFE1VitalsModel::RemainingSeconds(10.0,11.5,4.f),2.5f);
    TestEqual(TEXT("Countdown never negative"),FSFE1VitalsModel::RemainingSeconds(10.0,20.0,4.f),0.f);
    TestEqual(TEXT("Not defeated has no countdown"),FSFE1VitalsModel::RemainingSeconds(-1.0,20.0,4.f),0.f);

    TestEqual(TEXT("Countdown rounds up and formats mm:ss"),FSFE1VitalsModel::Countdown(3.2f),FString(TEXT("00:04")));
    TestEqual(TEXT("Countdown at zero"),FSFE1VitalsModel::Countdown(0.f),FString(TEXT("00:00")));
    TestEqual(TEXT("Respawn point title"),FSFE1VitalsModel::RespawnTitle(TEXT("Silverbrook"),TEXT("north_gate")),FString(TEXT("Silverbrook North Gate")));
    TestEqual(TEXT("Respawn title without a zone"),FSFE1VitalsModel::RespawnTitle(TEXT(""),TEXT("arrival")),FString(TEXT("Arrival")));

    TestEqual(TEXT("Service deadline countdown rounds up"), FSFE1VitalsModel::RespawnStatus(42.2f, true), FString(TEXT("Respawn available in 43s")));
    TestEqual(TEXT("Expired gate is ready, never a claimed request"), FSFE1VitalsModel::RespawnStatus(0, true), FString(TEXT("Ready to respawn.")));
    TestEqual(TEXT("Missing deadline awaits authority"), FSFE1VitalsModel::RespawnStatus(0, false), FString(TEXT("Waiting for respawn information\u2026")));
    TestEqual(TEXT("Only accepted request shows respawning"), FSFE1VitalsModel::RespawnStatus(0, true, true), FString(TEXT("Respawning\u2026")));
    TestEqual(TEXT("Rejected request returns to ready"), FSFE1VitalsModel::RespawnStatus(0, true, false), FString(TEXT("Ready to respawn.")));

    FSFE1VitalsModel Model;
    Model.Observe(0,1.0);
    TestFalse(TEXT("Zero health before combat is inactive, so no HUD"),Model.bActive);
    TestFalse(TEXT("Inactive is not defeated"),Model.IsDefeated());
    Model.Observe(80,2.0);
    TestTrue(TEXT("Positive health activates the HUD"),Model.bActive);
    Model.Observe(20,3.0);
    TestFalse(TEXT("Damaged is alive"),Model.IsDefeated());
    Model.Observe(0,5.0);
    TestTrue(TEXT("Zero after positive is a defeat"),Model.IsDefeated());
    TestEqual(TEXT("Defeat time captured"),Model.DefeatedAt,5.0);
    Model.Observe(0,6.0);
    TestEqual(TEXT("Repeated zero keeps the first defeat time"),Model.DefeatedAt,5.0);
    Model.Observe(80,9.0);
    TestFalse(TEXT("Respawn health clears the overlay"),Model.IsDefeated());
    TestEqual(TEXT("Respawn clears the defeat time"),Model.DefeatedAt,-1.0);
    return true;
}
#endif
