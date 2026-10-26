#include "Construction/SFConstructionNetwork.h"
#include "Characters/SFE1VitalsHud.h"
#include "Creatures/SFRestAreaVolume.h"
#include "Exchange/SFExchangeWidgets.h"
#include "Framework/SFPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Characters/SFE1VitalsHud.h"
#include "Framework/SFHudLayout.h"
#include "UI/SFInventoryWidget.h"
#include "Inventory/SFLootComponent.h"
#include "Interaction/SFActionTargetComponent.h"
#include "UI/SFWindowStack.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Combat/SFCombatPilot.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Gathering/SFGatheringHud.h"
#include "Gathering/SFGatheringPilot.h"
#include "Gathering/SFGatheringSkillsPanel.h"
#include "Inventory/SFInventory.h"
#include "SFConstructionNetworkProjection.h"
#include "ProceduralMeshComponent.h"
#include "Construction/SFConstructionAnchor.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionLine.h"
#include "Construction/SFConstructionSandboxPalette.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Construction/SFConstructionSandboxFoundation.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Construction/SFConstructionSupport.h"
#include "Construction/SFConstructionDoorSupport.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Construction/SFConstructionClearance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
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
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#include "Quest/SFQuestPilotEvents.h"
#include "Quest/SFQuestBoard.h"
#include "Gathering/SFStationRecipes.h"
#include "UI/SFHudWidgets.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFCharacterSession.h"
#include "Services/SFGameplayServicesClient.h"
#include "Characters/SFE1Character.h"

