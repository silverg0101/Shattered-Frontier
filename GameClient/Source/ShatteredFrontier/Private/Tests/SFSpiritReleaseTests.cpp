#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFSpiritReleaseRules.h"
#include "Combat/SFCombatPilot.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSpiritReleaseRulesTest,
    "ShatteredFrontier.Combat.Pilot.SpiritRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFSpiritReleaseRulesTest::RunTest(const FString& Parameters)
{
    const USFCombatPilotSettings* Settings = GetDefault<USFCombatPilotSettings>();
    FString Error;
    TestTrue(TEXT("Shipped spirit fraction is valid"), Settings->Validate(Error));
    TestEqual(TEXT("Spirit healer restores half health"), FSFSpiritReleaseRules::RestoredHealth(80, Settings->SpiritResFraction), 40);
    TestTrue(TEXT("Dead owned sandbox pawn can release"), FSFSpiritReleaseRules::CanRelease(true, true, true, 0, true));
    TestFalse(TEXT("Living pawn cannot release"), FSFSpiritReleaseRules::CanRelease(true, true, true, 80, true));
    TestFalse(TEXT("Repeated release is denied"), FSFSpiritReleaseRules::CanRelease(true, true, true, 40, true));
    TestFalse(TEXT("Client cannot authorize release"), FSFSpiritReleaseRules::CanRelease(true, false, true, 0, true));
    TestFalse(TEXT("Unowned pawn cannot release"), FSFSpiritReleaseRules::CanRelease(true, true, false, 0, true));
    TestFalse(TEXT("Missing point blocks release"), FSFSpiritReleaseRules::CanRelease(true, true, true, 0, false));
    TestFalse(TEXT("Other maps cannot release"), FSFSpiritReleaseRules::CanRelease(false, true, true, 0, true));
    return true;
}
#endif
