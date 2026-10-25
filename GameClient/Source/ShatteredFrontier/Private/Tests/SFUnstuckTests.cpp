#include "Misc/AutomationTest.h"
#include "Framework/SFUnstuckRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFUnstuckRulesTest,
    "ShatteredFrontier.E1.Unstuck.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFUnstuckRulesTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Dead characters cannot request"), SFUnstuckRules::CanRequest(false, false, false, 1000, -1));
    TestFalse(TEXT("Combat blocks request"), SFUnstuckRules::CanRequest(true, true, false, 1000, -1));
    TestFalse(TEXT("Instance blocks request"), SFUnstuckRules::CanRequest(true, false, true, 1000, -1));
    TestFalse(TEXT("Second request before 15 minutes"), SFUnstuckRules::CanRequest(true, false, false, 999, 100));
    TestTrue(TEXT("Request at 15 minutes"), SFUnstuckRules::CanRequest(true, false, false, 1000, 100));
    TestTrue(TEXT("First request"), SFUnstuckRules::CanRequest(true, false, false, 1, -1));
    TestFalse(TEXT("Under two metres"), SFUnstuckRules::IsNearbyDestination(FVector::ZeroVector, FVector(199,0,0)));
    TestTrue(TEXT("Two metres"), SFUnstuckRules::IsNearbyDestination(FVector::ZeroVector, FVector(200,0,0)));
    TestTrue(TEXT("Thirty metres"), SFUnstuckRules::IsNearbyDestination(FVector::ZeroVector, FVector(3000,0,0)));
    TestFalse(TEXT("Over thirty metres"), SFUnstuckRules::IsNearbyDestination(FVector::ZeroVector, FVector(3001,0,0)));
    return true;
}
