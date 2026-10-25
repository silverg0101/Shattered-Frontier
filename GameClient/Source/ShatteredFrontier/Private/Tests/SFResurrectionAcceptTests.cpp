#include "Misc/AutomationTest.h"
#include "Combat/SFResurrectionAcceptRules.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFResurrectionAcceptTest,
    "ShatteredFrontier.Combat.Pilot.ResurrectionAccept",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFResurrectionAcceptTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Default player ability restores 35 percent of 80"),
        FSFResurrectionAcceptRules::RestoredHealth(80, 0.35f), 28);
    TestTrue(TEXT("Dead owner accepts live matching offer"),
        FSFResurrectionAcceptRules::CanAccept(true, true, true, 0, 7, 7, 10.0, 70.0));
    TestFalse(TEXT("Expiry is exclusive"),
        FSFResurrectionAcceptRules::CanAccept(true, true, true, 0, 7, 7, 70.0, 70.0));
    TestFalse(TEXT("Stale offer cannot accept a newer death"),
        FSFResurrectionAcceptRules::CanAccept(true, true, true, 0, 8, 7, 10.0, 70.0));
    TestFalse(TEXT("Replay after revival is denied"),
        FSFResurrectionAcceptRules::CanAccept(true, true, true, 28, 0, 7, 10.0, 70.0));
    TestFalse(TEXT("Other client cannot accept"),
        FSFResurrectionAcceptRules::CanAccept(true, true, false, 0, 7, 7, 10.0, 70.0));
    TestFalse(TEXT("Client cannot authorize"),
        FSFResurrectionAcceptRules::CanAccept(true, false, true, 0, 7, 7, 10.0, 70.0));
    TestFalse(TEXT("Other maps cannot accept"),
        FSFResurrectionAcceptRules::CanAccept(false, true, true, 0, 7, 7, 10.0, 70.0));
    return true;
}
#endif
