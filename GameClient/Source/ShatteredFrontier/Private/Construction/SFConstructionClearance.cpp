#include "Construction/SFConstructionClearance.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Construction/SFConstructionSandboxState.h"
#include "Chaos/Convex.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

using SFConstructionClearance::EResult;

namespace
{
bool ValidBox(const FBox& Box)
{
    return Box.IsValid && !Box.Min.ContainsNaN() && !Box.Max.ContainsNaN()
        && Box.Min.X < Box.Max.X && Box.Min.Y < Box.Max.Y && Box.Min.Z < Box.Max.Z;
}
bool DoorProjectionsAvailable(UWorld* World)
{
    for (TActorIterator<ASFConstructionSandboxDoorProjection> It(World); It; ++It)
    { if (It->IsActive() && !It->IsProjectionIntact()) { return false; } }
    return true;
}
SFConstructionClearance::EResult Admit(UPrimitiveComponent* Component, bool& bSkip,
    const ASFConstructionSandboxDoorProjection* OwnOldDoor = nullptr)
{
    using namespace SFConstructionClearance;
    bSkip = false;
    if (!IsValid(Component)) { return EResult::Unavailable; }
    if (const auto* Door = Cast<ASFConstructionSandboxDoorProjection>(Component->GetOwner()))
    {
        bool bKnownCollider = false;
        for (int32 Index = 0; Index < 5; ++Index) { bKnownCollider |= Door->GetCollider(Index) == Component; }
        if (!Door->IsActive() && bKnownCollider)
        {
            if (!Door->IsProjectionIntact()) { return EResult::Unavailable; }
            bSkip = true; return EResult::Clear;
        }
        if (Door->IsActive() && !Door->IsProjectionIntact()) { return EResult::Unavailable; }
        if (Door == OwnOldDoor && bKnownCollider) { bSkip = true; return EResult::Clear; }
    }
    if (const auto* Preview = Cast<ASFConstructionSandboxPreview>(Component->GetOwner()))
    { if (Preview->IsStagedComponent(Component)) { bSkip = true; return EResult::Clear; } }
    if (!Component->IsQueryCollisionEnabled()) { bSkip = true; return EResult::Clear; }
    UBodySetup* Setup = Component->GetBodySetup();
    if (!Setup || Setup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple
        || Setup->AggGeom.GetElementCount() == 0 || Component->IsA<USkeletalMeshComponent>())
    { return EResult::Unavailable; }
    return EResult::Clear;
}
}

bool FSFConstructionReplacementSet::Validate(UWorld* World) const
{
    return IsInGameThread() && IsValid(World) && Owner.IsValid() && Owner->GetWorld() == World
        && Owner->IsReplacementQueryCurrent(*this);
}

bool FSFConstructionReplacementSet::Contains(const UPrimitiveComponent* Component) const
{
    for (const auto& Entry : Components) { if (Entry.Get() == Component) { return true; } }
    return false;
}

static SFConstructionClearance::EResult EvaluateSolidInternal(UWorld* World, int32 Shape,
    int32 Rotation, const FVector& CellCorner, const FSFConstructionReplacementSet* Replacement)
{
    FSFConstructionSolid Solid;
    if (!IsInGameThread() || !IsValid(World) || !FMath::IsFinite(CellCorner.X)
        || !FMath::IsFinite(CellCorner.Y) || !FMath::IsFinite(CellCorner.Z)
        || !SFConstructionGeometry::Build(Shape, Rotation, Solid) || !DoorProjectionsAvailable(World)
        || (Replacement && !Replacement->Validate(World))) { return EResult::Unavailable; }

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
        bool bSkip;
        if (Admit(Component,bSkip) == EResult::Unavailable) { return EResult::Unavailable; }
        if (bSkip || (Replacement && Replacement->Contains(Component))) { continue; }
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

static SFConstructionClearance::EResult EvaluateBoxInternal(UWorld* World, const FBox& Bounds,
    const ASFConstructionSandboxDoorProjection* OwnOldDoor, const FSFConstructionReplacementSet* Replacement)
{
    if (!IsInGameThread() || !IsValid(World) || !ValidBox(Bounds) || !DoorProjectionsAvailable(World)
        || (Replacement && !Replacement->Validate(World))
        || (OwnOldDoor && (OwnOldDoor->GetWorld() != World || !OwnOldDoor->IsActive() || !OwnOldDoor->IsProjectionIntact()))) { return EResult::Unavailable; }
    TArray<FOverlapResult> Candidates;
    const FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllObjects);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionDoorReservation),false);
    const auto Shape = FCollisionShape::MakeBox(Bounds.GetExtent());
    World->OverlapMultiByObjectType(Candidates,Bounds.GetCenter(),FQuat::Identity,Objects,Shape,Query);
    for (const auto& Candidate : Candidates)
    {
        auto* Component = Candidate.GetComponent();
        bool bSkip;
        if (Admit(Component,bSkip,OwnOldDoor) == EResult::Unavailable) { return EResult::Unavailable; }
        if (bSkip || (Replacement && Replacement->Contains(Component))) { continue; }
        FBodyInstance* Body = Component->GetBodyInstance(NAME_None,true,Candidate.GetItemIndex());
        if (!Body || !Body->IsValidBodyInstance()) { return EResult::Unavailable; }
        FMTDResult Penetration;
        const bool bHit = Body->OverlapTest(Bounds.GetCenter(),FQuat::Identity,Shape,&Penetration,false);
        if (!FMath::IsFinite(Penetration.Distance)) { return EResult::Unavailable; }
        // Box-vs-convex GJK reports ~1e-14 cm at exact shared floor contact.
        // This numerical zero guard does not inset the reservation or omit bodies.
        constexpr double NumericalContactZeroCm = 1.e-9;
        if (bHit && Penetration.Distance > NumericalContactZeroCm) { return EResult::Blocked; }
    }
    return EResult::Clear;
}