namespace
{
bool HasLivingOwnedPawn(const ASFPlayerController* Controller)
{
    const auto* Pawn=IsValid(Controller) ? Cast<ASFE1Character>(Controller->GetPawn()) : nullptr;
    return IsValid(Pawn) && Pawn->HasAuthority() && Pawn->GetController()==Controller
        && Pawn->GetOwner()==Controller && Pawn->CombatHealth()>0 && !Pawn->IsActorBeingDestroyed();
}

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
    case ESFConstructionNetworkResult::InvalidLine: return TEXT("Use one straight axis within the line limit.");
    case ESFConstructionNetworkResult::DoorRemoved: return TEXT("Door removed.");
    case ESFConstructionNetworkResult::ServiceUnavailable: return TEXT("Housing service unavailable or refused. Nothing changed.");
    case ESFConstructionNetworkResult::Claimed: return TEXT("Plot claimed.");
    case ESFConstructionNetworkResult::Granted: return TEXT("Build permission granted.");
    case ESFConstructionNetworkResult::PermissionDenied: return TEXT("Observer only: building permission required.");
    case ESFConstructionNetworkResult::Stale: return TEXT("The structure changed. Aim again and retry.");
    case ESFConstructionNetworkResult::InvalidCell: return TEXT("You need a claim to place this here.");
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

// Housing persists the BLD-01 identity: orientation 0 is identity, then the other rotations in the catalog's own
// lexicographic order, so only the identity moves to the front.
int32 ToHousingOrientation(int32 Rotation) { const int32 I=SFConstructionGeometry::IdentityRotation(); return Rotation==I ? 0 : Rotation<I ? Rotation+1 : Rotation; }
int32 FromHousingOrientation(int32 Index) { const int32 I=SFConstructionGeometry::IdentityRotation(); return Index==0 ? I : Index<=I ? Index-1 : Index; }
FString HousingShapeKey(int32 Shape) { FSFConstructionSolid Solid; return SFConstructionGeometry::Build(Shape,SFConstructionGeometry::IdentityRotation(),Solid) ? Solid.ShapeKey : FString(); }
int32 ShapeFromHousingKey(const FString& Key)
{
    for (int32 Shape=0; Shape<SFConstructionGeometry::ShapeCount; ++Shape) { if (HousingShapeKey(Shape)==Key) { return Shape; } }
    return INDEX_NONE;
}

// BLD-02 A2: a zone has one state per claim. An RPC names its claim; with no claim id only a lone (sandbox) state answers.
ASFConstructionNetworkState* ResolveState(UWorld* World, const FString& ClaimId)
{
    if (!World) { return nullptr; }
    ASFConstructionNetworkState* Lone=nullptr; int32 Count=0;
    for (TActorIterator<ASFConstructionNetworkState> It(World); It; ++It)
    {
        if (!ClaimId.IsEmpty() && It->GetClaimId()==ClaimId) { return *It; }
        Lone=*It; ++Count;
    }
    return ClaimId.IsEmpty() && Count==1 ? Lone : nullptr;
}

// The claim state whose 20 m plot (plus 5 m) contains the pawn; the lone state in the sandbox.
ASFConstructionNetworkState* FindStateFor(UWorld* World, const APawn* Pawn)
{
    if (!World) { return nullptr; }
    ASFConstructionNetworkState* Best=nullptr; double BestDistance=1500.0; int32 Count=0;
    for (TActorIterator<ASFConstructionNetworkState> It(World); It; ++It)
    {
        ++Count;
        if (It->GetClaimId().IsEmpty()) { Best=*It; continue; }
        const double Distance=Pawn ? FMath::Max(FMath::Abs(It->GetActorLocation().X-Pawn->GetActorLocation().X),FMath::Abs(It->GetActorLocation().Y-Pawn->GetActorLocation().Y)) : TNumericLimits<double>::Max();
        if (Distance<BestDistance) { Best=*It; BestDistance=Distance; }
    }
    return Best;
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
    uint8 Version=6;
    uint64 WireRevision=Revision;
    uint16 Count=uint16(Members.Num());
    bOutSuccess=false;
    if (Ar.IsSaving() && (Members.Num()>4096 || Doors.Num()>64 || (Revision==0 && (!Members.IsEmpty() || !Doors.IsEmpty())))) { return false; }
    Ar << Version; Ar << WireRevision; Ar.SerializeBits(&Count,13);
    if (Version!=6 || Count>4096 || (WireRevision==0 && Count!=0)) { return false; }
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
    if (Ar.IsError() || DoorCount>64 || (WireRevision==0 && (Count!=0 || DoorCount!=0))) { return false; }
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
    const auto* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>();
    if (!HasAuthority() || !Mode || !Mode->IsAdmitted()) { return; }
    const auto* Settings = GetDefault<USFConstructionSandboxSettings>();
    FString Error;
    if (!Settings->Validate(Error)) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_CONFIGURATION_FAILED %s"),*Error); return; }
    ReachCentimetres = double(Settings->BuildReachMetres)*100.0;
    MaximumMembers = Settings->MaximumMembers;
    MaximumDoors = Settings->MaximumDoors;
    MaximumLineCells = Settings->MaximumLineCells;
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
    DOREPLIFETIME(ASFConstructionNetworkState, MaximumLineCells);
    DOREPLIFETIME(ASFConstructionNetworkState, AnchorCell);
    DOREPLIFETIME(ASFConstructionNetworkState, ClaimId);
    DOREPLIFETIME(ASFConstructionNetworkState, ClaimMask);
    DOREPLIFETIME(ASFConstructionNetworkState, MaskOrigin);
    DOREPLIFETIME(ASFConstructionNetworkState, MaskWidth);
    DOREPLIFETIME(ASFConstructionNetworkState, MaskHeight);
}

bool ASFConstructionNetworkState::SetClaimMask(const TArray<uint8>& Bits, int32 OriginX, int32 OriginY, int32 Width, int32 Height)
{
    const int64 Count=int64(Width)*Height;
    if (!HasAuthority() || ClaimId.IsEmpty() || Width<1 || Height<1 || Width>100 || Height>100 ||
        Count>3600 || Bits.Num()!=(Count+7)/8 || Bits.IsEmpty()) { return false; }
    ClaimMask=Bits; MaskOrigin=FIntPoint(OriginX,OriginY); MaskWidth=Width; MaskHeight=Height;
    ForceNetUpdate(); return true;
}

bool ASFConstructionNetworkState::ContainsClaimCell(const FIntVector& Cell) const
{
    if (!SFConstructionAnchor::Contains(Cell)) { return false; }
    if (ClaimId.IsEmpty()) { return true; }
    const int32 X=FMath::FloorToInt(double(Cell.X)*0.5)-MaskOrigin.X;
    const int32 Y=FMath::FloorToInt(double(Cell.Y)*0.5)-MaskOrigin.Y;
    if (X<0 || Y<0 || X>=MaskWidth || Y>=MaskHeight) { return false; }
    const int32 Index=Y*MaskWidth+X;
    return ClaimMask.IsValidIndex(Index>>3) && (ClaimMask[Index>>3] & (1u<<(Index&7)))!=0;
}

bool ASFConstructionNetworkState::RestoreFromHousing(const TArray<FSFConstructionNetworkRestoredPiece>& Pieces)
{
    if (!HasAuthority() || Snapshot.Revision!=0 || Pieces.IsEmpty()) { return Pieces.IsEmpty(); }
    FSFConstructionNetworkSnapshot Candidate; Candidate.Revision=1;
    for (const FSFConstructionNetworkRestoredPiece& Piece : Pieces)
    {
        if (!ContainsClaimCell(Piece.Cell) || Candidate.Find(Piece.Cell)!=INDEX_NONE || Piece.Shape<0 || Piece.Shape>=SFConstructionGeometry::ShapeCount
            || Piece.Rotation<0 || Piece.Rotation>=SFConstructionGeometry::RotationCount)
        { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_RESTORE_REJECTED Claim=%s Cell=%s Shape=%d Rotation=%d"),*ClaimId,*Piece.Cell.ToString(),Piece.Shape,Piece.Rotation); return false; }
        FSFConstructionNetworkMember Member; Member.Cell=Piece.Cell; Member.Shape=uint8(Piece.Shape); Member.Rotation=uint8(Piece.Rotation);
        Candidate.Members.Add(Member);
    }
    Candidate.Members.Sort([](const auto& A,const auto& B) { return A.Cell.X!=B.Cell.X ? A.Cell.X<B.Cell.X : A.Cell.Y!=B.Cell.Y ? A.Cell.Y<B.Cell.Y : A.Cell.Z<B.Cell.Z; });
    if (!Activate(Candidate)) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_RESTORE_REJECTED Claim=%s Projection=0"),*ClaimId); return false; }
    Snapshot=MoveTemp(Candidate); ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_RESTORED Claim=%s Members=%d Anchor=%s Content=%s"),*ClaimId,Snapshot.Members.Num(),*AnchorCell.ToString(),*GetContentDigest());
    return true;
}

FVector ASFConstructionNetworkState::GetAnchorOffsetCm() const { return SFConstructionAnchor::OffsetCm(AnchorCell); }

bool ASFConstructionNetworkState::HasExpectedTransform() const
{
    return SFConstructionAnchor::IsValid(AnchorCell)
        && GetActorTransform().Equals(FTransform(FQuat::Identity,GetAnchorOffsetCm(),FVector::OneVector));
}

bool ASFConstructionNetworkState::ConfigureAnchor(const FIntVector& Cell)
{
    // Immutable once members exist: re-anchoring would silently move every placed piece.
    if (!HasAuthority() || !SFConstructionAnchor::IsValid(Cell) || !Snapshot.Members.IsEmpty() || !Snapshot.Doors.IsEmpty()) { return false; }
    AnchorCell=Cell;
    return true;
}

void ASFConstructionNetworkState::AdmitPair()
{
    auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFGameMode>() : nullptr;
    if (HasAuthority() && Mode && Mode->IsAdmitted() && Mode->GetNumPlayers() >= 2)
    { bPairReady = true; ForceNetUpdate(); UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_PAIR_READY Players=%d"),Mode->GetNumPlayers()); }
}

bool ASFConstructionNetworkState::Activate(const FSFConstructionNetworkSnapshot& Candidate)
{
    if (Candidate.Revision<=ProjectionRevision || Candidate.Members.Num()>4096 || Candidate.Doors.Num()>64 || (Candidate.Revision==0 && (!Candidate.Members.IsEmpty() || !Candidate.Doors.IsEmpty()))
        || !HasExpectedTransform()) { return false; }
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
        || ProjectedMembers.Num()!=Snapshot.Members.Num() || DoorProjections.Num()!=Snapshot.Doors.Num() || ProjectedDoors.Num()!=Snapshot.Doors.Num() || !HasExpectedTransform()) { return false; }
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
        Inside=FVector(M.Cell)*50.0+Inside/Solid.Vertices.Num()+GetAnchorOffsetCm();
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
    if (!IsInGameThread() || !ContainsClaimCell(Cell)
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
    FString Canonical=TEXT("SF_NETWORK_ASSEMBLY_V6|");
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

ESFConstructionNetworkResult ASFConstructionNetworkState::ValidateEditContext(ASFPlayerController* Controller, uint64 ExpectedRevision, const APawn*& OutPawn, const AActor*& OutFoundation) const
{
    OutPawn=nullptr; OutFoundation=nullptr;
    const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFGameMode>() : nullptr;
    if (!IsInGameThread() || !HasAuthority() || IsActorBeingDestroyed() || !bConfigurationReady || !Mode || !Mode->IsAdmitted() || GetClass() != StaticClass()
        || !IsValid(Controller) || Controller->IsActorBeingDestroyed() || !Controller->HasAuthority() || Controller->GetWorld() != GetWorld()) { return ESFConstructionNetworkResult::InvalidContext; }
    // A claim's build rights belong to Housing (owner or grant); the sandbox keeps its first-admitted builder.
    if (ClaimId.IsEmpty() && !Mode->IsBuilder(Controller)) { return ESFConstructionNetworkResult::PermissionDenied; }
    // Without Housing a claim cannot be checked: only the development fallback (bAllowUnclaimedBuilding, never in
    // shipping) lets anyone build on the plot.
    bool bUnclaimedFallback=false;
#if !UE_BUILD_SHIPPING
    bUnclaimedFallback=GetDefault<USFConstructionSandboxSettings>()->bAllowUnclaimedBuilding;
#endif
    if (!ClaimId.IsEmpty() && !Mode->IsHousingActive() && !bUnclaimedFallback) { return ESFConstructionNetworkResult::ServiceUnavailable; }
    const APawn* Pawn = Controller->GetPawn();
    if (!HasLivingOwnedPawn(Controller) || Pawn->GetWorld() != GetWorld()) { return ESFConstructionNetworkResult::InvalidContext; }
    if (ExpectedRevision != Snapshot.Revision) { return ESFConstructionNetworkResult::Stale; }
    if (Snapshot.Revision==MAX_uint64) { return ESFConstructionNetworkResult::InvalidContext; }
    if (Snapshot.Revision > 0 && !HasCurrentCollision()) { return ESFConstructionNetworkResult::ProjectionFailed; }
    const AActor* Foundation = nullptr;
    if (!SFConstructionSandboxFoundation::MatchesKnownPlaneGeometry(GetWorld(),&Foundation,GetAnchorOffsetCm())) { return ESFConstructionNetworkResult::InvalidContext; }
    OutPawn=Pawn; OutFoundation=Foundation;
    return ESFConstructionNetworkResult::Accepted;
}

ESFConstructionNetworkResult ASFConstructionNetworkState::ValidateMemberCell(const APawn* Pawn, const AActor* Foundation, FIntVector Cell, bool bRemove, uint8 Shape, uint8 Rotation) const
{
    if (!bRemove && (Shape>=SFConstructionGeometry::ShapeCount || Rotation>=SFConstructionGeometry::RotationCount)) { return ESFConstructionNetworkResult::InvalidPiece; }
    if (!ContainsClaimCell(Cell)) { return ESFConstructionNetworkResult::InvalidCell; }
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
    const FVector Centre = FVector(Cell)*50.0+FVector(25)+GetAnchorOffsetCm();
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
        SightTarget=FVector(Cell)*50.0+SightTarget/Solid.Vertices.Num()+GetAnchorOffsetCm();
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
    return ESFConstructionNetworkResult::Accepted;
}

bool ASFConstructionNetworkState::IsLineSupportedForPreview(FIntVector Start, FIntVector End, uint8 Shape, uint8 Rotation) const
{
    TArray<FIntVector> Cells;
    if (Shape>=SFConstructionGeometry::ShapeCount || Rotation>=SFConstructionGeometry::RotationCount
        || !SFConstructionLine::Enumerate(Start,End,MaximumLineCells,Cells) || Snapshot.Members.Num()+Cells.Num()>MaximumMembers) { return false; }
    auto Members=Snapshot.Members;
    for (const auto& Cell : Cells)
    {
        if (!ContainsClaimCell(Cell) || Snapshot.Find(Cell)!=INDEX_NONE) { return false; }
        FSFConstructionNetworkMember Member; Member.Cell=Cell; Member.Shape=Shape; Member.Rotation=Rotation; Members.Add(Member);
    }
    return SupportsMembers(Members,Snapshot.Doors);
}

ESFConstructionNetworkResult ASFConstructionNetworkState::PlaceLine(ASFPlayerController* Controller, FIntVector Start, FIntVector End, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation)
{
    const APawn* Pawn=nullptr; const AActor* Foundation=nullptr;
    const auto Admission=ValidateEditContext(Controller,ExpectedRevision,Pawn,Foundation);
    if (Admission!=ESFConstructionNetworkResult::Accepted) { return Admission; }
    TArray<FIntVector> Cells;
    if (!SFConstructionLine::Enumerate(Start,End,MaximumLineCells,Cells)) { return ESFConstructionNetworkResult::InvalidLine; }
    if (Snapshot.Members.Num()+Cells.Num()>MaximumMembers) { return ESFConstructionNetworkResult::LimitReached; }
    auto Candidate=Snapshot;
    for (const auto& Cell : Cells)
    {
        const auto CellAdmission=ValidateMemberCell(Pawn,Foundation,Cell,false,Shape,Rotation);
        if (CellAdmission!=ESFConstructionNetworkResult::Accepted) { return CellAdmission; }
        FSFConstructionNetworkMember Member; Member.Cell=Cell; Member.Shape=Shape; Member.Rotation=Rotation; Candidate.Members.Add(Member);
    }
    Candidate.Members.Sort([](const auto& A,const auto& B) { return A.Cell.X!=B.Cell.X ? A.Cell.X<B.Cell.X : A.Cell.Y!=B.Cell.Y ? A.Cell.Y<B.Cell.Y : A.Cell.Z<B.Cell.Z; });
    Candidate.Revision=Snapshot.Revision+1;
    if (!SupportsMembers(Candidate.Members,Candidate.Doors)) { return ESFConstructionNetworkResult::Unsupported; }
    if (!Activate(Candidate)) { return ESFConstructionNetworkResult::ProjectionFailed; }
    Snapshot=MoveTemp(Candidate); ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_LINE Revision=%llu Members=%d Doors=%d Cells=%d Start=%s End=%s Content=%s Shape=%u Rotation=%u"),Snapshot.Revision,Snapshot.Members.Num(),Snapshot.Doors.Num(),Cells.Num(),*Start.ToString(),*End.ToString(),*GetContentDigest(),Shape,Rotation);
    return ESFConstructionNetworkResult::Accepted;
}

ESFConstructionNetworkResult ASFConstructionNetworkState::Edit(ASFPlayerController* Controller, FIntVector Cell, uint64 ExpectedRevision, bool bRemove, uint8 Shape, uint8 Rotation, bool bCommit)
{
    const APawn* Pawn=nullptr; const AActor* Foundation=nullptr;
    const auto Admission=ValidateEditContext(Controller,ExpectedRevision,Pawn,Foundation);
    if (Admission!=ESFConstructionNetworkResult::Accepted) { return Admission; }
    const auto CellAdmission=ValidateMemberCell(Pawn,Foundation,Cell,bRemove,Shape,Rotation);
    if (CellAdmission!=ESFConstructionNetworkResult::Accepted) { return CellAdmission; }
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
    if (!bCommit) { return bRemove ? ESFConstructionNetworkResult::Removed : ESFConstructionNetworkResult::Accepted; }
    if (!Activate(Candidate)) { return ESFConstructionNetworkResult::ProjectionFailed; }
    Snapshot = Candidate; // Publish only after synchronous server physics activation.
    ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_EDIT Action=%s Revision=%llu Members=%d Cell=%s Content=%s Shape=%u Rotation=%u"),bRemove ? TEXT("Remove") : TEXT("Place"),Snapshot.Revision,Snapshot.Members.Num(),*Cell.ToString(),*GetContentDigest(),Shape,Rotation);
    return bRemove ? ESFConstructionNetworkResult::Removed : ESFConstructionNetworkResult::Accepted;
}

ESFConstructionNetworkResult ASFConstructionNetworkState::PlaceDoor(ASFPlayerController* Controller, FIntVector Origin, uint64 ExpectedRevision, uint8 Yaw)
{
    const APawn* Pawn=nullptr; const AActor* Foundation=nullptr;
    const auto Admission=ValidateEditContext(Controller,ExpectedRevision,Pawn,Foundation);
    if (Admission!=ESFConstructionNetworkResult::Accepted) { return Admission; }
    if (Snapshot.FindDoor(Origin)!=INDEX_NONE) { return ESFConstructionNetworkResult::Occupied; }
    if (Snapshot.Doors.Num()>=MaximumDoors) { return ESFConstructionNetworkResult::LimitReached; }
    FSFConstructionDoorProjectionProfile Profile;
    if (!ASFConstructionSandboxDoorProjection::BuildProfile(Origin,Yaw,false,Profile)) { return ESFConstructionNetworkResult::InvalidCell; }
    const FVector Centre=Profile.Envelope.GetCenter()+GetAnchorOffsetCm();
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
        if (SFConstructionClearance::EvaluateBox(GetWorld(),Bounds.ShiftBy(GetAnchorOffsetCm()))!=SFConstructionClearance::EResult::Clear) { return ESFConstructionNetworkResult::Occupied; }
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
            || FVector::DistSquared(Pawn->GetActorLocation(),Profile.Envelope.GetCenter()+GetAnchorOffsetCm())>FMath::Square(ReachCentimetres)
            || !RayEnvelopeEntry(Start,End,Profile.Envelope.ShiftBy(GetAnchorOffsetCm()),T)) { continue; }
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

ESFConstructionNetworkResult ASFConstructionNetworkState::ValidateDoorTarget(ASFPlayerController* Controller, FIntVector Origin, uint64 ExpectedRevision, int32& OutIndex) const
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
    if (Pawn->GetActorLocation().ContainsNaN() || FVector::DistSquared(Pawn->GetActorLocation(),Profile.Envelope.GetCenter()+GetAnchorOffsetCm())>FMath::Square(ReachCentimetres)) { return ESFConstructionNetworkResult::OutOfReach; }
    FVector Eyes; FRotator Rotation; Pawn->GetActorEyesViewPoint(Eyes,Rotation);
    if (Eyes.ContainsNaN()) { return ESFConstructionNetworkResult::Occluded; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkDoorUse),false,Pawn);
    FHitResult CentreHit; FIntVector CentreOrigin;
    // Clear sight into the aperture is sufficient, even without a leaf/frame hit.
    bool bVisible=!GetWorld()->LineTraceSingleByChannel(CentreHit,Eyes,Profile.Envelope.GetCenter()+GetAnchorOffsetCm(),ECC_Visibility,Query)
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

ESFConstructionNetworkResult ASFConstructionNetworkState::RemoveDoor(ASFPlayerController* Controller, FIntVector Origin, uint64 ExpectedRevision)
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

ESFConstructionNetworkResult ASFConstructionNetworkState::ToggleDoor(ASFPlayerController* Controller, FIntVector Origin, uint64 ExpectedRevision)
{
    int32 Index=INDEX_NONE;
    const auto Admission=ValidateDoorTarget(Controller,Origin,ExpectedRevision,Index);
    if (Admission!=ESFConstructionNetworkResult::Accepted) { return Admission; }
    const auto& Door=Snapshot.Doors[Index];
    const auto* Projection=DoorProjections.Find(Origin);
    if (!SFConstructionNetworkProjection::IsDoorMotionClear(this,Door,*Projection))
    {
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        GetWorld()->GetAuthGameMode<ASFGameMode>()->DiscardFiniteDoorObstruction();
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
    if (bOpen) { GetWorld()->GetAuthGameMode<ASFGameMode>()->SpawnFiniteDoorObstruction(Origin,Snapshot.Doors[Index].Yaw); }
#endif
    return bOpen ? ESFConstructionNetworkResult::DoorOpened : ESFConstructionNetworkResult::DoorClosed;
}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void ASFGameMode::SpawnFiniteDoorObstruction(const FIntVector& Origin, uint8 Yaw)
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

void ASFGameMode::DiscardFiniteDoorObstruction()
{
    auto* Obstacle=FiniteDoorObstruction.Get();
    if (IsAdmitted() && FParse::Param(FCommandLine::Get(),TEXT("ConstructionNetworkDoorObstruction")) && Obstacle && Obstacle->GetOwner()==this)
    { Obstacle->Destroy(); FiniteDoorObstruction.Reset(); UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_OBSTRUCTION_REMOVED")); }
}
#endif

bool ASFGameMode::IsZoneGatheringWorld() const
{
    // The zone with a record (gathering nodes, build plots): Silverbrook.
    return GetWorld() && GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook");
}

void ASFGameMode::PlaceForZoneScenario(APawn* Pawn, bool bPrimary) const
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // The claim runner: both test pawns stand on the first build plot's pad, the first-admitted one to the west.
    if (Pawn && FParse::Param(FCommandLine::Get(),TEXT("ClaimScenario")) && !ClaimPlots.IsEmpty() && ClaimPlots[0].State.IsValid())
    {
        const FVector Pad=ClaimPlots[0].State->GetAnchorOffsetCm();
        Pawn->SetActorLocation(Pad+FVector(bPrimary ? -175 : 175,-125,120),false,nullptr,ETeleportType::TeleportPhysics);
        return;
    }
    // The finite network scenario only: put each test pawn 2 m from its node.
    const bool bUiStation = FParse::Param(FCommandLine::Get(), TEXT("UiEvidenceStation"));
    if (!Pawn || (!FParse::Param(FCommandLine::Get(),TEXT("GatheringPilotScenario")) && !bUiStation)) { return; }
    // Same choice as the client scenario: the oak nearest the origin, and the rock nearest that oak.
    const ASFGatheringPilotNode* Oak=nullptr; const ASFGatheringPilotNode* Rock=nullptr;
    for (TActorIterator<ASFGatheringPilotNode> It(GetWorld()); It; ++It)
    { if (It->Kind==ESFGatheringPilotKind::Oak && (!Oak || It->GetActorLocation().Size2D()<Oak->GetActorLocation().Size2D())) { Oak=*It; } }
    for (TActorIterator<ASFGatheringPilotNode> It(GetWorld()); It && Oak; ++It)
    {
        if ((It->Kind==ESFGatheringPilotKind::Stone || It->Kind==ESFGatheringPilotKind::Iron) && (!Rock || FVector::Dist2D(It->GetActorLocation(),Oak->GetActorLocation())<FVector::Dist2D(Rock->GetActorLocation(),Oak->GetActorLocation())))
        { Rock=*It; }
    }
    const ASFGatheringPilotNode* Target=bPrimary ? Oak : Rock;
    if (bUiStation)
    {
        for (TActorIterator<ASFGatheringPilotNode> It(GetWorld()); It; ++It)
        { if (It->Kind==ESFGatheringPilotKind::Sawmill) { Target=*It; break; } }
    }
    if (!Target) { return; }
    const FVector Node=Target->GetActorLocation(); const FVector Away=(-Node*FVector(1,1,0)).GetSafeNormal();
    Pawn->SetActorLocation(Node+Away*200+FVector(0,0,80),false,nullptr,ETeleportType::TeleportPhysics);
#endif
}

int32 ASFGameMode::SpawnZoneGatheringNodes()
{
    // The zone record supplies local-mode positions and host-mode stable slot identities.
    const FString Path=FPaths::Combine(FPaths::ProjectDir(),TEXT("../content/world/zones/greenlands_silverbrook.json"));
    FString Text; TSharedPtr<FJsonObject> Zone;
    if (!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Zone) || !Zone.IsValid())
    { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_ZONE_RECORD_MISSING %s"),*Path); return 0; }
    int32 Spawned=0;
    for (const TSharedPtr<FJsonValue>& GroupValue : Zone->GetArrayField(TEXT("spawn_groups")))
    {
        const TSharedPtr<FJsonObject> Group=GroupValue->AsObject();
        if (Group->GetStringField(TEXT("spawn_kind"))!=TEXT("gathering_node")) { continue; }
        const FString Id=Group->GetStringField(TEXT("group_id"));
        // Wood groups are oak nodes, iron ore groups are iron veins, stone groups are rock nodes yielding rough stone.
        const ESFGatheringPilotKind Kind=Id.EndsWith(TEXT("_wood")) ? ESFGatheringPilotKind::Oak
            : Id.EndsWith(TEXT("_iron_ore")) ? ESFGatheringPilotKind::Iron : ESFGatheringPilotKind::Stone;
        // GAT-01: the group's first definition may declare a growth_profile (stages regrow young to mature).
        TArray<FSFGrowthStageDef> Growth; double GrowFraction=0.5;
        const TArray<TSharedPtr<FJsonValue>>* Definitions=nullptr; const TSharedPtr<FJsonObject>* Profile=nullptr;
        if (Group->TryGetArrayField(TEXT("definitions"),Definitions) && Definitions->Num()>0 && (*Definitions)[0]->AsObject()
            && (*Definitions)[0]->AsObject()->TryGetObjectField(TEXT("growth_profile"),Profile))
        {
            (*Profile)->TryGetNumberField(TEXT("grow_fraction"),GrowFraction);
            for (const TSharedPtr<FJsonValue>& Stage : (*Profile)->GetArrayField(TEXT("stages")))
            {
                bool bBlocking=true;
                Stage->AsObject()->TryGetBoolField(TEXT("blocking"),bBlocking);
                Growth.Add({FName(*Stage->AsObject()->GetStringField(TEXT("name"))),float(Stage->AsObject()->GetNumberField(TEXT("duration_fraction"))),float(Stage->AsObject()->GetNumberField(TEXT("yield_fraction"))),bBlocking});
            }
        }
        // Host placement replaces these initial positions as soon as its zone snapshot arrives.
        const TArray<TSharedPtr<FJsonValue>> Points=Group->GetArrayField(TEXT("points"));
        const int32 Active=FMath::Min(int32(Group->GetNumberField(TEXT("active_count"))),Points.Num());
        for (int32 I=0; I<Active; ++I)
        {
            const TArray<TSharedPtr<FJsonValue>>& At=Points[I]->AsObject()->GetArrayField(TEXT("position"));
            if (At.Num()<3) { continue; }
            // Metres to centimetres; the node origin sits 70 cm above the ground, as in the sandbox fixture.
            const FTransform Where(FRotator(0,Points[I]->AsObject()->GetNumberField(TEXT("facing")),0),
                FVector(At[0]->AsNumber()*100,At[1]->AsNumber()*100,At[2]->AsNumber()*100+70));
            auto* Node=GetWorld()->SpawnActorDeferred<ASFGatheringPilotNode>(ASFGatheringPilotNode::StaticClass(),Where);
            if (!Node) { continue; }
            Node->Kind=Kind;
            Node->SetServiceNodeId(FString::Printf(TEXT("%s/%s/%d"),*Zone->GetStringField(TEXT("id")),*Id,I));
            if (Growth.Num()>0) { Node->SetGrowthProfile(Growth,GrowFraction); }
            Node->FinishSpawning(Where); ++Spawned;
        }
    }
    // ponytail: the zone record has no station content yet; a sawmill, forge and workbench stand east of the arrival point.
    const TArray<TSharedPtr<FJsonValue>>* Respawns=nullptr;
    if (Zone->TryGetArrayField(TEXT("respawn_points"),Respawns) && Respawns->Num()>0)
    {
        const TArray<TSharedPtr<FJsonValue>>& Arrival=(*Respawns)[0]->AsObject()->GetArrayField(TEXT("position"));
        for (int32 I=0; Arrival.Num()>=2 && I<3; ++I)
        {
            FVector At(Arrival[0]->AsNumber()*100+500,Arrival[1]->AsNumber()*100+I*400,0);
            FHitResult Hit;
            if (GetWorld()->LineTraceSingleByChannel(Hit,At+FVector(0,0,20000),At-FVector(0,0,20000),ECC_WorldStatic)) { At.Z=Hit.ImpactPoint.Z; }
            const FTransform Where(FRotator(0,180,0),At+FVector(0,0,40));
            auto* Station=GetWorld()->SpawnActorDeferred<ASFGatheringPilotNode>(ASFGatheringPilotNode::StaticClass(),Where);
            if (!Station) { continue; }
            Station->Kind=I==0 ? ESFGatheringPilotKind::Sawmill : I==1 ? ESFGatheringPilotKind::Forge : ESFGatheringPilotKind::Workbench;
            Station->FinishSpawning(Where); ++Spawned;
        }
    }
    if (GetDefault<USFGameplayServicesSettings>()->bEnabled) { RefreshZoneGatheringNodes(); }
    return Spawned;
}

void ASFGameMode::RefreshZoneGatheringNodes()
{
    if (!IsZoneGatheringWorld() || !GetDefault<USFGameplayServicesSettings>()->bEnabled) { return; }
    const TSharedPtr<FSFGameplayServicesClient> Client=USFBackpackComponent::Services();
    if (!Client) { return; }
    FSFGameplayRequest Request; Request.Rpc=ESFGameplayRpc::ListGatheringZoneNodes;
    Request.ZoneId=TEXT("zone/greenlands_silverbrook");
    TWeakObjectPtr<ASFGameMode> Weak(this);
    Client->Call(Request,[Weak](const FSFGameplayResponse& Response)
    {
        ASFGameMode* Self=Weak.Get();
        if (!Self) { return; }
        if (Response.Status!=ESFGameplayStatus::Ok)
        {
            UE_LOG(LogTemp,Warning,TEXT("GATHERING_ZONE_LIST_FAILED Code=%d"),Response.GrpcCode);
            return;
        }
        TMap<FString,ASFGatheringPilotNode*> ById;
        for (TActorIterator<ASFGatheringPilotNode> It(Self->GetWorld()); It; ++It)
        { if (!It->GetServiceNodeId().IsEmpty()) { ById.Add(It->GetServiceNodeId(),*It); } }
        for (const auto& Node : Response.ZoneNodes)
        { if (ASFGatheringPilotNode** Actor=ById.Find(Node.Id)) { (*Actor)->ApplyZoneNode(Node); } }
        UE_LOG(LogTemp,Display,TEXT("GATHERING_ZONE_PLACEMENTS Count=%d"),Response.ZoneNodes.Num());
    });
}

int32 ASFGameMode::PrepareClaimPlots()
{
    // Every 20 m build plot of the zone record (area purpose "claims", outside the protected town) gets a level pad and
    // its own construction state at its anchor (BLD-02 A2). ClaimTerritory, through Housing, decides who may own each.
    // ponytail: the pad sits above the highest hit in the plot so no terrain or prop overlaps a cell; real levelling is DEC-03.
    const FString Path=FPaths::Combine(FPaths::ProjectDir(),TEXT("../content/world/zones/greenlands_silverbrook.json"));
    FString Text; TSharedPtr<FJsonObject> Zone;
    const TArray<TSharedPtr<FJsonValue>>* Areas=nullptr;
    if (!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Zone) || !Zone || !Zone->TryGetArrayField(TEXT("areas"),Areas))
    { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_CLAIM_FAILED ZoneRecord=%s"),*Path); return 0; }
    FString ZoneName; Zone->GetStringField(TEXT("id")).Split(TEXT("/"),nullptr,&ZoneName);
    TArray<FBox2D> Plots;
    for (const TSharedPtr<FJsonValue>& Area : *Areas)
    {
        const TArray<TSharedPtr<FJsonValue>>* Polygons=nullptr;
        if (!Area->AsObject() || Area->AsObject()->GetStringField(TEXT("purpose"))!=TEXT("claims") || !Area->AsObject()->TryGetArrayField(TEXT("polygons"),Polygons)) { continue; }
        for (const TSharedPtr<FJsonValue>& Polygon : *Polygons)
        {
            FBox2D Plot(ForceInit);
            for (const TSharedPtr<FJsonValue>& Corner : Polygon->AsArray())
            { const auto& XY=Corner->AsArray(); if (XY.Num()>=2) { Plot+=FVector2D(XY[0]->AsNumber(),XY[1]->AsNumber()); } }
            Plots.Add(Plot);
        }
    }
    for (int32 Index=0; Index<Plots.Num(); ++Index)
    {
        const FBox2D& Plot=Plots[Index];
        // The claim extent is fixed at 40 x 40 cells, so only a 20 m square plot fits it.
        if (!Plot.bIsValid || !FMath::IsNearlyEqual(Plot.GetSize().X,20.0) || !FMath::IsNearlyEqual(Plot.GetSize().Y,20.0))
        { UE_LOG(LogTemp,Warning,TEXT("CONSTRUCTION_NETWORK_CLAIM_SKIPPED Plot=%d NotTwentyMetres=1"),Index); continue; }
        const FVector Centre(Plot.GetCenter().X*100.0,Plot.GetCenter().Y*100.0,0);
        double Highest=-TNumericLimits<double>::Max(), Lowest=TNumericLimits<double>::Max(); int32 Hits=0;
        for (int32 X=-10; X<=10; ++X) for (int32 Y=-10; Y<=10; ++Y)
        {
            FHitResult Hit; const FVector At=Centre+FVector(X*100.0,Y*100.0,0);
            if (GetWorld()->LineTraceSingleByChannel(Hit,At+FVector(0,0,50000),At-FVector(0,0,50000),ECC_WorldStatic))
            { Highest=FMath::Max(Highest,Hit.ImpactPoint.Z); Lowest=FMath::Min(Lowest,Hit.ImpactPoint.Z); ++Hits; }
        }
        if (Hits==0) { UE_LOG(LogTemp,Warning,TEXT("CONSTRUCTION_NETWORK_CLAIM_SKIPPED Plot=%d NoGround=1"),Index); continue; }
        const double Cell=SFConstructionGeometry::CellCentimetres;
        const FIntVector Anchor(FMath::RoundToInt(Centre.X/Cell),FMath::RoundToInt(Centre.Y/Cell),FMath::CeilToInt(Highest/Cell)+1);
        const FVector Offset=SFConstructionAnchor::OffsetCm(Anchor);
        // The exact transform is the fixture contract; replication is decided before the pad finishes spawning.
        const FTransform SlabAt(FQuat::Identity,FVector(0,0,-50)+Offset,FVector(20,20,1));
        auto* Slab=GetWorld()->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(),SlabAt,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Slab) { continue; }
        Slab->SetReplicates(true); Slab->bAlwaysRelevant=true;
        Slab->SetMobility(EComponentMobility::Movable);
        Slab->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Slab->GetStaticMeshComponent()->SetIsReplicated(true);
        Slab->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
        Slab->SetActorHiddenInGame(true);   // BLD-02 A2 fixture: the pad's visible form is DEC-03 levelling, not a graybox cube in the woods (WP6i r4)
        Slab->FinishSpawning(SlabAt);
        FClaimPlot Claim; Claim.ClaimId=FString::Printf(TEXT("clm_%s_%d"),*ZoneName,Index+1); Claim.CentreMetres=Plot.GetCenter(); Claim.SideMetres=20;
        const FTransform StateAt(FQuat::Identity,Offset,FVector::OneVector);
        auto* State=GetWorld()->SpawnActorDeferred<ASFConstructionNetworkState>(ASFConstructionNetworkState::StaticClass(),StateAt);
        if (!State) { continue; }
        State->ConfigureAnchor(Anchor); State->SetClaimId(Claim.ClaimId);
        TArray<uint8> InitialMask; InitialMask.Init(0xff,50); // 20 x 20 one-metre legacy plot.
        if (!State->SetClaimMask(InitialMask,-10,-10,20,20)) { State->Destroy(); continue; }
        State->FinishSpawning(StateAt);
        Claim.State=State;
        ClaimPlots.Add(MoveTemp(Claim));
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_CLAIM_PLOT Claim=%s CentreM=%s SideM=20 Anchor=%s GroundRangeCm=%.0f"),*ClaimPlots.Last().ClaimId,*Plot.GetCenter().ToString(),*Anchor.ToString(),Highest-Lowest);
    }
    return ClaimPlots.Num();
}

FString ASFGameMode::RestHouseClaimAt(const FVector& WorldPosition) const
{
    if (!HasAuthority() || !IsHousingActive()) return FString();
    const FVector2D Metres(WorldPosition.X / 100.0, WorldPosition.Y / 100.0);
    for (const FClaimPlot& Plot : ClaimPlots)
    {
        if (!Plot.bLoaded || !Plot.bClaimed || Plot.Structures.IsEmpty() || !Plot.State.IsValid()) continue;
        const FVector2D Delta = Metres - Plot.CentreMetres;
        if (FMath::Abs(Delta.X) > Plot.SideMetres * 0.5 || FMath::Abs(Delta.Y) > Plot.SideMetres * 0.5) continue;
        const double Floor = Plot.State->GetAnchorOffsetCm().Z;
        if (WorldPosition.Z >= Floor - 200 && WorldPosition.Z <= Floor + 500) return Plot.ClaimId;
    }
    return FString();
}

bool ASFGameMode::IsHousingActive() const
{
    return IsAdmitted() && !ClaimPlots.IsEmpty() && USFBackpackComponent::Services().IsValid();
}

ASFGameMode::FClaimPlot* ASFGameMode::FindPlot(const FString& ClaimId)
{
    return ClaimId.IsEmpty() ? nullptr : ClaimPlots.FindByPredicate([&](const FClaimPlot& Plot) { return Plot.ClaimId==ClaimId; });
}

ASFConstructionNetworkState* ASFGameMode::FindClaimState(const FString& ClaimId) const
{
    const FClaimPlot* Plot=ClaimPlots.FindByPredicate([&](const FClaimPlot& Candidate) { return Candidate.ClaimId==ClaimId; });
    return Plot ? Plot->State.Get() : nullptr;
}

void ASFGameMode::LoadClaims()
{
    // A7 for the network path: Housing is the only store. Each plot's claim and structures are read back at start (and
    // retried while the service is unreachable); a structure outside the plot extent rejects that claim's projection.
    bool bPending=false;
    for (FClaimPlot& Plot : ClaimPlots)
    {
        if (Plot.bLoaded) { continue; }
        bPending=true;
        FSFGameplayRequest Get; Get.Rpc=ESFGameplayRpc::GetClaim; Get.ClaimId=Plot.ClaimId;
        USFBackpackComponent::Services()->Call(Get,[Weak=TWeakObjectPtr<ASFGameMode>(this),ClaimId=Plot.ClaimId](const FSFGameplayResponse& Claim)
        {
            ASFGameMode* Self=Weak.Get(); FClaimPlot* Plot=Self ? Self->FindPlot(ClaimId) : nullptr;
            if (!Plot || Plot->bLoaded || Claim.Status==ESFGameplayStatus::Unavailable) { return; }
            if (Claim.Status!=ESFGameplayStatus::Ok)
            { Plot->bLoaded=true; UE_LOG(LogTemp,Display,TEXT("HOUSING_CLAIM_LOADED Claim=%s Claimed=0"),*ClaimId); return; }
            Plot->OwnerId=Claim.OwnerId; Plot->PermissionRevision=Claim.PermissionRevision;
            FSFGameplayRequest List; List.Rpc=ESFGameplayRpc::ListStructures; List.ClaimId=ClaimId;
            USFBackpackComponent::Services()->Call(List,[Weak,ClaimId](const FSFGameplayResponse& Structures)
            {
                ASFGameMode* Again=Weak.Get(); FClaimPlot* Loaded=Again ? Again->FindPlot(ClaimId) : nullptr;
                if (!Loaded || Loaded->bLoaded || Structures.Status!=ESFGameplayStatus::Ok) { return; }
                // An unknown shape key or orientation maps out of range, so the restore rejects the whole claim.
                TArray<FSFConstructionNetworkRestoredPiece> Pieces;
                for (const FSFGameplayResponse::FStructure& S : Structures.Structures)
                {
                    Pieces.Add({S.Cell,ShapeFromHousingKey(S.ShapeKey),S.OrientationIndex>=0 && S.OrientationIndex<SFConstructionGeometry::RotationCount ? FromHousingOrientation(S.OrientationIndex) : INDEX_NONE});
                    Loaded->Structures.Add(S.Cell,S.StructureId);
                }
                Loaded->StructureRevision=Structures.StructureRevision;
                Loaded->bClaimed=true; Loaded->bLoaded=true;
                const bool bMask=Loaded->State.IsValid() && Loaded->State->SetClaimMask(Structures.ClaimMask,
                    Structures.MaskOriginX,Structures.MaskOriginY,Structures.MaskWidth,Structures.MaskHeight);
                const bool bRestored=bMask && Loaded->State->RestoreFromHousing(Pieces);
                if (!bMask) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_RESTORE_REJECTED Claim=%s Mask=invalid"),*ClaimId); }
                UE_LOG(LogTemp,Display,TEXT("HOUSING_CLAIM_LOADED Claim=%s Claimed=1 Owner=%s Structures=%d StructureRevision=%lld Restored=%d Mask=%dx%d"),*ClaimId,*Loaded->OwnerId,Pieces.Num(),Loaded->StructureRevision,bRestored,Structures.MaskWidth,Structures.MaskHeight);
            });
        });
    }
    if (bPending)
    { GetWorldTimerManager().SetTimer(ClaimLoadTimer,FTimerDelegate::CreateUObject(this,&ASFGameMode::LoadClaims),3.0f,false); }
}

void ASFGameMode::ClaimPlot(ASFPlayerController* Controller, const FString& ClaimId, TFunction<void(ESFConstructionNetworkResult)> Done)
{
    if (!HasLivingOwnedPawn(Controller)) { Done(ESFConstructionNetworkResult::InvalidContext); return; }
    USFBackpackComponent* Pack=USFBackpackComponent::FindOrCreate(Controller);
    FClaimPlot* Plot=FindPlot(ClaimId);
    if (!IsHousingActive() || !Pack || !Plot || !Plot->bLoaded) { Done(ESFConstructionNetworkResult::ServiceUnavailable); return; }
    if (Plot->bClaimed) { Done(Plot->OwnerId==Pack->GetCharacterId() ? ESFConstructionNetworkResult::Claimed : ESFConstructionNetworkResult::PermissionDenied); return; }
    FSFGameplayRequest Claim; Claim.Rpc=ESFGameplayRpc::ClaimPlot; Claim.OperationId=TEXT("claim/")+ClaimId+TEXT("/")+Pack->GetCharacterId(); Claim.ClaimId=ClaimId;
    Claim.CharacterId=Pack->GetCharacterId(); Claim.CenterX=Plot->CentreMetres.X; Claim.CenterY=Plot->CentreMetres.Y; Claim.SideMetres=Plot->SideMetres; Claim.PrepaidDays=30;
    USFBackpackComponent::Services()->Call(Claim,[Weak=TWeakObjectPtr<ASFGameMode>(this),ClaimId,Done=MoveTemp(Done)](const FSFGameplayResponse& Reply)
    {
        ASFGameMode* Self=Weak.Get(); FClaimPlot* Claimed=Self ? Self->FindPlot(ClaimId) : nullptr;
        if (!Claimed) { return; }
        UE_LOG(LogTemp,Display,TEXT("HOUSING_CLAIM Claim=%s Ok=%d Code=%d Owner=%s State=%s PermissionRevision=%lld %s"),*ClaimId,Reply.Status==ESFGameplayStatus::Ok,Reply.GrpcCode,*Reply.OwnerId,*Reply.State,Reply.PermissionRevision,*Reply.Message);
        if (Reply.Status!=ESFGameplayStatus::Ok)
        { Done(Reply.Status==ESFGameplayStatus::Unavailable ? ESFConstructionNetworkResult::ServiceUnavailable : ESFConstructionNetworkResult::PermissionDenied); return; }
        Claimed->bClaimed=true; Claimed->OwnerId=Reply.OwnerId; Claimed->PermissionRevision=Reply.PermissionRevision;
        Done(ESFConstructionNetworkResult::Claimed);
    });
}

void ASFGameMode::GrantBuild(ASFPlayerController* Controller, const FString& ClaimId, const FString& TargetCharacterId, TFunction<void(ESFConstructionNetworkResult)> Done)
{
    if (!HasLivingOwnedPawn(Controller)) { Done(ESFConstructionNetworkResult::InvalidContext); return; }
    USFBackpackComponent* Pack=USFBackpackComponent::FindOrCreate(Controller);
    FClaimPlot* Plot=FindPlot(ClaimId);
    if (!IsHousingActive() || !Pack || !Plot || !Plot->bClaimed || TargetCharacterId.IsEmpty()) { Done(ESFConstructionNetworkResult::ServiceUnavailable); return; }
    // The service checks that the actor owns the claim; the client only names the grantee.
    FSFGameplayRequest Grant; Grant.Rpc=ESFGameplayRpc::Grant; Grant.ClaimId=ClaimId; Grant.CharacterId=Pack->GetCharacterId(); Grant.GranteeId=TargetCharacterId;
    Grant.Rights=1; Grant.ExpectedPermissionRevision=Plot->PermissionRevision;
    Grant.OperationId=FString::Printf(TEXT("grant/%s/%s/%lld"),*ClaimId,*TargetCharacterId,Plot->PermissionRevision);
    USFBackpackComponent::Services()->Call(Grant,[Weak=TWeakObjectPtr<ASFGameMode>(this),ClaimId,TargetCharacterId,Done=MoveTemp(Done)](const FSFGameplayResponse& Reply)
    {
        ASFGameMode* Self=Weak.Get(); FClaimPlot* Granted=Self ? Self->FindPlot(ClaimId) : nullptr;
        if (!Granted) { return; }
        UE_LOG(LogTemp,Display,TEXT("HOUSING_GRANT Claim=%s Target=%s Ok=%d Code=%d PermissionRevision=%lld %s"),*ClaimId,*TargetCharacterId,Reply.Status==ESFGameplayStatus::Ok,Reply.GrpcCode,Reply.PermissionRevision,*Reply.Message);
        if (Reply.Status==ESFGameplayStatus::Ok) { Granted->PermissionRevision=Reply.PermissionRevision; }
        Done(Reply.Status==ESFGameplayStatus::Ok ? ESFConstructionNetworkResult::Granted
            : Reply.Status==ESFGameplayStatus::Unavailable ? ESFConstructionNetworkResult::ServiceUnavailable : ESFConstructionNetworkResult::PermissionDenied);
    });
}

void ASFGameMode::CommitHousing(ASFPlayerController* Controller, const FString& ClaimId, FIntVector Cell, bool bRemove, int32 Shape, int32 Rotation,
    TFunction<void(ESFConstructionNetworkResult Result, const FString& Operation)> Done)
{
    TWeakObjectPtr<ASFGameMode> Weak(this);
    TWeakObjectPtr<ASFPlayerController> WeakController(Controller);
    FClaimPlot* Target=FindPlot(ClaimId);
    if (!HasLivingOwnedPawn(Controller)) { Done(ESFConstructionNetworkResult::InvalidContext,FString()); return; }
    if (!Target || !Target->bLoaded) { Done(ESFConstructionNetworkResult::ServiceUnavailable,FString()); return; }
    // The first placement on an unclaimed plot claims it for the builder; on someone else's claim the service decides.
    auto Commit=[Weak,WeakController,ClaimId,Cell,bRemove,Shape,Rotation,Done](ESFConstructionNetworkResult ClaimResult)
    {
        ASFGameMode* Self=Weak.Get();
        FClaimPlot* Plot=Self ? Self->FindPlot(ClaimId) : nullptr;
        USFBackpackComponent* Pack=USFBackpackComponent::FindOrCreate(WeakController.Get());
        const FString* Existing=Plot ? Plot->Structures.Find(Cell) : nullptr;
        // ponytail: every pilot shape is a wall piece and costs the shipped wall bundle; per-shape content follows BLD-01 pieces.
        const FSFStationRecipe* Cost=SFStationRecipes::Find(TEXT("recipe/wall_material_bundle"));
        if (!HasLivingOwnedPawn(WeakController.Get())) { Done(ESFConstructionNetworkResult::InvalidContext,FString()); return; }
        if (ClaimResult==ESFConstructionNetworkResult::ServiceUnavailable) { Done(ClaimResult,FString()); return; }
        if (!Plot || !Plot->bClaimed || !Pack || !Cost || (bRemove && !Existing)) { Done(ESFConstructionNetworkResult::ServiceUnavailable,FString()); return; }
        const FString Operation=(bRemove ? TEXT("remove/") : TEXT("place/"))+FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
        const FString Structure=bRemove ? *Existing : FString::Printf(TEXT("str_%s_x%d_y%d_z%d"),*ClaimId.RightChop(4),Cell.X,Cell.Y,Cell.Z);
        FSFItemStack Material=Cost->Outputs[0];
        if (!bRemove)
        {
            static const TCHAR* Grades[] = {TEXT("crude"), TEXT(""), TEXT("fine"), TEXT("superior"), TEXT("masterwork")};
            auto* Bag=Pack->GetOwner()->FindComponentByClass<USFInventoryComponent>();
            for (const TCHAR* Grade : Grades)
            {
                const FString Id=*Grade ? Material.DefinitionId+TEXT("/")+Grade : Material.DefinitionId;
                const auto Asset=TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(SFStationRecipes::ItemAssetPath(Id)));
                if (Bag && SFInventoryTransactions::GetItemQuantity(Bag,Asset)>=Material.Quantity)
                { Material.DefinitionId=Id; break; }
            }
        }
        TSharedRef<FSFGameplayResponse> Receipt=MakeShared<FSFGameplayResponse>();
        FSFBackpackMutation Mutation; Mutation.Label=bRemove ? TEXT("housing_remove") : TEXT("housing_place"); Mutation.OperationId=Operation;
        Mutation.Run=[Weak,WeakController,Receipt,ClaimId,Cell,bRemove,Shape,Rotation,Structure,Material,WeakPack=TWeakObjectPtr<USFBackpackComponent>(Pack)](const FString& Op,int64 Expected,TFunction<void(ESFBackpackOutcome,int64)> Report)
        {
            ASFGameMode* Mode=Weak.Get(); FClaimPlot* Current=Mode ? Mode->FindPlot(ClaimId) : nullptr; USFBackpackComponent* Owner=WeakPack.Get();
            if (!Current || !Owner || !HasLivingOwnedPawn(WeakController.Get())) { Report(ESFBackpackOutcome::Rejected,-1); return; }
            FSFGameplayRequest Q; Q.Rpc=bRemove ? ESFGameplayRpc::Remove : ESFGameplayRpc::Place; Q.OperationId=Op; Q.ClaimId=ClaimId;
            Q.CharacterId=Owner->GetCharacterId(); Q.ExpectedPermissionRevision=Current->PermissionRevision; Q.ExpectedStructureRevision=Current->StructureRevision;
            Q.StructureId=Structure; Q.Cell=Cell; Q.ContentId=TEXT("piece/wall"); Q.MaterialId=Material.DefinitionId; Q.MaterialQuantity=Material.Quantity;
            Q.ContainerId=Owner->GetContainerId(); Q.ExpectedContainerRevision=Expected; Q.RemovalKind=TEXT("voluntary");
            Q.ShapeKey=HousingShapeKey(Shape); Q.OrientationIndex=ToHousingOrientation(Rotation);
            USFBackpackComponent::Services()->Call(Q,[Receipt,Report](const FSFGameplayResponse& R)
            {
                *Receipt=R;
                // The reply carries no container revision, so the backpack re-reads before its next mutation.
                Report(R.Status==ESFGameplayStatus::Ok ? ESFBackpackOutcome::Committed : R.Status==ESFGameplayStatus::Unavailable ? ESFBackpackOutcome::Unavailable
                    : R.GrpcCode==9 ? ESFBackpackOutcome::Stale : ESFBackpackOutcome::Rejected,-1);
            });
        };
        Mutation.Finished=[Weak,Receipt,ClaimId,Cell,bRemove,Structure,Material,Operation,WeakPack=TWeakObjectPtr<USFBackpackComponent>(Pack),Done](ESFBackpackOutcome Outcome)
        {
            ASFGameMode* Mode=Weak.Get(); FClaimPlot* Current=Mode ? Mode->FindPlot(ClaimId) : nullptr; USFBackpackComponent* Owner=WeakPack.Get();
            const bool bCommitted=Outcome==ESFBackpackOutcome::Committed && (Receipt->State==TEXT("Committed") || Receipt->State==TEXT("Removed"));
            UE_LOG(LogTemp,Display,TEXT("HOUSING_OUTCOME Op=%s Claim=%s Action=%s Structure=%s Outcome=%d State=%s StructureRevision=%lld Code=%d %s"),*Operation,*ClaimId,
                bRemove ? TEXT("Remove") : TEXT("Place"),*Structure,int32(Outcome),*Receipt->State,Receipt->StructureRevision,Receipt->GrpcCode,*Receipt->Message);
            if (bCommitted && Current)
            {
                Current->StructureRevision=Receipt->StructureRevision;
                if (bRemove) { Current->Structures.Remove(Cell); } else { Current->Structures.Add(Cell,Structure); }
                // Placement consumed exactly the bundle; a refund's quantity is not in the reply, so re-read the backpack.
                if (Owner) { if (bRemove) { Owner->Reconcile(); } else { Owner->ApplyToBag({{Material.DefinitionId,-Material.Quantity}}); } }
                // The placement reply has no claim mask. Read the durable mask before projecting the edit so a
                // Housing growth or shrink is reflected in the replicated state on this same action.
                FSFGameplayRequest List; List.Rpc=ESFGameplayRpc::ListStructures; List.ClaimId=ClaimId;
                USFBackpackComponent::Services()->Call(List,[Weak,ClaimId,Operation,Done](const FSFGameplayResponse& Structures)
                {
                    ASFGameMode* Again=Weak.Get(); FClaimPlot* Plot=Again ? Again->FindPlot(ClaimId) : nullptr;
                    const bool bMask=Plot && Plot->State.IsValid() && Structures.Status==ESFGameplayStatus::Ok
                        && Plot->State->SetClaimMask(Structures.ClaimMask,Structures.MaskOriginX,Structures.MaskOriginY,
                            Structures.MaskWidth,Structures.MaskHeight);
                    if (!bMask)
                    { UE_LOG(LogTemp,Error,TEXT("HOUSING_MASK_REFRESH_FAILED Claim=%s Op=%s Code=%d"),*ClaimId,*Operation,Structures.GrpcCode); }
                    Done(bMask ? ESFConstructionNetworkResult::Accepted : ESFConstructionNetworkResult::ServiceUnavailable,Operation);
                });
                return;
            }
            // A refusal for rights (no build grant on someone else's claim) reaches the player as such.
            Done(bCommitted ? ESFConstructionNetworkResult::Accepted : Receipt->GrpcCode==7 ? ESFConstructionNetworkResult::PermissionDenied
                : ESFConstructionNetworkResult::ServiceUnavailable,Operation);
        };
        Pack->Enqueue(MoveTemp(Mutation));
    };
    if (Target->bClaimed) { Commit(ESFConstructionNetworkResult::Claimed); }
    else { ClaimPlot(Controller,ClaimId,Commit); }
}

// The sandbox map and the dev review maps under /Maps/Dev/ (GM-01 build-mode review) run construction on a flat floor.
static bool IsConstructionFixtureMap(const UWorld* World)
{
    const FString Name = World->GetOutermost()->GetName();
    return Name == TEXT("/Game/Frontier/Maps/Greenlands_ConstructionSandbox") ||
        Name == TEXT("/Game/Frontier/Maps/Sandbox/SB_RestAreas") || Name.StartsWith(TEXT("/Game/Frontier/Maps/Dev/"));
}

bool ASFGameMode::IsAdmitted() const
{
    // GM-01: on wherever the world has construction content (the sandbox map, a zone with build plots), on any authority,
    // so dedicated, listen, standalone and PIE all build. Typed bNetworkConstructionEnabled turns it off.
    return HasAuthority() && !IsActorBeingDestroyed() && GetClass() == StaticClass() && GetWorld() && GetNetMode() != NM_Client
        && GetDefault<USFConstructionSandboxSettings>()->bNetworkConstructionEnabled
        && (IsConstructionFixtureMap(GetWorld()) || IsZoneGatheringWorld());
}

void ASFGameMode::StartConstruction()
{
    if (!IsAdmitted())
    {
        // Worlds with no construction content (E1, other zones) stay silent, as before the GM-01 fold.
        if (IsConstructionFixtureMap(GetWorld()) || IsZoneGatheringWorld())
        { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DISABLED")); }
        return;
    }
    // The sandbox keeps one identity-anchored state and its map-placed foundation; a zone gets one claim state per
    // build plot (BLD-02 A2), rebuilt from Housing when the services are on.
    const bool bHousingPlotMap = IsZoneGatheringWorld() || GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_RestAreas");
    if (!(bHousingPlotMap && PrepareClaimPlots()>0))
    {
        auto* State = GetWorld()->SpawnActor<ASFConstructionNetworkState>();
        if (!State) { UE_LOG(LogTemp,Fatal,TEXT("Construction network fixture state could not spawn")); }
    }
    else if (IsHousingActive()) { LoadClaims(); }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_READY UnlimitedDevelopmentStock=%d Shapes=CubeSlabSlope Support=RoughTimber Persistence=%d"),
        USFConstructionSandboxSettings::AllowsUnpaidEdits(),IsHousingActive());
    if (USFGatheringPilotComponent::IsPilotRequested())
    {
        FString Error;
        if (!GetDefault<USFGatheringPilotSettings>()->Validate(Error))
        { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_CONFIGURATION_FAILED %s"),*Error); return; }
        if (IsZoneGatheringWorld())
        {
            const int32 Nodes=SpawnZoneGatheringNodes();
            int32 Stations=0;
            for (TActorIterator<ASFGatheringPilotNode> It(GetWorld()); It; ++It) { Stations+=It->IsRefinery(); }
            UE_LOG(LogTemp,Display,TEXT("GATHERING_PILOT_READY Source=ZoneRecord Nodes=%d Stations=%d SessionOnly=1"),Nodes,Stations);
            return;
        }
        // The sandbox map has no zone record; its four nodes are a development test fixture.
        bool bFixtureNodes=false;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        bFixtureNodes=FParse::Param(FCommandLine::Get(),TEXT("GatheringFixtureNodes"));
#endif
        if (!bFixtureNodes) { return; }
        const FVector Positions[4]={FVector(0,-350,70),FVector(0,-650,70),FVector(-400,-600,40),FVector(400,-600,40)};
        for (int32 I=0; I<4; ++I)
        {
            auto* Node=GetWorld()->SpawnActorDeferred<ASFGatheringPilotNode>(ASFGatheringPilotNode::StaticClass(),FTransform(Positions[I]));
            if (!Node) { UE_LOG(LogTemp,Fatal,TEXT("Gathering pilot node could not spawn")); }
            Node->Kind=static_cast<ESFGatheringPilotKind>(I);
            Node->FinishSpawning(FTransform(Positions[I]));
        }
        UE_LOG(LogTemp,Display,TEXT("GATHERING_PILOT_READY Resources=2 Stations=2 Inventory=SFInventory SessionOnly=1"));
    }
}

void ASFGameMode::PostLoginConstruction(APlayerController* NewPlayer)
{
    auto* Controller = Cast<ASFPlayerController>(NewPlayer);
    if (!IsAdmitted() || !Controller) { return; }
    const bool bAssignBuilder = !bBuilderAssigned;
    bBuilderAssigned = true;
    if (bAssignBuilder) { Builder = Controller; }
    Controller->AssignRole(bAssignBuilder);
    if (APawn* Pawn = Controller->GetPawn(); Pawn && !IsZoneGatheringWorld()) { Pawn->SetActorLocation(FVector(bAssignBuilder ? -200 : 200,-400,96)); }
    if (IsZoneGatheringWorld()) { PlaceForZoneScenario(Controller->GetPawn(), bAssignBuilder); }
    if (IsZoneGatheringWorld()) { RefreshZoneGatheringNodes(); }
    FString RestScenario;
    const bool bRestScenario = FParse::Value(FCommandLine::Get(), TEXT("RestAreaScenario="), RestScenario);
    if (GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_RestAreas") && Controller->GetPawn() &&
        (bRestScenario || FParse::Param(FCommandLine::Get(), TEXT("ClaimScenario"))))
    {
        FVector At = RestScenario == TEXT("Inn") ? FVector(-1200, 0, 150) : FVector(2000, 0, 150);
        if ((RestScenario == TEXT("House") || RestScenario.IsEmpty()) && !ClaimPlots.IsEmpty() && ClaimPlots[0].State.IsValid())
        { At = ClaimPlots[0].State->GetAnchorOffsetCm() + FVector(RestScenario.IsEmpty() ? -175 : 0, -125, 120); }
        Controller->GetPawn()->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
    }
    if (GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook") &&
        RestScenario == TEXT("InnOutside") && Controller->GetPawn())
    {
        FVector At(90000, 90000, 10000);  // wilderness, outside the protected settlement
        if (bAssignBuilder)
        {
            ASFRestAreaVolume* Inn = nullptr;
            for (TActorIterator<ASFRestAreaVolume> It(GetWorld()); It; ++It)
            {
                if (It->Kind == ESFRestAreaKind::Inn) { Inn = *It; break; }
            }
            if (!ensureMsgf(Inn, TEXT("Greenlands inn rest volume is missing"))) { return; }
            At = Inn->GetActorLocation();
        }
        Controller->GetPawn()->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogTemp, Display, TEXT("REST_AREA_SCENARIO_PLACED InsideInn=%d At=%s"),
            ASFRestAreaVolume::IsInsideInn(GetWorld(), At), *At.ToString());
    }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_ROLE Builder=%d"),bAssignBuilder);
    USFCharacterSessionComponent::AttachIfEnabled(Controller);
    for (TActorIterator<ASFConstructionNetworkState> It(GetWorld()); It; ++It) { It->AdmitPair(); }
}

void ASFGameMode::RestartConstruction(AController* NewPlayer)
{
    const auto* Controller = Cast<ASFPlayerController>(NewPlayer);
    if (IsAdmitted() && !IsZoneGatheringWorld() && Controller && Controller->GetPawn())
    { Controller->GetPawn()->SetActorLocation(FVector(IsBuilder(Controller) ? -200 : 200,-400,96)); }
    if (IsAdmitted() && IsZoneGatheringWorld() && Controller) { PlaceForZoneScenario(Controller->GetPawn(), IsBuilder(Controller)); }
}

bool ASFGameMode::IsBuilder(const ASFPlayerController* Controller) const
{
    return IsAdmitted() && Builder.Get() == Controller;
}

void ASFPlayerController::AssignRole(bool bBuilder)
{
    if (!HasAuthority()) { return; }
    bBuilderRole = bBuilder; bRoleAssigned = true;
    if (GatheringPilot) { GatheringPilot->AssignPilotRole(bBuilder); }
    ForceNetUpdate();
}

ASFPlayerController::ASFPlayerController()
{
    PrimaryActorTick.bCanEverTick=true;
    SelectedRotation=uint8(SFConstructionGeometry::IdentityRotation());
    GatheringPilot=CreateDefaultSubobject<USFGatheringPilotComponent>(TEXT("GatheringPilot"));
    CreateDefaultSubobject<USFLootComponent>(TEXT("Loot"));
    CreateDefaultSubobject<USFActionTargetComponent>(TEXT("ActionTarget"));
    auto* Inventory=CreateDefaultSubobject<USFInventoryComponent>(TEXT("GatheringPilotInventory"));
    PilotInventory=Inventory;
    Inventory->AllowedEquipmentSlots=USFGatheringPilotComponent::EquipmentSlots();
    Inventory->NumSlots=GetDefault<USFGatheringPilotSettings>()->InventorySlots;
    Inventory->MaxWeight=GetDefault<USFGatheringPilotSettings>()->InventoryWeightKg;
}

void ASFPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ASFPlayerController,bRoleAssigned,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ASFPlayerController,bBuilderRole,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ASFPlayerController,PartyView,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ASFPlayerController,PartyInvites,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ASFPlayerController,PartyLeaderName,COND_OwnerOnly);
}

