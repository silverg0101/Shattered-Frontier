#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionGeometry.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

ASFConstructionSandboxPreview::ASFConstructionSandboxPreview()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("MinimumCellCorner")));
    bReplicates = false;
    PrimaryActorTick.bCanEverTick = false;
}

bool ASFConstructionSandboxPreview::Rebuild(int32 Shape, int32 Rotation, uint64 Generation)
{
    if (!IsInGameThread() || !HasAuthority() || !GetWorld()
        || !GetWorld()->GetAuthGameMode<ASFConstructionSandboxGameMode>()
        || GetNetMode() != NM_Standalone || !GetActorScale3D().Equals(FVector::OneVector)
        || !GetActorQuat().Equals(FQuat::Identity)
        || Generation == 0 || Generation <= ActiveGeneration) { return false; }
    FSFConstructionSolid Solid;
    if (!SFConstructionGeometry::Build(Shape, Rotation, Solid)) { return false; }

    // Stage an entirely new projection. The old one remains active if cooking fails.
    UProceduralMeshComponent* Candidate = NewObject<UProceduralMeshComponent>(this);
    Candidate->SetupAttachment(GetRootComponent());
    Candidate->bUseAsyncCooking = false;
    Candidate->bUseComplexAsSimpleCollision = false;
    Candidate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Candidate->SetCollisionObjectType(ECC_WorldStatic);
    Candidate->SetCollisionResponseToAllChannels(ECR_Block);
    Candidate->SetVisibility(false);
    TArray<FVector> RenderVertices, Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UV;
    for (const auto& Face : Solid.Faces)
    {
        const int32 Base = RenderVertices.Num();
        const FVector Normal = FVector::CrossProduct(Solid.Vertices[Face[1]] - Solid.Vertices[Face[0]], Solid.Vertices[Face[2]] - Solid.Vertices[Face[0]]).GetSafeNormal();
        for (int32 Vertex : Face)
        {
            RenderVertices.Add(Solid.Vertices[Vertex]);
            Normals.Add(Normal);
            UV.Add(FVector2D(Solid.Vertices[Vertex].X, Solid.Vertices[Vertex].Y) / SFConstructionGeometry::CellCentimetres);
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
    if (ActiveMesh) { ActiveMesh->DestroyComponent(); }
    ActiveMesh = Candidate;
    ActivePayloadHash = Solid.PayloadSha256;
    ActiveGeneration = Generation;
    Candidate->SetVisibility(true);
    return true;
}

void ASFConstructionSandboxGameMode::StartPlay()
{
    Super::StartPlay();
    // This is a local physics/render fixture, not a replicated construction tool.
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
    { UE_LOG(LogTemp, Display, TEXT("CONSTRUCTION_PILOT_READY LocalGeometryOnly=1 Shapes=%d Edits=0 Economy=0"), Installed); }
}
