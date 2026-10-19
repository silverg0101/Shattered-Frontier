#include "Construction/SFConstructionNetwork.h"
#include "SFConstructionNetworkProjection.h"
#include "ProceduralMeshComponent.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandboxFoundation.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Construction/SFConstructionSupport.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
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
    case ESFConstructionNetworkResult::PermissionDenied: return TEXT("Observer only: building permission required.");
    case ESFConstructionNetworkResult::Stale: return TEXT("The structure changed. Aim again and retry.");
    case ESFConstructionNetworkResult::InvalidCell: return TEXT("Outside the construction area.");
    case ESFConstructionNetworkResult::OutOfReach: return TEXT("Out of reach. Move closer.");
    case ESFConstructionNetworkResult::Occluded: return TEXT("Blocked view. Move so you can see the placement.");
    case ESFConstructionNetworkResult::Occupied: return TEXT("Space occupied. Aim at an empty neighboring cell.");
    case ESFConstructionNetworkResult::Unsupported: return TEXT("Needs support. This edit would leave unsupported blocks.");
    case ESFConstructionNetworkResult::LimitReached: return TEXT("Construction member limit reached.");
    case ESFConstructionNetworkResult::ProjectionFailed: return TEXT("Construction collision unavailable. Placement was not applied.");
    default: return TEXT("Construction unavailable. Wait for the server and try again.");
    }
}

ASFConstructionNetworkState* FindState(UWorld* World)
{
    if (!World) { return nullptr; }
    for (TActorIterator<ASFConstructionNetworkState> It(World); It; ++It) { return *It; }
    return nullptr;
}

}

bool FSFConstructionNetworkSnapshot::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    uint8 Version=4;
    uint64 WireRevision=Revision;
    uint16 Count=uint16(Members.Num());
    bOutSuccess=false;
    if (Ar.IsSaving() && (Members.Num()>4096 || Revision<uint64(Members.Num()))) { return false; }
    Ar << Version; Ar << WireRevision; Ar.SerializeBits(&Count,13);
    if (Version!=4 || Count>4096 || WireRevision<uint64(Count)) { return false; }
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
    if (Ar.IsError()) { return false; }
    if (Ar.IsLoading()) { Revision=WireRevision; Members=MoveTemp(Received); }
    bOutSuccess=true;
    return true;
}

ASFConstructionNetworkState::ASFConstructionNetworkState()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ConstructionOrigin")));
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
    bConfigurationReady = true;
}

void ASFConstructionNetworkState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFConstructionNetworkState, Snapshot);
    DOREPLIFETIME(ASFConstructionNetworkState, bPairReady);
    DOREPLIFETIME(ASFConstructionNetworkState, ReachCentimetres);
    DOREPLIFETIME(ASFConstructionNetworkState, MaximumMembers);
}

