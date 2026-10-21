#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFTabTargetSelection.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFTabTargetSelectionTest,
    "ShatteredFrontier.Combat.TabTarget.Selection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFTabTargetSelectionTest::RunTest(const FString& Parameters)
{
    const TArray<FSFTabTargetCandidate> Candidates = {{3, 210.0}, {1, 80.0}, {4, 300.0}, {2, 150.0}};
    const double RangeCm = 280.0;
    TestEqual(TEXT("Nearest first"), FSFTabTargetSelection::Next(Candidates, INDEX_NONE, RangeCm), 1);
    TestEqual(TEXT("Cycle by distance"), FSFTabTargetSelection::Next(Candidates, 1, RangeCm), 2);
    TestEqual(TEXT("Cycle continues"), FSFTabTargetSelection::Next(Candidates, 2, RangeCm), 3);
    TestEqual(TEXT("Wrap to nearest"), FSFTabTargetSelection::Next(Candidates, 3, RangeCm), 1);
    TestEqual(TEXT("Out of range excluded"), FSFTabTargetSelection::Next(Candidates, 4, RangeCm), 1);
    TestEqual(TEXT("Empty clears"), FSFTabTargetSelection::Next({}, 1, RangeCm), INDEX_NONE);
    TestEqual(TEXT("Current gone selects nearest"), FSFTabTargetSelection::Next(Candidates, 99, RangeCm), 1);
    return true;
}
#endif
