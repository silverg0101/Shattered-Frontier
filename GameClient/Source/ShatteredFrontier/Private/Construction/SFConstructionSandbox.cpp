#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionGeometry.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Chaos/Convex.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ProceduralMeshComponent.h"
#include "Tests/SFConstructionInputScenario.h"
#include "Tests/SFConstructionDoorInputScenario.h"
#include "Tests/SFConstructionSaveRestartScenario.h"
#include "Tests/SFConstructionLineInputScenario.h"

namespace
{
template<typename VertexArray>
bool MatchesCanonicalVertices(const VertexArray& Vertices, const TArray<FVector>& Expected)
{
    if (Vertices.Num() != Expected.Num()) { return false; }
    TArray<bool> Matched;
    Matched.Init(false,Expected.Num());
    for (const auto& Vertex : Vertices)
    {
        const FVector Position(Vertex);
        bool bFound = false;
        for (int32 Index = 0; Index < Expected.Num(); ++Index)
        {
            if (!Matched[Index] && Position.Equals(Expected[Index],1.e-9))
            { Matched[Index] = true; bFound = true; break; }
        }
        if (!bFound) { return false; }
    }
    for (bool bVertexMatched : Matched) { if (!bVertexMatched) { return false; } }
    return true;
}
}

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
    Candidate->SetHiddenInGame(true);
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
    PreparedShape = Shape;
    PreparedRotation = Rotation;
    PreparedPayloadHash = Solid.PayloadSha256;
    PreparedGeneration = Generation;
    PreparedTransform = GetActorTransform();
    return true;
}

void ASFConstructionSandboxPreview::DiscardPrepared()
{
    if (PreparedMesh) { PreparedMesh->DestroyComponent(); }
    PreparedMesh = nullptr;
    PreparedShape = INDEX_NONE;
    PreparedRotation = INDEX_NONE;
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
    ActiveShape = PreparedShape;
    ActiveRotation = PreparedRotation;
    ActivePayloadHash = MoveTemp(PreparedPayloadHash);
    ActiveGeneration = PreparedGeneration;
    PreparedMesh = nullptr;
    PreparedShape = INDEX_NONE;
    PreparedRotation = INDEX_NONE;
    PreparedGeneration = 0;
    Candidate->SetVisibility(true);
    Candidate->SetHiddenInGame(false);
    return true;
}

bool ASFConstructionSandboxPreview::IsStagedComponent(const UPrimitiveComponent* Component) const
{
    if (!IsInGameThread() || !IsValid(Component) || Component != PreparedMesh || PreparedGeneration == 0
        || PreparedGeneration <= ActiveGeneration || !Component->IsRegistered() || !Component->IsPhysicsStateCreated()
        || Component->GetAttachParent() != GetRootComponent()
        || !Component->GetRelativeTransform().Equals(FTransform::Identity,1.e-12)
        || Component->IsVisible() || !Component->bHiddenInGame
        || Component->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics
        || Component->GetCollisionObjectType() != ECC_WorldStatic || Component->GetGenerateOverlapEvents()
        || Component->GetCollisionResponseToChannels() != FCollisionResponseContainer(ECR_Ignore)) { return false; }
    FSFConstructionSolid Expected;
    if (!SFConstructionGeometry::Build(PreparedShape,PreparedRotation,Expected)
        || PreparedPayloadHash != Expected.PayloadSha256) { return false; }
    const UBodySetup* Body = const_cast<UPrimitiveComponent*>(Component)->GetBodySetup();
    if (!Body || !Body->bCreatedPhysicsMeshes || Body->bFailedToCreatePhysicsMeshes
        || Body->CollisionTraceFlag != CTF_UseSimpleAndComplex || Body->AggGeom.GetElementCount() != 1
        || Body->AggGeom.ConvexElems.Num() != 1 || !Body->AggGeom.BoxElems.IsEmpty()
        || !Body->AggGeom.SphereElems.IsEmpty() || !Body->AggGeom.SphylElems.IsEmpty()) { return false; }
    const FKConvexElem& Element = Body->AggGeom.ConvexElems[0];
    const auto& ChaosMesh = Element.GetChaosConvexMesh();
    return ChaosMesh.IsValid() && Element.GetTransform().Equals(FTransform::Identity,1.e-12)
        && MatchesCanonicalVertices(Element.VertexData,Expected.Vertices)
        && MatchesCanonicalVertices(ChaosMesh->GetVertices(),Expected.Vertices);
}

