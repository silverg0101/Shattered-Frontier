#include "Construction/SFConstructionNetwork.h"
#include "SFConstructionNetworkProjection.h"
#include "ProceduralMeshComponent.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandboxFoundation.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Construction/SFConstructionSupport.h"
#include "Construction/SFConstructionDoorSupport.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Construction/SFConstructionClearance.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Hash/Blake3.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
const TCHAR* PlacementResultText(ESFConstructionNetworkResult Result)
{
    switch (Result)
    {
    case ESFConstructionNetworkResult::Accepted: return TEXT("Placed.");
    case ESFConstructionNetworkResult::Removed: return TEXT("Removed.");
    case ESFConstructionNetworkResult::NotFound: return TEXT("That block is no longer there. Aim again.");
    case ESFConstructionNetworkResult::InvalidPiece: return TEXT("Unknown construction piece or rotation.");
    case ESFConstructionNetworkResult::DoorBlocked: return TEXT("Doorway blocked. Step out of the swing space.");
    case ESFConstructionNetworkResult::DoorOpened: return TEXT("Door opened.");
    case ESFConstructionNetworkResult::DoorClosed: return TEXT("Door closed.");
    case ESFConstructionNetworkResult::DoorRemoved: return TEXT("Door removed.");
    case ESFConstructionNetworkResult::PermissionDenied: return TEXT("Observer only: building permission required.");
    case ESFConstructionNetworkResult::Stale: return TEXT("The structure changed. Aim again and retry.");
    case ESFConstructionNetworkResult::InvalidCell: return TEXT("Outside the construction area.");
    case ESFConstructionNetworkResult::OutOfReach: return TEXT("Out of reach. Move closer.");
    case ESFConstructionNetworkResult::Occluded: return TEXT("Blocked view. Move so you can see the placement.");
    case ESFConstructionNetworkResult::Occupied: return TEXT("Space occupied. Aim at an empty neighboring cell.");
    case ESFConstructionNetworkResult::Unsupported: return TEXT("Needs support. Keep every remaining block supported.");
    case ESFConstructionNetworkResult::LimitReached: return TEXT("Construction member limit reached.");
    case ESFConstructionNetworkResult::ProjectionFailed: return TEXT("Construction collision unavailable. Placement was not applied.");
    default: return TEXT("Construction unavailable. Wait for the server and try again.");
    }
}

bool RayEnvelopeEntry(const FVector& Start, const FVector& End, const FBox& Box, double& OutT)
{
    if (!Box.IsValid || Start.ContainsNaN() || End.ContainsNaN() || (End-Start).ContainsNaN() || (End-Start).IsNearlyZero()) { return false; }
    const FVector Delta=End-Start;
    double Near=0, Far=1;
    for (int32 Axis=0; Axis<3; ++Axis)
    {
        if (FMath::Abs(Delta[Axis])<1.e-9)
        { if (Start[Axis]<Box.Min[Axis] || Start[Axis]>Box.Max[Axis]) { return false; } continue; }
        double A=(Box.Min[Axis]-Start[Axis])/Delta[Axis], B=(Box.Max[Axis]-Start[Axis])/Delta[Axis];
        if (A>B) { Swap(A,B); }
        Near=FMath::Max(Near,A); Far=FMath::Min(Far,B);
        if (Near>Far) { return false; }
    }
    OutT=Near; return true;
}

ASFConstructionNetworkState* FindState(UWorld* World)
{
    if (!World) { return nullptr; }
    for (TActorIterator<ASFConstructionNetworkState> It(World); It; ++It) { return *It; }
    return nullptr;
}

bool SupportsMembers(const TArray<FSFConstructionNetworkMember>& Members, const TArray<FSFConstructionNetworkDoor>& Doors)
{
    TArray<FSFConstructionSupportMember> Inputs;
    for (const auto& M : Members)
    { Inputs.Add({M.Cell,M.Shape,M.Rotation,1.0,SFConstructionSandboxFoundation::TouchesPlane(M.Cell,M.Shape,M.Rotation)}); }
    TArray<FSFConstructionSupportResult> Results;
    if (Doors.IsEmpty())
    { if (!SFConstructionSupport::Evaluate(Inputs,FSFConstructionSupportProfile(),Results)) { return false; } }
    else
    {
        TArray<FSFConstructionDoorSupportInput> DoorInputs;
        for (const auto& D : Doors) { DoorInputs.Add({D.Origin,D.Yaw}); }
        TArray<FSFConstructionDoorSupportResult> DoorResults;
        if (!SFConstructionDoorSupport::Evaluate(Inputs,DoorInputs,FSFConstructionDoorSupportProfile(),true,Results,DoorResults)) { return false; }
        for (const auto& R : DoorResults) { if (R.State==ESFConstructionSupportState::Unsupported) { return false; } }
    }
    for (const auto& R : Results) { if (R.State==ESFConstructionSupportState::Unsupported) { return false; } }
    return true;
}

}

bool FSFConstructionNetworkSnapshot::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    uint8 Version=5;
    uint64 WireRevision=Revision;
    uint16 Count=uint16(Members.Num());
    bOutSuccess=false;
    if (Ar.IsSaving() && (Members.Num()>4096 || Doors.Num()>64 || Revision<uint64(Members.Num()+Doors.Num()))) { return false; }
    Ar << Version; Ar << WireRevision; Ar.SerializeBits(&Count,13);
    if (Version!=5 || Count>4096 || WireRevision<uint64(Count)) { return false; }
    TArray<FSFConstructionNetworkMember> Received;
    TSet<FIntVector> Unique;
    for (uint16 I=0; I<Count; ++I)
    {
        FSFConstructionNetworkMember Member=Ar.IsSaving() ? Members[I] : FSFConstructionNetworkMember();
        const auto& C=Member.Cell;
        if (Ar.IsSaving() && (C.X < -20 || C.X >= 20 || C.Y < -20 || C.Y >= 20 || C.Z < 0 || C.Z >= 20
            || Member.Shape>=SFConstructionGeometry::ShapeCount || Member.Rotation>=SFConstructionGeometry::RotationCount)) { return false; }
        uint8 X=uint8(C.X+20),Y=uint8(C.Y+20),Z=uint8(C.Z);
        Ar.SerializeBits(&X,6); Ar.SerializeBits(&Y,6); Ar.SerializeBits(&Z,5);
        Ar.SerializeBits(&Member.Shape,2); Ar.SerializeBits(&Member.Rotation,5);
        Member.Cell=FIntVector(int32(X)-20,int32(Y)-20,int32(Z));
        if (Ar.IsError() || X>=40 || Y>=40 || Z>=20 || Member.Shape>=SFConstructionGeometry::ShapeCount
            || Member.Rotation>=SFConstructionGeometry::RotationCount || Unique.Contains(Member.Cell)) { return false; }
        Unique.Add(Member.Cell);
        if (Ar.IsLoading()) { Received.Add(Member); }
    }
    uint8 DoorCount=uint8(Doors.Num());
    Ar.SerializeBits(&DoorCount,7);
    if (Ar.IsError() || DoorCount>64 || WireRevision<uint64(Count)+DoorCount) { return false; }
    TArray<FSFConstructionNetworkDoor> ReceivedDoors;
    TSet<FIntVector> UniqueDoors;
    for (uint8 I=0; I<DoorCount; ++I)
    {
        auto D=Ar.IsSaving() ? Doors[I] : FSFConstructionNetworkDoor();
        FSFConstructionDoorProjectionProfile Profile;
        if (Ar.IsSaving() && !ASFConstructionSandboxDoorProjection::BuildProfile(D.Origin,D.Yaw,D.bOpen,Profile)) { return false; }
        uint8 X=uint8(D.Origin.X+20),Y=uint8(D.Origin.Y+20),Z=uint8(D.Origin.Z),Open=D.bOpen ? 1 : 0;
        Ar.SerializeBits(&X,6); Ar.SerializeBits(&Y,6); Ar.SerializeBits(&Z,5); Ar.SerializeBits(&D.Yaw,2); Ar.SerializeBits(&Open,1);
        D.Origin=FIntVector(int32(X)-20,int32(Y)-20,int32(Z)); D.bOpen=Open!=0;
        if (Ar.IsError() || X>=40 || Y>=40 || Z>=20 || UniqueDoors.Contains(D.Origin)
            || !ASFConstructionSandboxDoorProjection::BuildProfile(D.Origin,D.Yaw,D.bOpen,Profile)) { return false; }
        UniqueDoors.Add(D.Origin);
        if (Ar.IsLoading()) { ReceivedDoors.Add(D); }
    }
    if (Ar.IsError()) { return false; }
    if (Ar.IsLoading()) { Revision=WireRevision; Members=MoveTemp(Received); Doors=MoveTemp(ReceivedDoors); }
    bOutSuccess=true;
    return true;
}

ASFConstructionNetworkState::ASFConstructionNetworkState()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ConstructionOrigin")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Box(TEXT("/Engine/BasicShapes/Cube.Cube"));
    DoorBoxAsset=Box.Object;
}

void ASFConstructionNetworkState::BeginPlay()
{
    Super::BeginPlay();
    const auto* Mode = GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>();
    if (!HasAuthority() || !Mode || !Mode->IsAdmitted()) { return; }
    const auto* Settings = GetDefault<USFConstructionSandboxSettings>();
    FString Error;
    if (!Settings->Validate(Error)) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_CONFIGURATION_FAILED %s"),*Error); return; }
    ReachCentimetres = double(Settings->BuildReachMetres)*100.0;
    MaximumMembers = Settings->MaximumMembers;
    MaximumDoors = Settings->MaximumDoors;
    bConfigurationReady = true;
}

void ASFConstructionNetworkState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFConstructionNetworkState, Snapshot);
    DOREPLIFETIME(ASFConstructionNetworkState, bPairReady);
    DOREPLIFETIME(ASFConstructionNetworkState, ReachCentimetres);
    DOREPLIFETIME(ASFConstructionNetworkState, MaximumMembers);
    DOREPLIFETIME(ASFConstructionNetworkState, MaximumDoors);
}

