#include "Construction/SFConstructionNetwork.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandboxFoundation.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Construction/SFConstructionSupport.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
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
#include "UObject/ConstructorHelpers.h"

namespace
{
const TCHAR* PlacementResultText(ESFConstructionNetworkResult Result)
{
    switch (Result)
    {
    case ESFConstructionNetworkResult::Accepted: return TEXT("Placed.");
    case ESFConstructionNetworkResult::PermissionDenied: return TEXT("Observer only: building permission required.");
    case ESFConstructionNetworkResult::Stale: return TEXT("The structure changed. Aim again and retry.");
    case ESFConstructionNetworkResult::InvalidCell: return TEXT("Outside the construction area.");
    case ESFConstructionNetworkResult::OutOfReach: return TEXT("Out of reach. Move closer.");
    case ESFConstructionNetworkResult::Occluded: return TEXT("Blocked view. Move so you can see the placement.");
    case ESFConstructionNetworkResult::Occupied: return TEXT("Space occupied. Aim at an empty neighboring cell.");
    case ESFConstructionNetworkResult::Unsupported: return TEXT("Needs support. Build from the ground or a supported block.");
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
bool CanonicalCube(const UStaticMesh* Mesh)
{
    if (!Mesh || Mesh->GetPathName() != TEXT("/Engine/BasicShapes/Cube.Cube")) { return false; }
    const auto* Body = Mesh->GetBodySetup();
    if (!Body || Body->CollisionTraceFlag == CTF_UseComplexAsSimple || Body->AggGeom.BoxElems.Num() != 1
        || Body->AggGeom.GetElementCount() != 1) { return false; }
    const auto& Box = Body->AggGeom.BoxElems[0];
    FSFConstructionSolid Solid;
    return FMath::IsNearlyEqual(Box.X,100.f) && FMath::IsNearlyEqual(Box.Y,100.f) && FMath::IsNearlyEqual(Box.Z,100.f)
        && Box.Center.IsNearlyZero() && Box.Rotation.IsNearlyZero()
        && SFConstructionGeometry::Build(0,SFConstructionGeometry::IdentityRotation(),Solid)
        && FBox(Solid.Vertices).Min.Equals(FVector::ZeroVector) && FBox(Solid.Vertices).Max.Equals(FVector(50));
}
}

bool FSFConstructionNetworkSnapshot::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    uint8 Version = 2;
    uint64 WireRevision = Revision;
    uint16 Count = uint16(Cells.Num());
    bOutSuccess = false;
    if (Ar.IsSaving() && (Cells.Num()>4096 || Revision!=uint64(Cells.Num()))) { return false; }
    Ar << Version;
    Ar << WireRevision;
    Ar.SerializeBits(&Count,13);
    if (Version!=2 || Count>4096 || WireRevision!=uint64(Count)) { return false; }
    TArray<FIntVector> Received;
    TSet<FIntVector> Unique;
    for (uint16 Index=0; Index<Count; ++Index)
    {
        const FIntVector Cell = Ar.IsSaving() ? Cells[Index] : FIntVector::ZeroValue;
        if (Ar.IsSaving() && (Cell.X < -20 || Cell.X >= 20 || Cell.Y < -20 || Cell.Y >= 20 || Cell.Z < 0 || Cell.Z >= 20)) { return false; }
        uint8 X = uint8(Cell.X+20), Y = uint8(Cell.Y+20), Z = uint8(Cell.Z);
        Ar.SerializeBits(&X,6); Ar.SerializeBits(&Y,6); Ar.SerializeBits(&Z,5);
        const FIntVector Address(int32(X)-20,int32(Y)-20,int32(Z));
        if (Ar.IsError() || X>=40 || Y>=40 || Z>=20 || Unique.Contains(Address)) { return false; }
        Unique.Add(Address);
        if (Ar.IsLoading()) { Received.Add(Address); }
    }
    if (Ar.IsError()) { return false; }
    if (Ar.IsLoading()) { Revision=WireRevision; Cells=MoveTemp(Received); }
    bOutSuccess = true;
    return true;
}

ASFConstructionNetworkState::ASFConstructionNetworkState()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ConstructionOrigin")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    CubeAsset = Mesh.Object;
}

