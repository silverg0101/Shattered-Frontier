#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionSandboxFoundation.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionGeometry.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionFoundationTest,
    "ShatteredFrontier.Construction.Support.SyntheticFoundation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionFoundationTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
    UWorld* World = Fixture.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = ASFConstructionSandboxGameMode::StaticClass();
    if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Floor = World->SpawnActor<AStaticMeshActor>();
    if (!Floor) { return false; }
    auto* Mesh = Floor->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Floor->SetActorLocation(FVector(0,0,-50));
    Floor->SetActorScale3D(FVector(20,20,1));
    TestFalse(TEXT("Generic collision is not compact-ground evidence"), SFConstructionSandboxFoundation::HasKnownPlane(World));
    Floor->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
    TestTrue(TEXT("Exact admitted synthetic floor anchors"), SFConstructionSandboxFoundation::HasKnownPlane(World));
    const int32 Identity = SFConstructionGeometry::IdentityRotation();
    TestTrue(TEXT("Canonical positive bottom contact on plane"), SFConstructionSandboxFoundation::Anchors(World, FIntVector(0,0,0), 0, Identity));
    TestFalse(TEXT("Floating cell does not anchor"), SFConstructionSandboxFoundation::Anchors(World, FIntVector(0,0,1), 0, Identity));
    TestFalse(TEXT("Outside physical foundation rejected"), SFConstructionSandboxFoundation::TouchesPlane(FIntVector(20,0,0), 0, Identity));
    bool bFoundUpperSlab = false;
    for (int32 Rotation = 0; Rotation < 24; ++Rotation)
    {
        FSFConstructionSolid Solid;
        SFConstructionGeometry::Build(1, Rotation, Solid);
        if (FBox(Solid.Vertices).Min.Z == 25)
        {
            bFoundUpperSlab = true;
            TestFalse(TEXT("Upper half slab floats above plane"), SFConstructionSandboxFoundation::TouchesPlane(FIntVector(0,0,0),1,Rotation));
        }
    }
    TestTrue(TEXT("Upper-slab rotation actually exercised"), bFoundUpperSlab);
    Floor->SetActorLocation(FVector(0,0,-49));
    TestFalse(TEXT("Changed foundation cannot reuse old anchor proof"), SFConstructionSandboxFoundation::HasKnownPlane(World));
    Floor->SetActorLocation(FVector(0,0,-50));
    Floor->SetActorRotation(FRotator(0,90,0));
    TestFalse(TEXT("Unexpected foundation orientation rejects"), SFConstructionSandboxFoundation::HasKnownPlane(World));
    Floor->SetActorRotation(FRotator::ZeroRotator);
    auto* Duplicate = World->SpawnActor<AStaticMeshActor>();
    Duplicate->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
    TestFalse(TEXT("Duplicate or invalid tagged foundation fails closed"), SFConstructionSandboxFoundation::HasKnownPlane(World));
    return true;
}
#endif
