#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionSandbox.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Framework/SFE1GameMode.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

namespace
{
bool PrepareIntegrityWorld(FTestWorldWrapper& Fixture, UWorld*& World, bool bSandbox)
{
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
    World = Fixture.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = bSandbox ? ASFConstructionSandboxGameMode::StaticClass() : ASFE1GameMode::StaticClass();
    if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionProjectionIntegrityTest,
    "ShatteredFrontier.Construction.ProjectionIntegrity.ActiveAndStagedGuards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionProjectionIntegrityTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    UWorld* World = nullptr;
    if (!PrepareIntegrityWorld(Fixture,World,true)) { Fixture.ForwardErrorMessages(this); return false; }
    constexpr uint64 Generation = 4;
    const FVector Corner(250,-100,0);
    auto* Preview = World->SpawnActor<ASFConstructionSandboxPreview>(Corner,FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Standalone preview spawned"),Preview)) { return false; }
    if (!TestTrue(TEXT("Canonical projection builds"),Preview->Rebuild(0,SFConstructionGeometry::IdentityRotation(),Generation))) { return false; }
    TestTrue(TEXT("Exact active projection passes integrity"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    TestFalse(TEXT("Wrong expected shape rejects"),Preview->IsProjectionIntact(1,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    TestFalse(TEXT("Wrong expected rotation rejects"),Preview->IsProjectionIntact(0,(SFConstructionGeometry::IdentityRotation()+1)%SFConstructionGeometry::RotationCount,Generation,Corner));
    TestFalse(TEXT("Wrong expected generation rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation+1,Corner));
    TestFalse(TEXT("Wrong expected corner rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner+FVector(0.001,0,0)));

    Preview->SetActorHiddenInGame(true);
    TestFalse(TEXT("Hidden active actor rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Preview->SetActorHiddenInGame(false);
    TestTrue(TEXT("Restored actor visibility returns to intact"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));

    const FTransform Original = Preview->GetActorTransform();
    Preview->AddActorWorldOffset(FVector(1,0,0));
    TestFalse(TEXT("Moved actor rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Preview->SetActorTransform(Original);
    Preview->AddActorLocalRotation(FRotator(0,1,0));
    TestFalse(TEXT("Rotated actor rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Preview->SetActorTransform(Original);
    Preview->SetActorScale3D(FVector(1,1.001,1));
    TestFalse(TEXT("Scaled actor rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Preview->SetActorTransform(Original);

    auto* Mesh = const_cast<UProceduralMeshComponent*>(Preview->GetMesh());
    if (!TestNotNull(TEXT("Active mesh available"),Mesh)) { return false; }
    Mesh->SetHiddenInGame(true);
    TestFalse(TEXT("Hidden active component rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Mesh->SetHiddenInGame(false);
    TestTrue(TEXT("Restored component visibility returns to intact"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Mesh->SetRelativeLocation(FVector(0.01,0,0));
    TestFalse(TEXT("Moved active component rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Mesh->SetRelativeTransform(FTransform::Identity);
    Mesh->SetRelativeRotation(FRotator(0,1,0));
    TestFalse(TEXT("Rotated active component rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Mesh->SetRelativeTransform(FTransform::Identity);
    Mesh->SetRelativeScale3D(FVector(1,1.001,1));
    TestFalse(TEXT("Scaled active component rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Mesh->SetRelativeTransform(FTransform::Identity);
    TestTrue(TEXT("Restored component transform returns to intact"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Mesh->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Ignore);
    TestFalse(TEXT("Changed response container rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Mesh->SetCollisionResponseToAllChannels(ECR_Block);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TestFalse(TEXT("Disabled collision rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TestTrue(TEXT("Restored collision returns to intact"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));

    UBodySetup* Body = Mesh->GetBodySetup();
    if (!TestNotNull(TEXT("Cooked body setup available"),Body) || !TestEqual(TEXT("Exactly one canonical convex"),Body->AggGeom.ConvexElems.Num(),1)) { return false; }
    const FVector OriginalVertex = Body->AggGeom.ConvexElems[0].VertexData[0];
    Body->AggGeom.ConvexElems[0].VertexData[0] += FVector(0.01,0,0);
    TestFalse(TEXT("Altered authored convex vertices reject"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Body->AggGeom.ConvexElems[0].VertexData[0] = OriginalVertex;
    Body->AggGeom.ConvexElems[0].SetTransform(FTransform(FVector(0.01,0,0)));
    TestFalse(TEXT("Altered convex element transform rejects"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Body->AggGeom.ConvexElems[0].SetTransform(FTransform::Identity);
    TestTrue(TEXT("Restored convex data returns to intact"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));

    UBoxComponent* Unexpected = NewObject<UBoxComponent>(Preview);
    Unexpected->SetupAttachment(Preview->GetRootComponent());
    Unexpected->RegisterComponent();
    TestFalse(TEXT("Unexpected primitive component rejects integrity"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    Unexpected->DestroyComponent();
    TestTrue(TEXT("Removing unexpected primitive restores integrity"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));

    const int32 NextRotation = (SFConstructionGeometry::IdentityRotation()+1)%SFConstructionGeometry::RotationCount;
    TestTrue(TEXT("A newly prepared hidden Ignore mesh is recognized"),Preview->PrepareRebuild(1,NextRotation,Generation+1,FSFConstructionNeighbors()));
    TestTrue(TEXT("Prepared mesh does not invalidate intact active projection"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));
    TArray<UPrimitiveComponent*> PreparedComponents;
    Preview->GetComponents(PreparedComponents);
    bool bFoundStaged = false;
    for (UPrimitiveComponent* Component : PreparedComponents)
    { if (Component != Preview->GetMesh()) { bFoundStaged |= Preview->IsStagedComponent(Component); } }
    TestTrue(TEXT("Exact prepared component passes staged guard"),bFoundStaged);
    Preview->DiscardPrepared();
    TestTrue(TEXT("Discarded staging leaves active projection intact"),Preview->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),Generation,Corner));

    FTestWorldWrapper NormalFixture;
    UWorld* Normal = nullptr;
    if (!PrepareIntegrityWorld(NormalFixture,Normal,false)) { NormalFixture.ForwardErrorMessages(this); return false; }
    auto* Injected = Normal->SpawnActor<ASFConstructionSandboxPreview>(FVector::ZeroVector,FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Normal mode preview actor exists for injection test"),Injected)) { return false; }
    TestFalse(TEXT("Normal E1 cannot create an active integrity-valid projection"),Injected->Rebuild(0,SFConstructionGeometry::IdentityRotation(),1));
    TestFalse(TEXT("Normal E1 cannot validate injected projection"),Injected->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),1,FVector::ZeroVector));
    return true;
}

#endif
