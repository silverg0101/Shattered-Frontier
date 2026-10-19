#include "Construction/SFConstructionNetwork.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandboxFoundation.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
ASFConstructionNetworkState* FindState(UWorld* World)
{
    if (!World) { return nullptr; }
    for (TActorIterator<ASFConstructionNetworkState> It(World); It; ++It) { return *It; }
    return nullptr;
}
bool CanonicalCube(const UStaticMeshComponent* Cube)
{
    if (!Cube || !Cube->GetStaticMesh() || Cube->GetStaticMesh()->GetPathName() != TEXT("/Engine/BasicShapes/Cube.Cube")) { return false; }
    const auto* Body = Cube->GetStaticMesh()->GetBodySetup();
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

ASFConstructionNetworkState::ASFConstructionNetworkState()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    Cube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CanonicalCube"));
    SetRootComponent(Cube);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Cube->SetStaticMesh(Mesh.Object);
    Cube->SetWorldScale3D(FVector(0.5));
    Cube->SetCollisionObjectType(ECC_WorldStatic);
    Cube->SetCollisionResponseToAllChannels(ECR_Block);
    Cube->SetGenerateOverlapEvents(false);
    Cube->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Cube->SetHiddenInGame(true);
}

void ASFConstructionNetworkState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFConstructionNetworkState, Snapshot);
    DOREPLIFETIME(ASFConstructionNetworkState, bPairReady);
}

void ASFConstructionNetworkState::AdmitPair()
{
    auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>() : nullptr;
    if (HasAuthority() && Mode && Mode->IsAdmitted() && Mode->GetNumPlayers() >= 2)
    { bPairReady = true; ForceNetUpdate(); UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_PAIR_READY Players=%d"),Mode->GetNumPlayers()); }
}

bool ASFConstructionNetworkState::Activate(const FSFConstructionNetworkSnapshot& Candidate)
{
    if (Candidate.Revision != 1 || Candidate.Cell.Z != 0 || Candidate.Cell.X < -20 || Candidate.Cell.X >= 20
        || Candidate.Cell.Y < -20 || Candidate.Cell.Y >= 20 || !CanonicalCube(Cube)) { return false; }
    const FTransform Previous = GetActorTransform();
    SetActorTransform(FTransform(FQuat::Identity,FVector(Candidate.Cell)*50.0+FVector(25),FVector(0.5)));
    Cube->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (!Cube->IsPhysicsStateCreated())
    {
        Cube->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SetActorTransform(Previous);
        return false;
    }
    Cube->SetHiddenInGame(false);
    bProjectionReady = true;
    return true;
}

bool ASFConstructionNetworkState::HasCurrentCollision() const
{
    if (!bProjectionReady || Snapshot.Revision != 1 || !CanonicalCube(Cube) || !Cube->IsPhysicsStateCreated()
        || Cube->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics
        || !GetActorLocation().Equals(FVector(Snapshot.Cell)*50.0+FVector(25))
        || !GetActorScale3D().Equals(FVector(0.5)) || !GetActorQuat().Equals(FQuat::Identity)) { return false; }
    FHitResult Hit;
    const FVector Centre = GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkProjection),false);
    return Cube->LineTraceComponent(Hit,Centre+FVector(0,0,100),Centre,Query) && Hit.GetComponent() == Cube;
}

