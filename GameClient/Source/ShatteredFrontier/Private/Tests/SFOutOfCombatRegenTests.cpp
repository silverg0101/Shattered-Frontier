#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFOutOfCombatRegen.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFOutOfCombatRegenTest,
    "ShatteredFrontier.Combat.Pilot.OutOfCombatRegen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFOutOfCombatRegenTest::RunTest(const FString& Parameters)
{
    FSFOutOfCombatRegen Clock;
    Clock.Hostile(0.0);
    TestEqual(TEXT("Threat prevents combat exit after timeout"), Clock.Advance(8.0, true, 40, 80, 10, 5, 5, .03), 40);
    TestTrue(TEXT("Still in combat while threatened"), Clock.bInCombat);
    Clock.Hostile(8.0);
    TestEqual(TEXT("Fresh hostile action resets exit"), Clock.Advance(12.99, false, 40, 80, 10, 5, 5, .03), 40);
    TestTrue(TEXT("Still in combat before five seconds"), Clock.bInCombat);
    TestEqual(TEXT("Combat exit does not heal"), Clock.Advance(13.0, false, 40, 80, 10, 5, 5, .03), 40);
    TestFalse(TEXT("Combat exited at five seconds"), Clock.bInCombat);
    TestEqual(TEXT("No healing before regeneration delay"), Clock.Advance(17.99, false, 40, 80, 10, 5, 5, .03), 40);
    TestEqual(TEXT("First one-second tick includes Constitution multiplier"), Clock.Advance(18.0, false, 40, 80, 10, 5, 5, .03), 42);
    TestEqual(TEXT("Fraction carries across ticks"), Clock.Advance(20.0, false, 42, 80, 10, 5, 5, .03), 47);
    Clock.Hostile(20.5);
    TestEqual(TEXT("New combat interrupts healing"), Clock.Advance(30.0, true, 47, 80, 10, 5, 5, .03), 47);
    TestEqual(TEXT("Dead player never regenerates"), Clock.Advance(40.0, false, 0, 80, 10, 5, 5, .03), 0);
    return true;
}
#endif