void ASFPlayerController::BeginPlay()
{
    Super::BeginPlay();
    BeginPlayInteraction();
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bScenarioCursor=FParse::Param(FCommandLine::Get(),TEXT("ConstructionNetworkScenario"));
#endif
    InitializeLocalControls();
}

void ASFPlayerController::InitializeLocalControls()
{
    if (!IsLocalController()) { return; }
    if (!bOwnsLookSuppression) { SetIgnoreLookInput(true); bOwnsLookSuppression=true; }
    if (!Palette && GetLocalPlayer() && GetLocalPlayer()->ViewportClient)
    {
        Palette=USFConstructionSandboxPalette::Create(this);
        if (Palette)
        {
            Palette->ConfigureNetworkPresentation(); Palette->OnAction.BindUObject(this,&ASFPlayerController::PaletteAction);
            Palette->AddToViewport(20); Palette->SetVisibility(ESlateVisibility::Collapsed); Palette->SetBuildModeActive(bBuildMode);
        }
        bool bRegisterFocus=true;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        bRegisterFocus=!bScenarioCursor;
#endif
        if (bRegisterFocus && !ActivationHandle.IsValid() && FSlateApplication::IsInitialized())
        { ActivationHandle=FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this,&ASFPlayerController::ApplicationActivationChanged); }
    }
    EnsureGatheringSkillsPanel();
    ApplyCursorMode();
}

