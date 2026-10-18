#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Construction/SFConstructionSupport.h"
#include "Construction/SFConstructionGeometry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionSupportPilotTest, "ShatteredFrontier.Construction.Support.Pilot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionSupportPilotTest::RunTest(const FString& Parameters)
{
    const int32 R=SFConstructionGeometry::IdentityRotation();
    FSFConstructionSupportMember Ground; Ground.Address=FIntVector(0,0,0); Ground.Shape=0; Ground.Rotation=R; Ground.bAnchored=true;
    FSFConstructionSupportMember Above=Ground; Above.Address=FIntVector(0,0,1); Above.bAnchored=false;
    TArray<FSFConstructionSupportResult> Out;
    TestTrue(TEXT("Grounded column evaluates"),SFConstructionSupport::Evaluate({Ground,Above},FSFConstructionSupportProfile(),Out));
    TestEqual(TEXT("Results sort by coordinate"),Out[0].Address,FIntVector(0,0,0));
    TestEqual(TEXT("Upper member has shortest anchor distance one"),Out[1].AnchorDistance,1);
    TestTrue(TEXT("Ordinary column remains supported"),Out[1].State!=ESFConstructionSupportState::Unsupported);

    FSFConstructionSupportMember SideAnchor=Ground; SideAnchor.Shape=1;
    FSFConstructionSupportMember BottomAnchor=Ground; BottomAnchor.Address=FIntVector(1,0,-1);
    FSFConstructionSupportMember Junction=Ground; Junction.Address=FIntVector(1,0,0); Junction.bAnchored=false;
    TestTrue(TEXT("Unequal two-parent graph evaluates"),SFConstructionSupport::Evaluate({Junction,BottomAnchor,SideAnchor},FSFConstructionSupportProfile(),Out));
    auto FindResult=[&](const FIntVector& Address)->const FSFConstructionSupportResult* { for(const auto& R:Out) if(R.Address==Address) return &R; return nullptr; };
    const auto* SideResult=FindResult(SideAnchor.Address); const auto* BottomResult=FindResult(BottomAnchor.Address);
    TestTrue(TEXT("Side anchor receives one-third share from half-face contact"),SideResult && FMath::IsNearlyEqual(SideResult->LoadRatio,(0.5+1.0/3.0)/64.0,1.e-8));
    TestTrue(TEXT("Bottom anchor receives two-thirds share from full-face contact"),BottomResult && FMath::IsNearlyEqual(BottomResult->LoadRatio,(1.0+2.0/3.0)/64.0,1.e-8));

    FSFConstructionSupportMember ThinAnchor=Ground; ThinAnchor.Shape=1;
    FSFConstructionSupportMember LeftChild=Ground; LeftChild.Address=FIntVector(-1,0,0); LeftChild.bAnchored=false;
    FSFConstructionSupportMember RightChild=Ground; RightChild.Address=FIntVector(1,0,0); RightChild.bAnchored=false;
    FSFConstructionSupportProfile ContactStrain; ContactStrain.Capacity=2.7;
    TestTrue(TEXT("Narrow contact strain graph evaluates"),SFConstructionSupport::Evaluate({ThinAnchor,LeftChild,RightChild},ContactStrain,Out));
    const auto* LeftResult=FindResult(LeftChild.Address);
    TestTrue(TEXT("Outgoing contact utilization drives strain"),LeftResult
        && FMath::IsNearlyEqual(LeftResult->LoadRatio,1.0/(ContactStrain.Capacity*0.5),1.e-8)
        && LeftResult->State==ESFConstructionSupportState::Strained
        && LeftResult->LimitingParent==ThinAnchor.Address);

    TArray<FSFConstructionSupportMember> Floating={Above};
    TestTrue(TEXT("Floating member evaluates"),SFConstructionSupport::Evaluate(Floating,FSFConstructionSupportProfile(),Out));
    TestEqual(TEXT("Floating member has no anchor path"),static_cast<uint8>(Out[0].Reason),static_cast<uint8>(ESFConstructionSupportReason::NoAnchorPath));

    FSFConstructionSupportMember Diagonal=Above; Diagonal.Address=FIntVector(1,1,1);
    TestTrue(TEXT("Diagonal member evaluates"),SFConstructionSupport::Evaluate({Ground,Diagonal},FSFConstructionSupportProfile(),Out));
    TestEqual(TEXT("Corner contact does not create anchor path"),static_cast<uint8>(Out[1].Reason),static_cast<uint8>(ESFConstructionSupportReason::NoAnchorPath));

    FSFConstructionSupportMember LowerSlab=Ground; LowerSlab.Shape=1;
    FSFConstructionSupportMember Upper=Above;
    TestTrue(TEXT("Separated slab stack evaluates"),SFConstructionSupport::Evaluate({LowerSlab,Upper},FSFConstructionSupportProfile(),Out));
    TestEqual(TEXT("Gap between lower slab and upper cell breaks support"),static_cast<uint8>(Out[1].Reason),static_cast<uint8>(ESFConstructionSupportReason::NoAnchorPath));

    FSFConstructionSupportMember RotatedSlab=Ground; RotatedSlab.Address=FIntVector(1,0,0); RotatedSlab.Shape=1; RotatedSlab.Rotation=5; RotatedSlab.bAnchored=false;
    TestTrue(TEXT("Rotated upper slab evaluates"),SFConstructionSupport::Evaluate({Ground,RotatedSlab},FSFConstructionSupportProfile(),Out));
    TestEqual(TEXT("Canonical rotated side face retains positive support contact"),Out[1].AnchorDistance,1);

    FSFConstructionSupportMember Hurt=Above; Hurt.CombatHealth=0.0;
    TestTrue(TEXT("Zero health is a valid failing state"),SFConstructionSupport::Evaluate({Ground,Hurt},FSFConstructionSupportProfile(),Out));
    TestEqual(TEXT("Zero health fails before division"),static_cast<uint8>(Out[1].Reason),static_cast<uint8>(ESFConstructionSupportReason::ZeroHealth));

    FSFConstructionSupportMember FailedAnchor=Ground;
    FSFConstructionSupportMember HealthyAnchor=Ground; HealthyAnchor.Address=FIntVector(1,0,1);
    FSFConstructionSupportMember Bridged=Ground; Bridged.Address=FIntVector(0,0,1); Bridged.bAnchored=false;
    FSFConstructionSupportMember Overloader=Ground; Overloader.Address=FIntVector(0,0,-1); Overloader.bAnchored=false;
    FSFConstructionSupportProfile LowCapacity; LowCapacity.Capacity=2.0;
    TestTrue(TEXT("Frozen two-parent failure graph evaluates"),SFConstructionSupport::Evaluate({FailedAnchor,HealthyAnchor,Bridged,Overloader},LowCapacity,Out));
    const auto* BridgedResult=FindResult(Bridged.Address);
    TestTrue(TEXT("Failed fixed parent propagates without rerouting"),BridgedResult && BridgedResult->Reason==ESFConstructionSupportReason::FailedParent);

    FSFConstructionSupportProfile Tight; Tight.Capacity=2.0;
    TestTrue(TEXT("Anchor aggregate boundary evaluates"),SFConstructionSupport::Evaluate({Ground,Above},Tight,Out));
    TestEqual(TEXT("Anchor aggregate capacity equality fails"),static_cast<uint8>(Out[0].Reason),static_cast<uint8>(ESFConstructionSupportReason::AggregateCapacity));

    FSFConstructionSupportProfile Short; Short.VerticalReachCm=50.0;
    TestTrue(TEXT("Reach boundary evaluates"),SFConstructionSupport::Evaluate({Ground,Above},Short,Out));
    TestEqual(TEXT("Reach equality fails"),static_cast<uint8>(Out[1].Reason),static_cast<uint8>(ESFConstructionSupportReason::Reach));

    FSFConstructionSupportProfile Strain; Strain.VerticalReachCm=50.0/0.7;
    TestTrue(TEXT("Strain boundary evaluates"),SFConstructionSupport::Evaluate({Ground,Above},Strain,Out));
    TestEqual(TEXT("Reach ratio 0.7 is strained"),static_cast<uint8>(Out[1].State),static_cast<uint8>(ESFConstructionSupportState::Strained));
    FSFConstructionSupportMember HalfHealth=Above; HalfHealth.CombatHealth=0.5;
    TestTrue(TEXT("Health-scaled reach evaluates"),SFConstructionSupport::Evaluate({Ground,HalfHealth},FSFConstructionSupportProfile(),Out));
    TestTrue(TEXT("Half health doubles reach burden"),FMath::IsNearlyEqual(Out[1].ReachRatio,0.125,1.e-8));

    TArray<FSFConstructionSupportResult> Permuted;
    TArray<FSFConstructionSupportResult> Baseline;
    TestTrue(TEXT("Canonical order baseline evaluates"),SFConstructionSupport::Evaluate({Ground,Above},FSFConstructionSupportProfile(),Baseline));
    TestTrue(TEXT("Input permutation evaluates"),SFConstructionSupport::Evaluate({Above,Ground},FSFConstructionSupportProfile(),Permuted));
    TestEqual(TEXT("Permutation keeps output ordering"),Permuted[0].Address,Baseline[0].Address);
    for(int32 I=0; I<Baseline.Num(); ++I)
    {
        TestTrue(TEXT("Permutation preserves load result"),FMath::IsNearlyEqual(Permuted[I].LoadRatio,Baseline[I].LoadRatio,1.e-12));
        TestTrue(TEXT("Permutation preserves reach result"),FMath::IsNearlyEqual(Permuted[I].ReachRatio,Baseline[I].ReachRatio,1.e-12));
        TestEqual(TEXT("Permutation preserves state"),static_cast<uint8>(Permuted[I].State),static_cast<uint8>(Baseline[I].State));
        TestEqual(TEXT("Permutation preserves reason"),static_cast<uint8>(Permuted[I].Reason),static_cast<uint8>(Baseline[I].Reason));
    }
    TArray<FSFConstructionSupportMember> Invalid={Ground,Ground};
    const int32 Before=Permuted.Num();
    TestFalse(TEXT("Duplicate coordinate rejected"),SFConstructionSupport::Evaluate(Invalid,FSFConstructionSupportProfile(),Permuted));
    TestEqual(TEXT("Invalid input preserves output"),Permuted.Num(),Before);
    return true;
}

#endif
