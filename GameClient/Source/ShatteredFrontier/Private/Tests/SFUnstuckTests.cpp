#include "Misc/AutomationTest.h"
#include "Framework/SFUnstuckRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFUnstuckRulesTest,
    "ShatteredFrontier.E1.Unstuck.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFUnstuckRulesTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Dead characters cannot request"), SFUnstuckRules::CanRequest(false, false, false, false, 1000, -1, 900));
    TestFalse(TEXT("Combat blocks request"), SFUnstuckRules::CanRequest(true, true, false, false, 1000, -1, 900));
    TestFalse(TEXT("Stun blocks request"), SFUnstuckRules::CanRequest(true, false, true, false, 1000, -1, 900));
    TestFalse(TEXT("Instance blocks request"), SFUnstuckRules::CanRequest(true, false, false, true, 1000, -1, 900));
    TestFalse(TEXT("Second request before 15 minutes"), SFUnstuckRules::CanRequest(true, false, false, false, 999, 100, 900));
    TestTrue(TEXT("Request at 15 minutes"), SFUnstuckRules::CanRequest(true, false, false, false, 1000, 100, 900));
    TestTrue(TEXT("First request"), SFUnstuckRules::CanRequest(true, false, false, false, 1, -1, 900));
    TestFalse(TEXT("Under two metres"), SFUnstuckRules::IsNearbyDestination(FVector::ZeroVector, FVector(199,0,0), 2, 30));
    TestTrue(TEXT("Two metres"), SFUnstuckRules::IsNearbyDestination(FVector::ZeroVector, FVector(200,0,0), 2, 30));
    TestTrue(TEXT("Thirty metres"), SFUnstuckRules::IsNearbyDestination(FVector::ZeroVector, FVector(3000,0,0), 2, 30));
    TestFalse(TEXT("Over thirty metres"), SFUnstuckRules::IsNearbyDestination(FVector::ZeroVector, FVector(3001,0,0), 2, 30));
    FVector Best = FVector::ZeroVector; bool bFound = false;
    SFUnstuckRules::ConsiderNavigable(FVector::ZeroVector, FVector(350,0,0), 2, 30, Best, bFound);
    SFUnstuckRules::ConsiderNavigable(FVector::ZeroVector, FVector(201,0,0), 2, 30, Best, bFound);
    SFUnstuckRules::ConsiderNavigable(FVector::ZeroVector, FVector(199,0,0), 2, 30, Best, bFound);
    TestTrue(TEXT("Nearest projected valid point wins regardless of probe order"), bFound && Best.Equals(FVector(201,0,0)));
    FVector2D PolygonPoint;
    const TArray<FVector> Crossing = { FVector(150,-100,0), FVector(350,-100,0), FVector(350,100,0), FVector(150,100,0) };
    TestTrue(TEXT("Nav polygon crossing exclusion circle has a landing"),
        SFUnstuckRules::ClosestPermittedOnPolygon(FVector2D::ZeroVector, Crossing, 200, 3000, PolygonPoint));
    TestTrue(TEXT("Nearest landing lies on exact two metre boundary"), FMath::IsNearlyEqual(PolygonPoint.Size(), 200.0, 0.01));
    const TArray<FVector> Farther = { FVector(350,-100,0), FVector(450,-100,0), FVector(450,100,0), FVector(350,100,0) };
    TestTrue(TEXT("Fully outside polygon has a nearest edge point"),
        SFUnstuckRules::ClosestPermittedOnPolygon(FVector2D::ZeroVector, Farther, 200, 3000, PolygonPoint));
    TestTrue(TEXT("Nearest outside edge is 3.5 metres away"), FMath::IsNearlyEqual(PolygonPoint.Size(), 350.0, 0.01));
    SFUnstuckRules::FToken Token; Token.Id = TEXT("server-only"); Token.CharacterId = TEXT("chr_one");
    Token.Sequence = 7; Token.Origin = FVector::ZeroVector; Token.Destination = FVector(201,0,0);
    Token.ExpiresAtUnixMs = 2000; Token.bOutstanding = true;
    TestFalse(TEXT("Forged sequence refused"), Token.Consume(8, TEXT("chr_one"), Token.Destination, 1000));
    TestFalse(TEXT("Other character refused"), Token.Consume(7, TEXT("chr_other"), Token.Destination, 1000));
    TestTrue(TEXT("Server-issued landing consumed once"), Token.Consume(7, TEXT("chr_one"), Token.Destination, 1000));
    TestFalse(TEXT("Token replay refused"), Token.Consume(7, TEXT("chr_one"), Token.Destination, 1001));
    return true;
}
