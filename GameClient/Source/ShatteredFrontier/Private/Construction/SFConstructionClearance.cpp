#include "Construction/SFConstructionClearance.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandbox.h"
#include "Chaos/Convex.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

SFConstructionClearance::EResult SFConstructionClearance::Evaluate(UWorld* World, int32 Shape,
    int32 Rotation, const FVector& CellCorner)
{
    FSFConstructionSolid Solid;
    if (!IsInGameThread() || !IsValid(World) || !FMath::IsFinite(CellCorner.X)
        || !FMath::IsFinite(CellCorner.Y) || !FMath::IsFinite(CellCorner.Z)
        || !SFConstructionGeometry::Build(Shape, Rotation, Solid)) { return EResult::Unavailable; }

    // Only broadphase uses bounds. Preserve exact canonical vertices and faces,
    // including the sloped plane; no inset, inflation or render mesh dependency.
    const FBox Bounds(Solid.Vertices);
    const FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllObjects);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionCanonicalClearance), false);
    TArray<FOverlapResult> Candidates;
    World->OverlapMultiByObjectType(Candidates, CellCorner + Bounds.GetCenter(), FQuat::Identity,
        Objects, FCollisionShape::MakeBox(Bounds.GetExtent()), Query);

    TArray<Chaos::FConvex::FVec3Type> Vertices;
    TArray<Chaos::FConvex::FPlaneType> Planes;
    for (const FVector& Vertex : Solid.Vertices) { Vertices.Add(Chaos::FConvex::FVec3Type(Vertex)); }
    for (const auto& Face : Solid.Faces)
    {
        const FVector Origin = Solid.Vertices[Face[0]];
        const FVector Normal = FVector::CrossProduct(Solid.Vertices[Face[1]] - Origin,
            Solid.Vertices[Face[2]] - Origin).GetSafeNormal();
        Planes.Emplace(Chaos::FConvex::FVec3Type(Origin), Chaos::FConvex::FVec3Type(Normal));
    }
    TArray<TArray<int32>> Faces = Solid.Faces;
    const Chaos::FConvex Convex(MoveTemp(Planes), MoveTemp(Faces), MoveTemp(Vertices));
    const auto Geometry = FPhysicsInterface::GetGeometryCollection(Convex);
    const FTransform Transform(FQuat::Identity, CellCorner);

    for (const FOverlapResult& Candidate : Candidates)
    {
        UPrimitiveComponent* Component = Candidate.GetComponent();
        if (!IsValid(Component)) { return EResult::Unavailable; }
        if (const auto* Preview = Cast<ASFConstructionSandboxPreview>(Component->GetOwner()))
        {
            // Staging retains cooked physics but ignores all responses. Only the
            // active canonical projection can participate in placement clearance.
            if (Preview->GetMesh() != Component) { continue; }
        }
        if (!Component->IsQueryCollisionEnabled()) { continue; }
        UBodySetup* Setup = Component->GetBodySetup();
        // Complex triangle collision and skeletal multi-body selection need a
        // separate validated adapter. Never silently interpret either as empty.
        if (!Setup || Setup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple
            || Setup->AggGeom.GetElementCount() == 0 || Component->IsA<USkeletalMeshComponent>())
        { return EResult::Unavailable; }
        FBodyInstance* Body = Component->GetBodyInstance(NAME_None, true, Candidate.GetItemIndex());
        if (!Body || !Body->IsValidBodyInstance()) { return EResult::Unavailable; }
        bool bRead = false, bOverlap = false;
        FMTDResult Penetration;
        FPhysicsCommand::ExecuteRead(Body->GetActorReferenceWithWelding(), [&](const FPhysicsActorHandle& Actor)
        {
            bRead = true;
            // Engine narrowphase iterates the body's compound simple shapes,
            // including their actual scale/rotation; Thickness is zero.
            bOverlap = FPhysicsInterface::Overlap_Geom(Body, Geometry, Transform, &Penetration, false);
        });
        if (!bRead || !FMath::IsFinite(Penetration.Distance)) { return EResult::Unavailable; }
        if (bOverlap && Penetration.Distance > 0) { return EResult::Blocked; }
    }
    return EResult::Clear;
}
