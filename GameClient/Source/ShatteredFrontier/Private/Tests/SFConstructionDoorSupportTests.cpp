#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Construction/SFConstructionDoorSupport.h"
#include "Construction/SFConstructionGeometry.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionDoorSupportTest, "ShatteredFrontier.Construction.DoorSupport.Pilot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionDoorSupportTest::RunTest(const FString& Parameters)
{
    const FIntVector Origin(4,5,2);
    const FIntVector Expected[4][2] = {
        { FIntVector(4,5,1), FIntVector(5,5,1) },
        { FIntVector(3,5,1), FIntVector(3,6,1) },
        { FIntVector(3,4,1), FIntVector(2,4,1) },
        { FIntVector(4,4,1), FIntVector(4,3,1) }
    };
    for (int32 Yaw=0; Yaw<4; ++Yaw)
    {
        TArray<FSFConstructionSupportResult> Structural;
        TArray<FSFConstructionDoorSupportResult> Doors;
        TestTrue(TEXT("Valid unsupported attachment evaluates"), SFConstructionDoorSupport::Evaluate({}, {{Origin,Yaw}},
            FSFConstructionDoorSupportProfile(), true, Structural, Doors));
        TestEqual(TEXT("Both fixed receivers are reported"), Doors[0].Receivers.Num(), 2);
        TestEqual(TEXT("First receiver matches profile yaw mapping"), Doors[0].Receivers[0], Expected[Yaw][0]);
        TestEqual(TEXT("Second receiver matches profile yaw mapping"), Doors[0].Receivers[1], Expected[Yaw][1]);
        TestEqual(TEXT("Missing receiver is unsupported"), static_cast<uint8>(Doors[0].State),
            static_cast<uint8>(ESFConstructionSupportState::Unsupported));
    }

    TArray<FSFConstructionSupportResult> Structural;
    TArray<FSFConstructionDoorSupportResult> Doors;
    TestTrue(TEXT("Known foundation evaluation succeeds"), SFConstructionDoorSupport::Evaluate({}, {{{0,0,0},3}},
        FSFConstructionDoorSupportProfile(), true, Structural, Doors));
    TestTrue(TEXT("Foundation door attaches directly"), Doors[0].bFoundationAttached);
    TestEqual(TEXT("Direct foundation contributes no structural receiver"), Doors[0].Receivers.Num(), 0);
    TestTrue(TEXT("Without foundation evidence the attachment remains evaluable"), SFConstructionDoorSupport::Evaluate({},
        {{{0,0,0},0}}, FSFConstructionDoorSupportProfile(), false, Structural, Doors));
    TestFalse(TEXT("Without foundation evidence door is unsupported"), Doors[0].bFoundationAttached);
    TestEqual(TEXT("Unsupported direct attachment reports unsupported"), static_cast<uint8>(Doors[0].State),
        static_cast<uint8>(ESFConstructionSupportState::Unsupported));

    const int32 Identity = SFConstructionGeometry::IdentityRotation();
    auto MakeAnchor = [Identity](const FIntVector& Cell)
    {
        FSFConstructionSupportMember M; M.Address=Cell; M.Shape=0; M.Rotation=Identity; M.bAnchored=true; return M;
    };
    TArray<FSFConstructionSupportMember> Receivers = { MakeAnchor({0,0,0}), MakeAnchor({1,0,0}) };
    TArray<FSFConstructionSupportResult> WithoutLoads;
    TestTrue(TEXT("Receiver baseline evaluates"), SFConstructionSupport::Evaluate(Receivers, FSFConstructionSupportProfile(), WithoutLoads));
    FSFConstructionDoorSupportProfile DoorProfile;
    TestTrue(TEXT("Default-weight supported door evaluates"), SFConstructionDoorSupport::Evaluate(Receivers, {{{0,0,1},0}},
        DoorProfile, false, Structural, Doors));
    TestEqual(TEXT("Default-weight attachment is stable"), static_cast<uint8>(Doors[0].State),
        static_cast<uint8>(ESFConstructionSupportState::Stable));
    const FSFConstructionSupportResult* DefaultWeightReceiver=nullptr;
    for (const auto& R : Structural) if (R.Address==FIntVector(0,0,0)) DefaultWeightReceiver=&R;
    TestTrue(TEXT("Default door assigns 0.5 load to each receiver"), DefaultWeightReceiver
        && FMath::IsNearlyEqual(DefaultWeightReceiver->LoadRatio, 1.5/64.0, 1.e-8));

    DoorProfile.Weight=94.0;
    TestTrue(TEXT("Supported door evaluation succeeds"), SFConstructionDoorSupport::Evaluate(Receivers, {{{0,0,1},0}},
        DoorProfile, false, Structural, Doors));
    TestEqual(TEXT("Both receivers support the door"), static_cast<uint8>(Doors[0].State),
        static_cast<uint8>(ESFConstructionSupportState::Strained));
    const FSFConstructionSupportResult* First = nullptr;
    for (const auto& R : Structural) if (R.Address == FIntVector(0,0,0)) First=&R;
    TestTrue(TEXT("Door share is assigned to first receiver"), First && FMath::IsNearlyEqual(First->LoadRatio, 0.75, 1.e-8));
    TestTrue(TEXT("Door share raises receiver utilization"), First && First->LoadRatio > WithoutLoads[0].LoadRatio);
    DoorProfile.Weight=1.0;

    TArray<FSFConstructionSupportResult> PartialStructural;
    TArray<FSFConstructionDoorSupportResult> PartialDoors;
    TestTrue(TEXT("Partial attachment graph evaluates"), SFConstructionDoorSupport::Evaluate({MakeAnchor({0,0,0})},
        {{{0,0,1},0}}, DoorProfile, false, PartialStructural, PartialDoors));
    TestEqual(TEXT("Missing second receiver keeps door unsupported"), static_cast<uint8>(PartialDoors[0].State),
        static_cast<uint8>(ESFConstructionSupportState::Unsupported));
    TestTrue(TEXT("Existing receiver gets exactly its fixed half share"), PartialStructural.Num()==1
        && FMath::IsNearlyEqual(PartialStructural[0].LoadRatio, 1.5/64.0, 1.e-8));

    FSFConstructionSupportMember PartialAnchor = MakeAnchor({0,0,0});
    PartialAnchor.Shape=1;
    TArray<FSFConstructionSupportResult> PartialShapeStructural;
    TArray<FSFConstructionDoorSupportResult> PartialShapeDoors;
    TestTrue(TEXT("Partial floor receiver remains a valid structural member"), SFConstructionDoorSupport::Evaluate(
        {PartialAnchor, MakeAnchor({1,0,0})}, {{{0,0,1},0}}, DoorProfile, false, PartialShapeStructural, PartialShapeDoors));
    TestEqual(TEXT("Door rejects partial floor receiver"), static_cast<uint8>(PartialShapeDoors[0].State),
        static_cast<uint8>(ESFConstructionSupportState::Unsupported));

    FSFConstructionSupportMember FailedAnchor = MakeAnchor({0,0,0});
    FailedAnchor.CombatHealth = 0.0;
    TArray<FSFConstructionSupportResult> FailedStructural;
    TArray<FSFConstructionDoorSupportResult> FailedDoors;
    TestTrue(TEXT("Failed fixed receiver graph evaluates"), SFConstructionDoorSupport::Evaluate(
        {FailedAnchor, MakeAnchor({1,0,0})}, {{{0,0,1},0}}, DoorProfile, false, FailedStructural, FailedDoors));
    TestEqual(TEXT("Failed receiver makes door unsupported without rerouting"), static_cast<uint8>(FailedDoors[0].State),
        static_cast<uint8>(ESFConstructionSupportState::Unsupported));
    const FSFConstructionSupportResult* HealthySecondReceiver=nullptr;
    for (const auto& R : FailedStructural) if (R.Address==FIntVector(1,0,0)) HealthySecondReceiver=&R;
    TestTrue(TEXT("Healthy receiver keeps exactly its half share when the other fails"), HealthySecondReceiver
        && FMath::IsNearlyEqual(HealthySecondReceiver->LoadRatio, 1.5/64.0, 1.e-8));

    const TArray<FSFConstructionSupportMember> SharedDoorReceivers = {
        MakeAnchor({0,0,0}), MakeAnchor({1,0,0}), MakeAnchor({0,1,0})
    };
    const TArray<FSFConstructionDoorSupportInput> SharedDoors = {{{0,0,1},0},{{1,0,1},1}};
    TArray<FSFConstructionSupportResult> SharedStructural;
    TArray<FSFConstructionDoorSupportResult> SharedDoorResults;
    TestTrue(TEXT("Two retained doors sharing a receiver evaluate"), SFConstructionDoorSupport::Evaluate(
        SharedDoorReceivers, SharedDoors, DoorProfile, false, SharedStructural, SharedDoorResults));
    TestEqual(TEXT("Both shared receiver doors remain supported"), static_cast<uint8>(SharedDoorResults[0].State),
        static_cast<uint8>(ESFConstructionSupportState::Stable));
    const FSFConstructionSupportResult* AccumulatedReceiver=nullptr;
    for (const auto& R : SharedStructural) if (R.Address==FIntVector(0,0,0)) AccumulatedReceiver=&R;
    TestTrue(TEXT("Two fixed half shares accumulate to total attached load one"), AccumulatedReceiver
        && FMath::IsNearlyEqual(AccumulatedReceiver->LoadRatio, 2.0/64.0, 1.e-8));

    TArray<FSFConstructionSupportResult> SentinelStructural = Structural;
    TArray<FSFConstructionDoorSupportResult> SentinelDoors = Doors;
    const FIntVector SentinelStructuralAddress=Structural[0].Address;
    const ESFConstructionSupportState SentinelStructuralState=Structural[0].State;
    const double SentinelStructuralLoad=Structural[0].LoadRatio;
    const FIntVector SentinelDoorOrigin=Doors[0].OriginCell;
    const ESFConstructionSupportState SentinelDoorState=Doors[0].State;
    const TArray<FIntVector> SentinelDoorReceivers=Doors[0].Receivers;
    FSFConstructionDoorSupportProfile BadProfile; BadProfile.Weight=0.0;
    TestFalse(TEXT("Invalid profile rejected"), SFConstructionDoorSupport::Evaluate(Receivers, {{{0,0,1},0}}, BadProfile,
        false, Structural, Doors));
    TestEqual(TEXT("Invalid input preserves structural output"), Structural.Num(), SentinelStructural.Num());
    TestEqual(TEXT("Invalid input preserves door output"), Doors.Num(), SentinelDoors.Num());
    TestEqual(TEXT("Invalid input preserves structural content"), Structural[0].Address, SentinelStructuralAddress);
    TestEqual(TEXT("Invalid input preserves structural state"), static_cast<uint8>(Structural[0].State), static_cast<uint8>(SentinelStructuralState));
    TestTrue(TEXT("Invalid input preserves structural load"), FMath::IsNearlyEqual(Structural[0].LoadRatio, SentinelStructuralLoad));
    TestEqual(TEXT("Invalid input preserves door origin"), Doors[0].OriginCell, SentinelDoorOrigin);
    TestEqual(TEXT("Invalid input preserves door state"), static_cast<uint8>(Doors[0].State), static_cast<uint8>(SentinelDoorState));
    TestEqual(TEXT("Invalid input preserves door receivers"), Doors[0].Receivers.Num(), SentinelDoorReceivers.Num());
    TestEqual(TEXT("Invalid input preserves first receiver"), Doors[0].Receivers[0], SentinelDoorReceivers[0]);
    FSFConstructionSupportMember Preloaded=MakeAnchor({0,0,0}); Preloaded.AttachedLoad=0.25;
    TestFalse(TEXT("Preassigned structural load rejected"), SFConstructionDoorSupport::Evaluate({Preloaded}, {},
        DoorProfile, false, Structural, Doors));
    TestFalse(TEXT("Duplicate door origin rejected"), SFConstructionDoorSupport::Evaluate({}, {{{1,1,0},0},{{1,1,0},2}},
        DoorProfile, true, Structural, Doors));

    FSFConstructionDoorSupportProfile MaximumWeight; MaximumWeight.Weight=1.e6;
    const TArray<FSFConstructionDoorSupportInput> OverAggregate = {{{0,0,1},0},{{1,0,1},1},{{1,1,1},2}};
    TestFalse(TEXT("Aggregate receiver load above profile bound rejects snapshot"), SFConstructionDoorSupport::Evaluate(
        {MakeAnchor({0,0,0})}, OverAggregate, MaximumWeight, false, Structural, Doors));
    TestEqual(TEXT("Overbound aggregate preserves structural results"), Structural.Num(), SentinelStructural.Num());
    TestEqual(TEXT("Overbound aggregate preserves door results"), Doors.Num(), SentinelDoors.Num());

    TestFalse(TEXT("Yaw zero envelope crossing positive X bound rejected"), SFConstructionDoorSupport::Evaluate({}, {{{19,0,0},0}},
        DoorProfile, true, Structural, Doors));
    TestFalse(TEXT("Yaw one envelope crossing negative X bound rejected"), SFConstructionDoorSupport::Evaluate({}, {{{-20,0,0},1}},
        DoorProfile, true, Structural, Doors));
    TestFalse(TEXT("Envelope exceeding height bound rejected"), SFConstructionDoorSupport::Evaluate({}, {{{0,0,17},0}},
        DoorProfile, false, Structural, Doors));
    FSFConstructionDoorSupportProfile NonFiniteProfile; NonFiniteProfile.Weight=std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("Nonfinite profile weight rejected"), SFConstructionDoorSupport::Evaluate({}, {}, NonFiniteProfile,
        false, Structural, Doors));
    TestFalse(TEXT("Malformed yaw rejected"), SFConstructionDoorSupport::Evaluate({}, {{{0,0,1},4}}, DoorProfile,
        false, Structural, Doors));
    TestFalse(TEXT("Maximum integer X rejected before envelope arithmetic"), SFConstructionDoorSupport::Evaluate({},
        {{{TNumericLimits<int32>::Max(),0,0},0}}, DoorProfile, true, Structural, Doors));
    TestFalse(TEXT("Minimum integer X rejected before envelope arithmetic"), SFConstructionDoorSupport::Evaluate({},
        {{{TNumericLimits<int32>::Min(),0,0},0}}, DoorProfile, true, Structural, Doors));
    TestFalse(TEXT("Maximum integer Y rejected before envelope arithmetic"), SFConstructionDoorSupport::Evaluate({},
        {{{0,TNumericLimits<int32>::Max(),0},0}}, DoorProfile, true, Structural, Doors));
    TestFalse(TEXT("Minimum integer Y rejected before envelope arithmetic"), SFConstructionDoorSupport::Evaluate({},
        {{{0,TNumericLimits<int32>::Min(),0},0}}, DoorProfile, true, Structural, Doors));
    TestEqual(TEXT("Integer-bound rejection preserves structural output"), Structural[0].Address, SentinelStructuralAddress);
    TestEqual(TEXT("Integer-bound rejection preserves door output"), Doors[0].OriginCell, SentinelDoorOrigin);

    FSFConstructionSupportMember InvalidStructural=MakeAnchor({0,0,0}); InvalidStructural.Shape=SFConstructionGeometry::ShapeCount;
    const int32 PreservedStructuralCount=Structural.Num();
    const int32 PreservedDoorCount=Doors.Num();
    const FIntVector PreservedStructuralAddress=Structural[0].Address;
    const FIntVector PreservedDoorOrigin=Doors[0].OriginCell;
    TestFalse(TEXT("Invalid structural geometry rejected"), SFConstructionDoorSupport::Evaluate({InvalidStructural}, {},
        DoorProfile, false, Structural, Doors));
    TestEqual(TEXT("Invalid structural input preserves structural count"), Structural.Num(), PreservedStructuralCount);
    TestEqual(TEXT("Invalid structural input preserves door count"), Doors.Num(), PreservedDoorCount);
    TestEqual(TEXT("Invalid structural input preserves structural content"), Structural[0].Address, PreservedStructuralAddress);
    TestEqual(TEXT("Invalid structural input preserves door content"), Doors[0].OriginCell, PreservedDoorOrigin);

    TArray<FSFConstructionDoorSupportInput> TooManyDoors;
    for (int32 I=0; I<65; ++I)
    {
        FSFConstructionDoorSupportInput Input;
        Input.OriginCell=FIntVector(-20+(I%40),-20+(I/40),0);
        TooManyDoors.Add(Input);
    }
    TestFalse(TEXT("Door count above 64 rejected"), SFConstructionDoorSupport::Evaluate({}, TooManyDoors,
        DoorProfile, true, Structural, Doors));
    TestEqual(TEXT("Door count rejection preserves structural count"), Structural.Num(), PreservedStructuralCount);
    TestEqual(TEXT("Door count rejection preserves door count"), Doors.Num(), PreservedDoorCount);
    TestEqual(TEXT("Door count rejection preserves structural content"), Structural[0].Address, PreservedStructuralAddress);
    TestEqual(TEXT("Door count rejection preserves door content"), Doors[0].OriginCell, PreservedDoorOrigin);

    TestTrue(TEXT("Envelope touching height bound is allowed"), SFConstructionDoorSupport::Evaluate({}, {{{0,0,16},0}},
        DoorProfile, false, Structural, Doors));

    TArray<FSFConstructionDoorSupportInput> UnsortedDoors = {{{2,0,0},0},{{1,0,0},0}};
    TestTrue(TEXT("Unsorted door inputs evaluate"), SFConstructionDoorSupport::Evaluate({}, UnsortedDoors,
        DoorProfile, true, Structural, Doors));
    TestEqual(TEXT("Door results sort by origin"), Doors[0].OriginCell, FIntVector(1,0,0));
    return true;
}

#endif