void ASFPlayerController::EnsureGatheringSkillsPanel()
{
    if (GatheringSkillsPanel || !IsLocalController() || !GetLocalPlayer()
        || !GetLocalPlayer()->ViewportClient || !GatheringPilot || !GatheringPilot->IsEnabled()) { return; }
    const TSubclassOf<USFGatheringSkillsPanel> SkillsClass=LoadClass<USFGatheringSkillsPanel>(nullptr,USFGatheringSkillsPanel::AssetPath);
    if (!SkillsClass) { UE_LOG(LogTemp,Error,TEXT("SKILLS_WBP_MISSING %s: run tools/ui/create_widgets.py"),USFGatheringSkillsPanel::AssetPath); return; }
    GatheringSkillsPanel=CreateWidget<USFGatheringSkillsPanel>(this,SkillsClass);
    if (!GatheringSkillsPanel) { return; }
    GatheringSkillsPanel->Configure(GatheringPilot);
    GatheringSkillsPanel->AddToViewport(20);
    GatheringSkillsPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void ASFPlayerController::ToggleGatheringSkillsPanel()
{
    EnsureGatheringSkillsPanel();
    if (!GatheringSkillsPanel || !GatheringPilot || !GatheringPilot->IsEnabled() || bCameraLookHeld) { return; }
    // A window of the stack like the inventory: it lines up at the left and Esc closes the newest.
    if (GetWindowStack()->IsOpen(GatheringSkillsPanel)) { GetWindowStack()->Close(GatheringSkillsPanel); } else { GetWindowStack()->Open(GatheringSkillsPanel); }
}

void ASFPlayerController::HandleEscape()
{
    if (GetWorld()) if (USFE1VitalsHudSubsystem* Vitals = GetWorld()->GetSubsystem<USFE1VitalsHudSubsystem>())
        if (Vitals->CloseNotice()) return;
    if (IsDialogueOpen()) { CloseDialogue(); return; }
    if (CloseSettingsPanel()) { return; }
    // The build catalogue is a window too: while it is the newest, Esc cancels a pending line start, then leaves build mode (which closes it).
    if (Palette && GetWindowStack()->Top()==Palette) { if (bBuildMode && !bHasLineSeed) { SetBuildMode(false); } else { CancelCursorAction(); } return; }
    if (GetWindowStack()->CloseTop()) { return; }  // Esc closes the newest menu window
    if (GatheringSkillsPanel && GatheringSkillsPanel->GetVisibility()==ESlateVisibility::Visible)
    { GatheringSkillsPanel->SetVisibility(ESlateVisibility::Collapsed); return; }
    // Build mode: the first Esc cancels a pending line start, the next leaves build mode.
    if (bBuildMode && !bHasLineSeed) { SetBuildMode(false); return; }
    if (CancelCursorAction()) { return; }
    ToggleGameMenu(true);  // nothing else wanted Esc: the game menu
}

void ASFPlayerController::ToggleBuildMode()
{
    // Only where something can be built: a construction state has replicated (the sandbox map or a zone build plot).
    if (!IsLocalController() || !HostsBuildMode() || (!bBuildMode && !TActorIterator<ASFConstructionNetworkState>(GetWorld()))) { return; }
    SetBuildMode(!bBuildMode);
}

void ASFPlayerController::SetBuildMode(bool bActive)
{
    if (!bActive) { CancelLine(); bDemolitionMode=false; bHasPlacementPreview=false; bHasRemovalPreview=false; bHasDoorTarget=false; }
    bBuildMode=bActive;
    if (Palette) { Palette->SetBuildModeActive(bActive); }
    if (HudLayout) { HudLayout->SetBuildModeActive(bActive); }
    UE_LOG(LogTemp,Display,TEXT("BUILD_MODE Enabled=%d"),bActive);
}

void ASFPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
    Super::ClientRestart_Implementation(NewPawn);
    TabTargetSnapshot.Reset(); NextTabTargetIndex = 0; LastTabAt = -1.0e30;
    if (!HostsBuildMode()) { return; }
    bOwnsLookSuppression=false; bCameraLookHeld=false; CancelLine();
    InitializeLocalControls();
}

void ASFPlayerController::OnNetCleanup(UNetConnection* Connection)
{
    auto* Session = FindComponentByClass<USFCharacterSessionComponent>();
    if (!Session || !Session->BeginDrain()) { Super::OnNetCleanup(Connection); return; }
    // The engine's cleanup without the Destroy: the connection is going away, the controller and pawn stay until the
    // session sees every pending outcome resolved (or LinkdeadDrainSeconds), then it destroys the controller.
    if (UWorld* World = GetWorld()) { World->DestroySwappedPC(Connection); }
    if (Player && Player->PlayerController == this) { Player->PlayerController = nullptr; }
    Player = nullptr; NetConnection = nullptr;
}

void ASFPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ActivationHandle.IsValid() && FSlateApplication::IsInitialized())
    { FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle); }
    if (Palette) { Palette->OnAction.Unbind(); Palette->RemoveFromParent(); Palette=nullptr; }
    if (GatheringSkillsPanel) { GatheringSkillsPanel->RemoveFromParent(); GatheringSkillsPanel=nullptr; }
    if (bOwnsLookSuppression) { SetIgnoreLookInput(false); bOwnsLookSuppression=false; }
    Super::EndPlay(Reason);
}

bool ASFPlayerController::CanUseConstructionControls() const
{
    if (!bApplicationActive || bCameraLookHeld) { return false; }
    if (bDispatchingPaletteAction) { return true; }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bScenarioCursor) { return !bSyntheticPointerOverPalette; }
#endif
    if (GatheringSkillsPanel && GatheringSkillsPanel->GetVisibility()==ESlateVisibility::Visible && GatheringSkillsPanel->IsHovered()) { return false; }
    return !Palette || !Palette->IsPointerOverPalette();
}

void ASFPlayerController::ApplyCursorMode()
{
    bShowMouseCursor=!bCameraLookHeld;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bScenarioCursor) { return; }
#endif
    if (!GetLocalPlayer() || !GetLocalPlayer()->ViewportClient) { return; }
    if (bCameraLookHeld)
    { FInputModeGameOnly Mode; Mode.SetConsumeCaptureMouseDown(false); SetInputMode(Mode); }
    else
    { FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode); }
}

void ASFPlayerController::BeginCameraLook()
{
    if (!CanUseConstructionControls()) { return; }
    bHasPlacementPreview=false; bHasRemovalPreview=false; bHasDoorTarget=false; SampledRevision=MAX_uint64;
    bCameraLookHeld=true;
    if (bOwnsLookSuppression) { SetIgnoreLookInput(false); bOwnsLookSuppression=false; }
    ApplyCursorMode();
}

void ASFPlayerController::EndCameraLook()
{
    if (!bCameraLookHeld) { return; }
    bCameraLookHeld=false;
    bHasPlacementPreview=false; bHasRemovalPreview=false; bHasDoorTarget=false; SampledRevision=MAX_uint64;
    if (!bOwnsLookSuppression) { SetIgnoreLookInput(true); bOwnsLookSuppression=true; }
    ApplyCursorMode();
}

void ASFPlayerController::ApplicationActivationChanged(bool bActive)
{
    bApplicationActive=bActive;
    if (!bActive)
    {
        if (GatheringPilot) { GatheringPilot->CancelLocalHarvest(); }
        EndCameraLook(); CancelLine(); bHasPlacementPreview=false; bHasRemovalPreview=false; bHasDoorTarget=false;
        if (PlayerInput) { PlayerInput->FlushPressedKeys(); }
        if (GetPawn()) { GetPawn()->ConsumeMovementInputVector(); }
    }
}

bool ASFPlayerController::GetConstructionRay(FVector& Start, FVector& Direction) const
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bScenarioCursor)
    { FRotator View; GetPlayerViewPoint(Start,View); Direction=View.Vector(); return !Start.ContainsNaN() && Direction.IsNormalized(); }
#endif
    float X=0,Y=0; int32 Width=0,Height=0; GetViewportSize(Width,Height);
    return GetMousePosition(X,Y) && FMath::IsFinite(X) && FMath::IsFinite(Y) && X>=0 && Y>=0 && X<Width && Y<Height
        && DeprojectScreenPositionToWorld(X,Y,Start,Direction) && !Start.ContainsNaN() && !Direction.ContainsNaN() && Direction.IsNormalized();
}

bool ASFPlayerController::PrepareCursorAction()
{
    if (!CanUseConstructionControls()) { return false; }
    auto* State=ActiveState();
    if (State && SampledRevision!=MAX_uint64 && State->GetSnapshot().Revision!=SampledRevision)
    { CancelLine(); bHasPlacementPreview=false; bHasRemovalPreview=false; bHasDoorTarget=false; LastPlacementStatus=TEXT("Build changed. Choose a fresh preview."); return false; }
    RefreshPlacementPreview(false); return State!=nullptr;
}

void ASFPlayerController::ToggleDemolition()
{
    if (!CanUseConstructionControls()) { return; }
    CancelLine(); bDemolitionMode=!bDemolitionMode;
    LastPlacementStatus=bDemolitionMode ? TEXT("Demolition mode: click a highlighted piece to remove it.") : TEXT("Placement mode.");
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DEMOLITION_MODE Enabled=%d"),bDemolitionMode);
}