void ASFConstructionNetworkState::OnRep_Snapshot()
{
    if (GetNetMode() != NM_Client || GetClass() != StaticClass()) { return; }
    if (Snapshot.Revision == 1 && Activate(Snapshot) && HasCurrentCollision())
    {
        FSFConstructionSolid Solid;
        SFConstructionGeometry::Build(0,SFConstructionGeometry::IdentityRotation(),Solid);
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_PROJECTED Revision=1 Cell=%s Collision=1 Payload=%s"),*Snapshot.Cell.ToString(),*Solid.PayloadSha256);
    }
    else { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_PROJECTION_FAILED")); }
}

ESFConstructionNetworkResult ASFConstructionNetworkState::Place(ASFConstructionNetworkController* Controller, FIntVector Cell, uint64 ExpectedRevision)
{
    const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>() : nullptr;
    if (!IsInGameThread() || !HasAuthority() || !Mode || !Mode->IsAdmitted() || GetClass() != StaticClass()
        || !IsValid(Controller) || !Controller->HasAuthority() || Controller->GetWorld() != GetWorld()) { return ESFConstructionNetworkResult::InvalidContext; }
    if (!Mode->IsBuilder(Controller)) { return ESFConstructionNetworkResult::PermissionDenied; }
    const APawn* Pawn = Controller->GetPawn();
    if (!IsValid(Pawn) || !Pawn->HasAuthority() || Pawn->GetController() != Controller || Pawn->GetOwner() != Controller
        || Pawn->GetWorld() != GetWorld() || Pawn->IsActorBeingDestroyed()) { return ESFConstructionNetworkResult::InvalidContext; }
    if (ExpectedRevision != Snapshot.Revision) { return ESFConstructionNetworkResult::Stale; }
    if (Snapshot.Revision != 0 || !SFConstructionSandboxFoundation::TouchesPlane(Cell,0,SFConstructionGeometry::IdentityRotation())) { return ESFConstructionNetworkResult::InvalidCell; }
    const AActor* Foundation = nullptr;
    if (!SFConstructionSandboxFoundation::MatchesKnownPlaneGeometry(GetWorld(),&Foundation)) { return ESFConstructionNetworkResult::InvalidContext; }
    const auto* Settings = GetDefault<USFConstructionSandboxSettings>();
    FString Error;
    if (!Settings->Validate(Error)) { return ESFConstructionNetworkResult::InvalidContext; }
    const FVector Centre = FVector(Cell)*50.0+FVector(25);
    if (Pawn->GetActorLocation().ContainsNaN() || FVector::DistSquared(Pawn->GetActorLocation(),Centre) > FMath::Square(double(Settings->BuildReachMetres)*100.0)) { return ESFConstructionNetworkResult::OutOfReach; }
    FVector Eyes; FRotator Rotation;
    Pawn->GetActorEyesViewPoint(Eyes,Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkAdmission),false,Pawn);
    FHitResult Hit;
    if (Eyes.ContainsNaN() || GetWorld()->LineTraceSingleByChannel(Hit,Eyes,Centre,ECC_Visibility,Query)) { return ESFConstructionNetworkResult::Occluded; }
    // Ignore only the already validated immutable fixture floor. Both players still block.
    FCollisionQueryParams Clearance(SCENE_QUERY_STAT(ConstructionNetworkClearance),false);
    Clearance.AddIgnoredActor(Foundation);
    Clearance.AddIgnoredActor(this);
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(Overlaps,Centre,FQuat::Identity,FCollisionObjectQueryParams::AllObjects,FCollisionShape::MakeBox(FVector(25)),Clearance);
    for (const auto& Overlap : Overlaps)
    {
        const auto* Component = Overlap.GetComponent();
        if (Component && Component->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block) { return ESFConstructionNetworkResult::Occupied; }
    }
    FSFConstructionNetworkSnapshot Candidate;
    Candidate.Revision = 1; Candidate.Cell = Cell;
    if (!Activate(Candidate)) { return ESFConstructionNetworkResult::ProjectionFailed; }
    Snapshot = Candidate; // Publish only after synchronous server physics activation.
    ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_ACCEPT Revision=1 Cell=%s"),*Cell.ToString());
    return ESFConstructionNetworkResult::Accepted;
}

ASFConstructionNetworkGameMode::ASFConstructionNetworkGameMode()
{
    PlayerControllerClass = ASFConstructionNetworkController::StaticClass();
}

bool ASFConstructionNetworkGameMode::IsAdmitted() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    return HasAuthority() && GetClass() == StaticClass() && GetWorld() && GetNetMode() == NM_DedicatedServer
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
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_READY UnlimitedDevelopmentStock=1 Capacity=1 Persistence=0"));
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
}

