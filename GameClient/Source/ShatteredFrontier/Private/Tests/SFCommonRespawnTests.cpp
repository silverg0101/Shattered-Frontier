#include "Combat/SFCommonRespawn.h"
#include "Combat/SFCombatPilot.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCommonRespawnTest,
    "ShatteredFrontier.Combat.Pilot.CommonRespawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCommonRespawnTest::RunTest(const FString& Parameters)
{
    const USFCombatPilotSettings* Settings = GetDefault<USFCombatPilotSettings>();
    const auto Delay = [Settings](int32 Players, double Unit)
    {
        return FSFCommonRespawn::DelaySeconds(Settings->CommonRespawnSeconds, Players,
            Settings->RespawnPlayerSlope, Settings->RespawnPlayerFloor,
            Settings->RespawnJitter, Unit);
    };
    TestEqual(TEXT("One player, lower jitter bound"), Delay(1, 0.0), 270.0);
    TestEqual(TEXT("One player, upper jitter bound"), Delay(1, 1.0), 330.0);
    TestTrue(TEXT("Two players hasten the common spawn"), Delay(2, 0.5) < 300.0);
    TestTrue(TEXT("Population floor applies"), FMath::IsNearlyEqual(Delay(100, 0.5), 105.0, 0.01));
    TestFalse(TEXT("Never before due time"), FSFCommonRespawn::IsDue(299.99, 300.0));
    TestTrue(TEXT("Exactly at due time"), FSFCommonRespawn::IsDue(300.0, 300.0));
    TestFalse(TEXT("Active slot has no pending due time"), FSFCommonRespawn::IsDue(400.0, -1.0));
    return true;
}
#endif