void ASFConstructionNetworkState::AdmitPair()
{
    auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>() : nullptr;
    if (HasAuthority() && Mode && Mode->IsAdmitted() && Mode->GetNumPlayers() >= 2)
    { bPairReady = true; ForceNetUpdate(); UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_PAIR_READY Players=%d"),Mode->GetNumPlayers()); }
}

bool ASFConstructionNetworkState::Activate(const FSFConstructionNetworkSnapshot& Candidate)
{
    if (Candidate.Revision<=ProjectionRevision || Candidate.Members.Num()>4096 || Candidate.Doors.Num()>64 || Candidate.Revision<uint64(Candidate.Members.Num()+Candidate.Doors.Num())
        || !GetActorTransform().Equals(FTransform::Identity)) { return false; }
    TSet<FIntVector> Addresses;
    for (const auto& M : Candidate.Members)
    {
        const auto& C=M.Cell;
        if (C.X < -20 || C.X >= 20 || C.Y < -20 || C.Y >= 20 || C.Z < 0 || C.Z >= 20 || Addresses.Contains(C)
            || M.Shape>=SFConstructionGeometry::ShapeCount || M.Rotation>=SFConstructionGeometry::RotationCount) { return false; }
        Addresses.Add(C);
    }
    TSet<FIntVector> DoorAddresses;
    for (const auto& D : Candidate.Doors)
    {
        FSFConstructionDoorProjectionProfile Profile;
        if (DoorAddresses.Contains(D.Origin) || !ASFConstructionSandboxDoorProjection::BuildProfile(D.Origin,D.Yaw,D.bOpen,Profile)) { return false; }
        DoorAddresses.Add(D.Origin);
    }
    TMap<FIntVector,TObjectPtr<UProceduralMeshComponent>> Staged;
    for (const auto& M : Candidate.Members)
    {
        const auto* Old=ProjectedMembers.Find(M.Cell);
        if (Old && Old->Shape==M.Shape && Old->Rotation==M.Rotation && Meshes.Contains(M.Cell)
            && SFConstructionNetworkProjection::Matches(Meshes.FindRef(M.Cell),M)) { continue; }
        auto* Mesh=SFConstructionNetworkProjection::Stage(this,M);
        if (!Mesh)
        {
            for (const auto& Pair : Staged) { Pair.Value->DestroyComponent(); }
            return false;
        }
        Staged.Add(M.Cell,Mesh);
    }
    TMap<FIntVector,FSFConstructionNetworkDoorProjection> StagedDoors;
    for (const auto& D : Candidate.Doors)
    {
        const auto* Old=ProjectedDoors.Find(D.Origin);
        const auto* Projection=DoorProjections.Find(D.Origin);
        if (Old && Projection && Old->Yaw==D.Yaw && Old->bOpen==D.bOpen && SFConstructionNetworkProjection::MatchesDoor(this,D,*Projection)) { continue; }
        FSFConstructionNetworkDoorProjection ProjectionCandidate;
        if (!SFConstructionNetworkProjection::StageDoor(this,DoorBoxAsset,D,ProjectionCandidate))
        {
            for (const auto& Pair : Staged) { Pair.Value->DestroyComponent(); }
            for (auto& Pair : StagedDoors) { SFConstructionNetworkProjection::DiscardDoor(Pair.Value); }
            return false;
        }
        StagedDoors.Add(D.Origin,MoveTemp(ProjectionCandidate));
    }
    // Every replacement is ready before an old component can be retired.
    for (auto It=Meshes.CreateIterator(); It; ++It)
    {
        if (!Addresses.Contains(It.Key()) || Staged.Contains(It.Key()))
        { It.Value()->DestroyComponent(); It.RemoveCurrent(); }
    }
    for (const auto& Pair : Staged) { Meshes.Add(Pair.Key,Pair.Value); Pair.Value->SetHiddenInGame(false); }
    ProjectedMembers.Reset();
    for (const auto& M : Candidate.Members) { ProjectedMembers.Add(M.Cell,M); }
    for (auto It=DoorProjections.CreateIterator(); It; ++It)
    {
        if (!DoorAddresses.Contains(It.Key()) || StagedDoors.Contains(It.Key()))
        { SFConstructionNetworkProjection::DiscardDoor(It.Value()); It.RemoveCurrent(); }
    }
    for (auto& Pair : StagedDoors)
    {
        for (auto& Box : Pair.Value.Boxes) { Box->SetHiddenInGame(false); }
        DoorProjections.Add(Pair.Key,MoveTemp(Pair.Value));
    }
    ProjectedDoors.Reset();
    for (const auto& D : Candidate.Doors) { ProjectedDoors.Add(D.Origin,D); }
    ProjectionRevision=Candidate.Revision;
    bProjectionReady=true;
    return true;
}

bool ASFConstructionNetworkState::HasCurrentCollision() const
{
    if (!bProjectionReady || Snapshot.Revision==0 || ProjectionRevision!=Snapshot.Revision || Meshes.Num()!=Snapshot.Members.Num()
        || ProjectedMembers.Num()!=Snapshot.Members.Num() || DoorProjections.Num()!=Snapshot.Doors.Num() || ProjectedDoors.Num()!=Snapshot.Doors.Num() || !GetActorTransform().Equals(FTransform::Identity)) { return false; }
    TInlineComponentArray<UPrimitiveComponent*> Primitives; GetComponents(Primitives);
    if (Primitives.Num()!=Meshes.Num()+Snapshot.Doors.Num()*5) { return false; }
    for (const auto& M : Snapshot.Members)
    {
        auto* Mesh=Meshes.FindRef(M.Cell).Get();
        const auto* Applied=ProjectedMembers.Find(M.Cell);
        if (!Mesh || !Applied || Applied->Shape!=M.Shape || Applied->Rotation!=M.Rotation || Mesh->GetAttachParent()!=GetRootComponent()
            || !SFConstructionNetworkProjection::Matches(Mesh,M)) { return false; }
        FSFConstructionSolid Solid;
        if (!SFConstructionGeometry::Build(M.Shape,M.Rotation,Solid)) { return false; }
        FVector Inside=FVector::ZeroVector;
        for (const auto& V : Solid.Vertices) { Inside+=V; }
        Inside=FVector(M.Cell)*50.0+Inside/Solid.Vertices.Num();
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkProjection),false);
        if (!Mesh->LineTraceComponent(Hit,Inside+FVector(0,0,100),Inside,Query) || Hit.GetComponent()!=Mesh) { return false; }
    }
    for (const auto& D : Snapshot.Doors)
    {
        const auto* Applied=ProjectedDoors.Find(D.Origin);
        const auto* Projection=DoorProjections.Find(D.Origin);
        if (!Applied || Applied->Yaw!=D.Yaw || Applied->bOpen!=D.bOpen || !Projection
            || !SFConstructionNetworkProjection::MatchesDoor(this,D,*Projection)) { return false; }
    }
    return true;
}

bool ASFConstructionNetworkState::GetCellForComponent(const UPrimitiveComponent* Component, FIntVector& OutCell) const
{
    for (const auto& Pair : Meshes)
    { if (Pair.Value.Get()==Component) { OutCell=Pair.Key; return true; } }
    return false;
}

bool ASFConstructionNetworkState::GetDoorForComponent(const UPrimitiveComponent* Component, FIntVector& OutOrigin) const
{
    for (const auto& Pair : DoorProjections) for (const auto& Box : Pair.Value.Boxes)
    { if (Box.Get()==Component) { OutOrigin=Pair.Key; return true; } }
    return false;
}

bool ASFConstructionNetworkState::OwnsDoorProjection(const FSFConstructionNetworkDoor& Door, const FSFConstructionNetworkDoorProjection& Projection) const
{
    if (GetClass()!=StaticClass() || !HasCurrentCollision()) { return false; }
    const int32 Index=Snapshot.FindDoor(Door.Origin);
    const auto* Current=DoorProjections.Find(Door.Origin);
    if (Index==INDEX_NONE || !Current || Snapshot.Doors[Index].Yaw!=Door.Yaw || Snapshot.Doors[Index].bOpen!=Door.bOpen
        || Current->Boxes.Num()!=Projection.Boxes.Num()) { return false; }
    for (int32 I=0; I<Current->Boxes.Num(); ++I) { if (Current->Boxes[I]!=Projection.Boxes[I]) { return false; } }
    return true;
}

bool ASFConstructionNetworkState::IsSupportAllowedForPreview(FIntVector Cell, uint8 Shape, uint8 Rotation, bool bRemove) const
{
    if (!IsInGameThread() || Cell.X < -20 || Cell.X >= 20 || Cell.Y < -20 || Cell.Y >= 20 || Cell.Z < 0 || Cell.Z >= 20
        || Shape>=SFConstructionGeometry::ShapeCount || Rotation>=SFConstructionGeometry::RotationCount) { return false; }
    auto Candidate=Snapshot;
    const int32 Index=Candidate.Find(Cell);
    if (bRemove)
    {
        if (Index==INDEX_NONE) { return false; }
        Candidate.Members.RemoveAt(Index);
    }
    else
    {
        if (Index!=INDEX_NONE || Candidate.Members.Num()>=4096) { return false; }
        FSFConstructionNetworkMember M; M.Cell=Cell; M.Shape=Shape; M.Rotation=Rotation;
        Candidate.Members.Add(M);
    }
    return SupportsMembers(Candidate.Members,Candidate.Doors);
}

FString ASFConstructionNetworkState::GetContentDigest() const
{
    FString Canonical=TEXT("SF_NETWORK_ASSEMBLY_V5|");
    auto Members=Snapshot.Members;
    Members.Sort([](const auto& A,const auto& B) { return A.Cell.X!=B.Cell.X ? A.Cell.X<B.Cell.X : A.Cell.Y!=B.Cell.Y ? A.Cell.Y<B.Cell.Y : A.Cell.Z<B.Cell.Z; });
    for (const auto& M : Members)
    {
        FSFConstructionSolid Solid;
        if (!SFConstructionGeometry::Build(M.Shape,M.Rotation,Solid)) { return FString(); }
        Canonical+=FString::Printf(TEXT("%d,%d,%d,%u,%u,%s;"),M.Cell.X,M.Cell.Y,M.Cell.Z,M.Shape,M.Rotation,*Solid.PayloadSha256);
    }
    Canonical+=TEXT("|DOORS|sandbox-rough-timber-door-v1:1|");
    auto Doors=Snapshot.Doors;
    Doors.Sort([](const auto& A,const auto& B) { return A.Origin.X!=B.Origin.X ? A.Origin.X<B.Origin.X : A.Origin.Y!=B.Origin.Y ? A.Origin.Y<B.Origin.Y : A.Origin.Z<B.Origin.Z; });
    for (const auto& D : Doors) { Canonical+=FString::Printf(TEXT("%d,%d,%d,%u,%u;"),D.Origin.X,D.Origin.Y,D.Origin.Z,D.Yaw,D.bOpen ? 1 : 0); }
    FTCHARToUTF8 Bytes(*Canonical);
    return LexToString(FBlake3::HashBuffer(Bytes.Get(),Bytes.Length()));
}