void ASFPlayerController::TogglePlacementPlane()
{ if (CanUseConstructionControls()) { CancelLine(); bDemolitionMode=false; bLayerPlacement=!bLayerPlacement; } }
void ASFPlayerController::LayerDown()
{ if (CanUseConstructionControls()) { CancelLine(); bDemolitionMode=false; bLayerPlacement=true; PlacementLayer=FMath::Max(0,PlacementLayer-1); } }
void ASFPlayerController::LayerUp()
{ if (CanUseConstructionControls()) { CancelLine(); bDemolitionMode=false; bLayerPlacement=true; PlacementLayer=FMath::Min(19,PlacementLayer+1); } }

FString ASFPlayerController::GetPlaneStatusText() const
{
    return bLayerPlacement ? FString::Printf(TEXT("Layer %d (%.1f m)"),PlacementLayer,PlacementLayer*SFConstructionGeometry::CellCentimetres/100.0) : TEXT("Surface");
}

bool ASFPlayerController::GetLayerPoint(const FVector& Start, const FVector& Direction, double RayLength, const FHitResult* Hit, FVector& OutPoint) const
{
    if (PlacementLayer<0 || PlacementLayer>19 || Start.ContainsNaN() || Direction.ContainsNaN() || !Direction.IsNormalized()
        || !FMath::IsFinite(RayLength) || RayLength<=0 || FMath::Abs(Direction.Z)<=1.e-8) { return false; }
    const auto* AnchorState=ActiveState();
    const double Height=PlacementLayer*SFConstructionGeometry::CellCentimetres+(AnchorState ? AnchorState->GetAnchorOffsetCm().Z : 0.0);
    const double T=(Height-Start.Z)/Direction.Z;
    if (!FMath::IsFinite(T) || T<0 || T>RayLength || (Hit && (!FMath::IsFinite(Hit->Distance) || Hit->Distance<T-1.e-3))) { return false; }
    FVector Point=Start+Direction*T; Point.Z=Height;
    const FVector Local=Point-(AnchorState ? AnchorState->GetAnchorOffsetCm() : FVector::ZeroVector);
    if (Point.ContainsNaN() || Local.X < -1000 || Local.X>=1000 || Local.Y < -1000 || Local.Y>=1000) { return false; }
    OutPoint=Point; return true;
}

bool ASFPlayerController::CancelCursorAction()
{
    bool bCancelled=GatheringPilot && GatheringPilot->CancelLocalHarvest();
    if (CanUseConstructionControls()) { bCancelled=bCancelled || bHasLineSeed; CancelLine(); LastPlacementStatus=TEXT("Line start canceled."); }
    return bCancelled;
}

void ASFPlayerController::PaletteAction(ESFConstructionPaletteAction Action)
{
    if (!bApplicationActive || bCameraLookHeld) { return; }
    TGuardValue<bool> Dispatch(bDispatchingPaletteAction,true);
    switch (Action)
    {
    case ESFConstructionPaletteAction::Cube: SelectCube(); break;
    case ESFConstructionPaletteAction::Slab: SelectSlab(); break;
    case ESFConstructionPaletteAction::Slope: SelectSlope(); break;
    case ESFConstructionPaletteAction::Door: SelectDoor(); break;
    case ESFConstructionPaletteAction::Rotate: RotatePiece(); break;
    case ESFConstructionPaletteAction::Single: CancelLine(); bLineMode=false; bDemolitionMode=false; break;
    case ESFConstructionPaletteAction::Line: if (!bLineMode || bDemolitionMode) { bLineMode=false; ToggleLineMode(); } break;
    case ESFConstructionPaletteAction::Demolish: ToggleDemolition(); break;
    case ESFConstructionPaletteAction::Surface: CancelLine(); bDemolitionMode=false; bLayerPlacement=false; break;
    case ESFConstructionPaletteAction::Layer: CancelLine(); bDemolitionMode=false; bLayerPlacement=true; break;
    case ESFConstructionPaletteAction::LayerDown: LayerDown(); break;
    case ESFConstructionPaletteAction::LayerUp: LayerUp(); break;
    case ESFConstructionPaletteAction::Cancel: CancelCursorAction(); break;
    default: break;
    }
}

void ASFPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    // The standalone sandbox fixture binds its own legacy keys (ASFConstructionSandboxController::SetupInputComponent).
    if (!HostsBuildMode()) { return; }
    auto* Input=Cast<UEnhancedInputComponent>(InputComponent);
    if (!Input) { UE_LOG(LogTemp,Error,TEXT("GM-01 controls need the Enhanced Input component (DefaultInput.ini)")); return; }
    const auto Bind=[this,Input](const TCHAR* Name,ETriggerEvent Event,void (ASFPlayerController::*Handler)())
    {
        if (const UInputAction* Action=LoadObject<UInputAction>(nullptr,*FString::Printf(TEXT("/Game/Frontier/Input/%s.%s"),Name,Name)))
        {
#if !UE_BUILD_SHIPPING
            // Review evidence: each mapped action logs when it reaches its handler.
            Input->BindActionInstanceLambda(Action,Event,[this,Handler,Name,Event](const FInputActionInstance&)
            { UE_LOG(LogTemp,Display,TEXT("INPUT_ACTION %s Event=%d"),Name,(int32)Event); (this->*Handler)(); });
#else
            Input->BindAction(Action,Event,this,Handler);
#endif
        }
        else { UE_LOG(LogTemp,Error,TEXT("GM-01 input action %s missing; run tools/Create-PlayerInputAssets.py"),Name); }
    };
    // IMC_Player (always on; CTL-01 key map). The camera itself is polled by ASFE1Character.
    Bind(TEXT("IA_BuildMode"),ETriggerEvent::Started,&ASFPlayerController::ToggleBuildMode);
    Bind(TEXT("IA_Inventory"),ETriggerEvent::Started,&ASFPlayerController::ToggleInventoryPanel);
    Bind(TEXT("IA_Equipment"),ETriggerEvent::Started,&ASFPlayerController::ToggleEquipmentPanel);
    Bind(TEXT("IA_Skills"),ETriggerEvent::Started,&ASFPlayerController::ToggleGatheringSkillsPanel);
    Bind(TEXT("IA_CycleRecipe"),ETriggerEvent::Started,&ASFPlayerController::CycleGatheringRecipe);
    Bind(TEXT("IA_ClaimPlot"),ETriggerEvent::Started,&ASFPlayerController::ClaimPlotFromView);
    Bind(TEXT("IA_Escape"),ETriggerEvent::Started,&ASFPlayerController::HandleEscape);
    Bind(TEXT("IA_MouseLook"),ETriggerEvent::Started,&ASFPlayerController::BeginCameraLook);
    Bind(TEXT("IA_MouseLook"),ETriggerEvent::Completed,&ASFPlayerController::EndCameraLook);
    Bind(TEXT("IA_Trade"),ETriggerEvent::Started,&ASFPlayerController::ToggleTradeDemo);
    Bind(TEXT("IA_Market"),ETriggerEvent::Started,&ASFPlayerController::ToggleMarketDemo);
    InputComponent->BindKey(EKeys::F10,IE_Pressed,this,&ASFPlayerController::ToggleSettingsPanel);
    Bind(TEXT("IA_TargetNext"),ETriggerEvent::Started,&ASFPlayerController::SelectNextCombatTarget);
    // IMC_Build (the character layers it on while build mode is active; it consumes these keys from IMC_Player).
    Bind(TEXT("IA_BuildPlace"),ETriggerEvent::Started,&ASFPlayerController::PlaceFromView);
    Bind(TEXT("IA_BuildDemolish"),ETriggerEvent::Started,&ASFPlayerController::ToggleDemolition);
    Bind(TEXT("IA_BuildRotate"),ETriggerEvent::Started,&ASFPlayerController::RotatePiece);
    Bind(TEXT("IA_BuildLine"),ETriggerEvent::Started,&ASFPlayerController::ToggleLineMode);
    Bind(TEXT("IA_BuildPlane"),ETriggerEvent::Started,&ASFPlayerController::TogglePlacementPlane);
    Bind(TEXT("IA_BuildLayerUp"),ETriggerEvent::Started,&ASFPlayerController::LayerUp);
    Bind(TEXT("IA_BuildLayerDown"),ETriggerEvent::Started,&ASFPlayerController::LayerDown);
    Bind(TEXT("IA_BuildDoor"),ETriggerEvent::Started,&ASFPlayerController::ToggleDoorFromView);
    // The hotbar keys 1-9, 0 (IMC_Player; build mode no longer takes 1-4, the hotbar's build pieces do that).
    for (int32 Slot=0; Slot<USFHudHotbarWidget::SlotCount; ++Slot)
    {
        const FString Name=FString::Printf(TEXT("IA_Hotbar%d"),Slot+1);
        if (const UInputAction* Action=LoadObject<UInputAction>(nullptr,*FString::Printf(TEXT("/Game/Frontier/Input/%s.%s"),*Name,*Name)))
        {
            Input->BindActionInstanceLambda(Action,ETriggerEvent::Started,[this,Slot,Name](const FInputActionInstance&)
            { UE_LOG(LogTemp,Display,TEXT("INPUT_ACTION %s Event=%d"),*Name,(int32)ETriggerEvent::Started); UseHotbarSlot(Slot); });
        }
        else { UE_LOG(LogTemp,Error,TEXT("GM-01 input action %s missing; run tools/Create-PlayerInputAssets.py"),*Name); }
    }
}

void ASFPlayerController::UseHotbarSlot(int32 Index)
{
    const USFHudHotbarWidget* Bar=HudWidget ? HudWidget->Hotbar.Get() : nullptr;
    if (!Bar || !Bar->IsSlotShown(Index) || !CanUseConstructionControls()) { return; }
    const FString Id=Bar->GetLayout().IsValidIndex(Index) ? Bar->GetLayout()[Index] : FString();
    const int32 Piece=USFHudHotbarWidget::BuildPieces().IndexOfByKey(Id);
    if (Piece!=INDEX_NONE)
    {
        if (!bBuildMode) { ToggleBuildMode(); }
        if (Piece==3) { SelectDoor(); } else { SelectShape(uint8(Piece)); }
        return;
    }
    if (Id.IsEmpty() || !GatheringPilot) { return; }
    const FSoftObjectPath Path(SFStationRecipes::ItemAssetPath(Id));
    const USFItemDefinition* Definition=TSoftObjectPtr<USFItemDefinition>(Path).LoadSynchronous();
    if (Definition && Definition->UseType==ESFItemUseType::Equip)
    {
        // Already worn: nothing to do. The server validates ownership and the slot.
        if (GatheringPilot->GetEquipped(Definition->EquipmentSlot)!=Path) { GatheringPilot->RequestEquip(Path,Definition->EquipmentSlot); }
        return;
    }
    if (HudWidget) { HudWidget->PushToast(FString::Printf(TEXT("%s can't be used yet."),Definition ? *Definition->DisplayName.ToString() : *Id)); }
}
void ASFPlayerController::CycleGatheringRecipe()
{
    FVector Start, Direction;
    if (CanUseConstructionControls() && GatheringPilot && GetConstructionRay(Start,Direction)) { GatheringPilot->CycleRecipeFromRay(Start,Direction); }
}

void ASFPlayerController::SelectCube() { SelectShape(0); }
void ASFPlayerController::SelectSlab() { SelectShape(1); }
void ASFPlayerController::SelectSlope() { SelectShape(2); }
void ASFPlayerController::SelectDoor()
{ if (!CanUseConstructionControls()) { return; } CancelLine(); bDemolitionMode=false; bLineMode=false; bDoorSelected=true; SelectedYaw=0; UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_SELECTION Yaw=0")); }
void ASFPlayerController::SelectShape(uint8 Shape)
{
    if (!CanUseConstructionControls()) { return; }
    CancelLine(); bDemolitionMode=false; bDoorSelected=false; SelectedShape=Shape; SelectedYaw=0; SelectedRotation=uint8(SFConstructionGeometry::IdentityRotation());
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SELECTION Shape=%u Yaw=%u Rotation=%u"),SelectedShape,SelectedYaw,SelectedRotation);
}
void ASFPlayerController::RotatePiece()
{
    if (!CanUseConstructionControls()) { return; }
    CancelLine();
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

void ASFPlayerController::CancelLine()
{
    bHasLineSeed=false; LineCache.Revision=MAX_uint64;
}

void ASFPlayerController::ToggleLineMode()
{
    if (!CanUseConstructionControls()) { return; }
    CancelLine(); bDemolitionMode=false; bLineMode=!bLineMode;
    if (bLineMode && bDoorSelected) { SelectCube(); }
    LastPlacementStatus=bLineMode ? TEXT("Line mode: choose start, then end. Escape cancels start.") : TEXT("Single-piece mode.");
}

void ASFPlayerController::AcknowledgePossession(APawn* P)
{
    Super::AcknowledgePossession(P);
    if (!IsLocalController() || !P || !HostsBuildMode()) { return; }
    // Keep the local placeholder body from hiding the build target. Other
    // players still see it, and its capsule/authority remain unchanged.
    TInlineComponentArray<UStaticMeshComponent*> Meshes;
    P->GetComponents(Meshes);
    for (auto* Mesh : Meshes) { Mesh->SetOwnerNoSee(true); }
}

void ASFPlayerController::PlaceFromView()
{
    if (!CanUseConstructionControls()) { return; }
    if (bDemolitionMode) { RemoveFromView(); return; }
    auto* CurrentState=ActiveState();
    if (bHasLineSeed && (!CurrentState || CurrentState->GetSnapshot().Revision!=LineRevision))
    { CancelLine(); LastPlacementStatus=TEXT("Build changed. Choose a new line start."); return; }
    if (!PrepareCursorAction()) { return; }
    auto* State = ActiveState();
    if (!bHasPlacementPreview || !State) { LastPlacementStatus=PreviewStatus; return; }
    LastPlacementStatus=TEXT("Placement requested. Waiting for server...");
    if (bDoorSelected)
    {
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_DOOR Cell=%s Yaw=%u"),*PreviewCell.ToString(),SelectedYaw);
        ServerPlaceDoor(PreviewCell,State->GetSnapshot().Revision,SelectedYaw,ActiveClaimId()); return;
    }
    if (bLineMode)
    {
        if (!bHasLineSeed)
        {
            LineStart=PreviewCell; LineRevision=State->GetSnapshot().Revision; bHasLineSeed=true;
            LastPlacementStatus=TEXT("Line start selected. Aim along one axis and click to place.");
            UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_LINE_SEED Revision=%llu Cell=%s"),LineRevision,*LineStart.ToString()); return;
        }
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_LINE Revision=%llu Start=%s End=%s"),LineRevision,*LineStart.ToString(),*PreviewCell.ToString());
        ServerPlaceLine(LineStart,PreviewCell,LineRevision,SelectedShape,SelectedRotation,ActiveClaimId()); return;
    }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_REQUEST Cell=%s Shape=%u Rotation=%u FromMember=%d"),*PreviewCell.ToString(),SelectedShape,SelectedRotation,bHasRemovalPreview);
    ServerPlace(PreviewCell,State->GetSnapshot().Revision,SelectedShape,SelectedRotation,ActiveClaimId());
}

void ASFPlayerController::RefreshPlacementPreview(bool bDrawOutline)
{
    bHasPlacementPreview=false;
    bHasRemovalPreview=false;
    bHasDoorTarget=false;
    RemovalStatus=TEXT("Aim at a placed piece to remove it.");
    auto* State = ActiveState();
    if (bHasLineSeed && (!State || State->GetSnapshot().Revision!=LineRevision)) { CancelLine(); }
    if (!CanUseConstructionControls()) { PreviewStatus=TEXT("Hold RMB to look; release to build."); return; }
    if (!State && GetPawn() && TActorIterator<ASFConstructionNetworkState>(GetWorld()))
    { PreviewStatus=TEXT("You need a claim to place this here. Stand on a build plot; K claims it."); return; }  // claims exist, none here
    if (!IsLocalController() || !GetPawn() || !bRoleAssigned || !State
        || State->GetReachCentimetres()<=0 || State->GetMaximumMembers()<=0)
    { PreviewStatus=TEXT("Waiting for server construction state..."); return; }
    if (!bBuilderRole) { PreviewStatus=TEXT("Observer: watch the builder's placements."); RemovalStatus=TEXT("Observer: removal unavailable."); return; }
    if (State->GetSnapshot().Revision>0 && !State->IsProjectionCurrent())
    { PreviewStatus=TEXT("Waiting for local construction collision..."); return; }
    FVector Start,Direction;
    if (!GetConstructionRay(Start,Direction)) { PreviewStatus=TEXT("Move the cursor into the world to aim."); return; }
    SampledRevision=State->GetSnapshot().Revision;
    const FVector Off=State->GetAnchorOffsetCm();
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionNetworkView),false,GetPawn());
    // Camera offset plus half a cell diagonal covers the server's pawn-centre reach sphere.
    const double TraceLength=State->GetReachCentimetres()+FVector::Dist(Start,GetPawn()->GetActorLocation())+FMath::Sqrt(3.0)*25.0;
    const FVector End=Start+Direction*TraceLength;
    const bool bHit=GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Query);
    bHasDoorTarget=State->FindDoorAlongRay(GetPawn(),Start,End,TargetDoorOrigin);
    if (!bHasDoorTarget && bHit)
    { bHasDoorTarget=Hit.GetActor()==State && State->GetDoorForComponent(Hit.GetComponent(),TargetDoorOrigin); }
    if (bHasDoorTarget && (!bLayerPlacement || bDemolitionMode))
    {
        PreviewStatus=bDemolitionMode ? TEXT("DEMOLISH: click this doorway to remove it.") : TEXT("Doorway targeted.");
        RemovalStatus=TEXT("E: open / close | Demolish + click: remove.");
        const int32 Index=State->GetSnapshot().FindDoor(TargetDoorOrigin);
        FSFConstructionDoorProjectionProfile Profile;
        if (bDrawOutline && Index!=INDEX_NONE && ASFConstructionSandboxDoorProjection::BuildProfile(TargetDoorOrigin,State->GetSnapshot().Doors[Index].Yaw,State->GetSnapshot().Doors[Index].bOpen,Profile))
        { DrawDebugBox(GetWorld(),Profile.Envelope.GetCenter()+Off,Profile.Envelope.GetExtent(),bDemolitionMode ? FColor::Orange : FColor::Cyan,false,-1.f,0,2.f); }
        return;
    }
    FVector Point;
    if (bLayerPlacement && !bDemolitionMode)
    {
        if (!GetLayerPoint(Start,Direction,TraceLength,bHit ? &Hit : nullptr,Point))
        { PreviewStatus=TEXT("Layer is occluded, behind the cursor, outside the plot or parallel/out of reach."); return; }
    }
    else
    {
        if (!bHit) { PreviewStatus=TEXT("Aim at the ground or a block face within reach."); return; }
        Point=Hit.ImpactPoint+Hit.ImpactNormal*0.1;
    }
    PreviewCell=FIntVector(FMath::FloorToInt((Point.X-Off.X)/50),FMath::FloorToInt((Point.Y-Off.Y)/50),FMath::FloorToInt((Point.Z-Off.Z)/50));
    bHasRemovalPreview=bHit && Hit.GetActor()==State && State->GetCellForComponent(Hit.GetComponent(),RemovalCell);
    if (bHasRemovalPreview && (!bLayerPlacement || bDemolitionMode))
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
    if (bDoorSelected && !bDemolitionMode)
    {
        FSFConstructionDoorProjectionProfile Profile;
        if (!ASFConstructionSandboxDoorProjection::BuildProfile(PreviewCell,SelectedYaw,false,Profile))
        { bHasPlacementPreview=false; PreviewStatus=TEXT("Door footprint is outside the construction area."); return; }
        const bool bReach=FVector::DistSquared(GetPawn()->GetActorLocation(),Profile.Envelope.GetCenter()+Off)<=FMath::Square(State->GetReachCentimetres());
        PreviewStatus=FString::Printf(TEXT("Door %d, %d, %d | %s"),PreviewCell.X,PreviewCell.Y,PreviewCell.Z,bReach ? TEXT("Click or B to request placement.") : TEXT("Out of reach."));
        RemovalStatus=bHasDoorTarget ? TEXT("E: open / close | Demolish + click: remove.") : TEXT("Door frame, swing space and both approaches must be clear.");
        if (bDrawOutline)
        {
            for (const auto& B : Profile.Boxes)
            { DrawDebugBox(GetWorld(),Profile.Transform.TransformPosition(B.Centre)+Off,B.Extent,Profile.Transform.GetRotation()*B.Rotation,bReach ? FColor(255,190,60) : FColor::Red,false,-1.f,0,2.f); }
        }
        return;
    }
    if (bLineMode && !bDemolitionMode)
    {
        TArray<FIntVector> Cells;
        const FIntVector StartCell=bHasLineSeed ? LineStart : PreviewCell;
        const bool bValid=SFConstructionLine::Enumerate(StartCell,PreviewCell,State->GetMaximumLineCells(),Cells);
        if (LineCache.State.Get()!=State || LineCache.Revision!=State->GetSnapshot().Revision || LineCache.Start!=StartCell
            || LineCache.End!=PreviewCell || LineCache.Shape!=SelectedShape || LineCache.Rotation!=SelectedRotation)
        {
            LineCache.State=State; LineCache.Revision=State->GetSnapshot().Revision; LineCache.Start=StartCell; LineCache.End=PreviewCell;
            LineCache.Shape=SelectedShape; LineCache.Rotation=SelectedRotation;
            LineCache.bSupported=State->IsLineSupportedForPreview(StartCell,PreviewCell,SelectedShape,SelectedRotation);
        }
        PreviewStatus=FString::Printf(TEXT("Line %d / %d | %s"),bValid ? Cells.Num() : 0,State->GetMaximumLineCells(),
            !bHasLineSeed ? TEXT("Click / B: choose start.") : !bValid ? TEXT("Use one axis within the limit.")
            : !LineCache.bSupported ? TEXT("Occupied or unsupported. Server checks full line.") : TEXT("Click / B: place full line. Escape: cancel start."));
        if (bDrawOutline && bValid)
        {
            FSFConstructionSolid Solid;
            if (SFConstructionGeometry::Build(SelectedShape,SelectedRotation,Solid))
            for (const auto& Cell : Cells) for (const auto& Face : Solid.Faces) for (int32 I=0; I<Face.Num(); ++I)
            { DrawDebugLine(GetWorld(),Off+FVector(Cell)*50+Solid.Vertices[Face[I]],Off+FVector(Cell)*50+Solid.Vertices[Face[(I+1)%Face.Num()]],LineCache.bSupported ? FColor(255,190,60) : FColor::Red,false,-1.f,0,2.f); }
        }
        return;
    }
    const FVector Centre=Off+FVector(PreviewCell)*50.0+FVector(25);
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
    { RemovalStatus=SupportCache.bRemovalAllowed ? TEXT("Demolish mode + click: request removal.") : TEXT("Cannot remove: supporting other blocks."); }
    if (bHasDoorTarget) { RemovalStatus=TEXT("E: open / close | Demolish + click: remove."); }
    if (bDemolitionMode)
    {
        bHasPlacementPreview=false;
        PreviewStatus=!bHasRemovalPreview ? TEXT("DEMOLISH: aim at a placed piece or doorway.") : SupportCache.bRemovalAllowed ? TEXT("DEMOLISH: click to request removal.") : TEXT("Cannot remove: supporting other pieces.");
        if (bHasRemovalPreview && bDrawOutline)
        { DrawDebugBox(GetWorld(),Off+FVector(RemovalCell)*50.0+FVector(25),FVector(25),SupportCache.bRemovalAllowed ? FColor::Orange : FColor::Red,false,-1.f,0,2.f); }
        return;
    }
    const TCHAR* BlockReason=nullptr;
    if (!State->ContainsClaimCell(PreviewCell))
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
            { DrawDebugLine(GetWorld(),Off+FVector(PreviewCell)*50+Solid.Vertices[Face[I]],Off+FVector(PreviewCell)*50+Solid.Vertices[Face[(I+1)%Face.Num()]],BlockReason ? FColor::Red : FColor(255,190,60),false,-1.f,0,2.f); }
        }
    }
}