bool ASFConstructionSandboxPreview::IsProjectionIntact(int32 ExpectedShape, int32 ExpectedRotation,
    uint64 ExpectedGeneration, const FVector& ExpectedCorner) const
{
    if (!IsInGameThread() || !HasAuthority() || !GetWorld() || GetNetMode() != NM_Standalone
        || !GetWorld()->GetAuthGameMode<ASFConstructionSandboxGameMode>() || ExpectedGeneration == 0
        || ActiveGeneration != ExpectedGeneration || IsHidden() || !FMath::IsFinite(ExpectedCorner.X)
        || !FMath::IsFinite(ExpectedCorner.Y) || !FMath::IsFinite(ExpectedCorner.Z)
        || !GetActorLocation().Equals(ExpectedCorner,1.e-9)
        || !GetActorQuat().Equals(FQuat::Identity,1.e-12)
        || !GetActorScale3D().Equals(FVector::OneVector,1.e-12)
        || ActiveShape != ExpectedShape || ActiveRotation != ExpectedRotation
        || !IsValid(ActiveMesh) || !ActiveMesh->IsRegistered() || !ActiveMesh->IsPhysicsStateCreated()
        || ActiveMesh->GetAttachParent() != GetRootComponent()
        || !ActiveMesh->GetRelativeTransform().Equals(FTransform::Identity,1.e-12)
        || !ActiveMesh->IsVisible() || ActiveMesh->bHiddenInGame
        || ActiveMesh->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics
        || ActiveMesh->GetCollisionObjectType() != ECC_WorldStatic || ActiveMesh->GetGenerateOverlapEvents()
        || ActiveMesh->bUseAsyncCooking || ActiveMesh->bUseComplexAsSimpleCollision
        || ActiveMesh->GetCollisionResponseToChannels() != FCollisionResponseContainer(ECR_Block)) { return false; }

    FSFConstructionSolid Expected;
    if (!SFConstructionGeometry::Build(ExpectedShape,ExpectedRotation,Expected)
        || ActivePayloadHash != Expected.PayloadSha256) { return false; }
    const UBodySetup* Body = ActiveMesh->GetBodySetup();
    if (!Body || !Body->bCreatedPhysicsMeshes || Body->bFailedToCreatePhysicsMeshes
        || Body->CollisionTraceFlag != CTF_UseSimpleAndComplex || Body->AggGeom.GetElementCount() != 1
        || Body->AggGeom.ConvexElems.Num() != 1 || Body->AggGeom.BoxElems.Num() != 0
        || Body->AggGeom.SphereElems.Num() != 0 || Body->AggGeom.SphylElems.Num() != 0) { return false; }
    const FKConvexElem& Element = Body->AggGeom.ConvexElems[0];
    const auto& ChaosMesh = Element.GetChaosConvexMesh();
    if (!ChaosMesh.IsValid() || !Element.GetTransform().Equals(FTransform::Identity,1.e-12)
        || !MatchesCanonicalVertices(Element.VertexData,Expected.Vertices)
        || !MatchesCanonicalVertices(ChaosMesh->GetVertices(),Expected.Vertices)) { return false; }
    FBodyInstance* BodyInstance = const_cast<UProceduralMeshComponent*>(ActiveMesh.Get())->GetBodyInstance();
    if (!BodyInstance || !BodyInstance->IsValidBodyInstance()) { return false; }

    TArray<UPrimitiveComponent*> PrimitiveComponents;
    GetComponents(PrimitiveComponents);
    for (const UPrimitiveComponent* Component : PrimitiveComponents)
    {
        if (Component == ActiveMesh) { continue; }
        if (!IsStagedComponent(Component)) { return false; }
    }
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
    StartConstructionDoorInputScenario(GetWorld());
    StartConstructionSaveRestartScenario(GetWorld());
    StartConstructionLineInputScenario(GetWorld());
#endif
}