void ASFConstructionNetworkState::BeginPlay()
{
    Super::BeginPlay();
    const auto* Mode = GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>();
    if (!HasAuthority() || !Mode || !Mode->IsAdmitted()) { return; }
    const auto* Settings = GetDefault<USFConstructionSandboxSettings>();
    FString Error;
    if (!Settings->Validate(Error) || !CanonicalCube(CubeAsset)) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_CONFIGURATION_FAILED %s"),*Error); return; }
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
    if (Candidate.Revision <= ProjectionRevision || Candidate.Cells.IsEmpty() || Candidate.Cells.Num()>4096
        || Candidate.Revision != uint64(Candidate.Cells.Num()) || !CanonicalCube(CubeAsset)
        || !GetActorTransform().Equals(FTransform::Identity)) { return false; }
    TSet<FIntVector> Addresses;
    for (const FIntVector& Cell : Candidate.Cells)
    {
        if (Cell.X < -20 || Cell.X >= 20 || Cell.Y < -20 || Cell.Y >= 20 || Cell.Z < 0 || Cell.Z >= 20
            || Addresses.Contains(Cell)) { return false; }
        Addresses.Add(Cell);
    }
    for (const auto& Pair : Cubes) { if (!Addresses.Contains(Pair.Key)) { return false; } }
    TMap<FIntVector,TObjectPtr<UStaticMeshComponent>> Staged;
    for (const FIntVector& Cell : Candidate.Cells)
    {
        if (Cubes.Contains(Cell)) { continue; }
        auto* Mesh = NewObject<UStaticMeshComponent>(this);
        Mesh->SetupAttachment(GetRootComponent());
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(CubeAsset);
        Mesh->SetRelativeLocation(FVector(Cell)*50.0+FVector(25));
        Mesh->SetRelativeScale3D(FVector(0.5));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCollisionObjectType(ECC_WorldStatic);
        Mesh->SetCollisionResponseToAllChannels(ECR_Block);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetHiddenInGame(true);
        Mesh->RegisterComponent();
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        if (!Mesh->IsRegistered() || !Mesh->IsPhysicsStateCreated())
        {
            Mesh->DestroyComponent();
            for (const auto& Pair : Staged) { Pair.Value->DestroyComponent(); }
            return false;
        }
        Staged.Add(Cell,Mesh);
    }
    for (const auto& Pair : Staged) { Cubes.Add(Pair.Key,Pair.Value); Pair.Value->SetHiddenInGame(false); }
    ProjectionRevision = Candidate.Revision;
    bProjectionReady = true;
    return true;
}

bool ASFConstructionNetworkState::HasCurrentCollision() const
{
    if (!bProjectionReady || Snapshot.Revision == 0 || ProjectionRevision != Snapshot.Revision
        || Cubes.Num()!=Snapshot.Cells.Num() || !GetActorTransform().Equals(FTransform::Identity)) { return false; }
    TInlineComponentArray<UPrimitiveComponent*> Primitives;
    GetComponents(Primitives);
    if (Primitives.Num()!=Cubes.Num()) { return false; }
    for (const FIntVector& Cell : Snapshot.Cells)
    {
        UStaticMeshComponent* Cube = Cubes.FindRef(Cell);
        const FVector Centre = FVector(Cell)*50.0+FVector(25);
        if (!Cube || !CanonicalCube(Cube->GetStaticMesh()) || !Cube->IsPhysicsStateCreated()
            || Cube->GetAttachParent()!=GetRootComponent() || Cube->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics
            || Cube->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block || Cube->GetCollisionResponseToChannel(ECC_Visibility)!=ECR_Block
            || !Cube->GetComponentLocation().Equals(Centre) || !Cube->GetComponentScale().Equals(FVector(0.5))
            || !Cube->GetComponentQuat().Equals(FQuat::Identity)) { return false; }
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkProjection),false);
        if (!Cube->LineTraceComponent(Hit,Centre+FVector(0,0,100),Centre,Query) || Hit.GetComponent()!=Cube) { return false; }
    }
    return true;
}