void ASFConstructionNetworkState::OnRep_Snapshot()
{
    if (GetNetMode() != NM_Client || GetClass() != StaticClass()) { return; }
    if (Snapshot.Revision==0 && Snapshot.Members.IsEmpty() && Snapshot.Doors.IsEmpty()) { return; }
    bProjectionReady=false;
    if (Activate(Snapshot) && HasCurrentCollision())
    {
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_PROJECTED Revision=%llu Members=%d Collision=1 Content=%s Doors=%d"),Snapshot.Revision,Snapshot.Members.Num(),*GetContentDigest(),Snapshot.Doors.Num());
    }
    else { bProjectionReady=false; UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_PROJECTION_FAILED")); }
}

ESFConstructionNetworkResult ASFConstructionNetworkState::ValidateEditContext(ASFConstructionNetworkController* Controller, uint64 ExpectedRevision, const APawn*& OutPawn, const AActor*& OutFoundation) const
{
    OutPawn=nullptr; OutFoundation=nullptr;
    const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>() : nullptr;
    if (!IsInGameThread() || !HasAuthority() || IsActorBeingDestroyed() || !bConfigurationReady || !Mode || !Mode->IsAdmitted() || GetClass() != StaticClass()
        || !IsValid(Controller) || Controller->IsActorBeingDestroyed() || !Controller->HasAuthority() || Controller->GetWorld() != GetWorld()) { return ESFConstructionNetworkResult::InvalidContext; }
    if (!Mode->IsBuilder(Controller)) { return ESFConstructionNetworkResult::PermissionDenied; }
    const APawn* Pawn = Controller->GetPawn();
    if (!IsValid(Pawn) || !Pawn->HasAuthority() || Pawn->GetController() != Controller || Pawn->GetOwner() != Controller
        || Pawn->GetWorld() != GetWorld() || Pawn->IsActorBeingDestroyed()) { return ESFConstructionNetworkResult::InvalidContext; }
    if (ExpectedRevision != Snapshot.Revision) { return ESFConstructionNetworkResult::Stale; }
    if (Snapshot.Revision==MAX_uint64) { return ESFConstructionNetworkResult::InvalidContext; }
    if (Snapshot.Revision > 0 && !HasCurrentCollision()) { return ESFConstructionNetworkResult::ProjectionFailed; }
    const AActor* Foundation = nullptr;
    if (!SFConstructionSandboxFoundation::MatchesKnownPlaneGeometry(GetWorld(),&Foundation)) { return ESFConstructionNetworkResult::InvalidContext; }
    OutPawn=Pawn; OutFoundation=Foundation;
    return ESFConstructionNetworkResult::Accepted;
}

ESFConstructionNetworkResult ASFConstructionNetworkState::Edit(ASFConstructionNetworkController* Controller, FIntVector Cell, uint64 ExpectedRevision, bool bRemove, uint8 Shape, uint8 Rotation)
{
    const APawn* Pawn=nullptr; const AActor* Foundation=nullptr;
    const auto Admission=ValidateEditContext(Controller,ExpectedRevision,Pawn,Foundation);
    if (Admission!=ESFConstructionNetworkResult::Accepted) { return Admission; }
    if (!bRemove && (Shape>=SFConstructionGeometry::ShapeCount || Rotation>=SFConstructionGeometry::RotationCount)) { return ESFConstructionNetworkResult::InvalidPiece; }
    if (Cell.X < -20 || Cell.X >= 20 || Cell.Y < -20 || Cell.Y >= 20 || Cell.Z < 0 || Cell.Z >= 20) { return ESFConstructionNetworkResult::InvalidCell; }
    if (bRemove && Snapshot.Find(Cell)==INDEX_NONE) { return ESFConstructionNetworkResult::NotFound; }
    if (!bRemove && Snapshot.Find(Cell)!=INDEX_NONE) { return ESFConstructionNetworkResult::Occupied; }
    if (!bRemove && Snapshot.Members.Num() >= MaximumMembers) { return ESFConstructionNetworkResult::LimitReached; }
    if (!bRemove)
    {
        for (const auto& D : Snapshot.Doors)
        {
            FSFConstructionDoorProjectionProfile Profile;
            if (!ASFConstructionSandboxDoorProjection::BuildProfile(D.Origin,D.Yaw,D.bOpen,Profile)) { return ESFConstructionNetworkResult::InvalidContext; }
            for (const auto& Bounds : {Profile.Envelope,Profile.Motion,Profile.ApproachFront,Profile.ApproachBack})
            {
                if (SFConstructionClearance::IntersectsReservation(Shape,Rotation,FVector(Cell)*50,Bounds)!=SFConstructionClearance::EResult::Clear)
                { return ESFConstructionNetworkResult::Occupied; }
            }
        }
    }
    const FVector Centre = FVector(Cell)*50.0+FVector(25);
    if (Pawn->GetActorLocation().ContainsNaN() || FVector::DistSquared(Pawn->GetActorLocation(),Centre) > FMath::Square(ReachCentimetres)) { return ESFConstructionNetworkResult::OutOfReach; }
    FVector Eyes; FRotator EyeRotation;
    Pawn->GetActorEyesViewPoint(Eyes,EyeRotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkAdmission),false,Pawn);
    FHitResult Hit;
    if (Eyes.ContainsNaN()) { return ESFConstructionNetworkResult::Occluded; }
    FVector SightTarget=Centre;
    if (bRemove)
    {
        const auto& Existing=Snapshot.Members[Snapshot.Find(Cell)];
        FSFConstructionSolid Solid;
        if (!SFConstructionGeometry::Build(Existing.Shape,Existing.Rotation,Solid)) { return ESFConstructionNetworkResult::InvalidPiece; }
        SightTarget=FVector::ZeroVector;
        for (const auto& V : Solid.Vertices) { SightTarget+=V; }
        SightTarget=FVector(Cell)*50.0+SightTarget/Solid.Vertices.Num();
    }
    const bool bHit=GetWorld()->LineTraceSingleByChannel(Hit,Eyes,SightTarget,ECC_Visibility,Query);
    if (bRemove ? (!bHit || Hit.GetComponent()!=Meshes.FindRef(Cell)) : bHit) { return ESFConstructionNetworkResult::Occluded; }
    // Ignore only the already validated immutable fixture floor. Both players still block.
    FCollisionQueryParams Clearance(SCENE_QUERY_STAT(ConstructionNetworkClearance),false);
    Clearance.AddIgnoredComponent(Cast<UPrimitiveComponent>(Foundation->GetRootComponent()));
    // Existing cubes have proven canonical transforms and unique integer addresses.
    // Distinct full grid cells cannot overlap; do not exempt any other component.
    for (const auto& Pair : Meshes) { Clearance.AddIgnoredComponent(Pair.Value.Get()); }
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(Overlaps,Centre,FQuat::Identity,FCollisionObjectQueryParams::AllObjects,FCollisionShape::MakeBox(FVector(25)),Clearance);
    for (const auto& Overlap : Overlaps)
    {
        const auto* Component = Overlap.GetComponent();
        if (Component && Component->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block) { return ESFConstructionNetworkResult::Occupied; }
    }
    auto Candidate=Snapshot;
    Candidate.Revision = Snapshot.Revision+1;
    if (bRemove) { Candidate.Members.RemoveAt(Candidate.Find(Cell)); }
    else
    {
        FSFConstructionNetworkMember Member; Member.Cell=Cell; Member.Shape=Shape; Member.Rotation=Rotation;
        Candidate.Members.Add(Member);
    }
    Candidate.Members.Sort([](const auto& A,const auto& B) { return A.Cell.X!=B.Cell.X ? A.Cell.X<B.Cell.X : A.Cell.Y!=B.Cell.Y ? A.Cell.Y<B.Cell.Y : A.Cell.Z<B.Cell.Z; });
    if (!SupportsMembers(Candidate.Members,Candidate.Doors)) { return ESFConstructionNetworkResult::Unsupported; }
    if (!Activate(Candidate)) { return ESFConstructionNetworkResult::ProjectionFailed; }
    Snapshot = Candidate; // Publish only after synchronous server physics activation.
    ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_EDIT Action=%s Revision=%llu Members=%d Cell=%s Content=%s Shape=%u Rotation=%u"),bRemove ? TEXT("Remove") : TEXT("Place"),Snapshot.Revision,Snapshot.Members.Num(),*Cell.ToString(),*GetContentDigest(),Shape,Rotation);
    return bRemove ? ESFConstructionNetworkResult::Removed : ESFConstructionNetworkResult::Accepted;
}

ESFConstructionNetworkResult ASFConstructionNetworkState::PlaceDoor(ASFConstructionNetworkController* Controller, FIntVector Origin, uint64 ExpectedRevision, uint8 Yaw)
{
    const APawn* Pawn=nullptr; const AActor* Foundation=nullptr;
    const auto Admission=ValidateEditContext(Controller,ExpectedRevision,Pawn,Foundation);
    if (Admission!=ESFConstructionNetworkResult::Accepted) { return Admission; }
    if (Snapshot.FindDoor(Origin)!=INDEX_NONE) { return ESFConstructionNetworkResult::Occupied; }
    if (Snapshot.Doors.Num()>=MaximumDoors) { return ESFConstructionNetworkResult::LimitReached; }
    FSFConstructionDoorProjectionProfile Profile;
    if (!ASFConstructionSandboxDoorProjection::BuildProfile(Origin,Yaw,false,Profile)) { return ESFConstructionNetworkResult::InvalidCell; }
    const FVector Centre=Profile.Envelope.GetCenter();
    if (Pawn->GetActorLocation().ContainsNaN() || FVector::DistSquared(Pawn->GetActorLocation(),Centre)>FMath::Square(ReachCentimetres)) { return ESFConstructionNetworkResult::OutOfReach; }
    FVector Eyes; FRotator Rotation; Pawn->GetActorEyesViewPoint(Eyes,Rotation);
    FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkDoorPlacement),false,Pawn);
    if (Eyes.ContainsNaN() || GetWorld()->LineTraceSingleByChannel(Hit,Eyes,Centre,ECC_Visibility,Query)) { return ESFConstructionNetworkResult::Occluded; }
    const FBox Reservations[]={Profile.Envelope,Profile.Motion,Profile.ApproachFront,Profile.ApproachBack};
    for (const auto& Bounds : Reservations)
    {
        for (const auto& M : Snapshot.Members)
        {
            if (SFConstructionClearance::IntersectsReservation(M.Shape,M.Rotation,FVector(M.Cell)*50,Bounds)!=SFConstructionClearance::EResult::Clear)
            { return ESFConstructionNetworkResult::Occupied; }
        }
        for (const auto& D : Snapshot.Doors)
        {
            FSFConstructionDoorProjectionProfile Other;
            if (!ASFConstructionSandboxDoorProjection::BuildProfile(D.Origin,D.Yaw,D.bOpen,Other)) { return ESFConstructionNetworkResult::InvalidContext; }
            for (const auto& B : {Other.Envelope,Other.Motion,Other.ApproachFront,Other.ApproachBack})
            {
                if (FMath::Min(B.Max.X,Bounds.Max.X)>FMath::Max(B.Min.X,Bounds.Min.X)
                    && FMath::Min(B.Max.Y,Bounds.Max.Y)>FMath::Max(B.Min.Y,Bounds.Min.Y)
                    && FMath::Min(B.Max.Z,Bounds.Max.Z)>FMath::Max(B.Min.Z,Bounds.Min.Z)) { return ESFConstructionNetworkResult::Occupied; }
            }
        }
        if (SFConstructionClearance::EvaluateBox(GetWorld(),Bounds)!=SFConstructionClearance::EResult::Clear) { return ESFConstructionNetworkResult::Occupied; }
    }
    auto Candidate=Snapshot;
    FSFConstructionNetworkDoor Door; Door.Origin=Origin; Door.Yaw=Yaw;
    Candidate.Doors.Add(Door);
    Candidate.Doors.Sort([](const auto& A,const auto& B) { return A.Origin.X!=B.Origin.X ? A.Origin.X<B.Origin.X : A.Origin.Y!=B.Origin.Y ? A.Origin.Y<B.Origin.Y : A.Origin.Z<B.Origin.Z; });
    Candidate.Revision=Snapshot.Revision+1;
    if (!SupportsMembers(Candidate.Members,Candidate.Doors)) { return ESFConstructionNetworkResult::Unsupported; }
    if (!Activate(Candidate)) { return ESFConstructionNetworkResult::ProjectionFailed; }
    Snapshot=MoveTemp(Candidate); ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_PLACE Revision=%llu Members=%d Doors=%d Cell=%s Yaw=%u Content=%s"),Snapshot.Revision,Snapshot.Members.Num(),Snapshot.Doors.Num(),*Origin.ToString(),Yaw,*GetContentDigest());
    return ESFConstructionNetworkResult::Accepted;
}