void ASFConstructionNetworkState::AdmitPair()
{
    auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>() : nullptr;
    if (HasAuthority() && Mode && Mode->IsAdmitted() && Mode->GetNumPlayers() >= 2)
    { bPairReady = true; ForceNetUpdate(); UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_PAIR_READY Players=%d"),Mode->GetNumPlayers()); }
}

bool ASFConstructionNetworkState::Activate(const FSFConstructionNetworkSnapshot& Candidate)
{
    if (Candidate.Revision<=ProjectionRevision || Candidate.Members.Num()>4096 || Candidate.Revision<uint64(Candidate.Members.Num())
        || !GetActorTransform().Equals(FTransform::Identity)) { return false; }
    TSet<FIntVector> Addresses;
    for (const auto& M : Candidate.Members)
    {
        const auto& C=M.Cell;
        if (C.X < -20 || C.X >= 20 || C.Y < -20 || C.Y >= 20 || C.Z < 0 || C.Z >= 20 || Addresses.Contains(C)
            || M.Shape>=SFConstructionGeometry::ShapeCount || M.Rotation>=SFConstructionGeometry::RotationCount) { return false; }
        Addresses.Add(C);
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
    // Every replacement is ready before an old component can be retired.
    for (auto It=Meshes.CreateIterator(); It; ++It)
    {
        if (!Addresses.Contains(It.Key()) || Staged.Contains(It.Key()))
        { It.Value()->DestroyComponent(); It.RemoveCurrent(); }
    }
    for (const auto& Pair : Staged) { Meshes.Add(Pair.Key,Pair.Value); Pair.Value->SetHiddenInGame(false); }
    ProjectedMembers.Reset();
    for (const auto& M : Candidate.Members) { ProjectedMembers.Add(M.Cell,M); }
    ProjectionRevision=Candidate.Revision;
    bProjectionReady=true;
    return true;
}

bool ASFConstructionNetworkState::HasCurrentCollision() const
{
    if (!bProjectionReady || Snapshot.Revision==0 || ProjectionRevision!=Snapshot.Revision || Meshes.Num()!=Snapshot.Members.Num()
        || ProjectedMembers.Num()!=Snapshot.Members.Num() || !GetActorTransform().Equals(FTransform::Identity)) { return false; }
    TInlineComponentArray<UPrimitiveComponent*> Primitives; GetComponents(Primitives);
    if (Primitives.Num()!=Meshes.Num()) { return false; }
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
    return true;
}

bool ASFConstructionNetworkState::GetCellForComponent(const UPrimitiveComponent* Component, FIntVector& OutCell) const
{
    for (const auto& Pair : Meshes)
    { if (Pair.Value.Get()==Component) { OutCell=Pair.Key; return true; } }
    return false;
}

FString ASFConstructionNetworkState::GetContentDigest() const
{
    FString Canonical=TEXT("SF_NETWORK_MEMBERS_V4|");
    auto Members=Snapshot.Members;
    Members.Sort([](const auto& A,const auto& B) { return A.Cell.X!=B.Cell.X ? A.Cell.X<B.Cell.X : A.Cell.Y!=B.Cell.Y ? A.Cell.Y<B.Cell.Y : A.Cell.Z<B.Cell.Z; });
    for (const auto& M : Members)
    {
        FSFConstructionSolid Solid;
        if (!SFConstructionGeometry::Build(M.Shape,M.Rotation,Solid)) { return FString(); }
        Canonical+=FString::Printf(TEXT("%d,%d,%d,%u,%u,%s;"),M.Cell.X,M.Cell.Y,M.Cell.Z,M.Shape,M.Rotation,*Solid.PayloadSha256);
    }
    FTCHARToUTF8 Bytes(*Canonical);
    return LexToString(FBlake3::HashBuffer(Bytes.Get(),Bytes.Length()));
}

void ASFConstructionNetworkState::OnRep_Snapshot()
{
    if (GetNetMode() != NM_Client || GetClass() != StaticClass()) { return; }
    if (Snapshot.Revision==0 && Snapshot.Members.IsEmpty()) { return; }
    bProjectionReady=false;
    if (Activate(Snapshot) && HasCurrentCollision())
    {
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_PROJECTED Revision=%llu Members=%d Collision=1 Content=%s"),Snapshot.Revision,Snapshot.Members.Num(),*GetContentDigest());
    }
    else { bProjectionReady=false; UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_PROJECTION_FAILED")); }
}

ESFConstructionNetworkResult ASFConstructionNetworkState::Edit(ASFConstructionNetworkController* Controller, FIntVector Cell, uint64 ExpectedRevision, bool bRemove, uint8 Shape, uint8 Rotation)
{
    const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>() : nullptr;
    if (!IsInGameThread() || !HasAuthority() || IsActorBeingDestroyed() || !bConfigurationReady || !Mode || !Mode->IsAdmitted() || GetClass() != StaticClass()
        || !IsValid(Controller) || Controller->IsActorBeingDestroyed() || !Controller->HasAuthority() || Controller->GetWorld() != GetWorld()) { return ESFConstructionNetworkResult::InvalidContext; }
    if (!Mode->IsBuilder(Controller)) { return ESFConstructionNetworkResult::PermissionDenied; }
    const APawn* Pawn = Controller->GetPawn();
    if (!IsValid(Pawn) || !Pawn->HasAuthority() || Pawn->GetController() != Controller || Pawn->GetOwner() != Controller
        || Pawn->GetWorld() != GetWorld() || Pawn->IsActorBeingDestroyed()) { return ESFConstructionNetworkResult::InvalidContext; }
    if (!bRemove && (Shape>=SFConstructionGeometry::ShapeCount || Rotation>=SFConstructionGeometry::RotationCount)) { return ESFConstructionNetworkResult::InvalidPiece; }
    if (ExpectedRevision != Snapshot.Revision) { return ESFConstructionNetworkResult::Stale; }
    if (Snapshot.Revision==MAX_uint64) { return ESFConstructionNetworkResult::InvalidContext; }
    if (Cell.X < -20 || Cell.X >= 20 || Cell.Y < -20 || Cell.Y >= 20 || Cell.Z < 0 || Cell.Z >= 20) { return ESFConstructionNetworkResult::InvalidCell; }
    if (bRemove && Snapshot.Find(Cell)==INDEX_NONE) { return ESFConstructionNetworkResult::NotFound; }
    if (!bRemove && Snapshot.Find(Cell)!=INDEX_NONE) { return ESFConstructionNetworkResult::Occupied; }
    if (!bRemove && Snapshot.Members.Num() >= MaximumMembers) { return ESFConstructionNetworkResult::LimitReached; }
    if (Snapshot.Revision > 0 && !HasCurrentCollision()) { return ESFConstructionNetworkResult::ProjectionFailed; }
    const AActor* Foundation = nullptr;
    if (!SFConstructionSandboxFoundation::MatchesKnownPlaneGeometry(GetWorld(),&Foundation)) { return ESFConstructionNetworkResult::InvalidContext; }
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
    FSFConstructionNetworkSnapshot Candidate;
    Candidate.Revision = Snapshot.Revision+1;
    Candidate.Members = Snapshot.Members;
    if (bRemove) { Candidate.Members.RemoveAt(Candidate.Find(Cell)); }
    else
    {
        FSFConstructionNetworkMember Member; Member.Cell=Cell; Member.Shape=Shape; Member.Rotation=Rotation;
        Candidate.Members.Add(Member);
    }
    Candidate.Members.Sort([](const auto& A,const auto& B) { return A.Cell.X!=B.Cell.X ? A.Cell.X<B.Cell.X : A.Cell.Y!=B.Cell.Y ? A.Cell.Y<B.Cell.Y : A.Cell.Z<B.Cell.Z; });
    TArray<FSFConstructionSupportMember> Members;
    for (const auto& Member : Candidate.Members)
    {
        Members.Add({Member.Cell,Member.Shape,Member.Rotation,1.0,
            SFConstructionSandboxFoundation::TouchesPlane(Member.Cell,Member.Shape,Member.Rotation)});
    }
    TArray<FSFConstructionSupportResult> Support;
    if (!SFConstructionSupport::Evaluate(Members,FSFConstructionSupportProfile(),Support)) { return ESFConstructionNetworkResult::Unsupported; }
    for (const auto& Result : Support) { if (Result.State == ESFConstructionSupportState::Unsupported) { return ESFConstructionNetworkResult::Unsupported; } }
    if (!Activate(Candidate)) { return ESFConstructionNetworkResult::ProjectionFailed; }
    Snapshot = Candidate; // Publish only after synchronous server physics activation.
    ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_EDIT Action=%s Revision=%llu Members=%d Cell=%s Content=%s Shape=%u Rotation=%u"),bRemove ? TEXT("Remove") : TEXT("Place"),Snapshot.Revision,Snapshot.Members.Num(),*Cell.ToString(),*GetContentDigest(),Shape,Rotation);
    return bRemove ? ESFConstructionNetworkResult::Removed : ESFConstructionNetworkResult::Accepted;
}

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
    InputComponent->BindKey(EKeys::R,IE_Pressed,this,&ASFConstructionNetworkController::RotatePiece);
}

void ASFConstructionNetworkController::SelectCube() { SelectShape(0); }
void ASFConstructionNetworkController::SelectSlab() { SelectShape(1); }
void ASFConstructionNetworkController::SelectSlope() { SelectShape(2); }
void ASFConstructionNetworkController::SelectShape(uint8 Shape)
{
    SelectedShape=Shape; SelectedYaw=0; SelectedRotation=uint8(SFConstructionGeometry::IdentityRotation());
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
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_REQUEST Cell=%s Shape=%u Rotation=%u FromMember=%d"),*PreviewCell.ToString(),SelectedShape,SelectedRotation,bHasRemovalPreview);
    ServerPlace(PreviewCell,State->GetSnapshot().Revision,SelectedShape,SelectedRotation);
}

void ASFConstructionNetworkController::RefreshPlacementPreview(bool bDrawOutline)
{
    bHasPlacementPreview=false;
    bHasRemovalPreview=false;
    auto* State = FindState(GetWorld());
    if (!IsLocalController() || GetNetMode()!=NM_Client || !GetPawn() || !bRoleAssigned || !State
        || State->GetReachCentimetres()<=0 || State->GetMaximumMembers()<=0)
    { PreviewStatus=TEXT("Waiting for server construction state..."); return; }
    if (!bBuilderRole) { PreviewStatus=TEXT("Observer: watch the builder's placements."); return; }
    if (State->GetSnapshot().Revision>0 && !State->IsProjectionCurrent())
    { PreviewStatus=TEXT("Waiting for local construction collision..."); return; }
    FVector Start; FRotator Rotation; GetPlayerViewPoint(Start,Rotation);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkView),false,GetPawn());
    // Camera offset plus half a cell diagonal covers the server's pawn-centre reach sphere.
    const double TraceLength=State->GetReachCentimetres()+FVector::Dist(Start,GetPawn()->GetActorLocation())+FMath::Sqrt(3.0)*25.0;
    if (!GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Rotation.Vector()*TraceLength,ECC_Visibility,Query))
    { PreviewStatus=TEXT("Aim at the ground or a block face within reach."); return; }
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
    const FVector Centre=FVector(PreviewCell)*50.0+FVector(25);
    const TCHAR* BlockReason=nullptr;
    if (PreviewCell.X < -20 || PreviewCell.X >= 20 || PreviewCell.Y < -20 || PreviewCell.Y >= 20 || PreviewCell.Z < 0 || PreviewCell.Z >= 20)
    { BlockReason=PlacementResultText(ESFConstructionNetworkResult::InvalidCell); }
    else if (FVector::DistSquared(GetPawn()->GetActorLocation(),Centre)>FMath::Square(State->GetReachCentimetres()))
    { BlockReason=PlacementResultText(ESFConstructionNetworkResult::OutOfReach); }
    else if (State->GetSnapshot().Find(PreviewCell)!=INDEX_NONE) { BlockReason=PlacementResultText(ESFConstructionNetworkResult::Occupied); }
    else if (State->GetSnapshot().Members.Num()>=State->GetMaximumMembers()) { BlockReason=PlacementResultText(ESFConstructionNetworkResult::LimitReached); }
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
    { DrawDebugBox(GetWorld(),FVector(RemovalCell)*50.0+FVector(25),FVector(25),FColor::Cyan,false,-1.f,0,2.f); }
}

