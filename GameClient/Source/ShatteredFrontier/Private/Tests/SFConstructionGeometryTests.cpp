#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandbox.h"
#include "Framework/SFGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionCanonicalTest, "ShatteredFrontier.Construction.Geometry.CanonicalPilot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionCanonicalTest::RunTest(const FString& Parameters)
{
    const TCHAR* Hashes[] = {
        TEXT("9455bd04dab9df9f4643f175c95fb17cb7e25e6248c239975a8f1280f040061a"),
        TEXT("e6236a7b600eccb0a2f558f74524e183ef57154a278938694629ed247ecfd154"),
        TEXT("e55a54e6824119dffb929bf7de536eba786e7ce145efa732620da147cca4cdda")};
    for (int32 Shape = 0; Shape < 3; ++Shape)
    {
        TSet<FString> Seen;
        for (int32 Rotation = 0; Rotation < 24; ++Rotation)
        {
            FSFConstructionSolid Solid;
            if (!TestTrue(TEXT("Approved shape/rotation builds"), SFConstructionGeometry::Build(Shape, Rotation, Solid))) { return false; }
            TestEqual(TEXT("Pinned canonical identity"), Solid.PayloadSha256, FString(Hashes[Shape]));
            const FVector X(Solid.Rotation[0]), Y(Solid.Rotation[1]), Z(Solid.Rotation[2]);
            TestEqual(TEXT("Proper rotation determinant"), FVector::DotProduct(X, FVector::CrossProduct(Y, Z)), 1.0);
            Seen.Add(X.ToString() + Y.ToString() + Z.ToString());
            for (const FVector& Vertex : Solid.Vertices)
            {
                TestTrue(TEXT("Affine rotation stays inside declared 50cm cell"), Vertex.GetMin() >= 0 && Vertex.GetMax() <= 50);
            }
            TestTrue(TEXT("Positive outward volume survives rotation"), FMath::IsNearlyEqual(SFConstructionGeometry::SignedVolume(Solid), Shape == 0 ? 125000.0 : 62500.0, 0.0001));
        }
        TestEqual(TEXT("24 unique allowed orientations"), Seen.Num(), 24);
    }
    FSFConstructionSolid Slab;
    SFConstructionGeometry::Build(1, SFConstructionGeometry::IdentityRotation(), Slab);
    const FBox Bounds(Slab.Vertices);
    TestEqual(TEXT("Identity minimum corner pivot"), Bounds.Min, FVector::ZeroVector);
    TestEqual(TEXT("Half slab is exactly 50x50x25cm"), Bounds.Max, FVector(50,50,25));
    TestFalse(TEXT("Reject unknown shape"), SFConstructionGeometry::Build(3, 0, Slab));
    TestFalse(TEXT("Reject arbitrary rotation"), SFConstructionGeometry::Build(0, 24, Slab));
    TestEqual(TEXT("Rejected build preserves output"), Slab.ShapeKey, FString(TEXT("slab-half")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionPhysicsTest, "ShatteredFrontier.Construction.Geometry.ConvexPhysicsAndGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionPhysicsTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Real synchronous Chaos convex queries in an isolated test world; no packaged/network/rendered traversal acceptance."));
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = ASFConstructionSandboxGameMode::StaticClass();
    if (!TestTrue(TEXT("Explicit sandbox mode created"), World->SetGameMode(FURL()))) { return false; }
    if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Preview = World->SpawnActor<ASFConstructionSandboxPreview>(FVector(1000,0,0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Isolated preview actor"), Preview)) { return false; }
    uint64 Generation = 0;
    for (int32 Shape = 0; Shape < 3; ++Shape)
    {
        for (int32 Rotation = 0; Rotation < 24; ++Rotation)
        {
            if (!TestTrue(TEXT("Synchronous projection installed"), Preview->Rebuild(Shape, Rotation, ++Generation))) { return false; }
            FSFConstructionSolid Solid;
            SFConstructionGeometry::Build(Shape, Rotation, Solid);
            const auto Transform = [&Solid, Preview](const FVector& Point)
            {
                const FVector P = Point - FVector(25);
                return Preview->GetActorLocation() + FVector(25) + FVector(Solid.Rotation[0])*P.X + FVector(Solid.Rotation[1])*P.Y + FVector(Solid.Rotation[2])*P.Z;
            };
            const auto Trace = [World, &Transform, Preview](double X, double Z)
            {
                FHitResult Hit;
                FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionPilot), false);
                return World->LineTraceSingleByChannel(Hit, Transform(FVector(X,-20,Z)), Transform(FVector(X,70,Z)), ECC_Visibility, Query)
                    && Hit.GetActor() == Preview;
            };
            TestTrue(TEXT("Actual occupied solid blocks simple physics query"), Trace(37,12));
            TestFalse(TEXT("Actual empty portion stays open after every rotation"), Shape == 0 ? Trace(60,25) : Trace(12,37));
            const auto* Mesh = Preview->GetMesh();
            const FString Hash = Preview->GetPayloadHash();
            TestFalse(TEXT("Same generation rejected"), Preview->Rebuild(0, 0, Generation));
            TestFalse(TEXT("Old generation rejected"), Preview->Rebuild(0, 0, Generation - 1));
            TestFalse(TEXT("Unknown shape rejected before mutation"), Preview->Rebuild(3, 0, Generation + 1));
            TestFalse(TEXT("Unknown rotation rejected before mutation"), Preview->Rebuild(0, 24, Generation + 1));
            TestTrue(TEXT("Rejected rebuild preserves component"), Preview->GetMesh() == Mesh);
            TestEqual(TEXT("Rejected rebuild preserves generation"), Preview->GetGeneration(), Generation);
            TestEqual(TEXT("Rejected rebuild preserves content identity"), Preview->GetPayloadHash(), Hash);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionIsolationTest, "ShatteredFrontier.Construction.Geometry.NormalModeIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionIsolationTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = ASFGameMode::StaticClass();
    if (!TestTrue(TEXT("Normal E1 mode created"), World->SetGameMode(FURL()))) { return false; }
    if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    int32 AutoSpawnedPreviews = 0;
    for (TActorIterator<ASFConstructionSandboxPreview> It(World); It; ++It) { ++AutoSpawnedPreviews; }
    TestEqual(TEXT("Normal E1 mode automatically spawns no sandbox previews"), AutoSpawnedPreviews, 0);
    auto* Preview = World->SpawnActor<ASFConstructionSandboxPreview>();
    if (!TestNotNull(TEXT("Manually injected preview"), Preview)) { return false; }
    TestFalse(TEXT("Normal E1 mode cannot activate sandbox projection"), Preview->Rebuild(0, SFConstructionGeometry::IdentityRotation(), 1));
    TestTrue(TEXT("Rejected preview has no mesh/collision"), Preview->GetMesh() == nullptr);
    TestEqual(TEXT("Rejected preview generation unchanged"), Preview->GetGeneration(), uint64(0));
    return true;
}
#endif
