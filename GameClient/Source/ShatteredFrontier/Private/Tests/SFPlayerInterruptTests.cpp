#include "Combat/SFPlayerInterruptRules.h"
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
