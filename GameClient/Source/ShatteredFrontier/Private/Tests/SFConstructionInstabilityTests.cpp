#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Construction/SFConstructionInstability.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionInstabilityLifecycleTest,
    "ShatteredFrontier.Construction.Instability.DeadlineLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionInstabilityLifecycleTest::RunTest(const FString& Parameters)
{
    FSFConstructionInstability Clock;
    const FIntVector A(2, 0, 0);
    const FIntVector B(-1, 3, 0);
    TSet<FIntVector> Unsupported;
    Unsupported.Add(A);
    Unsupported.Add(B);
    TestTrue(TEXT("Initial unsupported episode accepted"), Clock.Refresh(Unsupported, 10.0, 10.0));

    double Deadline = -1.0;
    TestTrue(TEXT("Deadline available"), Clock.GetDeadline(A, Deadline));
    TestEqual(TEXT("Episode deadline is world time plus grace"), Deadline, 20.0);
    TestTrue(TEXT("Repeated refresh keeps active episode"), Clock.Refresh(Unsupported, 15.0, 10.0));
    TestTrue(TEXT("Existing deadline remains unchanged"), Clock.GetDeadline(A, Deadline));
    TestEqual(TEXT("Unrelated refresh does not extend grace"), Deadline, 20.0);
    TArray<FIntVector> Due;
    Due.Add(FIntVector(99, 99, 99));
    TestTrue(TEXT("Due query before deadline succeeds"), Clock.Due(19.999, Due));
    TestEqual(TEXT("Nothing due just before deadline"), Due.Num(), 0);
    TestTrue(TEXT("Exact deadline is due"), Clock.Due(20.0, Due));
    TestEqual(TEXT("Both members due at exact deadline"), Due.Num(), 2);
    TestTrue(TEXT("Due results have stable lexicographic order"), Due[0] == B && Due[1] == A);

    TestFalse(TEXT("Refresh cannot move backwards after a due read"), Clock.Refresh(Unsupported, 19.0, 10.0));
    TestFalse(TEXT("Due reads cannot move backwards"), Clock.Due(19.0, Due));
    TestEqual(TEXT("Rejected backwards read preserves due output"), Due.Num(), 2);
    TestTrue(TEXT("Overdue wave remains queryable after read"), Clock.Due(25.0, Due));
    TestEqual(TEXT("Due read does not consume deadlines"), Due.Num(), 2);

    TSet<FIntVector> Recovered;
    Recovered.Add(B);
    TestTrue(TEXT("Recovery reconciliation accepted"), Clock.Refresh(Recovered, 25.0, 10.0));
    TestEqual(TEXT("Recovered member deadline removed"), Clock.Num(), 1);
    TestFalse(TEXT("Recovered member has no deadline"), Clock.GetDeadline(A, Deadline));
    TSet<FIntVector> NewEpisode;
    NewEpisode.Add(A);
    TestTrue(TEXT("New support-loss episode accepted"), Clock.Refresh(NewEpisode, 26.0, 10.0));
    TestTrue(TEXT("New episode receives a new deadline"), Clock.GetDeadline(A, Deadline));
    TestEqual(TEXT("New episode deadline starts from its own time"), Deadline, 36.0);

    TArray<FIntVector> PreservedOut{B};
    TestFalse(TEXT("Backward due time rejected"), Clock.Due(25.0, PreservedOut));
    TestTrue(TEXT("Invalid due query preserves output"), PreservedOut.Num() == 1 && PreservedOut[0] == B);
    TestFalse(TEXT("NaN due time rejected"), Clock.Due(std::numeric_limits<double>::quiet_NaN(), PreservedOut));
    TestTrue(TEXT("NaN due query preserves output"), PreservedOut.Num() == 1 && PreservedOut[0] == B);
    TestFalse(TEXT("Infinite due time rejected"), Clock.Due(std::numeric_limits<double>::infinity(), PreservedOut));
    TestFalse(TEXT("Backward refresh rejected"), Clock.Refresh(NewEpisode, 24.0, 10.0));
    TestFalse(TEXT("NaN refresh time rejected"), Clock.Refresh(NewEpisode, std::numeric_limits<double>::quiet_NaN(), 10.0));
    TestFalse(TEXT("Infinite refresh time rejected"), Clock.Refresh(NewEpisode, std::numeric_limits<double>::infinity(), 10.0));
    TestFalse(TEXT("Grace below one second rejected"), Clock.Refresh(NewEpisode, 26.0, 0.99));
    TestFalse(TEXT("Grace above sixty seconds rejected"), Clock.Refresh(NewEpisode, 26.0, 60.01));
    TestTrue(TEXT("Invalid refreshes preserve episode deadline"), Clock.GetDeadline(A, Deadline));
    TestEqual(TEXT("Invalid refreshes preserve deadline value"), Deadline, 36.0);
    TestEqual(TEXT("Invalid refreshes preserve member set"), Clock.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionInstabilityBoundsTest,
    "ShatteredFrontier.Construction.Instability.MemberBoundAndOrdering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionInstabilityBoundsTest::RunTest(const FString& Parameters)
{
    TSet<FIntVector> AtLimit;
    for (int32 Index = 0; Index < FSFConstructionInstability::MaximumTrackedMembers; ++Index)
    {
        AtLimit.Add(FIntVector(Index, 0, 0));
    }
    FSFConstructionInstability Clock;
    TestTrue(TEXT("Maximum bounded member set accepted"), Clock.Refresh(AtLimit, 0.0, 1.0));
    TestEqual(TEXT("All bounded deadlines stored"), Clock.Num(), FSFConstructionInstability::MaximumTrackedMembers);

    TSet<FIntVector> OverLimit = AtLimit;
    OverLimit.Add(FIntVector(-1, 0, 0));
    TestFalse(TEXT("Set above member bound rejected"), Clock.Refresh(OverLimit, 1.0, 1.0));
    TestEqual(TEXT("Over-limit input leaves prior state intact"), Clock.Num(), FSFConstructionInstability::MaximumTrackedMembers);
    double Next = -1.0;
    TestTrue(TEXT("Next deadline available"), Clock.GetNextDeadline(Next));
    TestEqual(TEXT("Invalid refresh leaves prior deadline intact"), Next, 1.0);

    TArray<FIntVector> Due;
    TestTrue(TEXT("All members due exactly at deadline"), Clock.Due(1.0, Due));
    TestEqual(TEXT("All due coordinates returned"), Due.Num(), FSFConstructionInstability::MaximumTrackedMembers);
    TestTrue(TEXT("Coordinate order deterministic"), Due[0] == FIntVector(0,0,0)
        && Due[1] == FIntVector(1,0,0) && Due.Last() == FIntVector(4095,0,0));
    return true;
}

#endif
