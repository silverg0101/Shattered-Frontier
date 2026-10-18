#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "Misc/AutomationTest.h"
#include "Construction/SFConstructionLine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionLineEnumerationTest,
    "ShatteredFrontier.Construction.Line.EnumerationAndBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionLineEnumerationTest::RunTest(const FString& Parameters)
{
    TArray<FIntVector> Cells;
    TestTrue(TEXT("Inclusive X line enumerates"),SFConstructionLine::Enumerate(FIntVector(3,2,-1),FIntVector(0,2,-1),4,Cells));
    TestTrue(TEXT("X output is canonical lexicographic order"),Cells==TArray<FIntVector>({FIntVector(0,2,-1),FIntVector(1,2,-1),FIntVector(2,2,-1),FIntVector(3,2,-1)}));
    TestTrue(TEXT("Reversed X input yields identical output"),SFConstructionLine::Enumerate(FIntVector(0,2,-1),FIntVector(3,2,-1),4,Cells));
    TestTrue(TEXT("Reversal is order independent"),Cells==TArray<FIntVector>({FIntVector(0,2,-1),FIntVector(1,2,-1),FIntVector(2,2,-1),FIntVector(3,2,-1)}));

    TestTrue(TEXT("Inclusive Y line enumerates"),SFConstructionLine::Enumerate(FIntVector(-1,5,2),FIntVector(-1,2,2),4,Cells));
    TestTrue(TEXT("Y output is sorted"),Cells==TArray<FIntVector>({FIntVector(-1,2,2),FIntVector(-1,3,2),FIntVector(-1,4,2),FIntVector(-1,5,2)}));
    TestTrue(TEXT("Inclusive Z line enumerates"),SFConstructionLine::Enumerate(FIntVector(1,-2,-1),FIntVector(1,-2,1),3,Cells));
    TestTrue(TEXT("Z output is sorted"),Cells==TArray<FIntVector>({FIntVector(1,-2,-1),FIntVector(1,-2,0),FIntVector(1,-2,1)}));
    TestTrue(TEXT("Equal endpoints mean one cell"),SFConstructionLine::Enumerate(FIntVector(-4,8,0),FIntVector(-4,8,0),1,Cells));
    TestTrue(TEXT("Single-cell output exact"),Cells==TArray<FIntVector>({FIntVector(-4,8,0)}));

    TestTrue(TEXT("Absolute 32-cell configured cap is accepted"),SFConstructionLine::Enumerate(FIntVector(-16,0,0),FIntVector(15,0,0),32,Cells));
    TestEqual(TEXT("32-cell line has exact count"),Cells.Num(),32);
    TArray<FIntVector> Sentinel={FIntVector(7,8,9),FIntVector(-3,4,1)};
    const TArray<FIntVector> OriginalSentinel=Sentinel;
    TestFalse(TEXT("Configured limit rejects longer line"),SFConstructionLine::Enumerate(FIntVector(0,0,0),FIntVector(2,0,0),2,Sentinel));
    TestTrue(TEXT("Limit rejection preserves output contents"),Sentinel==OriginalSentinel);
    TestFalse(TEXT("Invalid zero limit rejects"),SFConstructionLine::Enumerate(FIntVector::ZeroValue,FIntVector::ZeroValue,0,Sentinel));
    TestTrue(TEXT("Zero-limit rejection preserves output"),Sentinel==OriginalSentinel);
    TestFalse(TEXT("Limit above absolute maximum rejects"),SFConstructionLine::Enumerate(FIntVector::ZeroValue,FIntVector::ZeroValue,33,Sentinel));
    TestTrue(TEXT("Overmaximum-limit rejection preserves output"),Sentinel==OriginalSentinel);
    TestFalse(TEXT("Diagonal rejects"),SFConstructionLine::Enumerate(FIntVector::ZeroValue,FIntVector(1,1,0),8,Sentinel));
    TestTrue(TEXT("Diagonal rejection preserves output"),Sentinel==OriginalSentinel);

    TestFalse(TEXT("Full int32 span rejects without overflow or allocation"),SFConstructionLine::Enumerate(
        FIntVector(MIN_int32,0,0),FIntVector(MAX_int32,0,0),SFConstructionLine::MaximumCells,Sentinel));
    TestTrue(TEXT("Extreme-span rejection preserves output"),Sentinel==OriginalSentinel);
    TestTrue(TEXT("Extreme endpoint subtraction is safe for a short line"),SFConstructionLine::Enumerate(
        FIntVector(MIN_int32,MAX_int32,MIN_int32),FIntVector(MIN_int32+2,MAX_int32,MIN_int32),3,Cells));
    TestTrue(TEXT("Short extreme line values are exact"),Cells==TArray<FIntVector>({
        FIntVector(MIN_int32,MAX_int32,MIN_int32),FIntVector(MIN_int32+1,MAX_int32,MIN_int32),
        FIntVector(MIN_int32+2,MAX_int32,MIN_int32)}));
    return true;
}

#endif
