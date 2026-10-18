#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionClearance.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandbox.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/WorldSettings.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include <limits>

namespace
{
UBoxComponent* ClearanceBox(UWorld* World, const FVector& Position, const FVector& Extent,
    const FQuat& Rotation = FQuat::Identity, const FVector& Scale = FVector::OneVector)
{
    auto* Actor = World->SpawnActor<AActor>();
    if (!Actor) { return nullptr; }
    auto* Box = NewObject<UBoxComponent>(Actor);
    Actor->SetRootComponent(Box);
    Box->SetBoxExtent(Extent);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->SetGenerateOverlapEvents(false);
    Box->SetWorldTransform(FTransform(Rotation, Position, Scale));
    Box->RegisterComponent();
    return Box;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionCanonicalClearanceTest,
    "ShatteredFrontier.Construction.Clearance.CanonicalConvexWorldPrimitives",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionCanonicalClearanceTest::RunTest(const FString& Parameters)
{
    using namespace SFConstructionClearance;
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    const FVector Corner(1000,-1000,1000);
    TestTrue(TEXT("Unknown shape fails closed"), Evaluate(World, 3, 0, Corner) == EResult::Unavailable);
    TestTrue(TEXT("Unknown rotation fails closed"), Evaluate(World, 0, 24, Corner) == EResult::Unavailable);
    FVector InvalidCorner = FVector::ZeroVector;
    InvalidCorner.X = std::numeric_limits<double>::quiet_NaN();
    TestTrue(TEXT("Nonfinite origin fails closed"), Evaluate(World, 0, 0, InvalidCorner) == EResult::Unavailable);
    for (int32 Shape = 0; Shape < 3; ++Shape)
    for (int32 Rotation = 0; Rotation < 24; ++Rotation)
    {
        FSFConstructionSolid Solid;
        SFConstructionGeometry::Build(Shape, Rotation, Solid);
        const auto Transform = [&Solid, &Corner](const FVector& Point)
        {
            const FVector P = Point - FVector(25);
            return Corner + FVector(25) + FVector(Solid.Rotation[0])*P.X
                + FVector(Solid.Rotation[1])*P.Y + FVector(Solid.Rotation[2])*P.Z;
        };
        const FQuat Quat = FRotationMatrix::MakeFromXY(FVector(Solid.Rotation[0]), FVector(Solid.Rotation[1])).ToQuat();
        auto* Box = ClearanceBox(World, Transform(FVector(37,25,12)), FVector(2,3,2), Quat, FVector(1.25,.75,1.5));
        if (!TestNotNull(TEXT("Rotated scaled fixture prop"), Box)) { return false; }
        TestTrue(TEXT("Positive overlap blocks every shape/rotation"), Evaluate(World, Shape, Rotation, Corner) == EResult::Blocked);
        Box->SetCollisionObjectType(ECC_PhysicsBody);
        TestTrue(TEXT("PhysicsBody props also block canonical occupancy"), Evaluate(World, Shape, Rotation, Corner) == EResult::Blocked);
        Box->SetWorldLocation(Transform(Shape == 0 ? FVector(60,25,25) : FVector(12,25,37)));
        TestTrue(TEXT("Actual empty slope/slab space admits scaled rotated prop"), Evaluate(World, Shape, Rotation, Corner) == EResult::Clear);
        Box->GetOwner()->Destroy();

        const FBox Bounds(Solid.Vertices);
        auto* Floor = ClearanceBox(World, Corner + FVector(25,25,Bounds.Min.Z - 10), FVector(100,100,10));
        if (!Floor) { return false; }
        TestTrue(TEXT("Canonical contact with floor permits placement"), Evaluate(World, Shape, Rotation, Corner) == EResult::Clear);
        Floor->AddWorldOffset(FVector(0,0,.01));
        TestTrue(TEXT("Positive floor intrusion smaller than old inset rejects"), Evaluate(World, Shape, Rotation, Corner) == EResult::Blocked);
        Floor->GetOwner()->Destroy();

        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Character = World->SpawnActor<ACharacter>(Transform(FVector(37,25,12)), Quat.Rotator(), Spawn);
        if (!Character) { return false; }
        Character->GetCapsuleComponent()->SetCapsuleSize(2,4);
        Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Character->GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
        TestTrue(TEXT("Actual character capsule rejects occupied solid"), Evaluate(World, Shape, Rotation, Corner) == EResult::Blocked);
        Character->SetActorLocation(Transform(Shape == 0 ? FVector(60,25,25) : FVector(12,25,37)));
        TestTrue(TEXT("Actual character capsule fits empty partial-solid space"), Evaluate(World, Shape, Rotation, Corner) == EResult::Clear);
        Character->SetActorLocation(Transform(FVector(51,25,12)));
        TestTrue(TEXT("Capsule centre outside still blocks when crossing solid"), Evaluate(World, Shape, Rotation, Corner) == EResult::Blocked);
        Character->Destroy();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionClearanceCompoundTest,
    "ShatteredFrontier.Construction.Clearance.CompoundsAndStaging",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionClearanceCompoundTest::RunTest(const FString& Parameters)
{
    using namespace SFConstructionClearance;
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
    UWorld* World = Fixture.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = ASFConstructionSandboxGameMode::StaticClass();
    if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    const FVector Corner(1000,1000,1000);
    const int32 Identity = SFConstructionGeometry::IdentityRotation();
    auto* Preview = World->SpawnActor<ASFConstructionSandboxPreview>(Corner, FRotator::ZeroRotator);
    if (!Preview || !Preview->PrepareRebuild(0, Identity, 1, FSFConstructionNeighbors())) { return false; }
    TestTrue(TEXT("Prepared ignore-response components never block clearance"), Evaluate(World, 0, Identity, Corner) == EResult::Clear);
    TestTrue(TEXT("Activate already prepared canonical projection"), Preview->ActivatePrepared());
    TestTrue(TEXT("Active canonical projection blocks overlap"), Evaluate(World, 0, Identity, Corner) == EResult::Blocked);
    TestTrue(TEXT("Active adjacent canonical face touch remains legal"), Evaluate(World, 0, Identity, Corner + FVector(50,0,0)) == EResult::Clear);
    Preview->Destroy();

    auto* Prop = World->SpawnActor<AActor>();
    if (!Prop) { return false; }
    auto* Mesh = NewObject<UProceduralMeshComponent>(Prop);
    Prop->SetRootComponent(Mesh);
    Mesh->bUseComplexAsSimpleCollision = false;
    Mesh->bUseAsyncCooking = false;
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetCollisionObjectType(ECC_WorldStatic);
    Mesh->SetWorldLocation(Corner);
    FSFConstructionSolid Cube;
    SFConstructionGeometry::Build(0, Identity, Cube);
    TArray<FVector> EmptyHull, OccupiedHull;
    for (const FVector& V : Cube.Vertices)
    {
        EmptyHull.Add(FVector(10,23,35) + V * .08);
        OccupiedHull.Add(FVector(35,23,10) + V * .08);
    }
    Mesh->SetCollisionConvexMeshes({EmptyHull, OccupiedHull});
    Mesh->RegisterComponent();
    TestTrue(TEXT("Compound prop detects occupied second simple hull"), Evaluate(World, 2, Identity, Corner) == EResult::Blocked);
    Mesh->SetCollisionConvexMeshes({EmptyHull});
    TestTrue(TEXT("Compound bounds cannot block truly empty slope space"), Evaluate(World, 2, Identity, Corner) == EResult::Clear);
    Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
    TestTrue(TEXT("Encountered unsupported collision policy fails closed"), Evaluate(World, 2, Identity, Corner) == EResult::Unavailable);
    Mesh->GetOwner()->Destroy();
    return true;
}
#endif