void ASFPlayerController::RemoveFromView()
{
    if (!PrepareCursorAction()) { return; }
    auto* State=ActiveState();
    if (!State) { return; }
    if (bHasDoorTarget)
    {
        LastPlacementStatus=TEXT("Door removal requested. Waiting for server...");
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_REMOVE_DOOR Cell=%s"),*TargetDoorOrigin.ToString());
        ServerRemoveDoor(TargetDoorOrigin,State->GetSnapshot().Revision,ActiveClaimId()); return;
    }
    if (!bHasRemovalPreview) { LastPlacementStatus=TEXT("Aim at a construction piece or door to remove it."); return; }
    LastPlacementStatus=TEXT("Removal requested. Waiting for server...");
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_REMOVE Cell=%s"),*RemovalCell.ToString());
    ServerRemove(RemovalCell,State->GetSnapshot().Revision,ActiveClaimId());
}

void ASFPlayerController::ServerPlaceLine_Implementation(FIntVector Start, FIntVector End, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation, const FString& ClaimId)
{
    auto* State=ResolveState(GetWorld(),ClaimId);
    auto Result=ESFConstructionNetworkResult::InvalidContext;
    // ponytail: housing commits one structure per placement; lines stay sandbox-only until their cost is batched.
    const auto* LineMode=GetWorld()->GetAuthGameMode<ASFGameMode>();
    if (State && LastRequestFrame!=GFrameCounter && !(LineMode && LineMode->IsHousingActive()) && USFConstructionSandboxSettings::AllowsUnpaidEdits())
    { LastRequestFrame=GFrameCounter; Result=State->PlaceLine(this,Start,End,ExpectedRevision,Shape,Rotation); }
    const uint64 Revision=State ? State->GetSnapshot().Revision : 0;
    if (Result==ESFConstructionNetworkResult::Accepted) { SFQuestPilotEvents::OnStructurePlaced().Broadcast(this,Revision,FString()); }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_LINE_RESULT Builder=%d Result=%d Revision=%llu Members=%d Collision=%d"),bBuilderRole,int32(Result),Revision,State ? State->GetSnapshot().Members.Num() : 0,State && State->HasCurrentCollision());
    ClientPlacementResult(Result,Revision);
}

void ASFPlayerController::ServerPlace_Implementation(FIntVector Cell, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation, const FString& ClaimId)
{ ProcessEdit(Cell,ExpectedRevision,false,Shape,Rotation,ClaimId); }

void ASFPlayerController::ServerRemove_Implementation(FIntVector Cell, uint64 ExpectedRevision, const FString& ClaimId)
{ ProcessEdit(Cell,ExpectedRevision,true,0,0,ClaimId); }

void ASFPlayerController::ToggleDoorFromView()
{
    if (!CanUseConstructionControls()) { return; }
    FVector GatherStart, GatherDirection;
    if (GatheringPilot && GetConstructionRay(GatherStart,GatherDirection) && GatheringPilot->InteractFromRay(GatherStart,GatherDirection)) { return; }
    if (!PrepareCursorAction()) { return; }
    auto* State=ActiveState();
    if (!bHasDoorTarget || !State) { LastPlacementStatus=TEXT("Aim at a door frame or leaf to open or close it."); return; }
    LastPlacementStatus=TEXT("Door request sent. Waiting for server...");
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_VIEW_TOGGLE Cell=%s"),*TargetDoorOrigin.ToString());
    ServerToggleDoor(TargetDoorOrigin,State->GetSnapshot().Revision,ActiveClaimId());
}

void ASFPlayerController::ServerRemoveDoor_Implementation(FIntVector Origin, uint64 ExpectedRevision, const FString& ClaimId)
{
    auto* State=ResolveState(GetWorld(),ClaimId);
    auto Result=ESFConstructionNetworkResult::InvalidContext;
    if (State && LastRequestFrame!=GFrameCounter)
    { LastRequestFrame=GFrameCounter; Result=USFConstructionSandboxSettings::AllowsUnpaidEdits() ? State->RemoveDoor(this,Origin,ExpectedRevision) : ESFConstructionNetworkResult::ServiceUnavailable; }
    const uint64 Revision=State ? State->GetSnapshot().Revision : 0;
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_REMOVE_DOOR_RESULT Builder=%d Result=%d Revision=%llu Doors=%d Collision=%d"),bBuilderRole,int32(Result),Revision,State ? State->GetSnapshot().Doors.Num() : 0,State && State->HasCurrentCollision());
    ClientPlacementResult(Result,Revision);
}

void ASFPlayerController::ServerToggleDoor_Implementation(FIntVector Origin, uint64 ExpectedRevision, const FString& ClaimId)
{
    auto* State=ResolveState(GetWorld(),ClaimId);
    auto Result=ESFConstructionNetworkResult::InvalidContext;
    if (State && LastRequestFrame!=GFrameCounter)
    { LastRequestFrame=GFrameCounter; Result=State->ToggleDoor(this,Origin,ExpectedRevision); }
    const uint64 Revision=State ? State->GetSnapshot().Revision : 0;
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_TOGGLE_RESULT Builder=%d Result=%d Revision=%llu Doors=%d Collision=%d"),bBuilderRole,int32(Result),Revision,State ? State->GetSnapshot().Doors.Num() : 0,State && State->HasCurrentCollision());
    ClientPlacementResult(Result,Revision);
}

void ASFPlayerController::ServerPlaceDoor_Implementation(FIntVector Origin, uint64 ExpectedRevision, uint8 Yaw, const FString& ClaimId)
{
    auto* State=ResolveState(GetWorld(),ClaimId);
    auto Result=ESFConstructionNetworkResult::InvalidContext;
    if (State && LastRequestFrame!=GFrameCounter)
    { LastRequestFrame=GFrameCounter; Result=USFConstructionSandboxSettings::AllowsUnpaidEdits() ? State->PlaceDoor(this,Origin,ExpectedRevision,Yaw) : ESFConstructionNetworkResult::ServiceUnavailable; }
    const uint64 Revision=State ? State->GetSnapshot().Revision : 0;
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_RESULT Builder=%d Result=%d Revision=%llu Doors=%d Collision=%d"),bBuilderRole,int32(Result),Revision,State ? State->GetSnapshot().Doors.Num() : 0,State && State->HasCurrentCollision());
    ClientPlacementResult(Result,Revision);
}

void ASFPlayerController::ProcessEdit(FIntVector Cell, uint64 ExpectedRevision, bool bRemove, uint8 Shape, uint8 Rotation, const FString& ClaimId)
{
    auto* State = ResolveState(GetWorld(),ClaimId);
    auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFGameMode>() : nullptr;
    ESFConstructionNetworkResult Result = ESFConstructionNetworkResult::InvalidContext;
    if (State && Mode && Mode->IsHousingActive() && !State->GetClaimId().IsEmpty() && LastRequestFrame != GFrameCounter)
    {
        // INT-04: check everything locally, commit through Housing (which pays from the backpack), then apply the
        // already-validated edit. Unavailable or refused leaves the claim and the world unchanged.
        LastRequestFrame = GFrameCounter;
        Result = USFCharacterSessionComponent::IsRestoring(this) ? ESFConstructionNetworkResult::ServiceUnavailable
            : bHousingPending ? ESFConstructionNetworkResult::Stale : State->Edit(this,Cell,ExpectedRevision,bRemove,Shape,Rotation,false);
        if (Result == (bRemove ? ESFConstructionNetworkResult::Removed : ESFConstructionNetworkResult::Accepted))
        {
            bHousingPending = true;
            Mode->CommitHousing(this,ClaimId,Cell,bRemove,Shape,Rotation,[Weak=TWeakObjectPtr<ASFPlayerController>(this),ClaimId,Cell,bRemove,Shape,Rotation](ESFConstructionNetworkResult Housing,const FString& Operation)
            {
                ASFPlayerController* Self = Weak.Get();
                auto* Current = Self ? ResolveState(Self->GetWorld(),ClaimId) : nullptr;
                const bool bCommitted = Housing == ESFConstructionNetworkResult::Accepted;
                if (!Self || !Current) { return; }
                Self->bHousingPending = false;
                ESFConstructionNetworkResult Applied = Housing == ESFConstructionNetworkResult::PermissionDenied ? Housing : ESFConstructionNetworkResult::ServiceUnavailable;
                if (bCommitted)
                {
                    Applied = Current->Edit(Self,Cell,Current->GetSnapshot().Revision,bRemove,Shape,Rotation);
                    if (Applied != (bRemove ? ESFConstructionNetworkResult::Removed : ESFConstructionNetworkResult::Accepted))
                    { UE_LOG(LogTemp,Error,TEXT("HOUSING_PROJECTION_MISMATCH Op=%s Result=%d"),*Operation,int32(Applied)); }
                }
                const uint64 After = Current->GetSnapshot().Revision;
                if (!bRemove && Applied == ESFConstructionNetworkResult::Accepted) { SFQuestPilotEvents::OnStructurePlaced().Broadcast(Self,After,Operation); }
                UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_RESULT Builder=%d Result=%d Revision=%llu Members=%d Collision=%d Action=%s Housing=%s"),Self->bBuilderRole,int32(Applied),After,
                    Current->GetSnapshot().Members.Num(),Current->HasCurrentCollision(),bRemove ? TEXT("Remove") : TEXT("Place"),*Operation);
                Self->ClientPlacementResult(Applied,After);
            });
            return;
        }
    }
    else if (State && LastRequestFrame != GFrameCounter)
    {
        // DEC-02: an edit that Housing does not pay for runs only under the explicit development stock fixture.
        LastRequestFrame = GFrameCounter;
        Result = USFConstructionSandboxSettings::AllowsUnpaidEdits() ? State->Edit(this,Cell,ExpectedRevision,bRemove,Shape,Rotation)
            : ESFConstructionNetworkResult::ServiceUnavailable;
    }
    const uint64 Revision = State ? State->GetSnapshot().Revision : 0;
    if (!bRemove && Result==ESFConstructionNetworkResult::Accepted) { SFQuestPilotEvents::OnStructurePlaced().Broadcast(this,Revision,FString()); }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_RESULT Builder=%d Result=%d Revision=%llu Members=%d Collision=%d Action=%s"),bBuilderRole,int32(Result),Revision,State ? State->GetSnapshot().Members.Num() : 0,State && State->HasCurrentCollision(),bRemove ? TEXT("Remove") : TEXT("Place"));
    ClientPlacementResult(Result,Revision);
}

void ASFPlayerController::ClientPlacementResult_Implementation(ESFConstructionNetworkResult Result, uint64 Revision)
{
    LastPlacementStatus=PlacementResultText(Result);
    LastResultCode=Result; ++ResultSerial;
    if (Result==ESFConstructionNetworkResult::Accepted && bHasLineSeed) { CancelLine(); }
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
            || (ScenarioStep==49 && Result==ESFConstructionNetworkResult::DoorRemoved && Revision==14)
            || ((ScenarioStep==51 || ScenarioStep==53) && Result==ESFConstructionNetworkResult::InvalidLine && Revision==14)
            || (ScenarioStep==55 && Result==ESFConstructionNetworkResult::Occupied && Revision==14)
            || (ScenarioStep==57 && Result==ESFConstructionNetworkResult::Accepted && Revision==15)
            || (ScenarioStep==59 && Result==ESFConstructionNetworkResult::Accepted && Revision==16))
        : (((ScenarioStep==1 || ScenarioStep==3 || ScenarioStep==5) && Result==ESFConstructionNetworkResult::PermissionDenied && Revision==6)
            || (ScenarioStep==7 && Result==ESFConstructionNetworkResult::PermissionDenied && Revision==11)
            || (ScenarioStep==9 && Result==ESFConstructionNetworkResult::PermissionDenied && Revision==13)
            || (ScenarioStep==11 && Result==ESFConstructionNetworkResult::PermissionDenied && Revision==14));
    if (!bExpected) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED Step=%d Result=%d Revision=%llu"),ScenarioStep,int32(Result),Revision); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    ++ScenarioStep;
#endif
}

void ASFPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (WindowStack && WindowStack->Num() > 0) { WindowStack->Relayout(DeltaSeconds); }  // slide windows into place; sizes or the viewport may have changed
    TickInteraction();
    if (!HostsBuildMode()) { return; }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TickClaimScenario();
    // Plain-client regression check (Test-QuestPilot.ps1 -Leg Plain): construction state replicated, and the gathering
    // pilot enabled, which is what the inventory key needs. Logged once.
    if (!bPlainCheckLogged && IsLocalController() && GetNetMode()==NM_Client && FParse::Param(FCommandLine::Get(),TEXT("PlainClientCheck")))
    {
        int32 States=0, Ready=0;
        for (TActorIterator<ASFConstructionNetworkState> It(GetWorld()); It; ++It) { ++States; Ready+=It->GetReachCentimetres()>0 && It->GetMaximumMembers()>0; }
        const bool bGathering=GatheringPilot && GatheringPilot->IsEnabled();
        if (bRoleAssigned && States>0 && Ready==States && bGathering)
        {
            bPlainCheckLogged=true;
            UE_LOG(LogTemp,Display,TEXT("PLAIN_CLIENT_READY States=%d Ready=%d GatheringEnabled=1 InventoryKeyAllowed=1 Status=%s"),States,Ready,*GatheringPilot->GetStatusText(FVector::ZeroVector,FVector::ForwardVector).Left(60));
        }
    }
#endif
    if (IsLocalController())
    {
        EnsureGatheringSkillsPanel();
        if (GEngine) { GEngine->RemoveOnScreenDebugMessage(uint64(GetUniqueID())); }
        // Previews and outlines only in build mode; outside it LMB, E and F belong to IMC_Player (attack, strafe, interact).
        if (bBuildMode) { RefreshPlacementPreview(true); }
        else { bHasPlacementPreview=false; bHasRemovalPreview=false; bHasDoorTarget=false; }
        if (Palette && bBuildMode)
        {
            // The player-facing line only; the full status block stays in the sf.HudDebugText overlay (GetBuildStatusText).
            const FString Status=PreviewStatus.IsEmpty() ? FString(TEXT("Select a piece, then place it.")) : PreviewStatus;
            // The panel shows only the validity word; the full line (target cell, reach) goes to the log and sf.HudDebugText.
            const FString Shown=bHasPlacementPreview ? FString(TEXT("Valid placement")) : FString(TEXT("Invalid placement"));
            if (Status!=CachedPaletteStatus) { UE_LOG(LogTemp,Verbose,TEXT("BUILD_PREVIEW %s"),*Status); CachedPaletteStatus=Status; }
            if (Shown!=CachedPaletteShown) { Palette->SetStatusText(FText::FromString(Shown)); Palette->SetPlacementValid(bHasPlacementPreview); CachedPaletteShown=Shown; }
        }
    }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TickFiniteScenario();
#endif
}