bool ASFConstructionNetworkState::FindDoorAlongRay(const APawn* Pawn, const FVector& Start, const FVector& End, FIntVector& OutOrigin) const
{
    if (!IsInGameThread() || !GetWorld() || !IsValid(Pawn) || Pawn->GetWorld()!=GetWorld()
        || !IsProjectionCurrent() || Start.ContainsNaN() || End.ContainsNaN() || Pawn->GetActorLocation().ContainsNaN()
        || (End-Start).ContainsNaN() || (End-Start).IsNearlyZero()
        || (End-Start).Size()>ReachCentimetres+FVector::Dist(Start,Pawn->GetActorLocation())+FMath::Sqrt(3.0)*25.0+1.e-6) { return false; }
    double BestT=TNumericLimits<double>::Max(); bool bFound=false;
    FIntVector Best=FIntVector::ZeroValue;
    for (const auto& Door : Snapshot.Doors)
    {
        FSFConstructionDoorProjectionProfile Profile; double T;
        if (!ASFConstructionSandboxDoorProjection::BuildProfile(Door.Origin,Door.Yaw,Door.bOpen,Profile)
            || FVector::DistSquared(Pawn->GetActorLocation(),Profile.Envelope.GetCenter())>FMath::Square(ReachCentimetres)
            || !RayEnvelopeEntry(Start,End,Profile.Envelope,T)) { continue; }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkDoorTarget),false,Pawn);
        FHitResult Hit; FIntVector HitOrigin;
        const FVector Probe=Start+(End-Start)*FMath::Min(1.0,T+1.e-5);
        if (GetWorld()->LineTraceSingleByChannel(Hit,Start,Probe,ECC_Visibility,Query)
            && !(Hit.GetActor()==this && GetDoorForComponent(Hit.GetComponent(),HitOrigin) && HitOrigin==Door.Origin)) { continue; }
        const bool bEarlier=Door.Origin.X<Best.X || (Door.Origin.X==Best.X && (Door.Origin.Y<Best.Y || (Door.Origin.Y==Best.Y && Door.Origin.Z<Best.Z)));
        if (!bFound || T<BestT-1.e-9 || (FMath::IsNearlyEqual(T,BestT,1.e-9) && bEarlier))
        { bFound=true; BestT=T; Best=Door.Origin; }
    }
    if (!bFound) { return false; }
    OutOrigin=Best; return true;
}

ESFConstructionNetworkResult ASFConstructionNetworkState::ValidateDoorTarget(ASFConstructionNetworkController* Controller, FIntVector Origin, uint64 ExpectedRevision, int32& OutIndex) const
{
    const APawn* Pawn=nullptr; const AActor* Foundation=nullptr;
    const auto Admission=ValidateEditContext(Controller,ExpectedRevision,Pawn,Foundation);
    if (Admission!=ESFConstructionNetworkResult::Accepted) { return Admission; }
    OutIndex=Snapshot.FindDoor(Origin);
    const int32 Index=OutIndex;
    const auto* Projection=DoorProjections.Find(Origin);
    if (Index==INDEX_NONE || !Projection) { return ESFConstructionNetworkResult::NotFound; }
    const auto& Door=Snapshot.Doors[Index];
    FSFConstructionDoorProjectionProfile Profile;
    if (!ASFConstructionSandboxDoorProjection::BuildProfile(Origin,Door.Yaw,Door.bOpen,Profile)) { return ESFConstructionNetworkResult::InvalidContext; }
    if (Pawn->GetActorLocation().ContainsNaN() || FVector::DistSquared(Pawn->GetActorLocation(),Profile.Envelope.GetCenter())>FMath::Square(ReachCentimetres)) { return ESFConstructionNetworkResult::OutOfReach; }
    FVector Eyes; FRotator Rotation; Pawn->GetActorEyesViewPoint(Eyes,Rotation);
    if (Eyes.ContainsNaN()) { return ESFConstructionNetworkResult::Occluded; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkDoorUse),false,Pawn);
    FHitResult CentreHit; FIntVector CentreOrigin;
    // Clear sight into the aperture is sufficient, even without a leaf/frame hit.
    bool bVisible=!GetWorld()->LineTraceSingleByChannel(CentreHit,Eyes,Profile.Envelope.GetCenter(),ECC_Visibility,Query)
        || (CentreHit.GetActor()==this && GetDoorForComponent(CentreHit.GetComponent(),CentreOrigin) && CentreOrigin==Origin);
    for (const auto& Box : Projection->Boxes)
    {
        if (bVisible) { break; }
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit,Eyes,Box->GetComponentLocation(),ECC_Visibility,Query) && Hit.GetComponent()==Box.Get())
        { bVisible=true; break; }
    }
    if (!bVisible) { return ESFConstructionNetworkResult::Occluded; }
    return ESFConstructionNetworkResult::Accepted;
}

ESFConstructionNetworkResult ASFConstructionNetworkState::RemoveDoor(ASFConstructionNetworkController* Controller, FIntVector Origin, uint64 ExpectedRevision)
{
    int32 Index=INDEX_NONE;
    const auto Admission=ValidateDoorTarget(Controller,Origin,ExpectedRevision,Index);
    if (Admission!=ESFConstructionNetworkResult::Accepted) { return Admission; }
    auto Candidate=Snapshot;
    Candidate.Doors.RemoveAt(Index);
    Candidate.Revision=Snapshot.Revision+1;
    if (!SupportsMembers(Candidate.Members,Candidate.Doors)) { return ESFConstructionNetworkResult::Unsupported; }
    if (!Activate(Candidate)) { return ESFConstructionNetworkResult::ProjectionFailed; }
    Snapshot=MoveTemp(Candidate); ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_REMOVE Revision=%llu Members=%d Doors=%d Cell=%s Content=%s"),Snapshot.Revision,Snapshot.Members.Num(),Snapshot.Doors.Num(),*Origin.ToString(),*GetContentDigest());
    return ESFConstructionNetworkResult::DoorRemoved;
}

ESFConstructionNetworkResult ASFConstructionNetworkState::ToggleDoor(ASFConstructionNetworkController* Controller, FIntVector Origin, uint64 ExpectedRevision)
{
    int32 Index=INDEX_NONE;
    const auto Admission=ValidateDoorTarget(Controller,Origin,ExpectedRevision,Index);
    if (Admission!=ESFConstructionNetworkResult::Accepted) { return Admission; }
    const auto& Door=Snapshot.Doors[Index];
    const auto* Projection=DoorProjections.Find(Origin);
    if (!SFConstructionNetworkProjection::IsDoorMotionClear(this,Door,*Projection))
    {
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>()->DiscardFiniteDoorObstruction();
#endif
        return ESFConstructionNetworkResult::DoorBlocked;
    }
    auto Candidate=Snapshot;
    Candidate.Revision=Snapshot.Revision+1;
    Candidate.Doors[Index].bOpen=!Door.bOpen;
    if (!SupportsMembers(Candidate.Members,Candidate.Doors)) { return ESFConstructionNetworkResult::Unsupported; }
    if (!Activate(Candidate)) { return ESFConstructionNetworkResult::ProjectionFailed; }
    Snapshot=MoveTemp(Candidate); ForceNetUpdate();
    const bool bOpen=Snapshot.Doors[Index].bOpen;
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_TOGGLE Revision=%llu Members=%d Doors=%d Open=%d Cell=%s Content=%s"),Snapshot.Revision,Snapshot.Members.Num(),Snapshot.Doors.Num(),bOpen,*Origin.ToString(),*GetContentDigest());
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bOpen) { GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>()->SpawnFiniteDoorObstruction(Origin,Snapshot.Doors[Index].Yaw); }
#endif
    return bOpen ? ESFConstructionNetworkResult::DoorOpened : ESFConstructionNetworkResult::DoorClosed;
}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void ASFConstructionNetworkGameMode::SpawnFiniteDoorObstruction(const FIntVector& Origin, uint8 Yaw)
{
    if (!IsAdmitted() || bFiniteObstructionSpawned || !FParse::Param(FCommandLine::Get(),TEXT("ConstructionNetworkDoorObstruction"))) { return; }
    bFiniteObstructionSpawned=true;
    FSFConstructionDoorProjectionProfile Profile;
    if (!ASFConstructionSandboxDoorProjection::BuildProfile(Origin,Yaw,true,Profile)) { return; }
    FActorSpawnParameters Spawn; Spawn.Owner=this; Spawn.ObjectFlags|=RF_Transient;
    auto* Obstacle=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Spawn);
    if (!Obstacle) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED DoorObstructionSpawn=1")); return; }
    auto* Box=NewObject<UBoxComponent>(Obstacle);
    Obstacle->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(10)); // Fixed20cm physics canary, not a gameplay setting.
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionObjectType(ECC_WorldDynamic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->SetGenerateOverlapEvents(false);
    Obstacle->SetActorLocation(Profile.Motion.GetCenter());
    Box->RegisterComponent();
    if (!Box->IsRegistered() || !Box->IsPhysicsStateCreated())
    { Obstacle->Destroy(); UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED DoorObstructionPhysics=1")); return; }
    FiniteDoorObstruction=Obstacle;
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_OBSTRUCTION_READY Physics=1"));
}

void ASFConstructionNetworkGameMode::DiscardFiniteDoorObstruction()
{
    auto* Obstacle=FiniteDoorObstruction.Get();
    if (IsAdmitted() && FParse::Param(FCommandLine::Get(),TEXT("ConstructionNetworkDoorObstruction")) && Obstacle && Obstacle->GetOwner()==this)
    { Obstacle->Destroy(); FiniteDoorObstruction.Reset(); UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_OBSTRUCTION_REMOVED")); }
}
#endif

ASFConstructionNetworkGameMode::ASFConstructionNetworkGameMode()
{
    HUDClass = ASFConstructionNetworkHUD::StaticClass();
    PlayerControllerClass = ASFConstructionNetworkController::StaticClass();
}

bool ASFConstructionNetworkGameMode::IsAdmitted() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    return HasAuthority() && !IsActorBeingDestroyed() && GetClass() == StaticClass() && GetWorld() && GetNetMode() == NM_DedicatedServer
        && GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Greenlands_ConstructionSandbox")
        && FParse::Param(FCommandLine::Get(),TEXT("ConstructionNetworkSandbox"));
#else
    return false;
#endif
}

void ASFConstructionNetworkGameMode::StartPlay()
{
    Super::StartPlay();
    if (!IsAdmitted()) { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DISABLED")); return; }
    const auto* State = GetWorld()->SpawnActor<ASFConstructionNetworkState>();
    if (!State) { UE_LOG(LogTemp,Fatal,TEXT("Construction network fixture state could not spawn")); }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_READY UnlimitedDevelopmentStock=1 Shapes=CubeSlabSlope Support=RoughTimber Persistence=0"));
}

void ASFConstructionNetworkGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    auto* Controller = Cast<ASFConstructionNetworkController>(NewPlayer);
    if (!IsAdmitted() || !Controller) { return; }
    const bool bAssignBuilder = !bBuilderAssigned;
    bBuilderAssigned = true;
    if (bAssignBuilder) { Builder = Controller; }
    Controller->AssignRole(bAssignBuilder);
    if (APawn* Pawn = Controller->GetPawn()) { Pawn->SetActorLocation(FVector(bAssignBuilder ? -200 : 200,-400,96)); }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_ROLE Builder=%d"),bAssignBuilder);
    if (auto* State = FindState(GetWorld())) { State->AdmitPair(); }
}

void ASFConstructionNetworkGameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);
    const auto* Controller = Cast<ASFConstructionNetworkController>(NewPlayer);
    if (IsAdmitted() && Controller && Controller->GetPawn())
    { Controller->GetPawn()->SetActorLocation(FVector(IsBuilder(Controller) ? -200 : 200,-400,96)); }
}

