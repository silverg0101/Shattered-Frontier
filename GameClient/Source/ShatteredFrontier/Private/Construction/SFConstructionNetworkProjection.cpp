#include "SFConstructionNetworkProjection.h"
#include "Construction/SFConstructionNetwork.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
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

bool CanonicalBoxAsset(const UStaticMesh* Asset)
{
    if (!Asset || Asset->GetPathName()!=TEXT("/Engine/BasicShapes/Cube.Cube")) { return false; }
    const auto* Body=Asset->GetBodySetup();
    if (!Body || Body->CollisionTraceFlag==CTF_UseComplexAsSimple || Body->AggGeom.GetElementCount()!=1 || Body->AggGeom.BoxElems.Num()!=1) { return false; }
    const auto& Box=Body->AggGeom.BoxElems[0];
    return FMath::IsNearlyEqual(Box.X,100.f) && FMath::IsNearlyEqual(Box.Y,100.f) && FMath::IsNearlyEqual(Box.Z,100.f)
        && Box.Center.IsNearlyZero() && Box.Rotation.IsNearlyZero();
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

void SFConstructionNetworkProjection::DiscardDoor(FSFConstructionNetworkDoorProjection& Projection)
{
    for (auto& Box : Projection.Boxes) { if (Box) { Box->DestroyComponent(); } }
    Projection.Boxes.Reset();
}

bool SFConstructionNetworkProjection::StageDoor(ASFConstructionNetworkState* Owner, UStaticMesh* Asset, const FSFConstructionNetworkDoor& Door, FSFConstructionNetworkDoorProjection& Out)
{
    FSFConstructionDoorProjectionProfile Profile;
    if (!IsInGameThread() || !IsValid(Owner) || Owner->GetClass()!=ASFConstructionNetworkState::StaticClass()
        || !Owner->GetActorTransform().Equals(FTransform::Identity) || !CanonicalBoxAsset(Asset)
        || !ASFConstructionSandboxDoorProjection::BuildProfile(Door.Origin,Door.Yaw,Door.bOpen,Profile)) { return false; }
    FSFConstructionNetworkDoorProjection Candidate;
    for (const auto& B : Profile.Boxes)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(Owner);
        Mesh->SetupAttachment(Owner->GetRootComponent());
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(Asset);
        Mesh->SetRelativeTransform(FTransform(B.Rotation,B.Centre,B.Extent*0.02)*Profile.Transform);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCollisionObjectType(ECC_WorldStatic);
        Mesh->SetCollisionResponseToAllChannels(ECR_Block);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetHiddenInGame(true);
        Candidate.Boxes.Add(Mesh);
        Mesh->RegisterComponent();
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
    if (!MatchesDoor(Owner,Door,Candidate)) { DiscardDoor(Candidate); return false; }
    Out=MoveTemp(Candidate);
    return true;
}

bool SFConstructionNetworkProjection::MatchesDoor(const ASFConstructionNetworkState* Owner, const FSFConstructionNetworkDoor& Door, const FSFConstructionNetworkDoorProjection& Projection)
{
    FSFConstructionDoorProjectionProfile Profile;
    if (!IsInGameThread() || !IsValid(Owner) || !Owner->GetActorTransform().Equals(FTransform::Identity)
        || !ASFConstructionSandboxDoorProjection::BuildProfile(Door.Origin,Door.Yaw,Door.bOpen,Profile) || Projection.Boxes.Num()!=Profile.Boxes.Num()) { return false; }
    for (int32 I=0; I<Profile.Boxes.Num(); ++I)
    {
        auto* Mesh=Projection.Boxes[I].Get();
        const auto& B=Profile.Boxes[I];
        const FTransform Expected=FTransform(B.Rotation,B.Centre,B.Extent*0.02)*Profile.Transform;
        if (!IsValid(Mesh) || Mesh->GetOwner()!=Owner || Mesh->GetAttachParent()!=Owner->GetRootComponent()
            || !CanonicalBoxAsset(Mesh->GetStaticMesh()) || !Mesh->IsRegistered() || !Mesh->IsPhysicsStateCreated()
            || Mesh->GetGenerateOverlapEvents() || Mesh->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics
            || Mesh->GetCollisionObjectType()!=ECC_WorldStatic || Mesh->GetCollisionResponseToChannels()!=FCollisionResponseContainer(ECR_Block)
            || !Mesh->GetComponentLocation().Equals(Expected.GetLocation(),1.e-9)
            || !Mesh->GetComponentQuat().Equals(Expected.GetRotation(),1.e-12) || !Mesh->GetComponentScale().Equals(Expected.GetScale3D(),1.e-12)) { return false; }
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkDoorProjection),false);
        const FVector Centre=Expected.GetLocation(),Direction=Expected.GetRotation().RotateVector(FVector::ForwardVector);
        if (!Mesh->LineTraceComponent(Hit,Centre+Direction*(B.Extent.X+10.0),Centre,Query) || Hit.GetComponent()!=Mesh) { return false; }
    }
    return true;
}

bool SFConstructionNetworkProjection::IsDoorMotionClear(const ASFConstructionNetworkState* Owner, const FSFConstructionNetworkDoor& Door, const FSFConstructionNetworkDoorProjection& Projection)
{
    FSFConstructionDoorProjectionProfile Profile;
    if (!IsInGameThread() || !IsValid(Owner) || !Owner->GetWorld() || !Owner->OwnsDoorProjection(Door,Projection)
        || !ASFConstructionSandboxDoorProjection::BuildProfile(Door.Origin,Door.Yaw,Door.bOpen,Profile)) { return false; }
    const FBox& Bounds=Profile.Motion;
    const auto Shape=FCollisionShape::MakeBox(Bounds.GetExtent());
    TArray<FOverlapResult> Candidates;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkDoorMotion),false);
    Owner->GetWorld()->OverlapMultiByObjectType(Candidates,Bounds.GetCenter(),FQuat::Identity,FCollisionObjectQueryParams::AllObjects,Shape,Query);
    for (const auto& Candidate : Candidates)
    {
        auto* Component=Candidate.GetComponent();
        if (!IsValid(Component)) { return false; }
        bool bOwn=false;
        for (const auto& Box : Projection.Boxes) { bOwn |= Box.Get()==Component; }
        if (bOwn) { continue; } // Exact proven target components, never its whole actor.
        if (!Component->IsQueryCollisionEnabled() || Component->GetCollisionResponseToChannel(ECC_WorldStatic)!=ECR_Block) { continue; }
        const auto* Setup=Component->GetBodySetup();
        if (!Setup || Setup->GetCollisionTraceFlag()==CTF_UseComplexAsSimple || Setup->AggGeom.GetElementCount()==0
            || Component->IsA<USkeletalMeshComponent>()) { return false; }
        auto* Body=Component->GetBodyInstance(NAME_None,true,Candidate.GetItemIndex());
        if (!Body || !Body->IsValidBodyInstance()) { return false; }
        FMTDResult Penetration;
        const bool bHit=Body->OverlapTest(Bounds.GetCenter(),FQuat::Identity,Shape,&Penetration,false);
        if (!FMath::IsFinite(Penetration.Distance)) { return false; }
        // Same numerical zero guard as the shared strict simple-body box query;
        // exact floor contact is legal, with no inset or omitted foreign body.
        if (bHit && Penetration.Distance>1.e-9) { return false; }
    }
    return true;
}
