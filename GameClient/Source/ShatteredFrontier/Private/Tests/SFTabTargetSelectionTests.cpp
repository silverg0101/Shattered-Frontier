#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFTabTargetSelection.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFTabTargetSelectionTest,
    "ShatteredFrontier.Combat.TabTarget.Selection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFTabTargetSelectionTest::RunTest(const FString& Parameters)
{
    const TArray<FSFTabTargetCandidate> Candidates = {
        {3, 210.0, 1.0, true}, {1, 80.0, 1.0, true}, {4, 3100.0, 1.0, true},
        {2, 150.0, 0.6, true}, {5, 90.0, -0.8, true}, {6, 60.0, 1.0, false}};
    TArray<int32> Snapshot = FSFTabTargetSelection::Snapshot(Candidates, 3000.0, 120.0, true);
    TestEqual(TEXT("Cone, range, and LOS filter"), Snapshot.Num(), 3);
    TestEqual(TEXT("Pure distance orders first"), Snapshot[0], 1);
    TestEqual(TEXT("Pure distance orders second"), Snapshot[1], 2);
    int32 NextIndex = 0;
    TestEqual(TEXT("First Tab"), FSFTabTargetSelection::Next(Snapshot, NextIndex), 1);
    TestEqual(TEXT("Second Tab"), FSFTabTargetSelection::Next(Snapshot, NextIndex), 2);
    TestEqual(TEXT("Third Tab"), FSFTabTargetSelection::Next(Snapshot, NextIndex), 3);
    TestTrue(TEXT("Wrap refreshes snapshot"), FSFTabTargetSelection::ShouldRefresh(1.0, 0.9, 1.5, NextIndex, Snapshot.Num()));
    NextIndex = 0;
    TestEqual(TEXT("Wrap picks nearest"), FSFTabTargetSelection::Next(Snapshot, NextIndex), 1);
    TestFalse(TEXT("Snapshot persists before timeout"), FSFTabTargetSelection::ShouldRefresh(2.0, 1.0, 1.5, NextIndex, Snapshot.Num()));
    TestTrue(TEXT("Inactivity resets snapshot"), FSFTabTargetSelection::ShouldRefresh(2.5, 1.0, 1.5, NextIndex, Snapshot.Num()));
    TestFalse(TEXT("Within drop hysteresis"), FSFTabTargetSelection::ShouldDrop(4400.0, 4500.0));
    TestTrue(TEXT("Beyond drop hysteresis"), FSFTabTargetSelection::ShouldDrop(4501.0, 4500.0));
    TestEqual(TEXT("Full cone includes behind"), FSFTabTargetSelection::Snapshot(Candidates, 3000.0, 360.0, false).Num(), 5);
    return true;
}
#endif
