#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Construction/SFConstructionSandbox.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

namespace
{
// Double-precision scene attachment/rotation arithmetic is not bit-exact after
// transform composition. These numerical guards are not placement tolerances.
bool MatchesTransform(const FTransform& Actual, const FTransform& Expected)
{
    return Actual.GetTranslation().Equals(Expected.GetTranslation(),1.e-9)
        && Actual.GetRotation().Equals(Expected.GetRotation(),1.e-12)
        && Actual.GetScale3D().Equals(Expected.GetScale3D(),1.e-12);
}
FVector QuarterTurn(const FVector& P, int32 Yaw)
{
    switch (Yaw)
    {
    case 1: return FVector(-P.Y,P.X,P.Z);
    case 2: return FVector(-P.X,-P.Y,P.Z);
    case 3: return FVector(P.Y,-P.X,P.Z);
    default: return P;
    }
}
FBox Bounds(const FBox& Local, const FVector& Corner, int32 Yaw)
{
    FBox Result(ForceInit);
    for (int32 X = 0; X < 2; ++X)
    for (int32 Y = 0; Y < 2; ++Y)
    for (int32 Z = 0; Z < 2; ++Z)
    { Result += Corner + QuarterTurn(FVector(X ? Local.Max.X : Local.Min.X, Y ? Local.Max.Y : Local.Min.Y, Z ? Local.Max.Z : Local.Min.Z), Yaw); }
    return Result;
}
void AppendVisualBox(const FSFConstructionDoorBoxProfile& Box, TArray<FVector>& Vertices,
    TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FVector2D>& UV)
{
    const FVector Unit[] = {{-1,-1,-1},{-1,-1,1},{-1,1,-1},{-1,1,1},{1,-1,-1},{1,-1,1},{1,1,-1},{1,1,1}};
    const int32 Faces[][4] = {{0,1,3,2},{0,2,6,4},{0,4,5,1},{1,5,7,3},{2,3,7,6},{4,6,7,5}};
    for (const auto& Face : Faces)
    {
        const int32 Base = Vertices.Num();
        const FVector A = Box.Rotation.RotateVector(Unit[Face[0]] * Box.Extent);
        const FVector B = Box.Rotation.RotateVector(Unit[Face[1]] * Box.Extent);
        const FVector C = Box.Rotation.RotateVector(Unit[Face[2]] * Box.Extent);
        const FVector Normal = FVector::CrossProduct(B-A,C-A).GetSafeNormal();
        for (int32 Index = 0; Index < 4; ++Index)
        {
            Vertices.Add(Box.Centre + Box.Rotation.RotateVector(Unit[Face[Index]] * Box.Extent));
            Normals.Add(Normal);
            UV.Add(FVector2D(Index == 2 || Index == 3 ? 1 : 0, Index == 1 || Index == 2 ? 1 : 0));
        }
        Triangles.Append({Base,Base+2,Base+1,Base,Base+3,Base+2});
    }
}
}

ASFConstructionSandboxDoorProjection::ASFConstructionSandboxDoorProjection()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DoorEnvelopeCorner")));
    SetActorHiddenInGame(true);
    bReplicates = false;
    PrimaryActorTick.bCanEverTick = false;
}

bool ASFConstructionSandboxDoorProjection::IsAllowedContext() const
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    return IsInGameThread() && HasAuthority() && !IsActorBeingDestroyed() && GetWorld()
        && GetNetMode() == NM_Standalone && GetWorld()->GetAuthGameMode<ASFConstructionSandboxGameMode>();
#else
    return false;
#endif
}