void ASFConstructionNetworkController::PlaceFromView()
{
    auto* State = FindState(GetWorld());
    if (!IsLocalController() || !GetPawn() || !State) { return; }
    FVector Start; FRotator Rotation; GetPlayerViewPoint(Start,Rotation);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkView),false,GetPawn());
    const auto* Settings = GetDefault<USFConstructionSandboxSettings>();
    if (!GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Rotation.Vector()*(Settings->BuildReachMetres*100.0+500.0),ECC_Visibility,Query)) { return; }
    const FVector Point = Hit.ImpactPoint + Hit.ImpactNormal*0.1;
    ServerPlace(FIntVector(FMath::FloorToInt(Point.X/50),FMath::FloorToInt(Point.Y/50),FMath::FloorToInt(Point.Z/50)),State->GetSnapshot().Revision);
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
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_RESULT Builder=%d Result=%d Revision=%llu Collision=%d"),bBuilderRole,int32(Result),Revision,State && State->HasCurrentCollision());
    ClientPlacementResult(Result,Revision);
}

void ASFConstructionNetworkController::ClientPlacementResult_Implementation(ESFConstructionNetworkResult Result, uint64 Revision)
{
    ClientMessage(FString::Printf(TEXT("Development network build: result %d, revision %llu"),int32(Result),Revision));
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!FParse::Param(FCommandLine::Get(),TEXT("ConstructionNetworkScenario"))) { return; }
    const bool bExpected = bBuilderRole
        ? ((ScenarioStep==1 && Result==ESFConstructionNetworkResult::Occupied && Revision==0)
            || (ScenarioStep==3 && Result==ESFConstructionNetworkResult::Accepted && Revision==1)
            || (ScenarioStep==5 && Result==ESFConstructionNetworkResult::Stale && Revision==1))
        : (ScenarioStep==1 && Result==ESFConstructionNetworkResult::PermissionDenied && Revision==1);
    if (!bExpected) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED Step=%d Result=%d Revision=%llu"),ScenarioStep,int32(Result),Revision); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    ++ScenarioStep;
    if ((bBuilderRole && ScenarioStep==6) || (!bBuilderRole && ScenarioStep==2))
    { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SCENARIO_SUCCESS Builder=%d Revision=1"),bBuilderRole); FPlatformMisc::RequestExitWithStatus(false,0); }
#endif
}

void ASFConstructionNetworkController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TickFiniteScenario();
#endif
}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void ASFConstructionNetworkController::TickFiniteScenario()
{
    if (!IsLocalController() || GetNetMode()!=NM_Client || !FParse::Param(FCommandLine::Get(),TEXT("ConstructionNetworkScenario"))) { return; }
    if (ScenarioStarted==0) { ScenarioStarted=FPlatformTime::Seconds(); }
    if (FPlatformTime::Seconds()-ScenarioStarted>45) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED Timeout=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    auto* State = FindState(GetWorld());
    if (!bRoleAssigned || !GetPawn() || !State || !State->IsPairReady()) { return; }
    const FVector Position = GetPawn()->GetActorLocation();
    if (FMath::Abs(Position.X-(bBuilderRole ? -200.0 : 200.0))>1.0 || FMath::Abs(Position.Y+400.0)>1.0) { return; }
    if (bBuilderRole && ScenarioStep==0)
    { ScenarioStep=1; ServerPlace(FIntVector(-4,-8,0),0); }
    else if (bBuilderRole && ScenarioStep==2)
    { ScenarioStep=3; ServerPlace(FIntVector(-4,-4,0),0); }
    else if (bBuilderRole && ScenarioStep==4 && State->HasCurrentCollision())
    { ScenarioStep=5; ServerPlace(FIntVector(-4,-4,0),0); }
    else if (!bBuilderRole && ScenarioStep==0 && State->HasCurrentCollision())
    { ScenarioStep=1; ServerPlace(FIntVector(4,-4,0),1); }
}
#endif