FString ASFPlayerController::GetBuildStatusText() const
{
    const auto* State=ActiveState();
    return FString::Printf(TEXT("CONSTRUCTION | %s | %s\nB: build mode    Click: apply    RMB hold: look    X / Delete: demolition    E: door    WASD: move\n1 Cube | 2 Half slab | 3 Slope | 4 Door | R Rotate | Selected: %s (%u deg) | L: %s | P: %s | Esc: cancel\n%s\n%s\nLast action: %s\nMembers: %d / %d | Doors: %d / %d | Reach: %.1f m\nAmber: place | Orange: demolish | Cyan: door | Red: blocked estimate | Server checks edits."),
        !bRoleAssigned ? TEXT("CONNECTING") : bBuilderRole ? TEXT("BUILDER") : TEXT("OBSERVER"),USFConstructionSandboxSettings::AllowsUnpaidEdits() ? TEXT("DEV: UNLIMITED STOCK") : TEXT("COSTS: BACKPACK"),
        bDoorSelected ? TEXT("Door") : SelectedShape==0 ? TEXT("Cube") : SelectedShape==1 ? TEXT("Half slab") : TEXT("Slope"),SelectedYaw*90,bDemolitionMode ? TEXT("Demolish") : bLineMode ? TEXT("Line") : TEXT("Single"),*GetPlaneStatusText(),
        *PreviewStatus,*RemovalStatus,*LastPlacementStatus,State ? State->GetSnapshot().Members.Num() : 0,
        State ? State->GetMaximumMembers() : 0,State ? State->GetSnapshot().Doors.Num() : 0,State ? State->GetMaximumDoors() : 0,State ? State->GetReachCentimetres()/100.0 : 0.0);
}

// The v4 HUD (SFHudLayout, the palette, the quest tracker and the gathering HUD) replaces these development text boxes.
static TAutoConsoleVariable<int32> CVarSFHudDebugText(TEXT("sf.HudDebugText"), 0, TEXT("1 draws the legacy construction and gathering debug text over the HUD."), ECVF_Default);

void ASFConstructionNetworkHUD::DrawHUD()
{
    Super::DrawHUD();
    if (CVarSFHudDebugText.GetValueOnGameThread() == 0) { return; }
    const auto* Controller=Cast<ASFPlayerController>(GetOwningPlayerController());
    if (!Controller || !Canvas || !GEngine) { return; }
    const float Scale=FMath::Clamp(Canvas->ClipX/1280.f,0.5f,1.5f);
    DrawRect(FLinearColor(0.02f,0.03f,0.04f,0.88f),16*Scale,Canvas->ClipY-202*Scale,FMath::Min(1040*Scale,Canvas->ClipX-32*Scale),186*Scale);
    DrawText(Controller->GetBuildStatusText(),FColor::White,28*Scale,Canvas->ClipY-192*Scale,GEngine->GetSmallFont(),1.25f*Scale);
    if (const auto* Gather=Controller->FindComponentByClass<USFGatheringPilotComponent>())
    {
        FVector Start,Direction;
        if (!Controller->DeprojectMousePositionToWorld(Start,Direction)) { FRotator Rotation; Controller->GetPlayerViewPoint(Start,Rotation); Direction=Rotation.Vector(); }
        const FString Text=Gather->GetStatusText(Start,Direction);
        if (!Text.IsEmpty())
        {
            DrawRect(FLinearColor(0.02f,0.03f,0.04f,0.88f),16*Scale,16*Scale,FMath::Min(920*Scale,Canvas->ClipX-32*Scale),76*Scale);
            DrawText(Text,FColor::White,28*Scale,22*Scale,GEngine->GetSmallFont(),1.25f*Scale);
        }
    }

}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void ASFPlayerController::ScenarioTap(const FKey& Key)
{
    // Pressed now, released on the next tick, so Enhanced Input sees a whole press and fires Started.
    InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0)));
    ScenarioHeldKeys.Add(Key);
}

