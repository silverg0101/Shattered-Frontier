#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionGeometry.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "Tests/SFConstructionInputScenario.h"

ASFConstructionSandboxPreview::ASFConstructionSandboxPreview()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("MinimumCellCorner")));
    bReplicates = false;
    PrimaryActorTick.bCanEverTick = false;
}

bool ASFConstructionSandboxPreview::Rebuild(int32 Shape, int32 Rotation, uint64 Generation)
{
    return RebuildWithNeighbors(Shape, Rotation, Generation, FSFConstructionNeighbors());
}

bool ASFConstructionSandboxPreview::RebuildWithNeighbors(int32 Shape, int32 Rotation, uint64 Generation,
    const FSFConstructionNeighbors& Neighbors)
{
    return PrepareRebuild(Shape, Rotation, Generation, Neighbors) && ActivatePrepared();
}

bool ASFConstructionSandboxPreview::PrepareRebuild(int32 Shape, int32 Rotation, uint64 Generation,
    const FSFConstructionNeighbors& Neighbors)
{
    if (!IsInGameThread()) { return false; }
    DiscardPrepared();
    if (!IsInGameThread() || !HasAuthority() || !GetWorld()
        || !GetWorld()->GetAuthGameMode<ASFConstructionSandboxGameMode>()
        || GetNetMode() != NM_Standalone || !GetActorScale3D().Equals(FVector::OneVector)
        || !GetActorQuat().Equals(FQuat::Identity)
        || Generation == 0 || Generation <= ActiveGeneration) { return false; }
    FSFConstructionSolid Solid;
    if (!SFConstructionGeometry::Build(Shape, Rotation, Solid)) { return false; }

    TArray<FSFConstructionSurfacePatch> Patches;
    if (!SFConstructionGeometry::BuildExposed(Shape, Rotation, Neighbors, Patches)) { return false; }
    // Stage an entirely new projection. The old one remains active if cooking fails.
    UProceduralMeshComponent* Candidate = NewObject<UProceduralMeshComponent>(this);
    Candidate->SetupAttachment(GetRootComponent());
    Candidate->bUseAsyncCooking = false;
    Candidate->bUseComplexAsSimpleCollision = false;
    Candidate->SetGenerateOverlapEvents(false);
    Candidate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Candidate->SetCollisionObjectType(ECC_WorldStatic);
    Candidate->SetCollisionResponseToAllChannels(ECR_Ignore);
    Candidate->SetVisibility(false);
    TArray<FVector> RenderVertices, Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UV;
    for (const auto& Patch : Patches)
    {
        const auto& Face = Patch.Vertices;
        const int32 Base = RenderVertices.Num();
        const FVector Normal = FVector::CrossProduct(Face[1] - Face[0], Face[2] - Face[0]).GetSafeNormal();
        for (const FVector& Vertex : Face)
        {
            RenderVertices.Add(Vertex);
            Normals.Add(Normal);
            UV.Add(FVector2D(Vertex.X, Vertex.Y) / SFConstructionGeometry::CellCentimetres);
        }
        // Unreal's front-face winding is opposite the canonical outward cycle.
        for (int32 Index = 1; Index + 1 < Face.Num(); ++Index)
        { Triangles.Append({Base, Base + Index + 1, Base + Index}); }
    }
    Candidate->CreateMeshSection(0, RenderVertices, Triangles, Normals, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
    Candidate->SetCollisionConvexMeshes({Solid.Vertices});
    UBodySetup* Body = Candidate->GetBodySetup();
    if (!Body || !Body->bCreatedPhysicsMeshes || Body->bFailedToCreatePhysicsMeshes
        || Body->AggGeom.ConvexElems.Num() != 1 || !Body->AggGeom.ConvexElems[0].GetChaosConvexMesh())
    {
        Candidate->DestroyComponent();
        return false;
    }
    // Do not inherit a project-level complex-as-simple default.
    Body->CollisionTraceFlag = CTF_UseSimpleAndComplex;
    // No callbacks or yields: synchronous cooking and activation on the game thread.
    Candidate->RegisterComponent();
    if (!Candidate->IsRegistered()) { Candidate->DestroyComponent(); return false; }
    Candidate->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (!Candidate->IsPhysicsStateCreated()) { Candidate->DestroyComponent(); return false; }
    DiscardPrepared();
    PreparedMesh = Candidate;
    PreparedPayloadHash = Solid.PayloadSha256;
    PreparedGeneration = Generation;
    PreparedTransform = GetActorTransform();
    return true;
}

void ASFConstructionSandboxPreview::DiscardPrepared()
{
    if (PreparedMesh) { PreparedMesh->DestroyComponent(); }
    PreparedMesh = nullptr;
    PreparedGeneration = 0;
    PreparedPayloadHash.Reset();
}

bool ASFConstructionSandboxPreview::CanActivatePrepared() const
{
    return IsInGameThread() && HasAuthority() && GetWorld()
        && GetWorld()->GetAuthGameMode<ASFConstructionSandboxGameMode>() && GetNetMode() == NM_Standalone
        && PreparedMesh && PreparedGeneration > ActiveGeneration
        && GetActorTransform().Equals(PreparedTransform) && PreparedMesh->IsPhysicsStateCreated();
}

bool ASFConstructionSandboxPreview::ActivatePrepared()
{
    if (!CanActivatePrepared()) { return false; }
    UProceduralMeshComponent* Candidate = PreparedMesh;
    // The staged component already has a cooked, registered physics state.
    // Switching responses does not destroy/recreate it or introduce a cook failure.
    Candidate->SetCollisionResponseToAllChannels(ECR_Block);
    if (ActiveMesh) { ActiveMesh->DestroyComponent(); }
    ActiveMesh = Candidate;
    ActivePayloadHash = MoveTemp(PreparedPayloadHash);
    ActiveGeneration = PreparedGeneration;
    PreparedMesh = nullptr;
    PreparedGeneration = 0;
    Candidate->SetVisibility(true);
    return true;
}

ASFConstructionSandboxGameMode::ASFConstructionSandboxGameMode()
{
    PlayerControllerClass = ASFConstructionSandboxController::StaticClass();
}

void ASFConstructionSandboxGameMode::StartPlay()
{
    Super::StartPlay();
    // Local reference solids; editable members belong to the standalone state.
    if (GetNetMode() != NM_Standalone) { return; }
    int32 Installed = 0;
    for (int32 Shape = 0; Shape < SFConstructionGeometry::ShapeCount; ++Shape)
    {
        ASFConstructionSandboxPreview* Preview = GetWorld()->SpawnActor<ASFConstructionSandboxPreview>(FVector(Shape * 100.0, 0, 0), FRotator::ZeroRotator);
        if (!Preview || !Preview->Rebuild(Shape, SFConstructionGeometry::IdentityRotation(), 1))
        { UE_LOG(LogTemp, Error, TEXT("CONSTRUCTION_PILOT_FAILED Shape=%d"), Shape); }
        else { ++Installed; }
    }
    if (Installed == SFConstructionGeometry::ShapeCount)
    { UE_LOG(LogTemp, Display, TEXT("CONSTRUCTION_PILOT_READY StandaloneOnly=1 ReferenceSolids=1 Shapes=%d Economy=0"), Installed); }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    StartConstructionInputScenario(GetWorld());
#endif
}
