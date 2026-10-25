#include "Misc/AutomationTest.h"
#include "Combat/SFFallDamage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFallDamageTest,
    "ShatteredFrontier.Combat.Pilot.FallDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFFallDamageTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("At threshold"), SFFallDamage::Amount(80, 12.0f, 12.0f), 0);
    TestEqual(TEXT("Twenty m/s over threshold"), SFFallDamage::Amount(80, 32.0f, 12.0f), 80);
    TestEqual(TEXT("Terminal speed capped coefficient"), SFFallDamage::Amount(80, 50.0f, 12.0f), 96);
    TestEqual(TEXT("Fractional hit rounds once"), SFFallDamage::Amount(80, 22.0f, 12.0f), 40);
    return true;
}
