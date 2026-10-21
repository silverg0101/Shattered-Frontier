#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Characters/SFCombatFramingLogic.h"
#include "Framework/SFE1Settings.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatFramingTest,
    "ShatteredFrontier.Camera.CombatFraming.Logic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatFramingTest::RunTest(const FString& Parameters)
{
    const USFE1Settings* Settings = GetDefault<USFE1Settings>();
    FString Error;
    TestTrue(TEXT("Shipped framing settings validate"), Settings->Validate(Error));
    TestTrue(TEXT("Combat target engages at threshold"), SFCombatFramingLogic::ShouldEngage(false, true, true, false, 450, 450));
    TestTrue(TEXT("Hysteresis retains frame up to 110 percent"), SFCombatFramingLogic::ShouldEngage(true, true, true, false, 490, 450));
    TestFalse(TEXT("Zoom out disengages"), SFCombatFramingLogic::ShouldEngage(true, true, true, false, 500, 450));
    TestFalse(TEXT("First person disengages"), SFCombatFramingLogic::ShouldEngage(true, true, true, true, 100, 450));
    TestFalse(TEXT("Lost target disengages"), SFCombatFramingLogic::ShouldEngage(true, false, true, false, 350, 450));
    TestEqual(TEXT("Blend in after half duration"), SFCombatFramingLogic::AdvanceBlend(0, true, 0.3f, 0.6f), 0.5f);
    TestEqual(TEXT("Blend out is twice as slow"), SFCombatFramingLogic::AdvanceBlend(1, false, 0.3f, 0.6f), 0.75f);
    TestEqual(TEXT("Deadzone does not steer"), SFCombatFramingLogic::AssistAxis(10, 15, 8, 2.5f, 1, 0.1f), 10.0f);
    TestTrue(TEXT("Assist turns across yaw wrap"), SFCombatFramingLogic::AssistAxis(175, -175, 8, 2.5f, 1, 0.1f) > 175.0f);
    TestEqual(TEXT("Zero blend never steers"), SFCombatFramingLogic::AssistAxis(0, 30, 8, 2.5f, 0, 0.1f), 0.0f);
    return true;
}
#endif
