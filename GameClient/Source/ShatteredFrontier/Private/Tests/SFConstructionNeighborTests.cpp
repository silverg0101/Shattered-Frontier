#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandbox.h"
#include "GameFramework/WorldSettings.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"

namespace
{
double Area(const TArray<FVector>& Points)
{
    FVector Sum = FVector::ZeroVector;
    for (int32 I = 1; I + 1 < Points.Num(); ++I)
    { Sum += FVector::CrossProduct(Points[I] - Points[0], Points[I + 1] - Points[0]); }
    return Sum.Size() * .5;
}
double BoundaryArea(const TArray<FSFConstructionSurfacePatch>& Patches, int32 Side)
{
    double Sum = 0;
    for (const auto& Patch : Patches)
    {
        bool bOn = true;
        for (const FVector& V : Patch.Vertices) { bOn &= V[Side / 2] == (Side % 2 ? 50 : 0); }
        if (bOn) { Sum += Area(Patch.Vertices); }
    }
    return Sum;
}
double MeshBoundaryArea(UProceduralMeshComponent* Mesh, int32 Side)
{
    const FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
    if (!Section) { return 0; }
    double Sum = 0;
    for (int32 I = 0; I < Section->ProcIndexBuffer.Num(); I += 3)
    {
        TArray<FVector> Triangle;
        bool bOn = true;
        for (int32 J = 0; J < 3; ++J)
        {
            const FVector P = Section->ProcVertexBuffer[Section->ProcIndexBuffer[I + J]].Position;
            Triangle.Add(P);
            bOn &= P[Side / 2] == (Side % 2 ? 50 : 0);
        }
        if (bOn) { Sum += Area(Triangle); }
    }
    return Sum;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionNeighborDifferenceTest,
    "ShatteredFrontier.Construction.Geometry.NeighborPolygonDifference",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionNeighborDifferenceTest::RunTest(const FString& Parameters)
{
    const int32 Identity = SFConstructionGeometry::IdentityRotation();
    FString OraclePath, OracleCsv = TEXT("own_shape,own_rotation,neighbor_shape,neighbor_rotation,side,area_cm2\n");
    const bool bWriteOracle = FParse::Value(FCommandLine::Get(), TEXT("ConstructionSurfaceOracle="), OraclePath);
    for (int32 Shape = 0; Shape < 3; ++Shape)
    for (int32 Rotation = 0; Rotation < 24; ++Rotation)
    {
        FSFConstructionSolid Solid;
        SFConstructionGeometry::Build(Shape, Rotation, Solid);
        TArray<FSFConstructionSurfacePatch> Original;
        SFConstructionGeometry::BuildExposed(Shape, Rotation, FSFConstructionNeighbors(), Original);
        for (int32 Side = 0; Side < 6; ++Side)
        {
            const double OwnArea = BoundaryArea(Original, Side);
            for (int32 Other = 0; Other < 3; ++Other)
            for (int32 OtherRotation = 0; OtherRotation < 24; ++OtherRotation)
            {
                FSFConstructionNeighbors Borders, Reverse;
                Borders.Faces[Side] = {true, Other, OtherRotation, 1};
                Reverse.Faces[Side ^ 1] = {true, Shape, Rotation, 1};
                TArray<FSFConstructionSurfacePatch> Exposed, OtherExposed, OtherOriginal;
                if (!SFConstructionGeometry::BuildExposed(Shape, Rotation, Borders, Exposed)
                    || !SFConstructionGeometry::BuildExposed(Other, OtherRotation, Reverse, OtherExposed)
                    || !SFConstructionGeometry::BuildExposed(Other, OtherRotation, FSFConstructionNeighbors(), OtherOriginal))
                { AddError(TEXT("Valid pair rejected")); return false; }
                const double Remaining = BoundaryArea(Exposed, Side);
                if (bWriteOracle)
                {
                    OracleCsv += FString::Printf(TEXT("%d,%d,%d,%d,%d,%.17g\n"), Shape, Rotation, Other, OtherRotation, Side, Remaining);
                }
                if (Remaining < -1.e-7 || Remaining > OwnArea + 1.e-7
                    || !FMath::IsNearlyEqual(OwnArea - Remaining,
                        BoundaryArea(OtherOriginal, Side ^ 1) - BoundaryArea(OtherExposed, Side ^ 1), 1.e-7))
                { AddError(FString::Printf(TEXT("Asymmetric contact %d/%d side %d vs %d/%d"), Shape, Rotation, Side, Other, OtherRotation)); return false; }
                if (Other == 0 && Remaining > 1.e-7)
                { AddError(TEXT("Full neighbor leaves an internal face")); return false; }
                for (const auto& Patch : Exposed)
                {
                    const auto& Face = Solid.Faces[Patch.SourceFace];
                    const FVector Normal = FVector::CrossProduct(Solid.Vertices[Face[1]] - Solid.Vertices[Face[0]], Solid.Vertices[Face[2]] - Solid.Vertices[Face[0]]);
                    if (Patch.Vertices.Num() < 3 || Area(Patch.Vertices) <= 1.e-7
                        || FVector::DotProduct(Normal, FVector::CrossProduct(Patch.Vertices[1] - Patch.Vertices[0], Patch.Vertices[2] - Patch.Vertices[0])) <= 0)
                    { AddError(TEXT("Degenerate or reversed exposed patch")); return false; }
                }
            }
        }
    }
    FSFConstructionNeighbors Crossing;
    Crossing.Faces[3] = {true, 2, 0, 1};
    TArray<FSFConstructionSurfacePatch> Patches;
    SFConstructionGeometry::BuildExposed(2, Identity, Crossing, Patches);
    TestEqual(TEXT("Crossing slope triangles retain exact quarter cell face"), BoundaryArea(Patches, 3), 625.0);
    Crossing.Faces[3] = {true, 1, Identity, 1};
    SFConstructionGeometry::BuildExposed(2, Identity, Crossing, Patches);
    TestEqual(TEXT("Triangle minus half rectangle keeps exact eighth face"), BoundaryArea(Patches, 3), 312.5);
    const int32 Before = Patches.Num();
    Crossing.Faces[0].bKnown = false;
    TestFalse(TEXT("Unresolved border blocks publication"), SFConstructionGeometry::BuildExposed(2, Identity, Crossing, Patches));
    TestEqual(TEXT("Unknown border preserves output"), Patches.Num(), Before);
    if (bWriteOracle)
    { TestTrue(TEXT("Write complete exposed-polygon area oracle"), FFileHelper::SaveStringToFile(OracleCsv, *OraclePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionNeighborRuntimeTest,
    "ShatteredFrontier.Construction.Geometry.NeighborRuntimeMeshAndRemoval",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionNeighborRuntimeTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = ASFConstructionSandboxGameMode::StaticClass();
    if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Preview = World->SpawnActor<ASFConstructionSandboxPreview>(FVector(-1600, 1550, 0), FRotator::ZeroRotator);
    if (!Preview) { return false; }
    const int32 Identity = SFConstructionGeometry::IdentityRotation();
    FSFConstructionNeighbors Borders;
    Borders.Faces[3] = {true, 2, 0, 1};
    if (!TestTrue(TEXT("Runtime crossing slope mesh cooks"), Preview->RebuildWithNeighbors(2, Identity, 1, Borders))) { return false; }
    auto* Mesh = const_cast<UProceduralMeshComponent*>(Preview->GetMesh());
    TestEqual(TEXT("Actual runtime triangles preserve crossing quarter face"), MeshBoundaryArea(Mesh, 3), 625.0);
    auto* Neighbor = World->SpawnActor<ASFConstructionSandboxPreview>(Preview->GetActorLocation() + FVector(0,50,0), FRotator::ZeroRotator);
    if (!Neighbor) { return false; }
    FSFConstructionNeighbors Reverse;
    Reverse.Faces[2] = {true, 2, Identity, 1};
    if (!TestTrue(TEXT("Adjacent crossing member installs reciprocal seam"), Neighbor->RebuildWithNeighbors(2, 0, 1, Reverse))) { return false; }
    TestEqual(TEXT("Actual adjacent mesh retains its uncovered quarter face"), MeshBoundaryArea(const_cast<UProceduralMeshComponent*>(Neighbor->GetMesh()), 2), 625.0);
    FSFConstructionSolid Canonical;
    SFConstructionGeometry::Build(2, Identity, Canonical);
    TestEqual(TEXT("Collision remains single canonical hull"), Mesh->GetBodySetup()->AggGeom.ConvexElems.Num(), 1);
    TestEqual(TEXT("Clipped mesh does not clip convex vertices"), Mesh->GetBodySetup()->AggGeom.ConvexElems[0].VertexData.Num(), Canonical.Vertices.Num());
    const auto Trace = [World, Preview](double X, double Z)
    {
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNeighbor), false);
        return World->LineTraceSingleByChannel(Hit, Preview->GetActorLocation() + FVector(X,-20,Z),
            Preview->GetActorLocation() + FVector(X,70,Z), ECC_Visibility, Query)
            && Hit.GetActor() == Preview;
    };
    TestTrue(TEXT("Canonical solid still blocks across culled seam"), Trace(37,12));
    TestFalse(TEXT("Slope empty region remains open"), Trace(12,37));
    Borders.Faces[3].bKnown = false;
    TestFalse(TEXT("Unknown neighbor cannot replace runtime projection"), Preview->RebuildWithNeighbors(2, Identity, 2, Borders));
    TestTrue(TEXT("Failed rebuild preserves active mesh"), Preview->GetMesh() == Mesh);
    TestEqual(TEXT("Failed rebuild preserves active generation"), Preview->GetGeneration(), uint64(1));
    Neighbor->Destroy();
    if (!TestTrue(TEXT("Known neighbor removal exposes surviving face"), Preview->Rebuild(2, Identity, 2))) { return false; }
    Mesh = const_cast<UProceduralMeshComponent*>(Preview->GetMesh());
    TestEqual(TEXT("Removed neighbor restores complete triangle face"), MeshBoundaryArea(Mesh, 3), 1250.0);
    TestTrue(TEXT("Removal keeps collision"), Trace(37,12));
    Borders = FSFConstructionNeighbors();
    Borders.Faces[3] = {true, 0, Identity, 2};
    if (!TestTrue(TEXT("Prepare staged seam"), Preview->PrepareRebuild(2, Identity, 3, Borders))) { return false; }
    TestTrue(TEXT("Staging preserves old active projection"), Preview->GetMesh() == Mesh);
    TestTrue(TEXT("Staged collision does not intercept old actor query"), Trace(37,12));
    Borders.Faces[0].bKnown = false;
    TestFalse(TEXT("Failed subsequent preparation invalidates earlier staging"), Preview->PrepareRebuild(2, Identity, 4, Borders));
    TestFalse(TEXT("Rejected input cannot later activate an older staged mesh"), Preview->ActivatePrepared());
    Preview->DiscardPrepared();
    TestTrue(TEXT("Aborted edit preserves mesh"), Preview->GetMesh() == Mesh);
    TestEqual(TEXT("Aborted edit preserves generation"), Preview->GetGeneration(), uint64(2));
    Borders.Faces[0].bKnown = true;
    TestTrue(TEXT("Valid staging can activate"), Preview->PrepareRebuild(2, Identity, 3, Borders) && Preview->CanActivatePrepared());
    const FVector OldLocation = Preview->GetActorLocation();
    Preview->SetActorLocation(OldLocation + FVector(1,0,0));
    TestFalse(TEXT("Moved actor invalidates activation preflight"), Preview->CanActivatePrepared());
    TestFalse(TEXT("Moved actor cannot publish stale projection"), Preview->ActivatePrepared());
    TestTrue(TEXT("Transform rejection preserves active mesh"), Preview->GetMesh() == Mesh);
    Preview->DiscardPrepared();
    Preview->SetActorLocation(OldLocation);
    uint64 Generation = 2;
    Borders = FSFConstructionNeighbors();
    for (auto& Border : Borders.Faces) { Border = {true, 0, Identity, 1}; }
    for (int32 Shape = 0; Shape < 3; ++Shape)
    for (int32 Rotation = 0; Rotation < 24; ++Rotation)
    {
        if (!TestTrue(TEXT("Every rotated neighbor-clipped runtime solid cooks"), Preview->RebuildWithNeighbors(Shape, Rotation, ++Generation, Borders))) { return false; }
        Mesh = const_cast<UProceduralMeshComponent*>(Preview->GetMesh());
        for (int32 Side = 0; Side < 6; ++Side)
        { TestEqual(TEXT("Runtime internal boundary triangles are fully removed"), MeshBoundaryArea(Mesh, Side), 0.0); }
        SFConstructionGeometry::Build(Shape, Rotation, Canonical);
        const auto& Hull = Mesh->GetBodySetup()->AggGeom.ConvexElems[0].VertexData;
        TestEqual(TEXT("All rotated culled collision vertices remain canonical"), Hull.Num(), Canonical.Vertices.Num());
        for (const FVector& Vertex : Canonical.Vertices)
        { TestTrue(TEXT("Canonical vertex retained in collision"), Hull.Contains(Vertex)); }
        const auto Transform = [Preview, &Canonical](const FVector& P)
        {
            const FVector Centered = P - FVector(25);
            return Preview->GetActorLocation() + FVector(25) + FVector(Canonical.Rotation[0])*Centered.X
                + FVector(Canonical.Rotation[1])*Centered.Y + FVector(Canonical.Rotation[2])*Centered.Z;
        };
        const auto RotatedTrace = [World, Preview, &Transform](double X, double Z)
        {
            FHitResult Hit;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNeighborRotation), false);
            return World->LineTraceSingleByChannel(Hit, Transform(FVector(X,-20,Z)), Transform(FVector(X,70,Z)), ECC_Visibility, Query)
                && Hit.GetActor() == Preview;
        };
        TestTrue(TEXT("Culled rotated canonical occupancy blocks real physics"), RotatedTrace(37,12));
        TestFalse(TEXT("Culled rotated empty portion remains open"), Shape == 0 ? RotatedTrace(60,25) : RotatedTrace(12,37));
    }
    return true;
}
#endif