void ASFConstructionNetworkController::RemoveFromView()
{
    RefreshPlacementPreview(false);
    auto* State=FindState(GetWorld());
    if (!bHasRemovalPreview || !State) { LastPlacementStatus=TEXT("Aim at a construction block to remove it."); return; }
    LastPlacementStatus=TEXT("Removal requested. Waiting for server...");
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_REMOVE Cell=%s"),*RemovalCell.ToString());
    ServerRemove(RemovalCell,State->GetSnapshot().Revision);
}

void ASFConstructionNetworkController::ServerPlace_Implementation(FIntVector Cell, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation)
{ ProcessEdit(Cell,ExpectedRevision,false,Shape,Rotation); }

void ASFConstructionNetworkController::ServerRemove_Implementation(FIntVector Cell, uint64 ExpectedRevision)
{ ProcessEdit(Cell,ExpectedRevision,true,0,0); }

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
            || (ScenarioStep==31 && Result==ESFConstructionNetworkResult::Accepted && Revision==8))
        : ((ScenarioStep==1 || ScenarioStep==3) && Result==ESFConstructionNetworkResult::PermissionDenied && Revision==6);
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
    return FString::Printf(TEXT("CONSTRUCTION | %s | DEV: UNLIMITED STOCK\nClick / B: place    Right click / X: remove    WASD: move    Space: jump\n1 Cube | 2 Half slab | 3 Slope | R Rotate | Selected: %s (%u deg)\n%s\nLast action: %s\nMembers: %d / %d | Reach: %.1f m\nAmber: placement target | Cyan: removal target | Server validates edits."),
        !bRoleAssigned ? TEXT("CONNECTING") : bBuilderRole ? TEXT("BUILDER") : TEXT("OBSERVER"),
        SelectedShape==0 ? TEXT("Cube") : SelectedShape==1 ? TEXT("Half slab") : TEXT("Slope"),SelectedYaw*90,
        *PreviewStatus,*LastPlacementStatus,State ? State->GetSnapshot().Members.Num() : 0,
        State ? State->GetMaximumMembers() : 0,State ? State->GetReachCentimetres()/100.0 : 0.0);
}

void ASFConstructionNetworkHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto* Controller=Cast<ASFConstructionNetworkController>(GetOwningPlayerController());
    if (!Controller || !Canvas || !GEngine) { return; }
    const float Scale=FMath::Clamp(Canvas->ClipX/1280.f,0.5f,1.5f);
    DrawRect(FLinearColor(0.02f,0.03f,0.04f,0.88f),16*Scale,Canvas->ClipY-180*Scale,820*Scale,164*Scale);
    DrawText(Controller->GetBuildStatusText(),FColor::White,28*Scale,Canvas->ClipY-170*Scale,GEngine->GetSmallFont(),1.25f*Scale);
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
    if (((bBuilderRole && ScenarioStep==24) || (!bBuilderRole && ScenarioStep==4))
        && State->GetSnapshot().Revision==6 && State->GetSnapshot().Members.IsEmpty() && State->HasCurrentCollision())
    {
        if (!bScenarioEmptyReported)
        { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_EMPTY_READY Builder=%d Revision=6 Members=0"),bBuilderRole); bScenarioEmptyReported=true; }
    }
    if (((bBuilderRole && ScenarioStep==32) || (!bBuilderRole && ScenarioStep==4))
        && State->GetSnapshot().Revision==8 && State->GetSnapshot().Members.Num()==2 && State->HasCurrentCollision())
    {
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SCENARIO_SUCCESS Builder=%d Revision=8 Members=2"),bBuilderRole);
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
        ScenarioStep=13; ServerRemove(FIntVector(-4,-4,0),3);
    }
    else if (bBuilderRole && ScenarioStep==14 && State->GetSnapshot().Revision==3 && State->HasCurrentCollision())
    {
        SetControlRotation((FVector(-175,-175,75)-GetPawn()->GetActorLocation()).Rotation());
        if (!bHasRemovalPreview || RemovalCell!=FIntVector(-4,-4,1)) { return; }
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
}
#endif
