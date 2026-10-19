#include "SFConstructionNetworkProjection.h"
#include "Construction/SFConstructionNetwork.h"
#include "Construction/SFConstructionGeometry.h"
#include "Chaos/Convex.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

namespace
{
template<typename VertexArray>
bool MatchesVertices(const VertexArray& Vertices, const TArray<FVector>& Expected)
{
    if (Vertices.Num()!=Expected.Num()) { return false; }
    TArray<bool> Matched; Matched.Init(false,Expected.Num());
    for (const auto& Vertex : Vertices)
    {
        bool bFound=false;
        for (int32 I=0; I<Expected.Num(); ++I)
        {
            if (!Matched[I] && FVector(Vertex).Equals(Expected[I],1.e-9))
            { Matched[I]=true; bFound=true; break; }
        }
        if (!bFound) { return false; }
    }
    return true;
}
}

UProceduralMeshComponent* SFConstructionNetworkProjection::Stage(ASFConstructionNetworkState* Owner, const FSFConstructionNetworkMember& Member)
{
    FSFConstructionSolid Solid;
    if (!IsInGameThread() || !IsValid(Owner) || Owner->GetClass()!=ASFConstructionNetworkState::StaticClass()
        || !Owner->GetActorTransform().Equals(FTransform::Identity)
        || !SFConstructionGeometry::Build(Member.Shape,Member.Rotation,Solid)) { return nullptr; }
    auto* Mesh=NewObject<UProceduralMeshComponent>(Owner);
    Mesh->SetupAttachment(Owner->GetRootComponent());
    Mesh->SetRelativeLocation(FVector(Member.Cell)*SFConstructionGeometry::CellCentimetres);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->bUseAsyncCooking=false;
    Mesh->bUseComplexAsSimpleCollision=false;
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCollisionObjectType(ECC_WorldStatic);
    Mesh->SetCollisionResponseToAllChannels(ECR_Block);
    Mesh->SetHiddenInGame(true);
    TArray<FVector> Vertices,Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UV;
    for (const auto& Face : Solid.Faces)
    {
        const int32 Base=Vertices.Num();
        const FVector Normal=FVector::CrossProduct(Solid.Vertices[Face[1]]-Solid.Vertices[Face[0]],Solid.Vertices[Face[2]]-Solid.Vertices[Face[0]]).GetSafeNormal();
        for (int32 Index : Face)
        { Vertices.Add(Solid.Vertices[Index]); Normals.Add(Normal); UV.Add(FVector2D(Solid.Vertices[Index].X,Solid.Vertices[Index].Y)/SFConstructionGeometry::CellCentimetres); }
        for (int32 I=1; I+1<Face.Num(); ++I) { Triangles.Append({Base,Base+I+1,Base+I}); }
    }
    Mesh->CreateMeshSection(0,Vertices,Triangles,Normals,UV,TArray<FColor>(),TArray<FProcMeshTangent>(),false);
    Mesh->SetCollisionConvexMeshes({Solid.Vertices});
    auto* Body=Mesh->GetBodySetup();
    if (!Body || !Body->bCreatedPhysicsMeshes || Body->bFailedToCreatePhysicsMeshes)
    { Mesh->DestroyComponent(); return nullptr; }
    Body->CollisionTraceFlag=CTF_UseSimpleAndComplex;
    Mesh->RegisterComponent();
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (!Matches(Mesh,Member)) { Mesh->DestroyComponent(); return nullptr; }
    return Mesh;
}

bool SFConstructionNetworkProjection::Matches(const UProceduralMeshComponent* Mesh, const FSFConstructionNetworkMember& Member)
{
    FSFConstructionSolid Solid;
    if (!IsInGameThread() || !IsValid(Mesh) || !Mesh->IsRegistered() || !Mesh->IsPhysicsStateCreated()
        || Mesh->bUseComplexAsSimpleCollision || Mesh->GetGenerateOverlapEvents()
        || Mesh->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics || Mesh->GetCollisionObjectType()!=ECC_WorldStatic
        || Mesh->GetCollisionResponseToChannels()!=FCollisionResponseContainer(ECR_Block)
        || !Mesh->GetComponentLocation().Equals(FVector(Member.Cell)*SFConstructionGeometry::CellCentimetres)
        || !Mesh->GetComponentScale().Equals(FVector::OneVector) || !Mesh->GetComponentQuat().Equals(FQuat::Identity)
        || !SFConstructionGeometry::Build(Member.Shape,Member.Rotation,Solid)) { return false; }
    const auto* Body=const_cast<UProceduralMeshComponent*>(Mesh)->GetBodySetup();
    if (!Body || !Body->bCreatedPhysicsMeshes || Body->bFailedToCreatePhysicsMeshes || Body->CollisionTraceFlag!=CTF_UseSimpleAndComplex
        || Body->AggGeom.GetElementCount()!=1 || Body->AggGeom.ConvexElems.Num()!=1) { return false; }
    const auto& Convex=Body->AggGeom.ConvexElems[0];
    const auto& Physics=Convex.GetChaosConvexMesh();
    return Physics.IsValid() && Convex.GetTransform().Equals(FTransform::Identity,1.e-12)
        && MatchesVertices(Convex.VertexData,Solid.Vertices) && MatchesVertices(Physics->GetVertices(),Solid.Vertices);
}