void ASFPlayerController::TickFiniteScenario()
{
    if (!IsLocalController() || GetNetMode()!=NM_Client || !FParse::Param(FCommandLine::Get(),TEXT("ConstructionNetworkScenario"))) { return; }
    for (const FKey& Key : ScenarioHeldKeys) { InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0))); }
    ScenarioHeldKeys.Reset();
    auto* State = ActiveState();
    if (!bRoleAssigned || !GetPawn() || !State || !State->IsPairReady()) { return; }
    const FVector Position = GetPawn()->GetActorLocation();
    if (FMath::Abs(Position.X-(bBuilderRole ? -200.0 : 200.0))>1.0 || FMath::Abs(Position.Y+400.0)>1.0) { return; }
    if (ScenarioStarted==0) { ScenarioStarted=FPlatformTime::Seconds(); }
    if (!bBuilderRole && ScenarioStep==0)
    { SetControlRotation((FVector(-150,-175,50)-PlayerCameraManager->GetCameraLocation()).Rotation()); }
    if (FPlatformTime::Seconds()-ScenarioStarted>45) { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED Timeout=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    // GM-01: the build keys live in IMC_Build, so press B first (again every 30 frames: a press before the character has
    // added IMC_Player is lost), then give the character two frames to layer IMC_Build on.
    if (!bBuildMode)
    {
        if (ScenarioBuildModeFrames++%30==0)
        { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SCENARIO_BUILD_KEY Attempt=%d"),ScenarioBuildModeFrames/30+1); ScenarioTap(EKeys::B); }
        return;
    }
    if (ScenarioBuildModeSettle<2) { ++ScenarioBuildModeSettle; return; }
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
        ScenarioStep=33; ServerPlaceDoor(FIntVector(2,0,0),8,255,ActiveClaimId()); return;
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
        { SetControlRotation((FVector(150,25,100)-PlayerCameraManager->GetCameraLocation()).Rotation()); bScenarioDoorCameraAimed=true; return; }
        if (!bScenarioDoorClosedReported)
        { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_CLOSED_READY Builder=%d Revision=11"),bBuilderRole); bScenarioDoorClosedReported=true; }
    }
    if (((bBuilderRole && ScenarioStep==44) || (!bBuilderRole && ScenarioStep==8))
        && State->GetSnapshot().Revision==12 && State->GetSnapshot().Doors.Num()==1 && State->GetSnapshot().Doors[0].bOpen && State->HasCurrentCollision())
    {
        if (bBuilderRole && !bScenarioDoorOpenReported)
        {
            SetControlRotation((FVector(150,25,100)-PlayerCameraManager->GetCameraLocation()).Rotation());
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
    { ScenarioStep=9; ServerRemoveDoor(FIntVector(2,0,0),13,ActiveClaimId()); return; }
    if (((bBuilderRole && ScenarioStep==48) || (!bBuilderRole && ScenarioStep==10))
        && State->GetSnapshot().Revision==13 && State->GetSnapshot().Doors.Num()==1
        && !State->GetSnapshot().Doors[0].bOpen && State->HasCurrentCollision() && !bScenarioDoorRemoveReported)
    { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_DOOR_REMOVE_READY Builder=%d Revision=13"),bBuilderRole); bScenarioDoorRemoveReported=true; }
    if (!bBuilderRole && ScenarioStep==10 && State->GetSnapshot().Revision==14 && State->HasCurrentCollision())
    { ScenarioStep=11; ServerPlaceLine(FIntVector(5,-4,0),FIntVector(7,-4,0),14,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId()); return; }
    if (((bBuilderRole && ScenarioStep==50) || (!bBuilderRole && ScenarioStep==12))
        && State->GetSnapshot().Revision==14 && State->GetSnapshot().Members.Num()==2 && State->GetSnapshot().Doors.IsEmpty() && State->HasCurrentCollision() && !bScenarioLineReadyReported)
    { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_LINE_READY Builder=%d Revision=14"),bBuilderRole); bScenarioLineReadyReported=true; }
    if (((bBuilderRole && ScenarioStep==58) || (!bBuilderRole && ScenarioStep==12))
        && State->GetSnapshot().Revision==15 && State->GetSnapshot().Members.Num()==5 && State->GetSnapshot().Doors.IsEmpty() && State->HasCurrentCollision() && !bScenarioLayerReadyReported)
    { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_LAYER_READY Builder=%d Revision=15"),bBuilderRole); bScenarioLayerReadyReported=true; }
    if (((bBuilderRole && ScenarioStep==60) || (!bBuilderRole && ScenarioStep==12))
        && State->GetSnapshot().Revision==16 && State->GetSnapshot().Members.Num()==8 && State->GetSnapshot().Doors.IsEmpty() && State->HasCurrentCollision())
    {
        for (int32 X=5; X<=7; ++X)
        {
            const int32 Index=State->GetSnapshot().Find(FIntVector(X,-4,1));
            if (Index==INDEX_NONE || State->GetSnapshot().Members[Index].Shape!=1 || State->GetSnapshot().Members[Index].Rotation!=SFConstructionGeometry::IdentityRotation())
            { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED UpperSlabContent=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        }
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SCENARIO_SUCCESS Builder=%d Revision=16 Members=8 Doors=0 UpperSlabs=3"),bBuilderRole);
        if (FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkScreenshot="),ScenarioScreenshotPath))
        { ScenarioStep=100; FScreenshotRequest::RequestScreenshot(ScenarioScreenshotPath,true,false); }
        else { FPlatformMisc::RequestExitWithStatus(false,0); }
        return;
    }
    if (bBuilderRole && ScenarioStep==0 && ScenarioCursorCheck<4)
    {
        if (ScenarioCursorCheck==0)
        {
            ScenarioCursorCheck=1;
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::RightMouseButton,IE_Pressed,1.f,1,FInputDeviceId::CreateFromInternalId(0))); return;
        }
        if (ScenarioCursorCheck==1)
        {
            if (!bCameraLookHeld) { return; }
            if (bShowMouseCursor || IsLookInputIgnored())
            { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED CameraCapture=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
            ScenarioCursorCheck=2;
            ScenarioTap(EKeys::LeftMouseButton);
            ScenarioTap(EKeys::X); return;
        }
        if (ScenarioCursorCheck==2)
        {
            if (bDemolitionMode || State->GetSnapshot().Revision!=0 || !State->GetSnapshot().Members.IsEmpty())
            { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED CameraWorldEdit=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
            ScenarioCursorCheck=3;
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::RightMouseButton,IE_Released,0.f,1,FInputDeviceId::CreateFromInternalId(0))); return;
        }
        if (bCameraLookHeld) { return; }
        if (!bShowMouseCursor || !IsLookInputIgnored())
        { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED CursorRestore=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        ScenarioCursorCheck=4;
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_CURSOR_INPUT CameraHold=1 BlockedWorldClick=1 CursorRestored=1 Revision=0"));
    }
    if (bBuilderRole && ScenarioStep==0)
    { ScenarioStep=1; ServerPlace(FIntVector(-4,-8,0),0,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==2)
    {
        // Aim through the third-person camera at the known floor cell; wait for
        // the ordinary camera/input stack rather than calling placement directly.
        SetControlRotation((FVector(-175,-175,0)-PlayerCameraManager->GetCameraLocation()).Rotation());
        if (!bHasPlacementPreview || PreviewCell!=FIntVector(-4,-4,0)) { return; }
        ScenarioStep=3;
        ScenarioTap(EKeys::LeftMouseButton);  // GM-01: B toggles build mode; LMB places
    }
    else if (bBuilderRole && ScenarioStep==4 && State->HasCurrentCollision())
    { ScenarioStep=5; ServerPlace(FIntVector(-4,-4,2),1,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==6)
    { ScenarioStep=7; ServerPlace(FIntVector(-4,-4,1),1,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==8)
    { ScenarioStep=9; ServerPlace(FIntVector(-3,-4,0),2,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==10 && State->GetSnapshot().Revision==3 && State->HasCurrentCollision())
    { ScenarioStep=11; ServerPlace(FIntVector(-3,-4,0),2,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==12)
    {
        // The owned runner opens this finite-test barrier only after both real
        // clients have projected the nonempty structure, so deletion is exercised.
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier)) { return; }
        if (!bHasRemovalPreview || RemovalCell!=FIntVector(-4,-4,0) || SupportCache.bRemovalAllowed)
        { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED SupportRemovalPreview=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SUPPORT_PREVIEW SupportingBaseAllowed=0 Revision=3"));
        ScenarioStep=13; ServerRemove(FIntVector(-4,-4,0),3,ActiveClaimId());
    }
    else if (bBuilderRole && ScenarioStep==14 && State->GetSnapshot().Revision==3 && State->HasCurrentCollision())
    {
        SetControlRotation((FVector(-175,-175,75)-PlayerCameraManager->GetCameraLocation()).Rotation());
        if (!bHasRemovalPreview || RemovalCell!=FIntVector(-4,-4,1)) { return; }
        if (!SupportCache.bRemovalAllowed)
        { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED SupportedTopPreview=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        if (!bDemolitionMode)
        {
            ScenarioTap(EKeys::X); return;
        }
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_SUPPORT_PREVIEW SupportedTopAllowed=1 Revision=3"));
        ScenarioStep=15;
        ScenarioTap(EKeys::LeftMouseButton);
    }
    else if (bBuilderRole && ScenarioStep==16 && State->GetSnapshot().Revision==4 && State->HasCurrentCollision())
    { ScenarioStep=17; ServerRemove(FIntVector(-4,-4,1),4,ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==18)
    { ScenarioStep=19; ServerRemove(FIntVector(-3,-4,0),3,ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==20)
    { ScenarioStep=21; ServerRemove(FIntVector(-3,-4,0),4,ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==22 && State->GetSnapshot().Revision==5 && State->HasCurrentCollision())
    { ScenarioStep=23; ServerRemove(FIntVector(-4,-4,0),5,ActiveClaimId()); }
    else if (!bBuilderRole && ScenarioStep==0 && State->GetSnapshot().Revision==6 && State->HasCurrentCollision())
    { ScenarioStep=1; ServerPlace(FIntVector(4,-4,0),6,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId()); }
    else if (!bBuilderRole && ScenarioStep==2)
    { ScenarioStep=3; ServerRemove(FIntVector(-4,-4,0),6,ActiveClaimId()); }
    else if (!bBuilderRole && ScenarioStep==4)
    { ScenarioStep=5; ServerPlaceDoor(FIntVector(2,0,0),6,0,ActiveClaimId()); }
    else if (!bBuilderRole && ScenarioStep==6 && State->GetSnapshot().Revision==11 && State->HasCurrentCollision())
    { ScenarioStep=7; ServerToggleDoor(FIntVector(2,0,0),11,ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==24 && bScenarioEmptyReported)
    {
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier+TEXT(".empty"))) { return; }
        ScenarioStep=25; ServerPlace(FIntVector(-4,-4,0),6,255,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId());
    }
    else if (bBuilderRole && ScenarioStep==26)
    { ScenarioStep=27; ServerPlace(FIntVector(-4,-4,0),6,0,255,ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==28)
    {
        SetControlRotation((FVector(-175,-175,0)-PlayerCameraManager->GetCameraLocation()).Rotation());
        if (SelectedShape!=1)
        {
            ScenarioTap(EKeys::Two); return;
        }
        if (!bHasPlacementPreview || PreviewCell!=FIntVector(-4,-4,0)) { return; }
        ScenarioStep=29;
        ScenarioTap(EKeys::LeftMouseButton);  // GM-01: B toggles build mode; LMB places
    }
    else if (bBuilderRole && ScenarioStep==30 && State->GetSnapshot().Revision==7 && State->HasCurrentCollision())
    {
        SetControlRotation((FVector(-175,-200,12.5)-PlayerCameraManager->GetCameraLocation()).Rotation());
        if (SelectedShape!=2)
        {
            ScenarioTap(EKeys::Three); return;
        }
        if (SelectedYaw!=1)
        {
            ScenarioTap(EKeys::R); return;
        }
        if (!bHasPlacementPreview || PreviewCell!=FIntVector(-4,-5,0) || !bHasRemovalPreview || RemovalCell!=FIntVector(-4,-4,0)) { return; }
        ScenarioStep=31;
        ScenarioTap(EKeys::LeftMouseButton);  // GM-01: B toggles build mode; LMB places
    }
    else if (bBuilderRole && ScenarioStep==34)
    {
        SetControlRotation((FVector(125,25,0)-PlayerCameraManager->GetCameraLocation()).Rotation());
        if (!bDoorSelected)
        {
            ScenarioTap(EKeys::Four); return;
        }
        if (!bHasPlacementPreview || PreviewCell!=FIntVector(2,0,0)) { return; }
        ScenarioStep=35;
        ScenarioTap(EKeys::LeftMouseButton);  // GM-01: B toggles build mode; LMB places
    }
    else if (bBuilderRole && ScenarioStep==36 && State->GetSnapshot().Revision==9 && State->HasCurrentCollision())
    { ScenarioStep=37; ServerPlace(FIntVector(2,-1,0),9,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==38)
    { ScenarioStep=39; ServerPlace(FIntVector(-6,-4,0),9,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==40 && State->GetSnapshot().Revision==10 && State->HasCurrentCollision())
    { ScenarioStep=41; ServerRemove(FIntVector(-6,-4,0),10,ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==42 && bScenarioDoorClosedReported)
    {
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier+TEXT(".door-closed")) || !bHasDoorTarget || TargetDoorOrigin!=FIntVector(2,0,0)) { return; }
        ScenarioStep=43;
        ScenarioTap(EKeys::E);
    }
    else if (bBuilderRole && ScenarioStep==44 && bScenarioDoorOpenReported)
    {
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier+TEXT(".door-open")) || !bHasDoorTarget) { return; }
        ScenarioStep=45;
        ScenarioTap(EKeys::E);
    }
    else if (bBuilderRole && ScenarioStep==46 && bHasDoorTarget)
    {
        ScenarioStep=47;
        ScenarioTap(EKeys::E);
    }
    else if (bBuilderRole && ScenarioStep==48 && bScenarioDoorRemoveReported)
    {
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier+TEXT(".door-remove")) || !bHasDoorTarget || TargetDoorOrigin!=FIntVector(2,0,0)) { return; }
        if (!bDemolitionMode)
        {
            ScenarioTap(EKeys::X); return;
        }
        ScenarioStep=49;
        ScenarioTap(EKeys::LeftMouseButton);
    }
    else if (bBuilderRole && ScenarioStep==50 && bScenarioLineReadyReported)
    {
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier+TEXT(".line"))) { return; }
        ScenarioStep=51; ServerPlaceLine(FIntVector(5,-4,0),FIntVector(7,-3,0),14,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId());
    }
    else if (bBuilderRole && ScenarioStep==52)
    { ScenarioStep=53; ServerPlaceLine(FIntVector(5,-4,0),FIntVector(5+State->GetMaximumLineCells(),-4,0),14,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==54)
    { ScenarioStep=55; ServerPlaceLine(FIntVector(-4,-4,0),FIntVector(-3,-4,0),14,0,uint8(SFConstructionGeometry::IdentityRotation()),ActiveClaimId()); }
    else if (bBuilderRole && ScenarioStep==56)
    {
        if (bDoorSelected || SelectedShape!=0)
        {
            ScenarioTap(EKeys::One); return;
        }
        if (!bLineMode)
        {
            ScenarioTap(EKeys::L); return;
        }
        if (bScenarioLineSeedQueued && !bHasLineSeed) { return; }
        if (bScenarioLineSeedQueued && !bScenarioLineSeedReported)
        {
            if (State->GetSnapshot().Revision!=14 || State->GetSnapshot().Members.Num()!=2 || !State->HasCurrentCollision())
            { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED LineSeedMutation=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
            UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_LINE_SEED_ONLY Revision=14 Members=2 Collision=1"));
            bScenarioLineSeedReported=true;
        }
        const FVector Target=bHasLineSeed ? FVector(375,-175,0) : FVector(275,-175,0);
        SetControlRotation((Target-PlayerCameraManager->GetCameraLocation()).Rotation());
        if (!bHasPlacementPreview || PreviewCell!=(bHasLineSeed ? FIntVector(7,-4,0) : FIntVector(5,-4,0))) { return; }
        const bool bSeedOnly=!bHasLineSeed;
        if (!bSeedOnly)
        {
            if (!LineCache.bSupported || LineCache.End!=FIntVector(7,-4,0)) { return; }
            ScenarioStep=57;
        }
        ScenarioTap(EKeys::LeftMouseButton);  // GM-01: B toggles build mode; LMB places
        if (bSeedOnly) { bScenarioLineSeedQueued=true; }
    }
    else if (bBuilderRole && ScenarioStep==58 && bScenarioLayerReadyReported)
    {
        FString Barrier;
        if (!FParse::Value(FCommandLine::Get(),TEXT("ConstructionNetworkRemovalBarrier="),Barrier) || !FPaths::FileExists(Barrier+TEXT(".layer"))) { return; }
        if (SelectedShape!=1)
        {
            ScenarioTap(EKeys::Two); return;
        }
        if (!bLayerPlacement)
        {
            ScenarioTap(EKeys::P); return;
        }
        if (PlacementLayer!=1)
        {
            ScenarioTap(EKeys::PageUp); return;
        }
        if (bScenarioLayerSeedQueued && !bHasLineSeed) { return; }
        if (bScenarioLayerSeedQueued && !bScenarioLayerSeedReported)
        {
            if (State->GetSnapshot().Revision!=15 || State->GetSnapshot().Members.Num()!=5 || !State->HasCurrentCollision())
            { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_NETWORK_SCENARIO_FAILED LayerSeedMutation=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
            UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_LAYER_SEED_ONLY Revision=15 Members=5 Layer=1"));
            bScenarioLayerSeedReported=true;
        }
        SetControlRotation(((bHasLineSeed ? FVector(375,-175,50) : FVector(275,-175,50))-PlayerCameraManager->GetCameraLocation()).Rotation());
        if (!bHasPlacementPreview || PreviewCell!=(bHasLineSeed ? FIntVector(7,-4,1) : FIntVector(5,-4,1))) { return; }
        const bool bSeedOnly=!bHasLineSeed;
        if (!bSeedOnly)
        { if (!LineCache.bSupported || LineCache.End!=FIntVector(7,-4,1)) { return; } ScenarioStep=59; }
        ScenarioTap(EKeys::LeftMouseButton);  // GM-01: B toggles build mode; LMB places
        if (bSeedOnly) { bScenarioLayerSeedQueued=true; }
    }
}
#endif

FString ASFGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
    const FString Result=Super::InitNewPlayer(NewPlayerController,UniqueId,Options,Portal);
    if (UsesGatewayAdmission())
    {
        const FString Ticket=UGameplayStatics::ParseOption(Options,TEXT("SFIdentityTicket"));
        FString AccountId, RealmId, ArchetypeId;
        TArray<uint8> Appearance;
        const FString Verified=TakeGatewayCharacter(Ticket,AccountId,RealmId,ArchetypeId,Appearance);
        if (!Verified.IsEmpty())
        {
            USFBackpackComponent::AssignGatewayIdentity(NewPlayerController,Verified,AccountId,RealmId,ArchetypeId,Appearance);
            if (APlayerState* State=NewPlayerController ? NewPlayerController->GetPlayerState<APlayerState>() : nullptr) { State->SetPlayerName(Verified); }
            UE_LOG(LogTemp,Display,TEXT("GATEWAY_IDENTITY_BOUND Character=%s"),*Verified);
            return Result;
        }
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        const FString Dev=UGameplayStatics::ParseOption(Options,TEXT("SFDevCharacter"));
        if (!(IsAdmitted() && FParse::Param(FCommandLine::Get(),TEXT("SFDevIdentity")) && USFBackpackComponent::IsValidDevName(Dev)))
#endif
        { return TEXT("World ticket was not verified."); }
    }
    const FString Name=UGameplayStatics::ParseOption(Options,TEXT("SFDevCharacter"));
    if (Name.IsEmpty()) { return Result; }
    // Development fixture until DEC-04: a stable character across server restarts for the claim runner. Honoured only in a
    // development build, in the admitted pilot world, with the server's -SFDevIdentity flag; never an identity path elsewhere.
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    const bool bPersonalLootFixture = GetWorld() &&
        GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_PersonalLoot") &&
        FParse::Param(FCommandLine::Get(), TEXT("PersonalLootScenario"));
    if ((IsAdmitted() || bPersonalLootFixture) && FParse::Param(FCommandLine::Get(),TEXT("SFDevIdentity")) && USFBackpackComponent::IsValidDevName(Name))
    {
        USFBackpackComponent::AssignDevIdentity(NewPlayerController,Name);
        if (APlayerState* State = NewPlayerController ? NewPlayerController->GetPlayerState<APlayerState>() : nullptr)
        { State->SetPlayerName(Name); }
        return Result;
    }
#endif
    UE_LOG(LogTemp,Warning,TEXT("DEV_IDENTITY_IGNORED Requested=%s"),*Name.Left(32));
    return Result;
}

ASFConstructionNetworkState* ASFPlayerController::ActiveState() const { return FindStateFor(GetWorld(),GetPawn()); }

FString ASFPlayerController::ActiveClaimId() const
{
    const ASFConstructionNetworkState* State=ActiveState();
    return State ? State->GetClaimId() : FString();
}

void ASFPlayerController::ClaimPlotFromView()
{
    const FString ClaimId=ActiveClaimId();
    if (ClaimId.IsEmpty()) { LastPlacementStatus=TEXT("Stand on a build plot to claim it."); return; }
    LastPlacementStatus=TEXT("Claim requested. Waiting for server...");
    ServerClaimPlot(ClaimId);
}

void ASFPlayerController::SFClaimPlot() { ClaimPlotFromView(); }

void ASFPlayerController::SFGrantBuild(const FString& DevName)
{
    const FString ClaimId=ActiveClaimId();
    if (ClaimId.IsEmpty() || !USFBackpackComponent::IsValidDevName(DevName)) { LastPlacementStatus=TEXT("Stand on your plot and name a dev character."); return; }
    ServerGrantBuild(ClaimId,TEXT("chr_dev_")+DevName);
}

void ASFPlayerController::ServerClaimPlot_Implementation(const FString& ClaimId)
{
    auto* Mode=GetWorld()->GetAuthGameMode<ASFGameMode>();
    if (!Mode) { return; }
    Mode->ClaimPlot(this,ClaimId,[Weak=TWeakObjectPtr<ASFPlayerController>(this),ClaimId](ESFConstructionNetworkResult Result)
    {
        ASFPlayerController* Self=Weak.Get();
        if (!Self) { return; }
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_CLAIM_RESULT Claim=%s Result=%d"),*ClaimId,int32(Result));
        Self->ClientPlacementResult(Result,0);
    });
}

void ASFPlayerController::ServerGrantBuild_Implementation(const FString& ClaimId, const FString& TargetCharacterId)
{
    auto* Mode=GetWorld()->GetAuthGameMode<ASFGameMode>();
    if (!Mode) { return; }
    Mode->GrantBuild(this,ClaimId,TargetCharacterId,[Weak=TWeakObjectPtr<ASFPlayerController>(this),ClaimId,TargetCharacterId](ESFConstructionNetworkResult Result)
    {
        ASFPlayerController* Self=Weak.Get();
        if (!Self) { return; }
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_NETWORK_GRANT_RESULT Claim=%s Target=%s Result=%d"),*ClaimId,*TargetCharacterId,int32(Result));
        Self->ClientPlacementResult(Result,0);
    });
}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void ASFPlayerController::TickClaimScenario()
{
    // Development runner for claims and housing persistence (tools/Test-ClaimHousing.ps1). Every action is the normal
    // RPC; the server places the pawns on the first plot. Each step waits for the server's reply to the one before.
    FString ScenarioRole;
    if (!IsLocalController() || GetNetMode()!=NM_Client || !FParse::Value(FCommandLine::Get(),TEXT("ClaimScenario="),ScenarioRole)) { return; }
    const ASFConstructionNetworkState* State=ActiveState();
    const double Now=GetWorld()->GetTimeSeconds();
    if (!State || State->GetClaimId().IsEmpty() || !GetPawn() || ClaimScenarioStep>=99) { return; }
    if (ClaimScenarioNextAt==0) { ClaimScenarioNextAt=Now+3; }
    if (Now<ClaimScenarioNextAt) { return; }
    const auto Fail=[&](const TCHAR* Why)
    {
        UE_LOG(LogTemp,Error,TEXT("CLAIM_SCENARIO_FAILED Role=%s Step=%d Why=%s Result=%d"),*ScenarioRole,ClaimScenarioStep,Why,int32(LastResultCode));
        FPlatformMisc::RequestExitWithStatus(false,1); ClaimScenarioStep=99;
    };
    const bool bReplied=ResultSerial!=ClaimScenarioSentSerial;
    const uint64 Revision=State->GetSnapshot().Revision; const FString ClaimId=State->GetClaimId();
    const uint8 Cube=0, Identity=uint8(SFConstructionGeometry::IdentityRotation());
    const uint8 Slab=1, Turned=0;  // The kept piece is not an identity cube, so the restart proves exact shape and orientation.
    const FIntVector Kept(0,0,0), Gone(1,0,0), Guest(2,0,0);
    if (ScenarioRole==TEXT("owner"))
    {
        if (ClaimScenarioStep>0 && !bReplied) { return; }
        const ESFConstructionNetworkResult Expected[]={ESFConstructionNetworkResult::Claimed,ESFConstructionNetworkResult::Claimed,
            ESFConstructionNetworkResult::Accepted,ESFConstructionNetworkResult::Accepted,ESFConstructionNetworkResult::Removed};
        if (ClaimScenarioStep>0 && LastResultCode!=Expected[ClaimScenarioStep]) { Fail(TEXT("unexpected reply")); return; }
        ClaimScenarioSentSerial=ResultSerial;
        switch (ClaimScenarioStep++)
        {
        case 0: ServerClaimPlot(ClaimId); break;
        case 1: ServerPlace(Kept,Revision,Slab,Turned,ClaimId); break;
        case 2: ServerPlace(Gone,Revision,Cube,Identity,ClaimId); break;
        case 3: ServerRemove(Gone,Revision,ClaimId); break;
        default:
            UE_LOG(LogTemp,Display,TEXT("CLAIM_SCENARIO_OWNER_DONE Claim=%s Members=%d Content=%s"),*ClaimId,State->GetSnapshot().Members.Num(),*State->GetContentDigest());
            ClaimScenarioStep=99; break;
        }
    }
    else if (ScenarioRole==TEXT("owner_restart"))
    {
        FString Barrier;
        if (ClaimScenarioStep==0 && State->GetSnapshot().Members.Num()==1 && State->GetSnapshot().Members[0].Cell==Kept && State->IsProjectionCurrent())
        { UE_LOG(LogTemp,Display,TEXT("CLAIM_SCENARIO_RESTORED Claim=%s Members=1 Cell=%s Content=%s"),*ClaimId,*Kept.ToString(),*State->GetContentDigest()); ClaimScenarioStep=1; }
        else if (ClaimScenarioStep==1 && FParse::Value(FCommandLine::Get(),TEXT("ClaimGrantBarrier="),Barrier) && FPaths::FileExists(Barrier))
        { ClaimScenarioSentSerial=ResultSerial; SFGrantBuild(TEXT("bravo")); ClaimScenarioStep=2; }
        else if (ClaimScenarioStep==2 && bReplied)
        {
            if (LastResultCode!=ESFConstructionNetworkResult::Granted) { Fail(TEXT("grant")); return; }
            UE_LOG(LogTemp,Display,TEXT("CLAIM_SCENARIO_GRANTED Claim=%s Target=chr_dev_bravo"),*ClaimId); ClaimScenarioStep=99;
        }
    }
    else if (ScenarioRole==TEXT("guest"))
    {
        // Refused on someone else's claim until the owner grants build rights; then the same request succeeds.
        if (ClaimScenarioStep==0) { ClaimScenarioSentSerial=ResultSerial; ServerPlace(Guest,Revision,Cube,Identity,ClaimId); ClaimScenarioStep=1; return; }
        if (!bReplied) { return; }
        if (LastResultCode==ESFConstructionNetworkResult::Accepted)
        { UE_LOG(LogTemp,Display,TEXT("CLAIM_SCENARIO_GUEST_BUILT Claim=%s Cell=%s Members=%d"),*ClaimId,*Guest.ToString(),State->GetSnapshot().Members.Num()); ClaimScenarioStep=99; return; }
        if (LastResultCode!=ESFConstructionNetworkResult::PermissionDenied) { Fail(TEXT("guest result")); return; }
        UE_LOG(LogTemp,Display,TEXT("CLAIM_SCENARIO_GUEST_REFUSED Claim=%s Text=%s"),*ClaimId,*LastPlacementStatus);
        ClaimScenarioNextAt=Now+3; ClaimScenarioStep=0;
    }
}
#endif


#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
// Dev (UI evidence): every visible widget of every viewport UserWidget with its real Slate rect in 1920x1080 viewport pixels, written to
// Saved/UiRects.json for tools/ui_pixel_diff.py (exact live geometry, including dark-on-dark edges).
static void WriteUiRects(UWorld* World)
{
    struct FEntry { FString Owner, Name, Class; FVector2D Pos, Size; };
    TArray<FEntry> Entries;
    FVector2D Origin = FVector2D::ZeroVector; double Scale = 1.0, BestArea = 0.0;
    for (TObjectIterator<UUserWidget> It; It; ++It)
    {
        UUserWidget* Root = *It;
        if (!Root->IsInViewport() || !Root->IsVisible() || Root->GetWorld() != World || !Root->WidgetTree) { continue; }
        const FGeometry RootGeometry = Root->GetCachedGeometry();
        const double Area = RootGeometry.GetAbsoluteSize().X * RootGeometry.GetAbsoluteSize().Y;
        if (Area > BestArea) { BestArea = Area; Origin = FVector2D(RootGeometry.GetAbsolutePosition()); Scale = RootGeometry.GetAbsoluteSize().X > 0.f ? 1920.0 / RootGeometry.GetAbsoluteSize().X : 1.0; }
        Root->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            if (!Widget) { return; }
            for (const UWidget* Node = Widget; Node; Node = Node->GetParent()) { if (!Node->IsVisible()) { return; } }  // a collapsed ancestor leaves stale geometry
            const FGeometry Geometry = Widget->GetCachedGeometry();
            if (Geometry.GetAbsoluteSize().X < 1.f || Geometry.GetAbsoluteSize().Y < 1.f) { return; }
            Entries.Add({Root->GetName(), Widget->GetName(), Widget->GetClass()->GetName(), FVector2D(Geometry.GetAbsolutePosition()), FVector2D(Geometry.GetAbsoluteSize())});
        });
    }
    FString Json = TEXT("[\n");
    for (const FEntry& Entry : Entries)
    {
        const FVector2D Pos = (Entry.Pos - Origin) * Scale, Size = Entry.Size * Scale;
        Json += FString::Printf(TEXT("{\"owner\":\"%s\",\"name\":\"%s\",\"class\":\"%s\",\"rect\":[%.1f,%.1f,%.1f,%.1f]},\n"), *Entry.Owner, *Entry.Name, *Entry.Class, Pos.X, Pos.Y, Size.X, Size.Y);
    }
    Json += TEXT("{}]\n");
    FFileHelper::SaveStringToFile(Json, *(FPaths::ProjectSavedDir() / TEXT("UiRects.json")));
    UE_LOG(LogTemp, Display, TEXT("UI_RECTS_WRITTEN %d"), Entries.Num());
}
#endif

void ASFPlayerController::SFUi(const FString& Screen)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (Screen.Equals(TEXT("rects"), ESearchCase::IgnoreCase)) { WriteUiRects(GetWorld()); return; }
#endif
    if (Screen.Equals(TEXT("inventory"), ESearchCase::IgnoreCase)) { ToggleInventoryPanel(); }
    else if (Screen.Equals(TEXT("equipment"), ESearchCase::IgnoreCase)) { ToggleEquipmentPanel(); }
    else if (Screen.Equals(TEXT("market"), ESearchCase::IgnoreCase)) { ToggleMarketDemo(); }
    else if (Screen.Equals(TEXT("trade"), ESearchCase::IgnoreCase)) { ToggleTradeDemo(); }
    else if (Screen.Equals(TEXT("tradelock"), ESearchCase::IgnoreCase) && TradeWindow) { TradeWindow->ToggleLock(); }
    else if (Screen.Equals(TEXT("build"), ESearchCase::IgnoreCase)) { ToggleBuildMode(); }
    else if (Screen.Equals(TEXT("settings"), ESearchCase::IgnoreCase)) { ToggleSettingsPanel(); }
    else if (Screen.Equals(TEXT("menu"), ESearchCase::IgnoreCase)) { ToggleGameMenu(!IsGameMenuOpen()); }
    else if (Screen.Equals(TEXT("journal"), ESearchCase::IgnoreCase) || Screen.Equals(TEXT("board"), ESearchCase::IgnoreCase)) { USFQuestBoardPanel::OpenForCapture(this, Screen.ToLower()); }
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    else if (Screen.Equals(TEXT("skills"), ESearchCase::IgnoreCase)) { ToggleGatheringSkillsPanel(); }
    else if (Screen.StartsWith(TEXT("defeated"), ESearchCase::IgnoreCase))
    {
        if (USFE1VitalsHudSubsystem* Vitals = GetWorld() ? GetWorld()->GetSubsystem<USFE1VitalsHudSubsystem>() : nullptr)
        {
            if (Screen == TEXT("defeated-waiting")) Vitals->StageDefeat(43);
            else if (Screen == TEXT("defeated-ready")) Vitals->StageDefeat(0);
            else if (Screen == TEXT("defeated-pending")) Vitals->StageDefeat(0, true);
            else if (Screen == TEXT("defeated-close")) Vitals->CloseNotice();
            else if (Screen == TEXT("defeated-open")) Vitals->ReopenNotice();
            else if (Screen == TEXT("defeated-clear")) Vitals->ClearStagedDefeat();
            else Vitals->StageDefeat();
        }
    }
    else if (Screen.Equals(TEXT("target"), ESearchCase::IgnoreCase))
    {
        ASFCombatNpc* NearestBoar = nullptr;
        double BestBoar = TNumericLimits<double>::Max();
        for (TActorIterator<ASFCombatNpc> It(GetWorld()); It && GetPawn(); ++It)
        {
            const double Distance = FVector::DistSquared(GetPawn()->GetActorLocation(), It->GetActorLocation());
            if (Distance < BestBoar) { BestBoar = Distance; NearestBoar = *It; }
        }
        if (NearestBoar && GetPawn()) { SetControlRotation((NearestBoar->GetActorLocation() - GetPawn()->GetPawnViewLocation()).Rotation()); }
        SelectNextCombatTarget();
        if (GatheringHud) { GatheringHud->CaptureToast(); }
        if (HudLayout) { HudLayout->CapturePrompt(); }
    }
    else if ((Screen.Equals(TEXT("station"), ESearchCase::IgnoreCase) || Screen.Equals(TEXT("aim"), ESearchCase::IgnoreCase))
        && GatheringPilot && GatheringPilot->IsEnabled() && GetPawn())
    {
        const bool bStation = Screen.Equals(TEXT("station"), ESearchCase::IgnoreCase);
        ASFGatheringPilotNode* Nearest = nullptr;
        double Best = TNumericLimits<double>::Max();
        for (TActorIterator<ASFGatheringPilotNode> It(GetWorld()); It; ++It)
        {
            if (bStation ? !It->IsRefinery() : It->Kind != ESFGatheringPilotKind::Oak) { continue; }
            const double Distance = FVector::DistSquared(GetPawn()->GetActorLocation(), It->GetActorLocation());
            if (Distance < Best) { Best = Distance; Nearest = *It; }
        }
        if (Nearest)
        {
            const FVector Start = GetPawn()->GetPawnViewLocation();
            const FVector Direction = (Nearest->GetActorLocation() - Start).GetSafeNormal();
            SetControlRotation(Direction.Rotation());
            FVector2D ScreenLocation;
            if (ProjectWorldLocationToScreen(Nearest->GetActorLocation(), ScreenLocation)) { SetMouseLocation(FMath::RoundToInt(ScreenLocation.X), FMath::RoundToInt(ScreenLocation.Y)); }
        }
    }
#endif
}