bool ASFConstructionNetworkGameMode::IsBuilder(const ASFConstructionNetworkController* Controller) const
{
    return IsAdmitted() && Builder.Get() == Controller;
}

void ASFConstructionNetworkController::AssignRole(bool bBuilder)
{
    if (!HasAuthority()) { return; }
    bBuilderRole = bBuilder; bRoleAssigned = true; ForceNetUpdate();
}

ASFConstructionNetworkController::ASFConstructionNetworkController()
{
    SelectedRotation=uint8(SFConstructionGeometry::IdentityRotation());
}

void ASFConstructionNetworkController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ASFConstructionNetworkController,bRoleAssigned,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ASFConstructionNetworkController,bBuilderRole,COND_OwnerOnly);
}

void ASFConstructionNetworkController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::B,IE_Pressed,this,&ASFConstructionNetworkController::PlaceFromView);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&ASFConstructionNetworkController::PlaceFromView);
    InputComponent->BindKey(EKeys::RightMouseButton,IE_Pressed,this,&ASFConstructionNetworkController::RemoveFromView);
    InputComponent->BindKey(EKeys::X,IE_Pressed,this,&ASFConstructionNetworkController::RemoveFromView);
    InputComponent->BindKey(EKeys::One,IE_Pressed,this,&ASFConstructionNetworkController::SelectCube);
    InputComponent->BindKey(EKeys::Two,IE_Pressed,this,&ASFConstructionNetworkController::SelectSlab);
    InputComponent->BindKey(EKeys::Three,IE_Pressed,this,&ASFConstructionNetworkController::SelectSlope);
    InputComponent->BindKey(EKeys::Four,IE_Pressed,this,&ASFConstructionNetworkController::SelectDoor);
    // Controller consumes E before the inherited pawn's probe binding.
    InputComponent->BindKey(EKeys::E,IE_Pressed,this,&ASFConstructionNetworkController::ToggleDoorFromView);
    InputComponent->BindKey(EKeys::R,IE_Pressed,this,&ASFConstructionNetworkController::RotatePiece);
}

void ASFConstructionNetworkController::SelectCube() { SelectShape(0); }
void ASFConstructionNetworkController::SelectSlab() { SelectShape(1); }
void ASFConstructionNetworkController::SelectSlope() { SelectShape(2); }
void ASFConstructionNetworkController::SelectDoor()
{ bDoorSelected=true; SelectedYaw=0; UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_SELECTION Yaw=0")); }
void ASFConstructionNetworkController::SelectShape(uint8 Shape)
{
    bDoorSelected=false; SelectedShape=Shape; SelectedYaw=0; SelectedRotation=uint8(SFConstructionGeometry::IdentityRotation());
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SELECTION Shape=%u Yaw=%u Rotation=%u"),SelectedShape,SelectedYaw,SelectedRotation);
}
void ASFConstructionNetworkController::RotatePiece()
{
    SelectedYaw=(SelectedYaw+1)%4;
    const FIntVector XAxes[4]={{1,0,0},{0,1,0},{-1,0,0},{0,-1,0}};
    const FIntVector X=XAxes[SelectedYaw],Y(-X.Y,X.X,0);
    for (int32 I=0; I<SFConstructionGeometry::RotationCount; ++I)
    {
        FSFConstructionSolid Solid;
        if (SFConstructionGeometry::Build(SelectedShape,I,Solid) && Solid.Rotation[0]==X && Solid.Rotation[1]==Y && Solid.Rotation[2]==FIntVector(0,0,1))
        { SelectedRotation=uint8(I); UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SELECTION Shape=%u Yaw=%u Rotation=%u"),SelectedShape,SelectedYaw,SelectedRotation); return; }
    }
}

void ASFConstructionNetworkController::AcknowledgePossession(APawn* P)
{
    Super::AcknowledgePossession(P);
    if (!IsLocalController() || !P) { return; }
    // Keep the local placeholder body from hiding the build target. Other
    // players still see it, and its capsule/authority remain unchanged.
    TInlineComponentArray<UStaticMeshComponent*> Meshes;
    P->GetComponents(Meshes);
    for (auto* Mesh : Meshes) { Mesh->SetOwnerNoSee(true); }
}

void ASFConstructionNetworkController::PlaceFromView()
{
    RefreshPlacementPreview(false);
    auto* State = FindState(GetWorld());
    if (!bHasPlacementPreview || !State) { LastPlacementStatus=PreviewStatus; return; }
    LastPlacementStatus=TEXT("Placement requested. Waiting for server...");
    if (bDoorSelected)
    {
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_DOOR Cell=%s Yaw=%u"),*PreviewCell.ToString(),SelectedYaw);
        ServerPlaceDoor(PreviewCell,State->GetSnapshot().Revision,SelectedYaw); return;
    }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_REQUEST Cell=%s Shape=%u Rotation=%u FromMember=%d"),*PreviewCell.ToString(),SelectedShape,SelectedRotation,bHasRemovalPreview);
    ServerPlace(PreviewCell,State->GetSnapshot().Revision,SelectedShape,SelectedRotation);
}

void ASFConstructionNetworkController::RefreshPlacementPreview(bool bDrawOutline)
{
    bHasPlacementPreview=false;
    bHasRemovalPreview=false;
    bHasDoorTarget=false;
    RemovalStatus=TEXT("Aim at a placed piece to remove it.");
    auto* State = FindState(GetWorld());
    if (!IsLocalController() || GetNetMode()!=NM_Client || !GetPawn() || !bRoleAssigned || !State
        || State->GetReachCentimetres()<=0 || State->GetMaximumMembers()<=0)
    { PreviewStatus=TEXT("Waiting for server construction state..."); return; }
    if (!bBuilderRole) { PreviewStatus=TEXT("Observer: watch the builder's placements."); RemovalStatus=TEXT("Observer: removal unavailable."); return; }
    if (State->GetSnapshot().Revision>0 && !State->IsProjectionCurrent())
    { PreviewStatus=TEXT("Waiting for local construction collision..."); return; }
    FVector Start; FRotator Rotation; GetPlayerViewPoint(Start,Rotation);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkView),false,GetPawn());
    // Camera offset plus half a cell diagonal covers the server's pawn-centre reach sphere.
    const double TraceLength=State->GetReachCentimetres()+FVector::Dist(Start,GetPawn()->GetActorLocation())+FMath::Sqrt(3.0)*25.0;
    const FVector End=Start+Rotation.Vector()*TraceLength;
    const bool bHit=GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Query);
    bHasDoorTarget=State->FindDoorAlongRay(GetPawn(),Start,End,TargetDoorOrigin);
    if (!bHasDoorTarget && bHit)
    { bHasDoorTarget=Hit.GetActor()==State && State->GetDoorForComponent(Hit.GetComponent(),TargetDoorOrigin); }
    if (bHasDoorTarget)
    {
        PreviewStatus=TEXT("Doorway targeted.");
        RemovalStatus=TEXT("E: open / close | Right click / X: remove door.");
        const int32 Index=State->GetSnapshot().FindDoor(TargetDoorOrigin);
        FSFConstructionDoorProjectionProfile Profile;
        if (bDrawOutline && Index!=INDEX_NONE && ASFConstructionSandboxDoorProjection::BuildProfile(TargetDoorOrigin,State->GetSnapshot().Doors[Index].Yaw,State->GetSnapshot().Doors[Index].bOpen,Profile))
        { DrawDebugBox(GetWorld(),Profile.Envelope.GetCenter(),Profile.Envelope.GetExtent(),FColor::Cyan,false,-1.f,0,2.f); }
        return;
    }
    if (!bHit) { PreviewStatus=TEXT("Aim at the ground or a block face within reach."); return; }
    const FVector Point = Hit.ImpactPoint + Hit.ImpactNormal*0.1;
    PreviewCell=FIntVector(FMath::FloorToInt(Point.X/50),FMath::FloorToInt(Point.Y/50),FMath::FloorToInt(Point.Z/50));
    bHasRemovalPreview=Hit.GetActor()==State && State->GetCellForComponent(Hit.GetComponent(),RemovalCell);
    if (bHasRemovalPreview)
    {
        // Partial/sloped faces can be inside their cell. Keep one shape per
        // address and choose its adjacent cell along the dominant face normal.
        const FVector N=Hit.ImpactNormal,A=N.GetAbs();
        FIntVector Step=FIntVector::ZeroValue;
        if (A.Z>=A.X && A.Z>=A.Y) { Step.Z=N.Z>=0 ? 1 : -1; }
        else if (A.X>=A.Y) { Step.X=N.X>=0 ? 1 : -1; }
        else { Step.Y=N.Y>=0 ? 1 : -1; }
        PreviewCell=RemovalCell+Step;
    }
    bHasPlacementPreview=true;
    if (bDoorSelected)
    {
        FSFConstructionDoorProjectionProfile Profile;
        if (!ASFConstructionSandboxDoorProjection::BuildProfile(PreviewCell,SelectedYaw,false,Profile))
        { bHasPlacementPreview=false; PreviewStatus=TEXT("Door footprint is outside the construction area."); return; }
        const bool bReach=FVector::DistSquared(GetPawn()->GetActorLocation(),Profile.Envelope.GetCenter())<=FMath::Square(State->GetReachCentimetres());
        PreviewStatus=FString::Printf(TEXT("Door %d, %d, %d | %s"),PreviewCell.X,PreviewCell.Y,PreviewCell.Z,bReach ? TEXT("Click or B to request placement.") : TEXT("Out of reach."));
        RemovalStatus=bHasDoorTarget ? TEXT("E: open / close | Right click / X: remove door.") : TEXT("Door frame, swing space and both approaches must be clear.");
        if (bDrawOutline)
        {
            for (const auto& B : Profile.Boxes)
            { DrawDebugBox(GetWorld(),Profile.Transform.TransformPosition(B.Centre),B.Extent,Profile.Transform.GetRotation()*B.Rotation,bReach ? FColor(255,190,60) : FColor::Red,false,-1.f,0,2.f); }
        }
        return;
    }
    const FVector Centre=FVector(PreviewCell)*50.0+FVector(25);
    if (SupportCache.State.Get()!=State || SupportCache.Revision!=State->GetSnapshot().Revision || SupportCache.PlacementCell!=PreviewCell
        || SupportCache.Shape!=SelectedShape || SupportCache.Rotation!=SelectedRotation || SupportCache.bHasRemoval!=bHasRemovalPreview
        || (bHasRemovalPreview && SupportCache.RemovalCell!=RemovalCell))
    {
        SupportCache.State=State; SupportCache.Revision=State->GetSnapshot().Revision; SupportCache.PlacementCell=PreviewCell;
        SupportCache.Shape=SelectedShape; SupportCache.Rotation=SelectedRotation;
        SupportCache.bHasRemoval=bHasRemovalPreview; SupportCache.RemovalCell=RemovalCell;
        SupportCache.bPlacementAllowed=State->IsSupportAllowedForPreview(PreviewCell,SelectedShape,SelectedRotation,false);
        SupportCache.bRemovalAllowed=bHasRemovalPreview && State->IsSupportAllowedForPreview(RemovalCell,SelectedShape,SelectedRotation,true);
    }
    if (bHasRemovalPreview)
    { RemovalStatus=SupportCache.bRemovalAllowed ? TEXT("Right click / X: request removal.") : TEXT("Cannot remove: supporting other blocks."); }
    if (bHasDoorTarget) { RemovalStatus=TEXT("E: open / close | Right click / X: remove door."); }
    const TCHAR* BlockReason=nullptr;
    if (PreviewCell.X < -20 || PreviewCell.X >= 20 || PreviewCell.Y < -20 || PreviewCell.Y >= 20 || PreviewCell.Z < 0 || PreviewCell.Z >= 20)
    { BlockReason=PlacementResultText(ESFConstructionNetworkResult::InvalidCell); }
    else if (FVector::DistSquared(GetPawn()->GetActorLocation(),Centre)>FMath::Square(State->GetReachCentimetres()))
    { BlockReason=PlacementResultText(ESFConstructionNetworkResult::OutOfReach); }
    else if (State->GetSnapshot().Find(PreviewCell)!=INDEX_NONE) { BlockReason=PlacementResultText(ESFConstructionNetworkResult::Occupied); }
    else if (State->GetSnapshot().Members.Num()>=State->GetMaximumMembers()) { BlockReason=PlacementResultText(ESFConstructionNetworkResult::LimitReached); }
    else if (!SupportCache.bPlacementAllowed) { BlockReason=TEXT("Needs support."); }
    PreviewStatus=FString::Printf(TEXT("Target %d, %d, %d | %s"),PreviewCell.X,PreviewCell.Y,PreviewCell.Z,
        BlockReason ? BlockReason : TEXT("Click or B to request placement."));
    if (bDrawOutline)
    {
        FSFConstructionSolid Solid;
        if (SFConstructionGeometry::Build(SelectedShape,SelectedRotation,Solid))
        {
            for (const auto& Face : Solid.Faces) for (int32 I=0; I<Face.Num(); ++I)
            { DrawDebugLine(GetWorld(),FVector(PreviewCell)*50+Solid.Vertices[Face[I]],FVector(PreviewCell)*50+Solid.Vertices[Face[(I+1)%Face.Num()]],BlockReason ? FColor::Red : FColor(255,190,60),false,-1.f,0,2.f); }
        }
    }
    if (bHasRemovalPreview && bDrawOutline)
    { DrawDebugBox(GetWorld(),FVector(RemovalCell)*50.0+FVector(25),FVector(25),SupportCache.bRemovalAllowed ? FColor::Cyan : FColor::Red,false,-1.f,0,2.f); }
}

