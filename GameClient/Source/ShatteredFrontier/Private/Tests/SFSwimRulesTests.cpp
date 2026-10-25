#include "Misc/AutomationTest.h"
#include "Characters/SFSwimRules.h"
#include "Framework/SFE1Settings.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSwimRulesTest, "ShatteredFrontier.Movement.Swimming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFSwimRulesTest::RunTest(const FString& Parameters)
{
    const USFE1Settings* Settings = GetDefault<USFE1Settings>();
    FString Error;
    TestTrue(TEXT("movement settings valid"), Settings->Validate(Error));
    TestEqual(TEXT("swim speed in cm/s"), Settings->SwimSpeedMetresPerSecond * 100.0f, 300.0f);
    TestFalse(TEXT("shallow water stays walk"), SFSwimRules::IsDeepEnough(129.0f, 0.0f, Settings->SwimDepthMetres));
    TestTrue(TEXT("exact threshold admits swim"), SFSwimRules::IsDeepEnough(130.0f, 0.0f, Settings->SwimDepthMetres));
    TestFalse(TEXT("invalid surface denied"), SFSwimRules::IsDeepEnough(NAN, 0.0f, Settings->SwimDepthMetres));
    return true;
}
#endif