FString ASFConstructionNetworkState::GetContentDigest() const
{
    FSFConstructionSolid Solid;
    if (!SFConstructionGeometry::Build(0,SFConstructionGeometry::IdentityRotation(),Solid)) { return FString(); }
    FString Canonical = TEXT("SF_NETWORK_CUBES_V2|")+Solid.PayloadSha256+TEXT("|");
    TArray<FIntVector> Cells = Snapshot.Cells;
    Cells.Sort([](const FIntVector& A,const FIntVector& B) { return A.X!=B.X ? A.X<B.X : A.Y!=B.Y ? A.Y<B.Y : A.Z<B.Z; });
    for (const FIntVector& Cell : Cells) { Canonical += FString::Printf(TEXT("%d,%d,%d;"),Cell.X,Cell.Y,Cell.Z); }
    FTCHARToUTF8 Bytes(*Canonical);
    return LexToString(FBlake3::HashBuffer(Bytes.Get(),Bytes.Length()));
}

void ASFConstructionNetworkState::OnRep_Snapshot()
{
    if (GetNetMode() != NM_Client || GetClass() != StaticClass()) { return; }
    if (Snapshot.Revision==0 && Snapshot.Cells.IsEmpty()) { return; }
    bProjectionReady=false;
    if (Activate(Snapshot) && HasCurrentCollision())
    {
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_PROJECTED Revision=%llu Members=%d Collision=1 Content=%s"),Snapshot.Revision,Snapshot.Cells.Num(),*GetContentDigest());
    }
    else { bProjectionReady=false; UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_PROJECTION_FAILED")); }
}

ESFConstructionNetworkResult ASFConstructionNetworkState::Place(ASFConstructionNetworkController* Controller, FIntVector Cell, uint64 ExpectedRevision)
{
    const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>() : nullptr;
    if (!IsInGameThread() || !HasAuthority() || IsActorBeingDestroyed() || !bConfigurationReady || !Mode || !Mode->IsAdmitted() || GetClass() != StaticClass()
        || !IsValid(Controller) || Controller->IsActorBeingDestroyed() || !Controller->HasAuthority() || Controller->GetWorld() != GetWorld()) { return ESFConstructionNetworkResult::InvalidContext; }
    if (!Mode->IsBuilder(Controller)) { return ESFConstructionNetworkResult::PermissionDenied; }
    const APawn* Pawn = Controller->GetPawn();
    if (!IsValid(Pawn) || !Pawn->HasAuthority() || Pawn->GetController() != Controller || Pawn->GetOwner() != Controller
        || Pawn->GetWorld() != GetWorld() || Pawn->IsActorBeingDestroyed()) { return ESFConstructionNetworkResult::InvalidContext; }
    if (ExpectedRevision != Snapshot.Revision) { return ESFConstructionNetworkResult::Stale; }
    if (Cell.X < -20 || Cell.X >= 20 || Cell.Y < -20 || Cell.Y >= 20 || Cell.Z < 0 || Cell.Z >= 20) { return ESFConstructionNetworkResult::InvalidCell; }
    if (Snapshot.Cells.Contains(Cell)) { return ESFConstructionNetworkResult::Occupied; }
    if (Snapshot.Cells.Num() >= MaximumMembers) { return ESFConstructionNetworkResult::LimitReached; }
    if (Snapshot.Revision > 0 && !HasCurrentCollision()) { return ESFConstructionNetworkResult::ProjectionFailed; }
    const AActor* Foundation = nullptr;
    if (!SFConstructionSandboxFoundation::MatchesKnownPlaneGeometry(GetWorld(),&Foundation)) { return ESFConstructionNetworkResult::InvalidContext; }
    const FVector Centre = FVector(Cell)*50.0+FVector(25);
    if (Pawn->GetActorLocation().ContainsNaN() || FVector::DistSquared(Pawn->GetActorLocation(),Centre) > FMath::Square(ReachCentimetres)) { return ESFConstructionNetworkResult::OutOfReach; }
    FVector Eyes; FRotator Rotation;
    Pawn->GetActorEyesViewPoint(Eyes,Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkAdmission),false,Pawn);
    FHitResult Hit;
    if (Eyes.ContainsNaN() || GetWorld()->LineTraceSingleByChannel(Hit,Eyes,Centre,ECC_Visibility,Query)) { return ESFConstructionNetworkResult::Occluded; }
    // Ignore only the already validated immutable fixture floor. Both players still block.
    FCollisionQueryParams Clearance(SCENE_QUERY_STAT(ConstructionNetworkClearance),false);
    Clearance.AddIgnoredComponent(Cast<UPrimitiveComponent>(Foundation->GetRootComponent()));
    // Existing cubes have proven canonical transforms and unique integer addresses.
    // Distinct full grid cells cannot overlap; do not exempt any other component.
    for (const auto& Pair : Cubes) { Clearance.AddIgnoredComponent(Pair.Value.Get()); }
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(Overlaps,Centre,FQuat::Identity,FCollisionObjectQueryParams::AllObjects,FCollisionShape::MakeBox(FVector(25)),Clearance);
    for (const auto& Overlap : Overlaps)
    {
        const auto* Component = Overlap.GetComponent();
        if (Component && Component->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block) { return ESFConstructionNetworkResult::Occupied; }
    }
    FSFConstructionNetworkSnapshot Candidate;
    Candidate.Revision = Snapshot.Revision+1;
    Candidate.Cells = Snapshot.Cells;
    Candidate.Cells.Add(Cell);
    Candidate.Cells.Sort([](const FIntVector& A,const FIntVector& B) { return A.X!=B.X ? A.X<B.X : A.Y!=B.Y ? A.Y<B.Y : A.Z<B.Z; });
    TArray<FSFConstructionSupportMember> Members;
    for (const FIntVector& Address : Candidate.Cells)
    {
        Members.Add({Address,0,SFConstructionGeometry::IdentityRotation(),1.0,
            SFConstructionSandboxFoundation::TouchesPlane(Address,0,SFConstructionGeometry::IdentityRotation())});
    }
    TArray<FSFConstructionSupportResult> Support;
    if (!SFConstructionSupport::Evaluate(Members,FSFConstructionSupportProfile(),Support)) { return ESFConstructionNetworkResult::Unsupported; }
    for (const auto& Result : Support) { if (Result.State == ESFConstructionSupportState::Unsupported) { return ESFConstructionNetworkResult::Unsupported; } }
    if (!Activate(Candidate)) { return ESFConstructionNetworkResult::ProjectionFailed; }
    Snapshot = Candidate; // Publish only after synchronous server physics activation.
    ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_ACCEPT Revision=%llu Members=%d Cell=%s Content=%s"),Snapshot.Revision,Snapshot.Cells.Num(),*Cell.ToString(),*GetContentDigest());
    return ESFConstructionNetworkResult::Accepted;
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
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_READY UnlimitedDevelopmentStock=1 Shapes=CanonicalCube Support=RoughTimber Persistence=0"));
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
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_REQUEST Cell=%s"),*PreviewCell.ToString());
    ServerPlace(PreviewCell,State->GetSnapshot().Revision);
}

