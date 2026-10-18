#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include <limits>
#include "Misc/AutomationTest.h"
#include "Construction/SFConstructionSupport.h"
#include "Construction/SFConstructionGeometry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionFunctionalLoadTest, "ShatteredFrontier.Construction.Support.FunctionalLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionFunctionalLoadTest::RunTest(const FString& Parameters)
{
    const int32 Rotation = SFConstructionGeometry::IdentityRotation();
    FSFConstructionSupportMember Ground; Ground.Address=FIntVector(0,0,0); Ground.Shape=0; Ground.Rotation=Rotation; Ground.bAnchored=true;
    FSFConstructionSupportMember Above=Ground; Above.Address=FIntVector(0,0,1); Above.bAnchored=false;
    TArray<FSFConstructionSupportResult> DefaultOut, ExplicitZeroOut;
    FSFConstructionSupportMember ExplicitZero=Above; ExplicitZero.AttachedLoad=0.0;
    TestTrue(TEXT("Default load evaluates"),SFConstructionSupport::Evaluate({Ground,Above},FSFConstructionSupportProfile(),DefaultOut));
    TestTrue(TEXT("Explicit zero load evaluates"),SFConstructionSupport::Evaluate({Ground,ExplicitZero},FSFConstructionSupportProfile(),ExplicitZeroOut));
    TestEqual(TEXT("Default load preserves result count"),DefaultOut.Num(),ExplicitZeroOut.Num());
    for (int32 I=0; I<FMath::Min(DefaultOut.Num(),ExplicitZeroOut.Num()); ++I)
    {
        TestTrue(TEXT("Default and explicit zero have identical addresses"),DefaultOut[I].Address==ExplicitZeroOut[I].Address);
        TestEqual(TEXT("Default and explicit zero have identical state"),static_cast<uint8>(DefaultOut[I].State),static_cast<uint8>(ExplicitZeroOut[I].State));
        TestEqual(TEXT("Default and explicit zero have identical reason"),static_cast<uint8>(DefaultOut[I].Reason),static_cast<uint8>(ExplicitZeroOut[I].Reason));
        TestEqual(TEXT("Default and explicit zero have identical load ratio"),DefaultOut[I].LoadRatio,ExplicitZeroOut[I].LoadRatio);
        TestEqual(TEXT("Default and explicit zero have identical reach ratio"),DefaultOut[I].ReachRatio,ExplicitZeroOut[I].ReachRatio);
    }

    FSFConstructionSupportProfile Wide; Wide.Capacity=1000.0;
    FSFConstructionSupportMember LoadedAbove=Above; LoadedAbove.AttachedLoad=7.0;
    TArray<FSFConstructionSupportResult> LoadedOut;
    TestTrue(TEXT("Attached load evaluates"),SFConstructionSupport::Evaluate({LoadedAbove,Ground},Wide,LoadedOut));
    TestTrue(TEXT("Result is address sorted"),LoadedOut.Num()==2 && LoadedOut[0].Address==Ground.Address && LoadedOut[1].Address==Above.Address);
    if (LoadedOut.Num()==2)
    {
        TestTrue(TEXT("Attached load increases member load by exactly seven units"),FMath::IsNearlyEqual(LoadedOut[1].LoadRatio,8.0/1000.0,1.e-12));
        TestTrue(TEXT("Attached load flows into its anchor parent"),FMath::IsNearlyEqual(LoadedOut[0].LoadRatio,9.0/1000.0,1.e-12));
    }
    TArray<FSFConstructionSupportResult> PermutedOut;
    TestTrue(TEXT("Permutation evaluates"),SFConstructionSupport::Evaluate({Ground,LoadedAbove},Wide,PermutedOut));
    TestEqual(TEXT("Permutation returns same result count"),LoadedOut.Num(),PermutedOut.Num());
    if (LoadedOut.Num()==PermutedOut.Num()) for (int32 I=0; I<LoadedOut.Num(); ++I)
        TestTrue(TEXT("Permutation preserves canonical result and load"),LoadedOut[I].Address==PermutedOut[I].Address && FMath::IsNearlyEqual(LoadedOut[I].LoadRatio,PermutedOut[I].LoadRatio,1.e-12));

    FSFConstructionSupportMember SideAnchor=Ground; SideAnchor.Address=FIntVector(1,0,1);
    FSFConstructionSupportMember Shared=Above; Shared.AttachedLoad=6.0;
    TArray<FSFConstructionSupportResult> SharedOut;
    TestTrue(TEXT("Equal-area two-parent graph evaluates"),SFConstructionSupport::Evaluate({Shared,SideAnchor,Ground},Wide,SharedOut));
    if (SharedOut.Num()==3)
    {
        TestTrue(TEXT("Equal contact areas split attached load evenly"),FMath::IsNearlyEqual(SharedOut[0].LoadRatio,SharedOut[2].LoadRatio,1.e-12));
        TestTrue(TEXT("Each parent receives half the child's attached load"),FMath::IsNearlyEqual(SharedOut[0].LoadRatio,4.5/1000.0,1.e-12));
    }

    FSFConstructionSupportMember UnequalSideAnchor=Ground; UnequalSideAnchor.Shape=1;
    FSFConstructionSupportMember UnequalBottomAnchor=Ground; UnequalBottomAnchor.Address=FIntVector(1,0,-1);
    FSFConstructionSupportMember UnequalChild=Ground; UnequalChild.Address=FIntVector(1,0,0); UnequalChild.bAnchored=false; UnequalChild.AttachedLoad=8.0;
    TArray<FSFConstructionSupportResult> UnequalOut, UnequalBaseline;
    TestTrue(TEXT("Unequal-area parent graph evaluates"),SFConstructionSupport::Evaluate({UnequalChild,UnequalBottomAnchor,UnequalSideAnchor},Wide,UnequalOut));
    UnequalChild.AttachedLoad=0.0;
    TestTrue(TEXT("Unequal-area baseline evaluates"),SFConstructionSupport::Evaluate({UnequalChild,UnequalBottomAnchor,UnequalSideAnchor},Wide,UnequalBaseline));
    if (UnequalOut.Num()==3 && UnequalBaseline.Num()==3)
    {
        const double SideShare=(UnequalOut[0].LoadRatio-UnequalBaseline[0].LoadRatio)*Wide.Capacity;
        const double BottomShare=(UnequalOut[1].LoadRatio-UnequalBaseline[1].LoadRatio)*Wide.Capacity;
        TestTrue(TEXT("Half-area slab parent receives its own weight plus one third of child load"),FMath::IsNearlyEqual(UnequalOut[0].LoadRatio,3.5/1000.0,1.e-12));
        TestTrue(TEXT("Full-area parent receives its own weight plus two thirds of child load"),FMath::IsNearlyEqual(UnequalOut[1].LoadRatio,7.0/1000.0,1.e-12));
        TestTrue(TEXT("Unequal contact areas allocate shares in a one-to-two ratio"),FMath::IsNearlyEqual(BottomShare,2.0*SideShare,1.e-9));
        TestTrue(TEXT("Unequal parent shares conserve attached load"),FMath::IsNearlyEqual(SideShare+BottomShare,8.0,1.e-9));
    }

    FSFConstructionSupportMember FailedParent=Ground; FailedParent.AttachedLoad=1000.0;
    FSFConstructionSupportMember HealthySide=Ground; HealthySide.Address=FIntVector(1,0,1);
    FSFConstructionSupportMember FixedChild=Above; FixedChild.AttachedLoad=6.0;
    TArray<FSFConstructionSupportResult> FailedParentOut;
    TestTrue(TEXT("Fixed-parent failure graph evaluates"),SFConstructionSupport::Evaluate({FixedChild,HealthySide,FailedParent},Wide,FailedParentOut));
    if (FailedParentOut.Num()==3)
    {
        TestEqual(TEXT("Failed parent propagates to child without rerouting"),static_cast<uint8>(FailedParentOut[1].Reason),static_cast<uint8>(ESFConstructionSupportReason::FailedParent));
        TestTrue(TEXT("Healthy parent retains only its frozen share"),FMath::IsNearlyEqual(FailedParentOut[2].LoadRatio,4.5/1000.0,1.e-12));
    }

    FSFConstructionSupportMember SlabAnchor=Ground; SlabAnchor.Shape=1; SlabAnchor.AttachedLoad=6.0;
    TArray<FSFConstructionSupportResult> SlabOut;
    TestTrue(TEXT("Attached load on half-slab evaluates"),SFConstructionSupport::Evaluate({SlabAnchor},Wide,SlabOut));
    if (SlabOut.Num()==1) TestTrue(TEXT("Attached load is not occupancy-scaled"),FMath::IsNearlyEqual(SlabOut[0].LoadRatio,6.5/1000.0,1.e-12));

    FSFConstructionSupportProfile Equality; Equality.Capacity=2.0;
    FSFConstructionSupportMember LoadedAnchor=Ground; LoadedAnchor.AttachedLoad=1.0;
    TArray<FSFConstructionSupportResult> BoundaryOut;
    TestTrue(TEXT("Anchored equality evaluates"),SFConstructionSupport::Evaluate({LoadedAnchor},Equality,BoundaryOut));
    if (BoundaryOut.Num()==1) TestEqual(TEXT("Attached load at anchored capacity equality fails"),static_cast<uint8>(BoundaryOut[0].Reason),static_cast<uint8>(ESFConstructionSupportReason::AggregateCapacity));
    FSFConstructionSupportMember EqualityAbove=Above; EqualityAbove.AttachedLoad=1.0;
    TestTrue(TEXT("Non-anchor equality evaluates"),SFConstructionSupport::Evaluate({Ground,EqualityAbove},Equality,BoundaryOut));
    if (BoundaryOut.Num()==2) TestEqual(TEXT("Attached load at non-anchor capacity equality fails"),static_cast<uint8>(BoundaryOut[1].Reason),static_cast<uint8>(ESFConstructionSupportReason::AggregateCapacity));
    EqualityAbove.AttachedLoad=2.0;
    TestTrue(TEXT("Non-anchor overload evaluates"),SFConstructionSupport::Evaluate({Ground,EqualityAbove},Equality,BoundaryOut));
    if (BoundaryOut.Num()==2) TestEqual(TEXT("Attached load above capacity fails"),static_cast<uint8>(BoundaryOut[1].Reason),static_cast<uint8>(ESFConstructionSupportReason::AggregateCapacity));

    FSFConstructionSupportMember HurtAbove=Above; HurtAbove.CombatHealth=0.5; HurtAbove.AttachedLoad=1.0;
    FSFConstructionSupportProfile HealthCapacity; HealthCapacity.Capacity=10.0;
    TestTrue(TEXT("Reduced-health attached load evaluates"),SFConstructionSupport::Evaluate({Ground,HurtAbove},HealthCapacity,BoundaryOut));
    if (BoundaryOut.Num()==2) TestTrue(TEXT("Load remains physical while capacity is health-scaled"),FMath::IsNearlyEqual(BoundaryOut[1].LoadRatio,2.0/5.0,1.e-12));

    TArray<FSFConstructionSupportResult> Preserved;
    FSFConstructionSupportResult Sentinel; Sentinel.Address=FIntVector(91,92,93); Sentinel.LoadRatio=123.0; Preserved.Add(Sentinel);
    const double InvalidLoads[]={-1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), 1000001.0};
    for (double InvalidLoad : InvalidLoads)
    {
        FSFConstructionSupportMember Invalid=Above; Invalid.AttachedLoad=InvalidLoad;
        TestFalse(TEXT("Invalid attached load is rejected"),SFConstructionSupport::Evaluate({Ground,Invalid},FSFConstructionSupportProfile(),Preserved));
        TestTrue(TEXT("Invalid attached load preserves existing output"),Preserved.Num()==1 && Preserved[0].Address==Sentinel.Address && Preserved[0].LoadRatio==Sentinel.LoadRatio);
    }
    FSFConstructionSupportMember MaxLoad=Above; MaxLoad.AttachedLoad=1000000.0;
    TestTrue(TEXT("One million load units is admitted"),SFConstructionSupport::Evaluate({Ground,MaxLoad},Wide,Preserved));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