void ASFConstructionNetworkController::RemoveFromView()
{
    RefreshPlacementPreview(false);
    auto* State=FindState(GetWorld());
    if (!State) { return; }
    if (bHasDoorTarget)
    {
        LastPlacementStatus=TEXT("Door removal requested. Waiting for server...");
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_REMOVE_DOOR Cell=%s"),*TargetDoorOrigin.ToString());
        ServerRemoveDoor(TargetDoorOrigin,State->GetSnapshot().Revision); return;
    }
    if (!bHasRemovalPreview) { LastPlacementStatus=TEXT("Aim at a construction piece or door to remove it."); return; }
    LastPlacementStatus=TEXT("Removal requested. Waiting for server...");
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_REMOVE Cell=%s"),*RemovalCell.ToString());
    ServerRemove(RemovalCell,State->GetSnapshot().Revision);
}

void ASFConstructionNetworkController::ServerPlace_Implementation(FIntVector Cell, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation)
{ ProcessEdit(Cell,ExpectedRevision,false,Shape,Rotation); }

void ASFConstructionNetworkController::ServerRemove_Implementation(FIntVector Cell, uint64 ExpectedRevision)
{ ProcessEdit(Cell,ExpectedRevision,true,0,0); }

void ASFConstructionNetworkController::ToggleDoorFromView()
{
    RefreshPlacementPreview(false);
    auto* State=FindState(GetWorld());
    if (!bHasDoorTarget || !State) { LastPlacementStatus=TEXT("Aim at a door frame or leaf to open or close it."); return; }
    LastPlacementStatus=TEXT("Door request sent. Waiting for server...");
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_TOGGLE Cell=%s"),*TargetDoorOrigin.ToString());
    ServerToggleDoor(TargetDoorOrigin,State->GetSnapshot().Revision);
}

void ASFConstructionNetworkController::ServerRemoveDoor_Implementation(FIntVector Origin, uint64 ExpectedRevision)
{
    auto* State=FindState(GetWorld());
    auto Result=ESFConstructionNetworkResult::InvalidContext;
    if (State && LastRequestFrame!=GFrameCounter)
    { LastRequestFrame=GFrameCounter; Result=State->RemoveDoor(this,Origin,ExpectedRevision); }
    const uint64 Revision=State ? State->GetSnapshot().Revision : 0;
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_REMOVE_DOOR_RESULT Builder=%d Result=%d Revision=%llu Doors=%d Collision=%d"),bBuilderRole,int32(Result),Revision,State ? State->GetSnapshot().Doors.Num() : 0,State && State->HasCurrentCollision());
    ClientPlacementResult(Result,Revision);
}

void ASFConstructionNetworkController::ServerToggleDoor_Implementation(FIntVector Origin, uint64 ExpectedRevision)
{
    auto* State=FindState(GetWorld());
    auto Result=ESFConstructionNetworkResult::InvalidContext;
    if (State && LastRequestFrame!=GFrameCounter)
    { LastRequestFrame=GFrameCounter; Result=State->ToggleDoor(this,Origin,ExpectedRevision); }
    const uint64 Revision=State ? State->GetSnapshot().Revision : 0;
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_TOGGLE_RESULT Builder=%d Result=%d Revision=%llu Doors=%d Collision=%d"),bBuilderRole,int32(Result),Revision,State ? State->GetSnapshot().Doors.Num() : 0,State && State->HasCurrentCollision());
    ClientPlacementResult(Result,Revision);
}

void ASFConstructionNetworkController::ServerPlaceDoor_Implementation(FIntVector Origin, uint64 ExpectedRevision, uint8 Yaw)
{
    auto* State=FindState(GetWorld());
    auto Result=ESFConstructionNetworkResult::InvalidContext;
    if (State && LastRequestFrame!=GFrameCounter)
    { LastRequestFrame=GFrameCounter; Result=State->PlaceDoor(this,Origin,ExpectedRevision,Yaw); }
    const uint64 Revision=State ? State->GetSnapshot().Revision : 0;
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_RESULT Builder=%d Result=%d Revision=%llu Doors=%d Collision=%d"),bBuilderRole,int32(Result),Revision,State ? State->GetSnapshot().Doors.Num() : 0,State && State->HasCurrentCollision());
    ClientPlacementResult(Result,Revision);
}

void ASFConstructionNetworkController::ProcessEdit(FIntVector Cell, uint64 ExpectedRevision, bool bRemove, uint8 Shape, uint8 Rotation)
{
    auto* State = FindState(GetWorld());
    ESFConstructionNetworkResult Result = ESFConstructionNetworkResult::InvalidContext;
    if (State && LastRequestFrame != GFrameCounter)
    {
        LastRequestFrame = GFrameCounter;
        Result = State->Edit(this,Cell,ExpectedRevision,bRemove,Shape,Rotation);
    }
    const uint64 Revision = State ? State->GetSnapshot().Revision : 0;
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_RESULT Builder=%d Result=%d Revision=%llu Members=%d Collision=%d Action=%s"),bBuilderRole,int32(Result),Revision,State ? State->GetSnapshot().Members.Num() : 0,State && State->HasCurrentCollision(),bRemove ? TEXT("Remove") : TEXT("Place"));
    ClientPlacementResult(Result,Revision);
}