void ASFConstructionNetworkController::RefreshPlacementPreview(bool bDrawOutline)
{
    bHasPlacementPreview=false;
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
    bHasPlacementPreview=true;
    const FVector Centre=FVector(PreviewCell)*50.0+FVector(25);
    const TCHAR* BlockReason=nullptr;
    if (PreviewCell.X < -20 || PreviewCell.X >= 20 || PreviewCell.Y < -20 || PreviewCell.Y >= 20 || PreviewCell.Z < 0 || PreviewCell.Z >= 20)
    { BlockReason=PlacementResultText(ESFConstructionNetworkResult::InvalidCell); }
    else if (FVector::DistSquared(GetPawn()->GetActorLocation(),Centre)>FMath::Square(State->GetReachCentimetres()))
    { BlockReason=PlacementResultText(ESFConstructionNetworkResult::OutOfReach); }
    else if (State->GetSnapshot().Cells.Contains(PreviewCell)) { BlockReason=PlacementResultText(ESFConstructionNetworkResult::Occupied); }
    else if (State->GetSnapshot().Cells.Num()>=State->GetMaximumMembers()) { BlockReason=PlacementResultText(ESFConstructionNetworkResult::LimitReached); }
    PreviewStatus=FString::Printf(TEXT("Target %d, %d, %d | %s"),PreviewCell.X,PreviewCell.Y,PreviewCell.Z,
        BlockReason ? BlockReason : TEXT("Click or B to request placement."));
    if (bDrawOutline)
    { DrawDebugBox(GetWorld(),Centre,FVector(25),BlockReason ? FColor::Red : FColor(255,190,60),false,-1.f,0,2.f); }
}