bool ASFConstructionSandboxDoorProjection::BuildProfile(const FIntVector& OriginCell, int32 YawQuarterTurns,
    bool bOpen, FSFConstructionDoorProjectionProfile& Out)
{
    if (YawQuarterTurns < 0 || YawQuarterTurns >= 4
        || OriginCell.X < -20 || OriginCell.X >= 20 || OriginCell.Y < -20 || OriginCell.Y >= 20
        || OriginCell.Z < 0 || OriginCell.Z >= 20) { return false; }
    const FVector Corner = FVector(OriginCell) * 50;
    const FBox NewEnvelope = Bounds(FBox(FVector::ZeroVector,FVector(100,50,200)), Corner, YawQuarterTurns);
    if (NewEnvelope.Min.X < -1000 || NewEnvelope.Max.X > 1000 || NewEnvelope.Min.Y < -1000
        || NewEnvelope.Max.Y > 1000 || NewEnvelope.Min.Z < 0 || NewEnvelope.Max.Z > 1000) { return false; }

    TArray<FSFConstructionDoorBoxProfile> CandidateBoxes = {
        {TEXT("LeftPost"), FVector(2.5,25,100), FVector(2.5,25,100), FQuat::Identity},
        {TEXT("RightPost"), FVector(97.5,25,100), FVector(2.5,25,100), FQuat::Identity},
        {TEXT("Sill"), FVector(50,25,2.5), FVector(45,25,2.5), FQuat::Identity},
        {TEXT("Lintel"), FVector(50,25,197.5), FVector(45,25,2.5), FQuat::Identity},
        {TEXT("Leaf"), bOpen ? FVector(8,72.5,100) : FVector(50,24.5,100), FVector(44.5,2.5,94.5),
            bOpen ? FQuat(FRotator(0,90,0)) : FQuat::Identity}
    };
    FSFConstructionDoorProjectionProfile Candidate;
    Candidate.Transform = FTransform(FRotator(0,YawQuarterTurns * 90,0),Corner,FVector::OneVector);
    Candidate.Boxes = MoveTemp(CandidateBoxes);
    Candidate.Envelope = NewEnvelope;
    Candidate.Motion = Bounds(FBox(FVector::ZeroVector,FVector(100,120,200)),Corner,YawQuarterTurns);
    Candidate.ApproachFront = Bounds(FBox(FVector(0,-100,0),FVector(100,0,200)),Corner,YawQuarterTurns);
    Candidate.ApproachBack = Bounds(FBox(FVector(0,50,0),FVector(100,150,200)),Corner,YawQuarterTurns);
    Out = MoveTemp(Candidate);
    return true;
}
bool ASFConstructionSandboxDoorProjection::Configure(const FIntVector& OriginCell, int32 YawQuarterTurns, bool bOpen)
{
    if (!IsAllowedContext() || bConfigured || YawQuarterTurns < 0 || YawQuarterTurns >= 4
        || OriginCell.X < -20 || OriginCell.X >= 20 || OriginCell.Y < -20 || OriginCell.Y >= 20
        || OriginCell.Z < 0 || OriginCell.Z >= 20 || !GetActorScale3D().Equals(FVector::OneVector, 0)
        || !GetActorQuat().Equals(FQuat::Identity, 0)) { return false; }
    FSFConstructionDoorProjectionProfile Profile;
    if (!BuildProfile(OriginCell,YawQuarterTurns,bOpen,Profile)) { return false; }
    const auto& CandidateBoxes = Profile.Boxes;
    TArray<UBoxComponent*> CandidateColliders;
    UProceduralMeshComponent* CandidateVisual = NewObject<UProceduralMeshComponent>(this);
    CandidateVisual->SetupAttachment(GetRootComponent());
    CandidateVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CandidateVisual->SetGenerateOverlapEvents(false);
    CandidateVisual->SetVisibility(false);
    TArray<FVector> Vertices, Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UV;
    for (const auto& Box : CandidateBoxes) { AppendVisualBox(Box,Vertices,Triangles,Normals,UV); }
    CandidateVisual->CreateMeshSection(0,Vertices,Triangles,Normals,UV,TArray<FColor>(),TArray<FProcMeshTangent>(),false);
    CandidateVisual->RegisterComponent();
    auto Discard = [&]()
    {
        for (auto* Box : CandidateColliders) { Box->DestroyComponent(); }
        CandidateVisual->DestroyComponent();
    };
    if (!CandidateVisual->IsRegistered()) { Discard(); return false; }
    for (const auto& BoxProfile : CandidateBoxes)
    {
        UBoxComponent* Box = NewObject<UBoxComponent>(this);
        CandidateColliders.Add(Box);
        Box->SetupAttachment(GetRootComponent());
        Box->SetRelativeTransform(FTransform(BoxProfile.Rotation,BoxProfile.Centre));
        Box->SetBoxExtent(BoxProfile.Extent);
        Box->SetGenerateOverlapEvents(false);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        UBodySetup* Body = Box->GetBodySetup();
        if (!Body) { Discard(); return false; }
        Body->CollisionTraceFlag = CTF_UseSimpleAndComplex;
        Box->RegisterComponent();
        if (!Box->IsRegistered() || !Box->IsPhysicsStateCreated() || !Body || Body->AggGeom.BoxElems.Num() != 1)
        { Discard(); return false; }
    }
    ConfiguredTransform = Profile.Transform;
    SetActorTransform(ConfiguredTransform, false, nullptr, ETeleportType::TeleportPhysics);
    Boxes = MoveTemp(Profile.Boxes);
    for (auto* Box : CandidateColliders) { Colliders.Add(Box); }
    Visual = CandidateVisual;
    ConfiguredCell = OriginCell;
    ConfiguredYaw = YawQuarterTurns;
    bConfiguredOpen = bOpen;
    Envelope = Profile.Envelope;
    Motion = Profile.Motion;
    ApproachFront = Profile.ApproachFront;
    ApproachBack = Profile.ApproachBack;
    bConfigured = true;
    return true;
}