void ASFConstructionNetworkController::ClientPlacementResult_Implementation(ESFConstructionNetworkResult Result, uint64 Revision)
{
    LastPlacementStatus=PlacementResultText(Result);
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_FEEDBACK Result=%d Revision=%llu Text=%s"),int32(Result),Revision,*LastPlacementStatus);
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!FParse::Param(FCommandLine::Get(),TEXT("ConstructionNetworkScenario"))) { return; }
    const bool bExpected = bBuilderRole
        ? ((ScenarioStep==1 && Result==ESFConstructionNetworkResult::Occupied && Revision==0)
            || (ScenarioStep==3 && Result==ESFConstructionNetworkResult::Accepted && Revision==1)
            || (ScenarioStep==5 && Result==ESFConstructionNetworkResult::Unsupported && Revision==1)
            || (ScenarioStep==7 && Result==ESFConstructionNetworkResult::Accepted && Revision==2)
            || (ScenarioStep==9 && Result==ESFConstructionNetworkResult::Accepted && Revision==3)
            || (ScenarioStep==11 && Result==ESFConstructionNetworkResult::Stale && Revision==3)
            || (ScenarioStep==13 && Result==ESFConstructionNetworkResult::Unsupported && Revision==3)
            || (ScenarioStep==15 && Result==ESFConstructionNetworkResult::Removed && Revision==4)
            || (ScenarioStep==17 && Result==ESFConstructionNetworkResult::NotFound && Revision==4)
            || (ScenarioStep==19 && Result==ESFConstructionNetworkResult::Stale && Revision==4)
            || (ScenarioStep==21 && Result==ESFConstructionNetworkResult::Removed && Revision==5)
            || (ScenarioStep==23 && Result==ESFConstructionNetworkResult::Removed && Revision==6)
            || ((ScenarioStep==25 || ScenarioStep==27) && Result==ESFConstructionNetworkResult::InvalidPiece && Revision==6)
            || (ScenarioStep==29 && Result==ESFConstructionNetworkResult::Accepted && Revision==7)
            || (ScenarioStep==31 && Result==ESFConstructionNetworkResult::Accepted && Revision==8)
            || (ScenarioStep==33 && Result==ESFConstructionNetworkResult::InvalidCell && Revision==8)
            || (ScenarioStep==35 && Result==ESFConstructionNetworkResult::Accepted && Revision==9)
            || (ScenarioStep==37 && Result==ESFConstructionNetworkResult::Occupied && Revision==9)
            || (ScenarioStep==39 && Result==ESFConstructionNetworkResult::Accepted && Revision==10)
            || (ScenarioStep==41 && Result==ESFConstructionNetworkResult::Removed && Revision==11)
            || (ScenarioStep==43 && Result==ESFConstructionNetworkResult::DoorOpened && Revision==12)
            || (ScenarioStep==45 && Result==ESFConstructionNetworkResult::DoorBlocked && Revision==12)
            || (ScenarioStep==47 && Result==ESFConstructionNetworkResult::DoorClosed && Revision==13)
            || (ScenarioStep==49 && Result==ESFConstructionNetworkResult::DoorRemoved && Revision==14))
        : (((ScenarioStep==1 || ScenarioStep==3 || ScenarioStep==5) && Result==ESFConstructionNetworkResult::PermissionDenied && Revision==6)
            || (ScenarioStep==7 && Result==ESFConstructionNetworkResult::PermissionDenied && Revision==11)
            || (ScenarioStep==9 && Result==ESFConstructionNetworkResult::PermissionDenied && Revision==13));
    if (!bExpected) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED Step=%d Result=%d Revision=%llu"),ScenarioStep,int32(Result),Revision); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    ++ScenarioStep;
#endif
}

void ASFConstructionNetworkController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsLocalController()) { RefreshPlacementPreview(true); }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TickFiniteScenario();
#endif
}

FString ASFConstructionNetworkController::GetBuildStatusText() const
{
    const auto* State=FindState(GetWorld());
    return FString::Printf(TEXT("CONSTRUCTION | %s | DEV: UNLIMITED STOCK\nClick / B: place    Right click / X: remove    E: door    WASD: move    Space: jump\n1 Cube | 2 Half slab | 3 Slope | 4 Door | R Rotate | Selected: %s (%u deg)\n%s\n%s\nLast action: %s\nMembers: %d / %d | Doors: %d / %d | Reach: %.1f m\nAmber: place target | Cyan: remove target | Red: blocked estimate | Server checks edits."),
        !bRoleAssigned ? TEXT("CONNECTING") : bBuilderRole ? TEXT("BUILDER") : TEXT("OBSERVER"),
        bDoorSelected ? TEXT("Door") : SelectedShape==0 ? TEXT("Cube") : SelectedShape==1 ? TEXT("Half slab") : TEXT("Slope"),SelectedYaw*90,
        *PreviewStatus,*RemovalStatus,*LastPlacementStatus,State ? State->GetSnapshot().Members.Num() : 0,
        State ? State->GetMaximumMembers() : 0,State ? State->GetSnapshot().Doors.Num() : 0,State ? State->GetMaximumDoors() : 0,State ? State->GetReachCentimetres()/100.0 : 0.0);
}

void ASFConstructionNetworkHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto* Controller=Cast<ASFConstructionNetworkController>(GetOwningPlayerController());
    if (!Controller || !Canvas || !GEngine) { return; }
    const float Scale=FMath::Clamp(Canvas->ClipX/1280.f,0.5f,1.5f);
    DrawRect(FLinearColor(0.02f,0.03f,0.04f,0.88f),16*Scale,Canvas->ClipY-202*Scale,820*Scale,186*Scale);
    DrawText(Controller->GetBuildStatusText(),FColor::White,28*Scale,Canvas->ClipY-192*Scale,GEngine->GetSmallFont(),1.25f*Scale);
    const float X=Canvas->ClipX/2,Y=Canvas->ClipY/2;
    DrawLine(X-7*Scale,Y,X+7*Scale,Y,FLinearColor::White);
    DrawLine(X,Y-7*Scale,X,Y+7*Scale,FLinearColor::White);
}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void ASFConstructionNetworkController::TickFiniteScenario()
{
    if (!IsLocalController() || GetNetMode()!=NM_Client || !FParse::Param(FCommandLine::Get(),TEXT("ConstructionNetworkScenario"))) { return; }
    auto* State = FindState(GetWorld());
    if (!bRoleAssigned || !GetPawn() || !State || !State->IsPairReady()) { return; }
    const FVector Position = GetPawn()->GetActorLocation();
    if (FMath::Abs(Position.X-(bBuilderRole ? -200.0 : 200.0))>1.0 || FMath::Abs(Position.Y+400.0)>1.0) { return; }
    if (ScenarioStarted==0) { ScenarioStarted=FPlatformTime::Seconds(); }
    if (!bBuilderRole && ScenarioStep==0)
    { SetControlRotation((FVector(-150,-175,50)-GetPawn()->GetActorLocation()).Rotation()); }
    if (FPlatformTime::Seconds()-ScenarioStarted>45) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED Timeout=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    if (ScenarioStep==100)
    {
        if (!FScreenshotRequest::IsScreenshotRequested() && FPaths::FileExists(ScenarioScreenshotPath))
        { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_CAPTURE_SAVED Builder=%d"),bBuilderRole); FPlatformMisc::RequestExitWithStatus(false,0); }
        return;
    }
    if (((bBuilderRole && ScenarioStep==24) || (!bBuilderRole && ScenarioStep==6))
        && State->GetSnapshot().Revision==6 && State->GetSnapshot().Members.IsEmpty() && State->HasCurrentCollision())
    {
        if (!bScenarioEmptyReported)
        { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_EMPTY_READY Builder=%d Revision=6 Members=0"),bBuilderRole); bScenarioEmptyReported=true; }
    }
    if (bBuilderRole && ScenarioStep==32
        && State->GetSnapshot().Revision==8 && State->GetSnapshot().Members.Num()==2 && State->HasCurrentCollision())
    {
        if (!bHasPlacementPreview || PreviewCell!=FIntVector(-4,-5,1) || !bHasRemovalPreview || RemovalCell!=FIntVector(-4,-5,0)) { return; }
        if (SupportCache.bPlacementAllowed || !SupportCache.bRemovalAllowed)
        { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED FinalSupportPreview=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SUPPORT_PREVIEW PlacementAllowed=0 RemovalAllowed=1 Revision=8"));
        ScenarioStep=33; ServerPlaceDoor(FIntVector(2,0,0),8,255); return;
    }
    if ((State->GetSnapshot().Revision==11 || State->GetSnapshot().Revision==12) && State->HasCurrentCollision())
    {
        const bool bOpen=State->GetSnapshot().Revision==12;
        bool& bReported=bOpen ? bScenarioDoorCapsuleOpenReported : bScenarioDoorCapsuleClosedReported;
        if (!bReported)
        {
            const auto* OwnedCharacter=Cast<ACharacter>(GetPawn());
            const auto* Capsule=OwnedCharacter ? OwnedCharacter->GetCapsuleComponent() : nullptr;
            bool bPass=Capsule!=nullptr;
            if (Capsule)
            {
                const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight());
                const FVector Front(150,-100,100), Back(150,150,100);
                FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkCapsulePassage),false,GetPawn());
                for (int32 Direction=0; Direction<2; ++Direction)
                {
                    FHitResult Hit; FIntVector Origin;
                    const bool bBlocked=GetWorld()->SweepSingleByChannel(Hit,Direction ? Back : Front,Direction ? Front : Back,FQuat::Identity,ECC_Pawn,Shape,Query);
                    bPass &= bOpen ? !bBlocked : bBlocked && Hit.GetActor()==State && State->GetDoorForComponent(Hit.GetComponent(),Origin) && Origin==FIntVector(2,0,0);
                }
            }
            if (!bPass)
            { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED CapsulePassage Open=%d"),bOpen); FPlatformMisc::RequestExitWithStatus(false,1); return; }
            UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_CAPSULE_PASSAGE Builder=%d Open=%d BothDirections=1 RadiusCm=%.1f HalfHeightCm=%.1f"),bBuilderRole,bOpen,Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight());
            bReported=true;
        }
    }
    if (((bBuilderRole && ScenarioStep==42) || (!bBuilderRole && ScenarioStep==8))
        && State->GetSnapshot().Revision==11 && State->GetSnapshot().Members.Num()==2 && State->GetSnapshot().Doors.Num()==1 && State->HasCurrentCollision())
    {
        if (!bBuilderRole && !bScenarioDoorCameraAimed)
        { SetControlRotation((FVector(150,25,100)-GetPawn()->GetActorLocation()).Rotation()); bScenarioDoorCameraAimed=true; return; }
        if (!bScenarioDoorClosedReported)
        { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_CLOSED_READY Builder=%d Revision=11"),bBuilderRole); bScenarioDoorClosedReported=true; }
    }
    if (((bBuilderRole && ScenarioStep==44) || (!bBuilderRole && ScenarioStep==8))
        && State->GetSnapshot().Revision==12 && State->GetSnapshot().Doors.Num()==1 && State->GetSnapshot().Doors[0].bOpen && State->HasCurrentCollision())
    {
        if (bBuilderRole && !bScenarioDoorOpenReported)
        {
            SetControlRotation((FVector(150,25,100)-GetPawn()->GetActorLocation()).Rotation());
            FVector Start; FRotator Aim; GetPlayerViewPoint(Start,Aim);
            FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkVirtualProof),false,GetPawn());
            FIntVector HitOrigin;
            const bool bPhysical=GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Aim.Vector()*1000,ECC_Visibility,Query)
                && Hit.GetActor()==State && State->GetDoorForComponent(Hit.GetComponent(),HitOrigin);
            if (bPhysical || !bHasDoorTarget || TargetDoorOrigin!=FIntVector(2,0,0)) { return; }
        }
        if (!bScenarioDoorOpenCaptureStarted)
        {
            bScenarioDoorOpenCaptureStarted=true;
            FString Path;
            if (FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkScreenshot="),Path))
            { ScenarioDoorOpenPath=FPaths::ChangeExtension(Path,TEXT("open.png")); FScreenshotRequest::RequestScreenshot(ScenarioDoorOpenPath,true,false); }
        }
        if (!bScenarioDoorOpenReported && (ScenarioDoorOpenPath.IsEmpty() || (!FScreenshotRequest::IsScreenshotRequested() && FPaths::FileExists(ScenarioDoorOpenPath))))
        { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_OPEN_READY Builder=%d Revision=12"),bBuilderRole); bScenarioDoorOpenReported=true;
            if (bBuilderRole) { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIRTUAL_DOOR_TARGET Revision=12 PhysicalDoorHit=0")); } }
    }
    if (!bBuilderRole && ScenarioStep==8 && State->GetSnapshot().Revision==13 && State->HasCurrentCollision())
    { ScenarioStep=9; ServerRemoveDoor(FIntVector(2,0,0),13); return; }
    if (((bBuilderRole && ScenarioStep==48) || (!bBuilderRole && ScenarioStep==10))
        && State->GetSnapshot().Revision==13 && State->GetSnapshot().Doors.Num()==1
        && !State->GetSnapshot().Doors[0].bOpen && State->HasCurrentCollision() && !bScenarioDoorRemoveReported)
    { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_REMOVE_READY Builder=%d Revision=13"),bBuilderRole); bScenarioDoorRemoveReported=true; }
    if (((bBuilderRole && ScenarioStep==50) || (!bBuilderRole && ScenarioStep==10))
        && State->GetSnapshot().Revision==14 && State->GetSnapshot().Members.Num()==2 && State->GetSnapshot().Doors.IsEmpty() && State->HasCurrentCollision())
    {
        if (bBuilderRole && bDoorSelected)
        {
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::One,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::One,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0))); return;
        }
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SCENARIO_SUCCESS Builder=%d Revision=14 Members=2 Doors=0"),bBuilderRole);
        if (FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkScreenshot="),ScenarioScreenshotPath))
        { ScenarioStep=100; FScreenshotRequest::RequestScreenshot(ScenarioScreenshotPath,true,false); }
        else { FPlatformMisc::RequestExitWithStatus(false,0); }
        return;
    }
    if (bBuilderRole && ScenarioStep==0)
    { ScenarioStep=1; ServerPlace(FIntVector(-4,-8,0),0,0,uint8(SFConstructionGeometry::IdentityRotation())); }
    else if (bBuilderRole && ScenarioStep==2)
    {
        // Aim through the third-person camera at the known floor cell; wait for
        // the ordinary camera/input stack rather than calling placement directly.
        SetControlRotation((FVector(-175,-175,0)-GetPawn()->GetActorLocation()).Rotation());
        if (!bHasPlacementPreview || PreviewCell!=FIntVector(-4,-4,0)) { return; }
        ScenarioStep=3;
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::B,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::B,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0)));
    }
    else if (bBuilderRole && ScenarioStep==4 && State->HasCurrentCollision())
    { ScenarioStep=5; ServerPlace(FIntVector(-4,-4,2),1,0,uint8(SFConstructionGeometry::IdentityRotation())); }
    else if (bBuilderRole && ScenarioStep==6)
    { ScenarioStep=7; ServerPlace(FIntVector(-4,-4,1),1,0,uint8(SFConstructionGeometry::IdentityRotation())); }
    else if (bBuilderRole && ScenarioStep==8)
    { ScenarioStep=9; ServerPlace(FIntVector(-3,-4,0),2,0,uint8(SFConstructionGeometry::IdentityRotation())); }
    else if (bBuilderRole && ScenarioStep==10 && State->GetSnapshot().Revision==3 && State->HasCurrentCollision())
    { ScenarioStep=11; ServerPlace(FIntVector(-3,-4,0),2,0,uint8(SFConstructionGeometry::IdentityRotation())); }
    else if (bBuilderRole && ScenarioStep==12)
    {
        // The owned runner opens this finite-test barrier only after both real
        // clients have projected the nonempty structure, so deletion is exercised.
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier)) { return; }
        if (!bHasRemovalPreview || RemovalCell!=FIntVector(-4,-4,0) || SupportCache.bRemovalAllowed)
        { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED SupportRemovalPreview=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SUPPORT_PREVIEW SupportingBaseAllowed=0 Revision=3"));
        ScenarioStep=13; ServerRemove(FIntVector(-4,-4,0),3);
    }
    else if (bBuilderRole && ScenarioStep==14 && State->GetSnapshot().Revision==3 && State->HasCurrentCollision())
    {
        SetControlRotation((FVector(-175,-175,75)-GetPawn()->GetActorLocation()).Rotation());
        if (!bHasRemovalPreview || RemovalCell!=FIntVector(-4,-4,1)) { return; }
        if (!SupportCache.bRemovalAllowed)
        { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED SupportedTopPreview=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SUPPORT_PREVIEW SupportedTopAllowed=1 Revision=3"));
        ScenarioStep=15;
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::X,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::X,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0)));
    }
    else if (bBuilderRole && ScenarioStep==16 && State->GetSnapshot().Revision==4 && State->HasCurrentCollision())
    { ScenarioStep=17; ServerRemove(FIntVector(-4,-4,1),4); }
    else if (bBuilderRole && ScenarioStep==18)
    { ScenarioStep=19; ServerRemove(FIntVector(-3,-4,0),3); }
    else if (bBuilderRole && ScenarioStep==20)
    { ScenarioStep=21; ServerRemove(FIntVector(-3,-4,0),4); }
    else if (bBuilderRole && ScenarioStep==22 && State->GetSnapshot().Revision==5 && State->HasCurrentCollision())
    { ScenarioStep=23; ServerRemove(FIntVector(-4,-4,0),5); }
    else if (!bBuilderRole && ScenarioStep==0 && State->GetSnapshot().Revision==6 && State->HasCurrentCollision())
    { ScenarioStep=1; ServerPlace(FIntVector(4,-4,0),6,0,uint8(SFConstructionGeometry::IdentityRotation())); }
    else if (!bBuilderRole && ScenarioStep==2)
    { ScenarioStep=3; ServerRemove(FIntVector(-4,-4,0),6); }
    else if (!bBuilderRole && ScenarioStep==4)
    { ScenarioStep=5; ServerPlaceDoor(FIntVector(2,0,0),6,0); }
    else if (!bBuilderRole && ScenarioStep==6 && State->GetSnapshot().Revision==11 && State->HasCurrentCollision())
    { ScenarioStep=7; ServerToggleDoor(FIntVector(2,0,0),11); }
    else if (bBuilderRole && ScenarioStep==24 && bScenarioEmptyReported)
    {
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier+TEXT(".empty"))) { return; }
        ScenarioStep=25; ServerPlace(FIntVector(-4,-4,0),6,255,uint8(SFConstructionGeometry::IdentityRotation()));
    }
    else if (bBuilderRole && ScenarioStep==26)
    { ScenarioStep=27; ServerPlace(FIntVector(-4,-4,0),6,0,255); }
    else if (bBuilderRole && ScenarioStep==28)
    {
        SetControlRotation((FVector(-175,-175,0)-GetPawn()->GetActorLocation()).Rotation());
        if (SelectedShape!=1)
        {
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Two,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Two,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0))); return;
        }
        if (!bHasPlacementPreview || PreviewCell!=FIntVector(-4,-4,0)) { return; }
        ScenarioStep=29;
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::B,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::B,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0)));
    }
    else if (bBuilderRole && ScenarioStep==30 && State->GetSnapshot().Revision==7 && State->HasCurrentCollision())
    {
        SetControlRotation((FVector(-175,-200,12.5)-GetPawn()->GetActorLocation()).Rotation());
        if (SelectedShape!=2)
        {
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Three,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Three,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0))); return;
        }
        if (SelectedYaw!=1)
        {
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::R,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::R,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0))); return;
        }
        if (!bHasPlacementPreview || PreviewCell!=FIntVector(-4,-5,0) || !bHasRemovalPreview || RemovalCell!=FIntVector(-4,-4,0)) { return; }
        ScenarioStep=31;
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::B,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::B,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0)));
    }
    else if (bBuilderRole && ScenarioStep==34)
    {
        SetControlRotation((FVector(125,25,0)-GetPawn()->GetActorLocation()).Rotation());
        if (!bDoorSelected)
        {
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Four,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Four,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0))); return;
        }
        if (!bHasPlacementPreview || PreviewCell!=FIntVector(2,0,0)) { return; }
        ScenarioStep=35;
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::B,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::B,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0)));
    }
    else if (bBuilderRole && ScenarioStep==36 && State->GetSnapshot().Revision==9 && State->HasCurrentCollision())
    { ScenarioStep=37; ServerPlace(FIntVector(2,-1,0),9,0,uint8(SFConstructionGeometry::IdentityRotation())); }
    else if (bBuilderRole && ScenarioStep==38)
    { ScenarioStep=39; ServerPlace(FIntVector(-6,-4,0),9,0,uint8(SFConstructionGeometry::IdentityRotation())); }
    else if (bBuilderRole && ScenarioStep==40 && State->GetSnapshot().Revision==10 && State->HasCurrentCollision())
    { ScenarioStep=41; ServerRemove(FIntVector(-6,-4,0),10); }
    else if (bBuilderRole && ScenarioStep==42 && bScenarioDoorClosedReported)
    {
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier+TEXT(".door-closed")) || !bHasDoorTarget || TargetDoorOrigin!=FIntVector(2,0,0)) { return; }
        ScenarioStep=43;
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0)));
    }
    else if (bBuilderRole && ScenarioStep==44 && bScenarioDoorOpenReported)
    {
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier+TEXT(".door-open")) || !bHasDoorTarget) { return; }
        ScenarioStep=45;
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0)));
    }
    else if (bBuilderRole && ScenarioStep==46 && bHasDoorTarget)
    {
        ScenarioStep=47;
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0)));
    }
    else if (bBuilderRole && ScenarioStep==48 && bScenarioDoorRemoveReported)
    {
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier+TEXT(".door-remove")) || !bHasDoorTarget || TargetDoorOrigin!=FIntVector(2,0,0)) { return; }
        ScenarioStep=49;
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::X,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
        InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::X,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0)));
    }
}
#endif
