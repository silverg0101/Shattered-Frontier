#include "Characters/SFTravelMountRules.h"
#include "Framework/SFE1Settings.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFTravelMountRulesTest, "ShatteredFrontier.E1.TravelMount.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFTravelMountRulesTest::RunTest(const FString& Parameters)
{
    const USFE1Settings* Settings = GetDefault<USFE1Settings>();
    FString Error;
    TestTrue(TEXT("Shipped travel mount configuration is valid"), Settings->Validate(Error));
    TestEqual(TEXT("Tier-1 mount moves at 11 m/s"), Settings->TravelMountSpeedMetresPerSecond, 11.0f);
    TestEqual(TEXT("Mount cast takes 1.5 s"), Settings->TravelMountCastSeconds, 1.5f);
    TestEqual(TEXT("Mounted cap is 20 m/s"), Settings->TravelMountSpeedCapMetresPerSecond, 20.0f);
    TestEqual(TEXT("Mounted speed respects cap"), SFTravelMountRules::MountedSpeedCm(25.0f, Settings->TravelMountSpeedCapMetresPerSecond), 2000.0f);
    TestTrue(TEXT("Owned living outdoor player may begin"), SFTravelMountRules::CanBegin(true, true, false, false, false, false, false));
    for (int32 Denial = 0; Denial < 7; ++Denial)
    {
        bool Flags[7] = { true, true, false, false, false, false, false };
        Flags[Denial] = !Flags[Denial];
        TestFalse(FString::Printf(TEXT("Admission denial %d"), Denial),
            SFTravelMountRules::CanBegin(Flags[0], Flags[1], Flags[2], Flags[3], Flags[4], Flags[5], Flags[6]));
    }
    return true;
}
#endif