SFConstructionClearance::EResult SFConstructionClearance::Evaluate(UWorld* World, int32 Shape,
    int32 Rotation, const FVector& CellCorner)
{ return EvaluateSolidInternal(World,Shape,Rotation,CellCorner,nullptr); }

SFConstructionClearance::EResult SFConstructionClearance::EvaluateReplacing(UWorld* World, int32 Shape,
    int32 Rotation, const FVector& CellCorner, const FSFConstructionReplacementSet& Replacement)
{ return EvaluateSolidInternal(World,Shape,Rotation,CellCorner,&Replacement); }

SFConstructionClearance::EResult SFConstructionClearance::EvaluateBox(UWorld* World, const FBox& Bounds,
    const ASFConstructionSandboxDoorProjection* OwnOldDoor)
{ return EvaluateBoxInternal(World,Bounds,OwnOldDoor,nullptr); }

SFConstructionClearance::EResult SFConstructionClearance::EvaluateBoxReplacing(UWorld* World,
    const FBox& Bounds, const FSFConstructionReplacementSet& Replacement)
{ return EvaluateBoxInternal(World,Bounds,nullptr,&Replacement); }

SFConstructionClearance::EResult SFConstructionClearance::IntersectsReservation(int32 Shape, int32 Rotation,
    const FVector& CellCorner, const FBox& Reservation)
{
    FSFConstructionSolid Solid;
    if (CellCorner.ContainsNaN() || !ValidBox(Reservation) || !SFConstructionGeometry::Build(Shape,Rotation,Solid))
    { return EResult::Unavailable; }
    for (auto& Vertex : Solid.Vertices) { Vertex += CellCorner; }
    TArray<FVector> Axes = {FVector(1,0,0),FVector(0,1,0),FVector(0,0,1)};
    const FVector BoxAxes[] = {FVector(1,0,0),FVector(0,1,0),FVector(0,0,1)};
    for (const auto& Face : Solid.Faces)
    {
        Axes.Add(FVector::CrossProduct(Solid.Vertices[Face[1]]-Solid.Vertices[Face[0]],Solid.Vertices[Face[2]]-Solid.Vertices[Face[0]]));
        for (int32 I = 0; I < Face.Num(); ++I)
        {
            const FVector Edge = Solid.Vertices[Face[(I+1)%Face.Num()]]-Solid.Vertices[Face[I]];
            for (const FVector& Axis : BoxAxes) { Axes.Add(FVector::CrossProduct(Edge,Axis)); }
        }
    }
    for (const FVector& Axis : Axes)
    {
        if (Axis.IsZero()) { continue; }
        double Min = TNumericLimits<double>::Max(), Max = TNumericLimits<double>::Lowest();
        for (const FVector& Vertex : Solid.Vertices)
        { const double P = FVector::DotProduct(Vertex,Axis); Min = FMath::Min(Min,P); Max = FMath::Max(Max,P); }
        const double Centre = FVector::DotProduct(Reservation.GetCenter(),Axis);
        const double Radius = FVector::DotProduct(Reservation.GetExtent(),Axis.GetAbs());
        // Exact pilot lattice/quarter-turn coordinates; positive volume only.
        if (Max <= Centre-Radius || Min >= Centre+Radius) { return EResult::Clear; }
    }
    return EResult::Blocked;
}