const UBoxComponent* ASFConstructionSandboxDoorProjection::GetCollider(int32 Index) const
{
    return Colliders.IsValidIndex(Index) ? Colliders[Index].Get() : nullptr;
}

bool ASFConstructionSandboxDoorProjection::IsProjectionIntact() const
{
    if (!IsAllowedContext() || !bConfigured || !MatchesTransform(GetActorTransform(),ConfiguredTransform)
        || !Visual || !Visual->IsRegistered() || Visual->GetAttachParent() != GetRootComponent()
        || Visual->GetCollisionEnabled() != ECollisionEnabled::NoCollision
        || !MatchesTransform(Visual->GetRelativeTransform(),FTransform::Identity) || Colliders.Num() != 5 || Boxes.Num() != 5)
    { return false; }
    for (int32 Index = 0; Index < Colliders.Num(); ++Index)
    {
        const auto* Box = Colliders[Index].Get();
        const auto& Profile = Boxes[Index];
        if (!IsValid(Box) || !Box->IsRegistered() || !Box->IsPhysicsStateCreated()
            || Box->GetAttachParent() != GetRootComponent()
            || !Box->GetUnscaledBoxExtent().Equals(Profile.Extent, 0)
            || !MatchesTransform(Box->GetRelativeTransform(),FTransform(Profile.Rotation,Profile.Centre))
            || Box->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics || Box->GetGenerateOverlapEvents()
            || Box->GetCollisionObjectType() != ECC_WorldStatic) { return false; }
        const UBodySetup* Body = const_cast<UBoxComponent*>(Box)->GetBodySetup();
        if (!Body || Body->CollisionTraceFlag != CTF_UseSimpleAndComplex || Body->AggGeom.BoxElems.Num() != 1) { return false; }
        const auto& PhysicalBox = Body->AggGeom.BoxElems[0];
        if (PhysicalBox.X != 2*Profile.Extent.X || PhysicalBox.Y != 2*Profile.Extent.Y || PhysicalBox.Z != 2*Profile.Extent.Z
            || !PhysicalBox.Center.Equals(FVector::ZeroVector,0) || !PhysicalBox.Rotation.Equals(FRotator::ZeroRotator,0)) { return false; }
        // ECC_MAX includes nonserialized transient flags; response storage holds
        // only the real collision channels. Compare the container itself.
        if (Box->GetCollisionResponseToChannels() != FCollisionResponseContainer(bActive ? ECR_Block : ECR_Ignore)) { return false; }
    }
    return true;
}

bool ASFConstructionSandboxDoorProjection::CanActivate() const
{
    return !bActive && IsProjectionIntact();
}

bool ASFConstructionSandboxDoorProjection::Activate()
{
    if (!CanActivate()) { return false; }
    for (auto& Box : Colliders) { Box->SetCollisionResponseToAllChannels(ECR_Block); }
    bActive = true;
    Visual->SetVisibility(true);
    SetActorHiddenInGame(false);
    return true;
}
