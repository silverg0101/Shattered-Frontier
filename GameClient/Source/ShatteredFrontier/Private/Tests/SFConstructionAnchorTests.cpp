#include "Construction/SFConstructionAnchor.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionAnchorTest,
    "ShatteredFrontier.Construction.Anchor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFConstructionAnchorTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Identity anchor offsets nothing"),SFConstructionAnchor::OffsetCm(FIntVector::ZeroValue).IsZero());
    TestTrue(TEXT("Z anchor is whole 50 cm cells"),SFConstructionAnchor::OffsetCm(FIntVector(0,0,53)).Equals(FVector(0,0,2650)));
    TestTrue(TEXT("Z anchors are valid"),SFConstructionAnchor::IsValid(FIntVector(0,0,53)));
    TestTrue(TEXT("A plot-centre X/Y anchor is valid"),SFConstructionAnchor::IsValid(FIntVector(860,430,53)));
    TestTrue(TEXT("X/Y offsets are whole cells"),SFConstructionAnchor::OffsetCm(FIntVector(860,430,0)).Equals(FVector(43000,21500,0)));
    TestFalse(TEXT("A runaway Z is rejected"),SFConstructionAnchor::IsValid(FIntVector(0,0,1000000)));

    // Claim edges: nothing outside the 40 x 40 x 20 extent is a cell, so no contact can cross a claim edge.
    TestTrue(TEXT("Minimum corner is inside"),SFConstructionAnchor::Contains(FIntVector(-20,-20,0)));
    TestTrue(TEXT("Maximum corner is inside"),SFConstructionAnchor::Contains(FIntVector(19,19,19)));
    TestFalse(TEXT("X past the edge"),SFConstructionAnchor::Contains(FIntVector(20,0,0)));
    TestFalse(TEXT("Y before the edge"),SFConstructionAnchor::Contains(FIntVector(0,-21,0)));
    TestFalse(TEXT("Below the foundation"),SFConstructionAnchor::Contains(FIntVector(0,0,-1)));
    TestFalse(TEXT("Above the extent"),SFConstructionAnchor::Contains(FIntVector(0,0,20)));
    return true;
}
#endif