void ASFConstructionNetworkController::ServerPlace_Implementation(FIntVector Cell, uint64 ExpectedRevision)
{
    auto* State = FindState(GetWorld());
    ESFConstructionNetworkResult Result = ESFConstructionNetworkResult::InvalidContext;
    if (State && LastRequestFrame != GFrameCounter)
    {
        LastRequestFrame = GFrameCounter;
        Result = State->Place(this,Cell,ExpectedRevision);
    }
    const uint64 Revision = State ? State->GetSnapshot().Revision : 0;
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_RESULT Builder=%d Result=%d Revision=%llu Members=%d Collision=%d"),bBuilderRole,int32(Result),Revision,State ? State->GetSnapshot().Cells.Num() : 0,State && State->HasCurrentCollision());
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
            || (ScenarioStep==11 && Result==ESFConstructionNetworkResult::Stale && Revision==3))
        : (ScenarioStep==1 && Result==ESFConstructionNetworkResult::PermissionDenied && Revision==3);
    if (!bExpected) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED Step=%d Result=%d Revision=%llu"),ScenarioStep,int32(Result),Revision); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    ++ScenarioStep;
    if ((bBuilderRole && ScenarioStep==12) || (!bBuilderRole && ScenarioStep==2))
    {
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SCENARIO_SUCCESS Builder=%d Revision=3 Members=3"),bBuilderRole);
        if (FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkScreenshot="),ScenarioScreenshotPath))
        { ScenarioStep=100; FScreenshotRequest::RequestScreenshot(ScenarioScreenshotPath,true,false); }
        else { FPlatformMisc::RequestExitWithStatus(false,0); }
    }
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
    return FString::Printf(TEXT("CONSTRUCTION | %s\nClick / B: place cube    WASD: move    Mouse: look    Space: jump\n%s\nServer: %s\nMembers: %d / %d | Reach: %.1f m\nAmber outline: target only; server checks support and clearance."),
        !bRoleAssigned ? TEXT("CONNECTING") : bBuilderRole ? TEXT("BUILDER") : TEXT("OBSERVER"),
        *PreviewStatus,*LastPlacementStatus,State ? State->GetSnapshot().Cells.Num() : 0,
        State ? State->GetMaximumMembers() : 0,State ? State->GetReachCentimetres()/100.0 : 0.0);
}

void ASFConstructionNetworkHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto* Controller=Cast<ASFConstructionNetworkController>(GetOwningPlayerController());
    if (!Controller || !Canvas || !GEngine) { return; }
    const float Scale=FMath::Clamp(Canvas->ClipX/1280.f,0.5f,1.5f);
    DrawRect(FLinearColor(0.02f,0.03f,0.04f,0.88f),16*Scale,Canvas->ClipY-158*Scale,820*Scale,142*Scale);
    DrawText(Controller->GetBuildStatusText(),FColor::White,28*Scale,Canvas->ClipY-148*Scale,GEngine->GetSmallFont(),1.25f*Scale);
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
    if (bBuilderRole && ScenarioStep==0)
    { ScenarioStep=1; ServerPlace(FIntVector(-4,-8,0),0); }
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
    { ScenarioStep=5; ServerPlace(FIntVector(-4,-4,2),1); }
    else if (bBuilderRole && ScenarioStep==6)
    { ScenarioStep=7; ServerPlace(FIntVector(-4,-4,1),1); }
    else if (bBuilderRole && ScenarioStep==8)
    { ScenarioStep=9; ServerPlace(FIntVector(-3,-4,0),2); }
    else if (bBuilderRole && ScenarioStep==10 && State->GetSnapshot().Revision==3 && State->HasCurrentCollision())
    { ScenarioStep=11; ServerPlace(FIntVector(-3,-4,0),2); }
    else if (!bBuilderRole && ScenarioStep==0 && State->GetSnapshot().Revision==3 && State->HasCurrentCollision())
    { ScenarioStep=1; ServerPlace(FIntVector(4,-4,0),3); }
}
#endif
