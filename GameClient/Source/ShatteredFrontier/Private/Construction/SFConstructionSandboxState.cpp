#include "Construction/SFConstructionSandboxState.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionLine.h"
#include "Construction/SFConstructionSupport.h"
#include "Construction/SFConstructionSandboxFoundation.h"
#include "Construction/SFConstructionClearance.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Engine/ScopedMovementUpdate.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Characters/SFE1Character.h"

struct FSFConstructionDoorPush
{
    TWeakObjectPtr<UCapsuleComponent> Capsule;
    TWeakObjectPtr<ASFConstructionSandboxDoorProjection> Door;
    uint64 Revision = 0;
    FVector From = FVector::ZeroVector;
    FVector To = FVector::ZeroVector;
};

namespace
{
const FIntVector Adjacent[] = { {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1} };
bool Finite(const FVector& P) { return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z); }
TArray<FBox> Reservations(const FSFConstructionDoorProjectionProfile& Profile)
{ return {Profile.Envelope,Profile.Motion,Profile.ApproachFront,Profile.ApproachBack}; }
bool PositiveIntersection(const FBox& A, const FBox& B)
{
    return A.Min.X < B.Max.X && A.Max.X > B.Min.X && A.Min.Y < B.Max.Y && A.Max.Y > B.Min.Y
        && A.Min.Z < B.Max.Z && A.Max.Z > B.Min.Z;
}
bool RayBox(const FVector& Start, const FVector& End, const FBox& Box, double& OutT)
{
    if (!Box.IsValid) { return false; }
    const FVector Delta = End-Start;
    double Near = 0.0, Far = 1.0;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (FMath::Abs(Delta[Axis]) < 1.e-9)
        { if (Start[Axis] < Box.Min[Axis] || Start[Axis] > Box.Max[Axis]) { return false; } continue; }
        double A = (Box.Min[Axis]-Start[Axis])/Delta[Axis];
        double B = (Box.Max[Axis]-Start[Axis])/Delta[Axis];
        if (A > B) { Swap(A,B); }
        Near = FMath::Max(Near,A); Far = FMath::Min(Far,B);
        if (Near > Far) { return false; }
    }
    OutT = Near;
    return true;
}
}

ASFConstructionSandboxState::ASFConstructionSandboxState()
{
    bReplicates = false;
    PrimaryActorTick.bCanEverTick = true;
}

void ASFConstructionSandboxState::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority() || GetNetMode() != NM_Standalone || !GetWorld()->GetAuthGameMode<ASFConstructionSandboxGameMode>()) { return; }
    const auto* Settings = GetDefault<USFConstructionSandboxSettings>();
    FString Error;
    if (!Settings->Validate(Error))
    { UE_LOG(LogTemp, Error, TEXT("CONSTRUCTION_SETTINGS_REJECTED %s"), *Error); return; }
    ReachCentimetres = Settings->BuildReachMetres * 100.0f;
    MaximumMembers = Settings->MaximumMembers;
    MaximumLineCells = Settings->MaximumLineCells;
    MaximumDoors = Settings->MaximumDoors;
    GraceSeconds = Settings->SupportLossGraceSeconds;
    RetrySeconds = Settings->CollapseRetrySeconds;
    bReady = true;
    UE_LOG(LogTemp, Display, TEXT("CONSTRUCTION_TOOLS_READY Standalone=1 UnlimitedDevelopmentStock=1 Support=FixedFoundation Collapse=GraceWaves EconomicSalvage=0 SnapshotAPI=1 Network=0 ReachMetres=%.1f MaximumMembers=%d"),
        ReachCentimetres / 100.0f, MaximumMembers);
}

FVector ASFConstructionSandboxState::CellCorner(const FIntVector& Cell)
{
    return FVector(Cell) * SFConstructionGeometry::CellCentimetres;
}

bool ASFConstructionSandboxState::FindCell(const AActor* Projection, FIntVector& OutCell) const
{
    if (!IsValid(Projection)) { return false; }
    for (const auto& Pair : Members)
    {
        if (Pair.Value.Projection.Get() == Projection) { OutCell = Pair.Key; return true; }
    }
    return false;
}

bool ASFConstructionSandboxState::FindDoorOrigin(const AActor* Projection, FIntVector& OutOrigin) const
{
    if (!IsValid(Projection)) { return false; }
    for (const auto& Pair : Doors)
    { if (Pair.Value.Projection.Get() == Projection) { OutOrigin = Pair.Key; return true; } }
    return false;
}

bool ASFConstructionSandboxState::FindDoorAlongRay(const ASFConstructionSandboxController* Controller,
    const FVector& Start, const FVector& End,
    FIntVector& OutOrigin, FVector* OutPoint) const
{
    const APawn* Pawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
    if (!IsInGameThread() || !GetWorld() || !HasAuthority() || GetNetMode() != NM_Standalone
        || !IsValid(Controller) || Controller->GetWorld() != GetWorld() || !IsValid(Pawn)
        || Pawn->GetWorld() != GetWorld() || Pawn->GetController() != Controller
        || Pawn->GetOwner() != Controller || !Finite(Start) || !Finite(End)
        || !Finite(Pawn->GetActorLocation()) || !Finite(End-Start)
        || (End-Start).SizeSquared() < 1.e-6) { return false; }
    const FVector Delta = End-Start;
    if (Delta.Size() > ReachCentimetres+FVector::Dist(Start,Pawn->GetActorLocation())+1.0) { return false; }
    double BestT = TNumericLimits<double>::Max();
    FIntVector Best = FIntVector::ZeroValue;
    bool bFound = false;
    for (const auto& Pair : Doors)
    {
        const auto* Door = Pair.Value.Projection.Get();
        double T;
        if (!Door || FVector::DistSquared(Pawn->GetActorLocation(),Door->GetEnvelopeBounds().GetCenter())
                > FMath::Square(static_cast<double>(ReachCentimetres))
            || !RayBox(Start,End,Door->GetEnvelopeBounds(),T)) { continue; }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionDoorVirtualTarget),false,Pawn);
        FHitResult Hit;
        // A blocker before the envelope entry occludes it. A collider on this
        // exact door at the entry is part of the selected virtual doorway.
        const FVector Probe = Start+Delta*FMath::Min(1.0,T+1.e-5);
        if (GetWorld()->LineTraceSingleByChannel(Hit,Start,Probe,ECC_Visibility,Query)
            && Hit.GetActor() != Door) { continue; }
        if (!bFound || T < BestT-1.e-9 || (FMath::IsNearlyEqual(T,BestT,1.e-9)
            && (Pair.Key.X < Best.X || (Pair.Key.X == Best.X && (Pair.Key.Y < Best.Y
                || (Pair.Key.Y == Best.Y && Pair.Key.Z < Best.Z))))))
        { bFound = true; BestT = T; Best = Pair.Key; }
    }
    if (!bFound) { return false; }
    OutOrigin = Best;
    // Slab-entry arithmetic can land a few ulps outside the exact boundary.
    // Clamp only this computed intersection; authority still rejects arbitrary
    // intent points outside the unchanged released envelope.
    if (OutPoint) { *OutPoint = Doors.FindChecked(Best).Projection->GetEnvelopeBounds().GetClosestPointTo(Start+Delta*BestT); }
    return true;
}

bool ASFConstructionSandboxState::IsReplacementQueryCurrent(const FSFConstructionReplacementSet& Replacement) const
{
    if (Replacement.Owner.Get() != this || Replacement.Revision != Revision
        || Members.Num() > MaximumMembers || Doors.Num() > MaximumDoors || !ValidateEnvironment().IsEmpty()
        || Replacement.Components.Num() != Members.Num()+Doors.Num()*5) { return false; }
    TSet<const UPrimitiveComponent*> Expected;
    for (const auto& Pair : Members) { Expected.Add(Pair.Value.Projection->GetMesh()); }
    for (const auto& Pair : Doors)
    { for (int32 Index = 0; Index < 5; ++Index) { Expected.Add(Pair.Value.Projection->GetCollider(Index)); } }
    if (Expected.Num() != Replacement.Components.Num()) { return false; }
    for (const auto& Component : Replacement.Components)
    { if (!Component.IsValid() || Expected.Remove(Component.Get()) != 1) { return false; } }
    return Expected.Num() == 0;
}

FSFConstructionReplacementSet ASFConstructionSandboxState::MakeReplacementSet() const
{
    FSFConstructionReplacementSet Replacement;
    Replacement.Owner = const_cast<ASFConstructionSandboxState*>(this); Replacement.Revision = Revision;
    for (const auto& Pair : Members) { Replacement.Components.Add(const_cast<UProceduralMeshComponent*>(Pair.Value.Projection->GetMesh())); }
    for (const auto& Pair : Doors)
    { for (int32 Index = 0; Index < 5; ++Index) { Replacement.Components.Add(const_cast<UBoxComponent*>(Pair.Value.Projection->GetCollider(Index))); } }
    return Replacement;
}

bool ASFConstructionSandboxState::IsDoorCloseQueryCurrent(const FSFConstructionDoorCloseQuery& Query) const
{
    const auto* Door = Query.Door.Get();
    const auto* Capsule = Query.Capsule.Get();
    const auto* Character = Capsule ? Cast<ASFE1Character>(Capsule->GetOwner()) : nullptr;
    const auto* Controller = Character ? Cast<ASFConstructionSandboxController>(Character->GetController()) : nullptr;
    const auto* Record = Door ? Doors.Find(Door->GetOriginCell()) : nullptr;
    return Query.Owner.Get() == this && Query.Revision == Revision && Door && Record
        && Record->Projection.Get() == Door && Record->bOpen && Door->IsActive() && Door->IsProjectionIntact()
        && Character && Character->GetCapsuleComponent() == Capsule && Controller
        && Controller->GetPawn() == Character && Character->GetOwner() == Controller
        && Capsule->GetWorld() == GetWorld() && Capsule->GetComponentLocation().Equals(Query.CapturedCentre,0.001)
        && FMath::IsNearlyEqual(Capsule->GetScaledCapsuleRadius(),Query.Radius,0.001f)
        && FMath::IsNearlyEqual(Capsule->GetScaledCapsuleHalfHeight(),Query.HalfHeight,0.001f);
}

bool ASFConstructionSandboxState::PlanDoorPush(const ASFConstructionSandboxController* Controller,
    const FIntVector& Origin, int32 Yaw, FSFConstructionDoorPush& Out, FSFConstructionDoorCloseQuery& OutQuery) const
{
    auto* Character = Controller ? Cast<ASFE1Character>(Controller->GetPawn()) : nullptr;
    auto* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
    auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    const auto* Record = Doors.Find(Origin);
    auto* Door = Record ? Record->Projection.Get() : nullptr;
    if (!Capsule || !Door || !Record->bOpen || Record->YawQuarterTurns != Yaw
        || Character->GetRootComponent() != Capsule || !Movement || Movement->UpdatedComponent != Capsule
        || !Capsule->IsQueryCollisionEnabled()
        || !Capsule->IsRegistered() || !Capsule->IsPhysicsStateCreated()
        || Capsule->IsSimulatingPhysics()) { return false; }
    const FVector From = Capsule->GetComponentLocation();
    const FQuat Rotation = Capsule->GetComponentQuat();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    if (!Finite(From) || Rotation.ContainsNaN() || !Rotation.IsNormalized()
        || !FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight)
        || Radius <= 0 || HalfHeight < Radius || FVector::DotProduct(Rotation.GetUpVector(),FVector::UpVector) < 0.9999)
    { return false; }
    FSFConstructionDoorProjectionProfile Closed;
    if (!ASFConstructionSandboxDoorProjection::BuildProfile(Origin,Yaw,false,Closed)) { return false; }
    FBox LocalMotion(ForceInit);
    for (int32 X = 0; X < 2; ++X)
    for (int32 Y = 0; Y < 2; ++Y)
    for (int32 Z = 0; Z < 2; ++Z)
    {
        const FVector Corner(X ? Closed.Motion.Max.X : Closed.Motion.Min.X,
            Y ? Closed.Motion.Max.Y : Closed.Motion.Min.Y,
            Z ? Closed.Motion.Max.Z : Closed.Motion.Min.Z);
        LocalMotion += Closed.Transform.InverseTransformPosition(Corner);
    }
    if (!LocalMotion.IsValid || !Finite(LocalMotion.Min) || !Finite(LocalMotion.Max)) { return false; }
    const FVector Local = Closed.Transform.InverseTransformPosition(From);
    // Only the capsule actually occupying this released whole-motion reservation can move.
    const double CX = FMath::Clamp(static_cast<double>(Local.X),LocalMotion.Min.X,LocalMotion.Max.X);
    const double CY = FMath::Clamp(static_cast<double>(Local.Y),LocalMotion.Min.Y,LocalMotion.Max.Y);
    if (FMath::Square(Local.X-CX)+FMath::Square(Local.Y-CY) >= FMath::Square(Radius)
        || From.Z-HalfHeight >= Closed.Motion.Max.Z || From.Z+HalfHeight <= Closed.Motion.Min.Z)
    { return false; }
    // 0.1 cm is a fixed numerical separation from a touching sweep endpoint.
    const double Front = LocalMotion.Min.Y-static_cast<double>(Radius)-0.1;
    const double Back = LocalMotion.Max.Y+static_cast<double>(Radius)+0.1;
    const double Destinations[2] = {FMath::Abs(Local.Y-Front) <= FMath::Abs(Local.Y-Back) ? Front : Back,
        FMath::Abs(Local.Y-Front) <= FMath::Abs(Local.Y-Back) ? Back : Front};
    FSFConstructionDoorCloseQuery CloseQuery;
    CloseQuery.Owner = const_cast<ASFConstructionSandboxState*>(this); CloseQuery.Door = Door;
    CloseQuery.Capsule = Capsule; CloseQuery.Revision = Revision; CloseQuery.CapturedCentre = From;
    CloseQuery.Radius = Radius; CloseQuery.HalfHeight = HalfHeight;
    for (double Destination : Destinations)
    {
        FVector CandidateLocal = Local; CandidateLocal.Y = Destination;
        FVector To = Closed.Transform.TransformPosition(CandidateLocal); To.Z = From.Z;
        if (!Finite(To) || (To-From).Size() > LocalMotion.Max.Y-LocalMotion.Min.Y+2.0*Radius+1.0) { continue; }
        // Conservative admission of the entire path, including bodies outside
        // Motion. Actual capsule sweeps below still test the old open leaf.
        const FVector Extent(Radius,Radius,HalfHeight);
        const FBox SweptBounds = FBox(From-Extent,From+Extent)+FBox(To-Extent,To+Extent);
        const auto SweptResult = SFConstructionClearance::EvaluateBoxClosing(GetWorld(),SweptBounds,Door,CloseQuery);
        if (SweptResult != SFConstructionClearance::EResult::Clear) { continue; }
        FCollisionQueryParams PathQuery(SCENE_QUERY_STAT(ConstructionDoorPushPath),false);
        PathQuery.AddIgnoredComponent(Capsule);
        const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius,HalfHeight);
        FHitResult Hit;
        if (GetWorld()->SweepSingleByChannel(Hit,From,To,Rotation,ECC_Pawn,Shape,PathQuery)) { continue; }
        if (GetWorld()->OverlapBlockingTestByChannel(To,Rotation,ECC_Pawn,Shape,PathQuery)) { continue; }
        bool bClosedBlocked = false;
        for (const auto& Box : Closed.Boxes)
        {
            const FVector Centre = Closed.Transform.TransformPosition(Box.Centre);
            const FQuat BoxRotation = Closed.Transform.GetRotation()*Box.Rotation;
            const FVector Relative = BoxRotation.UnrotateVector(To-Centre);
            const double DX = FMath::Max(FMath::Abs(Relative.X)-Box.Extent.X,0.0);
            const double DY = FMath::Max(FMath::Abs(Relative.Y)-Box.Extent.Y,0.0);
            if (DX*DX+DY*DY < FMath::Square(Radius)
                && FMath::Abs(Relative.Z) < HalfHeight+Box.Extent.Z) { bClosedBlocked = true; break; }
        }
        if (bClosedBlocked) { continue; }
        Out.Capsule = Capsule; Out.Door = Door; Out.Revision = Revision; Out.From = From; Out.To = To;
        OutQuery = CloseQuery;
        return true;
    }
    return false;
}

FString ASFConstructionSandboxState::ValidateStableSnapshot(const ASFConstructionSandboxController* Controller, uint64 ExpectedRevision) const
{
    const FString Error = ValidateOwner(Controller,ExpectedRevision);
    if (!Error.IsEmpty()) { return Error; }
    if (Instability.Num() != 0 || DoorInstability.Num() != 0)
    { return TEXT("Stable snapshot required; saving or restoring cannot reset active support-loss grace"); }
    TArray<FSFConstructionSupportResult> Structural;
    TArray<FSFConstructionDoorSupportResult> Functional;
    const FString SupportError = SolveCombined(Members,Doors,Structural,Functional);
    if (!SupportError.IsEmpty()) { return SupportError; }
    for (const auto& Entry : Structural)
    { if (Entry.State == ESFConstructionSupportState::Unsupported) { return TEXT("Current construction is unsupported; snapshot unavailable"); } }
    for (const auto& Entry : Functional)
    { if (Entry.State == ESFConstructionSupportState::Unsupported) { return TEXT("Current door attachment is unsupported; snapshot unavailable"); } }
    return FString();
}

bool ASFConstructionSandboxState::ExportSnapshot(const ASFConstructionSandboxController* Controller, uint64 ExpectedRevision,
    FSFConstructionSandboxSnapshot& OutSnapshot, FString& Result) const
{
    Result = ValidateStableSnapshot(Controller,ExpectedRevision);
    if (!Result.IsEmpty()) { return false; }
    FSFConstructionSandboxSnapshot Candidate;
    Candidate.SourceRevision = Revision;
    for (const auto& Pair : Members) { Candidate.Cells.Add({Pair.Key,Pair.Value.Shape,Pair.Value.Rotation}); }
    for (const auto& Pair : Doors) { Candidate.Doors.Add({Pair.Key,Pair.Value.YawQuarterTurns,Pair.Value.bOpen}); }
    TArray<uint8> Bytes;
    FSFConstructionSandboxSnapshot Canonical;
    if (!SFConstructionSaveCodec::Encode(Candidate,Bytes) || !SFConstructionSaveCodec::Decode(Bytes,Canonical))
    { Result = TEXT("Snapshot content invalid; export output preserved"); return false; }
    OutSnapshot = MoveTemp(Canonical);
    Result = TEXT("Stable development snapshot exported; no inventory or salvage provenance");
    return true;
}

bool ASFConstructionSandboxState::ImportSnapshot(ASFConstructionSandboxController* Controller, uint64 ExpectedRevision,
    const FSFConstructionSandboxSnapshot& Snapshot, FString& Result)
{
    Result = ValidateStableSnapshot(Controller,ExpectedRevision);
    if (!Result.IsEmpty()) { return false; }
    if (Snapshot.Cells.Num() > MaximumMembers || Snapshot.Doors.Num() > MaximumDoors)
    { Result = TEXT("Snapshot exceeds captured sandbox cell or door limits"); return false; }
    TArray<uint8> Bytes;
    FSFConstructionSandboxSnapshot Canonical;
    if (!SFConstructionSaveCodec::Encode(Snapshot,Bytes) || !SFConstructionSaveCodec::Decode(Bytes,Canonical))
    { Result = TEXT("Invalid or incompatible development snapshot; construction preserved"); return false; }
    TMap<FIntVector,FSFConstructionSandboxMember> Candidate;
    TMap<FIntVector,FSFConstructionSandboxDoor> CandidateDoors;
    TArray<FIntVector> Changed;
    for (const auto& Pair : Members) { Changed.Add(Pair.Key); }
    for (const auto& Cell : Canonical.Cells)
    {
        auto Member = Members.Contains(Cell.Cell) ? Members.FindChecked(Cell.Cell) : FSFConstructionSandboxMember();
        Member.Shape = Cell.Shape; Member.Rotation = Cell.Rotation;
        Candidate.Add(Cell.Cell,Member); Changed.AddUnique(Cell.Cell);
    }
    for (const auto& Door : Canonical.Doors)
    {
        auto Member = Doors.Contains(Door.OriginCell) ? Doors.FindChecked(Door.OriginCell) : FSFConstructionSandboxDoor();
        Member.YawQuarterTurns = Door.YawQuarterTurns; Member.bOpen = Door.bOpen;
        CandidateDoors.Add(Door.OriginCell,Member);
    }
    TArray<FSFConstructionSupportResult> Structural;
    TArray<FSFConstructionDoorSupportResult> Functional;
    Result = SolveCombined(Candidate,CandidateDoors,Structural,Functional);
    if (!Result.IsEmpty()) { return false; }
    for (const auto& Entry : Structural)
    { if (Entry.State == ESFConstructionSupportState::Unsupported) { Result = TEXT("Snapshot contains unsupported structural occupancy"); return false; } }
    for (const auto& Entry : Functional)
    { if (Entry.State == ESFConstructionSupportState::Unsupported) { Result = TEXT("Snapshot contains unsupported door attachments"); return false; } }
    TArray<FSFConstructionDoorProjectionProfile> Profiles;
    for (const auto& Door : Canonical.Doors)
    {
        FSFConstructionDoorProjectionProfile Profile;
        if (!ASFConstructionSandboxDoorProjection::BuildProfile(Door.OriginCell,Door.YawQuarterTurns,Door.bOpen,Profile))
        { Result = TEXT("Snapshot door profile unavailable"); return false; }
        Profiles.Add(MoveTemp(Profile));
    }
    for (int32 I = 0; I < Profiles.Num(); ++I)
    for (const auto& A : Reservations(Profiles[I]))
    {
        for (const auto& Cell : Canonical.Cells)
        {
            if (SFConstructionClearance::IntersectsReservation(Cell.Shape,Cell.Rotation,CellCorner(Cell.Cell),A) != SFConstructionClearance::EResult::Clear)
            { Result = TEXT("Snapshot structure invades door solid, motion or access reservation"); return false; }
        }
        for (int32 J = 0; J < I; ++J)
        for (const auto& B : Reservations(Profiles[J]))
        { if (PositiveIntersection(A,B)) { Result = TEXT("Snapshot door reservations overlap"); return false; } }
    }
    const FSFConstructionReplacementSet Replacement = MakeReplacementSet();
    if (!Replacement.Validate(GetWorld())) { Result = TEXT("Old owned projection set invalid; restore fenced"); return false; }
    for (const auto& Cell : Canonical.Cells)
    {
        if (SFConstructionClearance::EvaluateReplacing(GetWorld(),Cell.Shape,Cell.Rotation,CellCorner(Cell.Cell),Replacement) != SFConstructionClearance::EResult::Clear)
        { Result = TEXT("Snapshot occupied solid blocked by current character, prop or unrelated projection"); return false; }
    }
    for (const auto& Profile : Profiles)
    for (const FBox& Box : Reservations(Profile))
    {
        if (SFConstructionClearance::EvaluateBoxReplacing(GetWorld(),Box,Replacement) != SFConstructionClearance::EResult::Clear)
        { Result = TEXT("Snapshot door reservation blocked by current world occupancy"); return false; }
    }
    Changed.Sort([](const FIntVector& A,const FIntVector& B)
    { return A.X != B.X ? A.X < B.X : A.Y != B.Y ? A.Y < B.Y : A.Z < B.Z; });
    if (!CommitCombined(MoveTemp(Candidate),MoveTemp(CandidateDoors),Changed,AuthorityTime(),Result)) { return false; }
    Controller->CancelRemoval();
    Result = TEXT("Whole development assembly restored atomically; source revision retained only as provenance");
    return true;
}

FString ASFConstructionSandboxState::StructuralReservations(const FSFConstructionSandboxIntent& Intent) const
{
    for (const auto& Pair : Doors)
    {
        FSFConstructionDoorProjectionProfile Profile;
        if (!ASFConstructionSandboxDoorProjection::BuildProfile(Pair.Key,Pair.Value.YawQuarterTurns,Pair.Value.bOpen,Profile))
        { return TEXT("Door reservation content unavailable"); }
        for (const FBox& Box : Reservations(Profile))
        {
            const auto Result = SFConstructionClearance::IntersectsReservation(Intent.Shape,Intent.Rotation,CellCorner(Intent.Cell),Box);
            if (Result == SFConstructionClearance::EResult::Unavailable) { return TEXT("Door reservation test unavailable"); }
            if (Result == SFConstructionClearance::EResult::Blocked) { return TEXT("Door motion or access reservation occupied"); }
        }
    }
    return FString();
}

FString ASFConstructionSandboxState::DoorReservations(const FIntVector& Origin, int32 Yaw, bool bOpen,
    bool bToggle, const ASFConstructionSandboxDoorProjection* OwnOldDoor,
    const FSFConstructionDoorCloseQuery* CloseQuery) const
{
    FSFConstructionDoorProjectionProfile Profile;
    if (!ASFConstructionSandboxDoorProjection::BuildProfile(Origin,Yaw,bOpen,Profile)) { return TEXT("Door profile outside sandbox bounds"); }
    const TArray<FBox> Required = bToggle ? TArray<FBox>{Profile.Motion} : Reservations(Profile);
    for (const auto& Pair : Doors)
    {
        if (Pair.Value.Projection.Get() == OwnOldDoor) { continue; }
        FSFConstructionDoorProjectionProfile Other;
        if (!ASFConstructionSandboxDoorProjection::BuildProfile(Pair.Key,Pair.Value.YawQuarterTurns,Pair.Value.bOpen,Other)) { return TEXT("Door reservation unavailable"); }
        for (const FBox& A : Required)
        for (const FBox& B : Reservations(Other))
        { if (PositiveIntersection(A,B)) { return TEXT("Another door reserves this motion or access space"); } }
    }
    for (const FBox& Box : Required)
    {
        for (const auto& Member : Members)
        {
            const auto Intersection = SFConstructionClearance::IntersectsReservation(Member.Value.Shape,Member.Value.Rotation,CellCorner(Member.Key),Box);
            if (Intersection == SFConstructionClearance::EResult::Unavailable) { return TEXT("Canonical structural reservation query unavailable"); }
            if (Intersection == SFConstructionClearance::EResult::Blocked) { return TEXT("Door reservation intersects canonical structural occupancy"); }
        }
        const auto Result = CloseQuery
            ? SFConstructionClearance::EvaluateBoxClosing(GetWorld(),Box,OwnOldDoor,*CloseQuery)
            : SFConstructionClearance::EvaluateBox(GetWorld(),Box,OwnOldDoor);
        if (Result == SFConstructionClearance::EResult::Unavailable) { return TEXT("Door clearance unavailable"); }
        if (Result == SFConstructionClearance::EResult::Blocked) { return TEXT("Door solid, motion or approach blocked by character, structure or prop"); }
    }
    return FString();
}

FString ASFConstructionSandboxState::EvaluateDoor(const ASFConstructionSandboxController* Controller, const FSFConstructionDoorIntent& Intent) const
{
    const FString OwnerError = ValidateOwner(Controller,Intent.ExpectedRevision);
    if (!OwnerError.IsEmpty()) { return OwnerError; }
    if (Intent.Action != ESFConstructionDoorAction::Place && Intent.Action != ESFConstructionDoorAction::Toggle
        && Intent.Action != ESFConstructionDoorAction::Remove) { return TEXT("Unknown door action"); }
    const auto* Existing = Doors.Find(Intent.OriginCell);
    const bool bPlace = Intent.Action == ESFConstructionDoorAction::Place;
    if (bPlace && Existing) { return TEXT("Door origin already occupied"); }
    if (!bPlace && !Existing) { return TEXT("No editable door at origin"); }
    if (bPlace && Doors.Num() >= MaximumDoors) { return TEXT("Development door limit reached"); }
    if (!bPlace && Intent.YawQuarterTurns != Existing->YawQuarterTurns) { return TEXT("Door yaw does not match retained identity"); }
    FSFConstructionDoorProjectionProfile Profile;
    const bool bOpen = !bPlace && Intent.Action == ESFConstructionDoorAction::Toggle ? !Existing->bOpen : Existing ? Existing->bOpen : false;
    if (!ASFConstructionSandboxDoorProjection::BuildProfile(Intent.OriginCell,Intent.YawQuarterTurns,bOpen,Profile)) { return TEXT("Invalid door origin or yaw"); }
    const APawn* Pawn = Controller->GetPawn();
    if (!Finite(Pawn->GetActorLocation()) || FVector::DistSquared(Pawn->GetActorLocation(),Profile.Envelope.GetCenter()) > FMath::Square(static_cast<double>(ReachCentimetres)))
    { return TEXT("Outside door reach"); }
    FVector Eyes; FRotator View;
    Pawn->GetActorEyesViewPoint(Eyes,View);
    if (!Finite(Eyes)) { return TEXT("Invalid pawn view"); }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionDoorIntent),false,Pawn);
    FHitResult Hit;
    const FVector Target = !bPlace && Intent.bHasInteractionPoint ? Intent.InteractionPoint : Profile.Envelope.GetCenter();
    if (!Finite(Target) || !Profile.Envelope.IsInsideOrOn(Target))
    { return TEXT("Door interaction point outside released doorway"); }
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit,Eyes,Target,ECC_Visibility,Query);
    if (bHit && (bPlace || Hit.GetActor() != Existing->Projection.Get()))
    { return TEXT("Door line of sight blocked or target unavailable"); }
    if (Intent.Action == ESFConstructionDoorAction::Toggle)
    {
        double Deadline;
        if (DoorInstability.GetDeadline(Intent.OriginCell,Deadline)) { return TEXT("Unstable door cannot toggle during grace"); }
    }
    if (Intent.Action != ESFConstructionDoorAction::Remove)
    {
        FSFConstructionDoorPush Push;
        FSFConstructionDoorCloseQuery CloseQuery;
        const bool bPush = Intent.Action == ESFConstructionDoorAction::Toggle && Existing->bOpen
            && PlanDoorPush(Controller,Intent.OriginCell,Intent.YawQuarterTurns,Push,CloseQuery);
        const FString Clearance = DoorReservations(Intent.OriginCell,Intent.YawQuarterTurns,bOpen,!bPlace,
            bPlace ? nullptr : Existing->Projection.Get(),bPush ? &CloseQuery : nullptr);
        if (!Clearance.IsEmpty()) { return Clearance; }
    }
    if (!bPlace) { return FString(); }
    auto CandidateDoors = Doors;
    FSFConstructionSandboxDoor Added;
    Added.YawQuarterTurns = Intent.YawQuarterTurns;
    CandidateDoors.Add(Intent.OriginCell,Added);
    TArray<FSFConstructionSupportResult> Structural;
    TArray<FSFConstructionDoorSupportResult> Functional;
    const FString Error = SolveCombined(Members,CandidateDoors,Structural,Functional);
    if (!Error.IsEmpty()) { return Error; }
    for (const auto& Entry : Structural)
    {
        double Deadline;
        if (Entry.State == ESFConstructionSupportState::Unsupported && !Instability.GetDeadline(Entry.Address,Deadline))
        { return TEXT("Door load overloads structural support"); }
    }
    for (const auto& Entry : Functional)
    {
        double Deadline;
        if (Entry.State == ESFConstructionSupportState::Unsupported
            && (Entry.OriginCell == Intent.OriginCell || !DoorInstability.GetDeadline(Entry.OriginCell,Deadline)))
        { return TEXT("Door requires two supported full-cell floor receivers or the known foundation"); }
    }
    return FString();
}

bool ASFConstructionSandboxState::ApplyDoor(ASFConstructionSandboxController* Controller, const FSFConstructionDoorIntent& Intent, FString& Result)
{
    Result = EvaluateDoor(Controller,Intent);
    if (!Result.IsEmpty()) { return false; }
    auto CandidateDoors = Doors;
    FSFConstructionDoorPush Push;
    FSFConstructionDoorCloseQuery CloseQuery;
    const bool bPush = Intent.Action == ESFConstructionDoorAction::Toggle
        && Doors.FindChecked(Intent.OriginCell).bOpen
        && PlanDoorPush(Controller,Intent.OriginCell,Intent.YawQuarterTurns,Push,CloseQuery);
    if (Intent.Action == ESFConstructionDoorAction::Remove) { CandidateDoors.Remove(Intent.OriginCell); }
    else if (Intent.Action == ESFConstructionDoorAction::Toggle) { CandidateDoors.FindChecked(Intent.OriginCell).bOpen = !CandidateDoors.FindChecked(Intent.OriginCell).bOpen; }
    else { FSFConstructionSandboxDoor Added; Added.YawQuarterTurns = Intent.YawQuarterTurns; CandidateDoors.Add(Intent.OriginCell,Added); }
    if (!CommitCombined(Members,MoveTemp(CandidateDoors),{},AuthorityTime(),Result,bPush ? &Push : nullptr)) { return false; }
    Result = TEXT("Door transaction published; economic salvage disabled");
    return true;
}

FString ASFConstructionSandboxState::Evaluate(const ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent, FString* SupportHint) const
{
    if (SupportHint) { SupportHint->Reset(); }
    const FString Validation = ValidateIntent(Controller, Intent);
    if (!Validation.IsEmpty()) { return Validation; }
    if (Intent.bConfirmSupportLoss && (!Intent.bRemove || ConfirmationController.Get() != Controller
        || ConfirmationCell != Intent.Cell || ConfirmationRevision != Revision))
    { return TEXT("Removal confirmation stale or missing; preview again"); }
    return CheckSupport(Intent, SupportHint);
}

FString ASFConstructionSandboxState::ValidateEnvironment() const
{
    if (!IsInGameThread() || !bReady || !HasAuthority() || IsActorBeingDestroyed() || !GetWorld()
        || GetNetMode() != NM_Standalone || !GetWorld()->GetAuthGameMode<ASFConstructionSandboxGameMode>())
    { return TEXT("Standalone construction sandbox only"); }
    if (!SFConstructionSandboxFoundation::HasKnownPlane(GetWorld()))
    { return TEXT("Known development foundation unavailable; edits and collapse blocked"); }
    for (const auto& Pair : Members)
    {
        if (!Pair.Value.Projection.IsValid() || Pair.Value.Projection->IsActorBeingDestroyed()
            || !Pair.Value.Projection->IsProjectionIntact(Pair.Value.Shape,Pair.Value.Rotation,Pair.Value.ProjectionGeneration,CellCorner(Pair.Key)))
        { return TEXT("Construction projection unavailable; edits blocked pending rebuild"); }
    }
    for (const auto& Pair : Doors)
    {
        const auto* Door = Pair.Value.Projection.Get();
        if (!IsValid(Door) || Door->IsActorBeingDestroyed() || !Door->IsActive() || !Door->IsProjectionIntact()
            || Door->GetOriginCell() != Pair.Key || Door->GetYawQuarterTurns() != Pair.Value.YawQuarterTurns
            || Door->IsOpen() != Pair.Value.bOpen)
        { return TEXT("Door projection unavailable or changed; edits and collapse blocked pending rebuild"); }
    }
    return FString();
}

FString ASFConstructionSandboxState::ValidateOwner(const ASFConstructionSandboxController* Controller, uint64 ExpectedRevision) const
{
    if (!IsInGameThread() || !bReady || !HasAuthority() || IsActorBeingDestroyed() || !GetWorld()
        || GetNetMode() != NM_Standalone || !GetWorld()->GetAuthGameMode<ASFConstructionSandboxGameMode>()
        || !IsValid(Controller) || Controller->IsActorBeingDestroyed() || !Controller->HasAuthority()
        || !Controller->IsLocalController() || Controller->GetWorld() != GetWorld())
    { return TEXT("Standalone construction sandbox only"); }
    const APawn* Pawn = Controller->GetPawn();
    if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed() || Pawn->GetWorld() != GetWorld()
        || Pawn->GetController() != Controller || Pawn->GetOwner() != Controller || !Pawn->HasAuthority())
    { return TEXT("No current owned pawn"); }
    if (ExpectedRevision != Revision) { return TEXT("Construction changed; preview refreshed"); }
    double Deadline;
    if ((Instability.GetNextDeadline(Deadline) && AuthorityTime() >= Deadline)
        || (DoorInstability.GetNextDeadline(Deadline) && AuthorityTime() >= Deadline))
    { return TEXT("Support collapse due; edits wait for the authoritative wave to resolve"); }
    if (Revision == MAX_uint64) { return TEXT("Revision exhausted; restart fixture"); }
    return ValidateEnvironment();
}

FString ASFConstructionSandboxState::ValidateIntent(const ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent) const
{
    const FString OwnerError = ValidateOwner(Controller,Intent.ExpectedRevision);
    if (!OwnerError.IsEmpty()) { return OwnerError; }
    const APawn* Pawn = Controller->GetPawn();
    // Release-bound plot matches the opt-in map floor. These are not tunable world topology.
    if (Intent.Cell.X < -20 || Intent.Cell.X >= 20 || Intent.Cell.Y < -20 || Intent.Cell.Y >= 20
        || Intent.Cell.Z < 0 || Intent.Cell.Z >= 20) { return TEXT("Outside 20 m sandbox plot or 10 m height"); }
    FSFConstructionSolid Solid;
    if (!SFConstructionGeometry::Build(Intent.Shape, Intent.Rotation, Solid)) { return TEXT("Unknown shape or rotation"); }
    const FVector Centre = CellCorner(Intent.Cell) + FVector(25);
    if (!Finite(Pawn->GetActorLocation()) || FVector::DistSquared(Pawn->GetActorLocation(), Centre) > FMath::Square(static_cast<double>(ReachCentimetres)))
    { return TEXT("Outside build reach"); }
    const auto* Member = Members.Find(Intent.Cell);
    if (Intent.bRemove && (!Member || !Member->Projection.IsValid())) { return TEXT("No editable sandbox member"); }
    if (!Intent.bRemove && Member) { return TEXT("Cell already occupied"); }
    if (!Intent.bRemove && Members.Num() >= MaximumMembers) { return TEXT("Development cell limit reached"); }
    FVector Eyes; FRotator EyeRotation;
    Pawn->GetActorEyesViewPoint(Eyes, EyeRotation);
    if (!Finite(Eyes)) { return TEXT("Invalid pawn view"); }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionIntent), false, Pawn);
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Centre, ECC_Visibility, Query)
        && (!Intent.bRemove || Hit.GetActor() != Member->Projection.Get()))
    { return TEXT("Build line of sight blocked"); }
    if (!Intent.bRemove)
    {
        const FString ReservationError = StructuralReservations(Intent);
        if (!ReservationError.IsEmpty()) { return ReservationError; }
        const auto Clearance = SFConstructionClearance::Evaluate(GetWorld(), Intent.Shape, Intent.Rotation, CellCorner(Intent.Cell));
        if (Clearance == SFConstructionClearance::EResult::Unavailable)
        { return TEXT("Canonical clearance unavailable"); }
        if (Clearance == SFConstructionClearance::EResult::Blocked)
        { return TEXT("Occupied solid, character or prop clearance"); }
    }
    return ValidateEnvironment();
}

FString ASFConstructionSandboxState::SolveSupport(const TMap<FIntVector, FSFConstructionSandboxMember>& Snapshot, TArray<FSFConstructionSupportResult>& Results) const
{
    TArray<FSFConstructionDoorSupportResult> DoorResults;
    return SolveCombined(Snapshot,Doors,Results,DoorResults);
}

FString ASFConstructionSandboxState::SolveCombined(const TMap<FIntVector, FSFConstructionSandboxMember>& Snapshot,
    const TMap<FIntVector, FSFConstructionSandboxDoor>& DoorSnapshot, TArray<FSFConstructionSupportResult>& Results,
    TArray<FSFConstructionDoorSupportResult>& DoorResults) const
{
    // One authoritative snapshot; never infer compact ground from a prop hit.
    if (!SFConstructionSandboxFoundation::HasKnownPlane(GetWorld()))
    { return TEXT("Known development foundation unavailable; edits blocked"); }
    TArray<FSFConstructionSupportMember> Candidate;
    Candidate.Reserve(Snapshot.Num());
    for (const auto& Pair : Snapshot)
    {
        Candidate.Add({Pair.Key, Pair.Value.Shape, Pair.Value.Rotation, 1.0,
            SFConstructionSandboxFoundation::TouchesPlane(Pair.Key, Pair.Value.Shape, Pair.Value.Rotation)});
    }
    // Approved release-bound profile only; intent cannot override material rules.
    TArray<FSFConstructionDoorSupportInput> DoorInputs;
    for (const auto& Pair : DoorSnapshot) { DoorInputs.Add({Pair.Key,Pair.Value.YawQuarterTurns}); }
    if (!SFConstructionDoorSupport::Evaluate(Candidate,DoorInputs,FSFConstructionDoorSupportProfile(),true,Results,DoorResults))
    { return TEXT("Support snapshot invalid; construction preserved"); }
    return FString();
}

FString ASFConstructionSandboxState::CheckSupport(const FSFConstructionSandboxIntent& Intent, FString* SupportHint) const
{
    auto Candidate = Members;
    if (Intent.bRemove) { Candidate.Remove(Intent.Cell); }
    else
    {
        FSFConstructionSandboxMember Added;
        Added.Shape = Intent.Shape; Added.Rotation = Intent.Rotation;
        Candidate.Add(Intent.Cell, Added);
    }
    TArray<FSFConstructionSupportResult> Results;
    TArray<FSFConstructionDoorSupportResult> DoorResults;
    const FString Error = SolveCombined(Candidate,Doors,Results,DoorResults);
    if (!Error.IsEmpty()) { return Error; }
    bool bStrained = false;
    int32 UnstableCount = 0;
    int32 NewlyUnstableCount = 0;
    int32 NewlyUnstableDoors = 0;
    for (const auto& Result : Results)
    {
        if (Result.State == ESFConstructionSupportState::Unsupported)
        {
            ++UnstableCount;
            double Deadline;
            const bool bExistingEpisode = Instability.GetDeadline(Result.Address, Deadline);
            if (Intent.bRemove)
            { if (!bExistingEpisode) { ++NewlyUnstableCount; } continue; }
            // Only actual existing unstable members are exempt during repair.
            // A new member must always have support, even at a former address.
            if (Result.Address != Intent.Cell && Members.Contains(Result.Address) && bExistingEpisode) { continue; }
            switch (Result.Reason)
            {
            case ESFConstructionSupportReason::NoAnchorPath: return TEXT("Unsupported: no positive-area path to the development foundation");
            case ESFConstructionSupportReason::Reach: return TEXT("Unsupported: span or height limit; add a support column");
            case ESFConstructionSupportReason::AggregateCapacity:
            case ESFConstructionSupportReason::ContactCapacity: return TEXT("Unsupported: load/contact capacity; add support or shorten the span");
            default: return TEXT("Unsupported: dependent support path failed; add support");
            }
        }
        bStrained |= Result.State == ESFConstructionSupportState::Strained;
    }
    for (const auto& Door : DoorResults)
    {
        if (Door.State != ESFConstructionSupportState::Unsupported) { continue; }
        double Deadline;
        if (DoorInstability.GetDeadline(Door.OriginCell,Deadline)) { continue; }
        if (!Intent.bRemove) { return TEXT("Placement overloads a retained door attachment"); }
        ++NewlyUnstableDoors;
    }
    if ((NewlyUnstableCount > 0 || NewlyUnstableDoors > 0) && !Intent.bConfirmSupportLoss)
    { return FString::Printf(TEXT("Removal destabilizes %d pieces and %d doors; explicit confirmation required"), NewlyUnstableCount,NewlyUnstableDoors); }
    if (SupportHint)
    { *SupportHint = UnstableCount > 0 ? FString::Printf(TEXT("UNSTABLE: %d existing pieces in grace; new piece supported"), UnstableCount)
        : bStrained ? TEXT("STRAINED support - shorten span or add a column") : TEXT("Stable support (rough-timber-v1; synthetic foundation)"); }
    return FString();
}

FString ASFConstructionSandboxState::BuildLineCandidate(const ASFConstructionSandboxController* Controller,
    const FSFConstructionLineIntent& Intent, TMap<FIntVector, FSFConstructionSandboxMember>& Candidate,
    TArray<FIntVector>& Cells, FString* SupportHint) const
{
    if (SupportHint) { SupportHint->Reset(); }
    const FString OwnerError = ValidateOwner(Controller,Intent.ExpectedRevision);
    if (!OwnerError.IsEmpty()) { return OwnerError; }
    if (!SFConstructionLine::Enumerate(Intent.StartCell,Intent.EndCell,MaximumLineCells,Cells))
    { return TEXT("Line must follow one grid axis within the captured cell limit"); }
    if (static_cast<int64>(Members.Num()) + Cells.Num() > MaximumMembers)
    { return TEXT("Development cell limit reached by whole line"); }
    Candidate = Members;
    for (const auto& Cell : Cells)
    {
        FSFConstructionSandboxIntent Single;
        Single.Cell = Cell; Single.Shape = Intent.Shape; Single.Rotation = Intent.Rotation; Single.ExpectedRevision = Intent.ExpectedRevision;
        const FString Error = ValidateIntent(Controller,Single);
        if (!Error.IsEmpty()) { return FString::Printf(TEXT("Line cell %s: %s"),*Cell.ToString(),*Error); }
        FSFConstructionSandboxMember Added; Added.Shape = Intent.Shape; Added.Rotation = Intent.Rotation;
        Candidate.Add(Cell,Added);
    }
    TArray<FSFConstructionSupportResult> Support;
    TArray<FSFConstructionDoorSupportResult> DoorSupport;
    const FString Error = SolveCombined(Candidate,Doors,Support,DoorSupport);
    if (!Error.IsEmpty()) { return Error; }
    int32 ExistingUnstable = 0;
    int32 ExistingUnstableDoors = 0;
    bool bStrained = false;
    for (const auto& Entry : Support)
    {
        if (Entry.State == ESFConstructionSupportState::Unsupported)
        {
            double Deadline;
            if (!Members.Contains(Entry.Address) || !Instability.GetDeadline(Entry.Address,Deadline))
            { return TEXT("Line unsupported or overloads retained structure; add support or shorten the line"); }
            ++ExistingUnstable;
        }
        bStrained |= Entry.State == ESFConstructionSupportState::Strained;
    }
    for (const auto& Entry : DoorSupport)
    {
        if (Entry.State != ESFConstructionSupportState::Unsupported) { continue; }
        double Deadline;
        if (!DoorInstability.GetDeadline(Entry.OriginCell,Deadline))
        { return TEXT("Line overloads a retained door attachment"); }
        ++ExistingUnstableDoors;
    }
    if (SupportHint)
    { *SupportHint = FString::Printf(TEXT("%s; %d existing structural and %d door grace episodes retained"),
        bStrained ? TEXT("Strained support") : TEXT("Stable line support"),ExistingUnstable,ExistingUnstableDoors); }
    return FString();
}

FString ASFConstructionSandboxState::EvaluateLine(const ASFConstructionSandboxController* Controller,
    const FSFConstructionLineIntent& Intent, FString* SupportHint) const
{
    TMap<FIntVector, FSFConstructionSandboxMember> Candidate;
    TArray<FIntVector> Cells;
    return BuildLineCandidate(Controller,Intent,Candidate,Cells,SupportHint);
}

bool ASFConstructionSandboxState::ApplyLine(ASFConstructionSandboxController* Controller,
    const FSFConstructionLineIntent& Intent, FString& Result)
{
    TMap<FIntVector, FSFConstructionSandboxMember> Candidate;
    TArray<FIntVector> Cells;
    FString SupportHint;
    Result = BuildLineCandidate(Controller,Intent,Candidate,Cells,&SupportHint);
    if (!Result.IsEmpty()) { return false; }
    if (!CommitCombined(MoveTemp(Candidate),Doors,Cells,AuthorityTime(),Result)) { return false; }
    Controller->CancelRemoval();
    Result = FString::Printf(TEXT("Placed %d cells atomically; %s"),Cells.Num(),*SupportHint);
    return true;
}

bool ASFConstructionSandboxState::Apply(ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent, FString& Result)
{
    Result = Evaluate(Controller, Intent);
    if (!Result.IsEmpty()) { return false; }
    auto Candidate = Members;
    if (Intent.bRemove) { Candidate.Remove(Intent.Cell); }
    else
    {
        FSFConstructionSandboxMember NewMember;
        NewMember.Shape = Intent.Shape; NewMember.Rotation = Intent.Rotation;
        Candidate.Add(Intent.Cell, NewMember);
    }
    if (!CommitCandidate(MoveTemp(Candidate), {Intent.Cell}, AuthorityTime(), Result)) { return false; }
    Result = Intent.bRemove ? TEXT("Removed; unstable survivors have individual grace deadlines; economic salvage disabled")
        : TEXT("Placed; rough-timber support checked");
    return true;
}

bool ASFConstructionSandboxState::PreviewRemoval(ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent, TArray<FIntVector>& Affected, FString& Result, FSFConstructionRemovalConsequences* Consequences)
{
    Result = ValidateIntent(Controller, Intent);
    if (!Result.IsEmpty()) { return false; }
    if (!Intent.bRemove || Intent.bConfirmSupportLoss) { Result = TEXT("Removal preview requires unconfirmed removal intent"); return false; }
    auto Candidate = Members;
    Candidate.Remove(Intent.Cell);
    TArray<FSFConstructionSupportResult> Results;
    TArray<FSFConstructionDoorSupportResult> DoorResults;
    Result = SolveCombined(Candidate,Doors,Results,DoorResults);
    if (!Result.IsEmpty()) { return false; }
    Affected.Reset();
    FSFConstructionRemovalConsequences Distinct;
    for (const auto& Support : Results)
    {
        double Deadline;
        if (Support.State == ESFConstructionSupportState::Unsupported && !Instability.GetDeadline(Support.Address, Deadline))
        { Affected.AddUnique(Support.Address); Distinct.StructuralCells.Add(Support.Address); }
    }
    for (const auto& Door : DoorResults)
    {
        double Deadline;
        if (Door.State == ESFConstructionSupportState::Unsupported && !DoorInstability.GetDeadline(Door.OriginCell,Deadline))
        { Affected.AddUnique(Door.OriginCell); Distinct.DoorOrigins.Add(Door.OriginCell); }
    }
    if (Consequences) { *Consequences = Distinct; }
    ConfirmationController.Reset();
    if (!Affected.IsEmpty())
    {
        ConfirmationController = Controller;
        ConfirmationCell = Intent.Cell;
        ConfirmationRevision = Revision;
        Result = FString::Printf(TEXT("Remove %s: %d structural pieces and %d doors lose support. Enter confirms this revision; Esc cancels."),
            *Intent.Cell.ToString(),Distinct.StructuralCells.Num(),Distinct.DoorOrigins.Num());
    }
    return true;
}

void ASFConstructionSandboxState::CancelRemoval(const ASFConstructionSandboxController* Controller)
{
    if (ConfirmationController.Get() == Controller) { ConfirmationController.Reset(); }
}

double ASFConstructionSandboxState::AuthorityTime() const
{
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    if (TestTimeOverride >= 0.0) { return TestTimeOverride; }
#endif
    return GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
}

bool ASFConstructionSandboxState::CommitCandidate(TMap<FIntVector, FSFConstructionSandboxMember> Candidate, const TArray<FIntVector>& ChangedCells, double Now, FString& Result)
{
    return CommitCombined(MoveTemp(Candidate),Doors,ChangedCells,Now,Result);
}

bool ASFConstructionSandboxState::CommitCombined(TMap<FIntVector, FSFConstructionSandboxMember> Candidate,
    TMap<FIntVector, FSFConstructionSandboxDoor> CandidateDoors, const TArray<FIntVector>& ChangedCells,
    double Now, FString& Result, const FSFConstructionDoorPush* Push)
{
    Result = ValidateEnvironment();
    if (!Result.IsEmpty()) { return false; }
    if (Revision == MAX_uint64) { Result = TEXT("Revision exhausted; restart fixture"); return false; }
    TArray<FSFConstructionSupportResult> Support;
    TArray<FSFConstructionDoorSupportResult> DoorSupport;
    Result = SolveCombined(Candidate,CandidateDoors,Support,DoorSupport);
    if (!Result.IsEmpty()) { return false; }
    TSet<FIntVector> Unsupported;
    for (const auto& Entry : Support)
    { if (Entry.State == ESFConstructionSupportState::Unsupported) { Unsupported.Add(Entry.Address); } }
    auto NextInstability = Instability;
    TSet<FIntVector> UnsupportedDoors;
    for (const auto& Entry : DoorSupport)
    {
        CandidateDoors.FindChecked(Entry.OriginCell).Support = Entry.State;
        if (Entry.State == ESFConstructionSupportState::Unsupported) { UnsupportedDoors.Add(Entry.OriginCell); }
    }
    auto NextDoorInstability = DoorInstability;
    if (!NextInstability.Refresh(Unsupported, Now, GraceSeconds) || !NextDoorInstability.Refresh(UnsupportedDoors,Now,GraceSeconds))
    { Result = TEXT("Invalid support lifecycle clock; construction preserved"); return false; }
    const uint64 NextRevision = Revision + 1;
    TArray<ASFConstructionSandboxPreview*> Added;
    TArray<ASFConstructionSandboxPreview*> Prepared;
    TArray<ASFConstructionSandboxDoorProjection*> StagedDoors;
    auto Rollback = [&]()
    {
        for (auto* Staged : Prepared) { Staged->DiscardPrepared(); }
        for (auto* NewActor : Added) { NewActor->Destroy(); }
        for (auto* Door : StagedDoors) { Door->Destroy(); }
    };
    for (auto& Pair : CandidateDoors)
    {
        const auto* Old = Doors.Find(Pair.Key);
        if (Old && Old->bOpen == Pair.Value.bOpen && Old->YawQuarterTurns == Pair.Value.YawQuarterTurns) { continue; }
        auto* Door = GetWorld()->SpawnActor<ASFConstructionSandboxDoorProjection>();
        if (Door) { StagedDoors.Add(Door); }
        if (!Door || !Door->Configure(Pair.Key,Pair.Value.YawQuarterTurns,Pair.Value.bOpen))
        { Rollback(); Result = TEXT("Door projection staging failed; construction preserved"); return false; }
        Pair.Value.Projection = Door;
    }
    for (auto& Pair : Candidate)
    {
        if (const auto* Old = Members.Find(Pair.Key))
        {
            if (Old->Shape != Pair.Value.Shape || Old->Rotation != Pair.Value.Rotation) { Pair.Value.Generation = NextRevision; }
            continue;
        }
        auto* Projection = GetWorld()->SpawnActor<ASFConstructionSandboxPreview>(CellCorner(Pair.Key), FRotator::ZeroRotator);
        if (!Projection)
        {
            Rollback();
            Result = TEXT("Unable to stage projection"); return false;
        }
        Pair.Value.Projection = Projection;
        Pair.Value.Generation = NextRevision;
        Added.Add(Projection);
    }
    TArray<FIntVector> Affected;
    for (const FIntVector& Changed : ChangedCells)
    {
        Affected.AddUnique(Changed);
        for (const FIntVector& Delta : Adjacent) { Affected.AddUnique(Changed + Delta); }
    }
    for (const auto& Cell : Affected)
    {
        auto* Member = Candidate.Find(Cell);
        if (!Member) { continue; }
        FSFConstructionNeighbors Neighbors;
        for (int32 Face = 0; Face < 6; ++Face)
        {
            if (const auto* Neighbor = Candidate.Find(Cell + Adjacent[Face]))
            {
                Neighbors.Faces[Face].Shape = Neighbor->Shape;
                Neighbors.Faces[Face].Rotation = Neighbor->Rotation;
                Neighbors.Faces[Face].Generation = Neighbor->Generation;
            }
        }
        auto* Projection = Member->Projection.Get();
        if (!Projection || !Projection->PrepareRebuild(Member->Shape, Member->Rotation, NextRevision, Neighbors))
        {
            Rollback();
            Result = TEXT("Projection staging failed; construction preserved"); return false;
        }
        Prepared.Add(Projection);
        Member->ProjectionGeneration = NextRevision;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
        if (TestFailAfterPreparedCount >= 1 && Prepared.Num() == TestFailAfterPreparedCount)
        { Rollback(); Result = TEXT("Editor fixture interrupted after prepared projection; construction preserved"); return false; }
#endif
    }
    for (auto* Projection : Prepared)
    {
        if (!Projection->CanActivatePrepared())
        {
            Rollback();
            Result = TEXT("Projection activation preflight failed; construction preserved"); return false;
        }
    }
    for (auto* Door : StagedDoors)
    {
        if (!Door->CanActivate()) { Rollback(); Result = TEXT("Door activation preflight failed; construction preserved"); return false; }
    }
    auto Publish = [&]() -> bool
    {
    Result = ValidateEnvironment();
    if (!Result.IsEmpty()) { return false; }
    // No callbacks/yields or fallible cooking between preflight and activation.
    // Treat an unexpected activation failure as an invariant violation rather
    // than publishing a member revision over partially installed geometry.
    for (auto* Projection : Prepared)
    {
        const bool bActivated = Projection->ActivatePrepared();
        checkf(bActivated, TEXT("Construction activation changed after synchronous preflight"));
        if (!bActivated) { UE_LOG(LogTemp, Fatal, TEXT("Construction activation invariant failed")); }
    }
    for (auto* Door : StagedDoors)
    {
        if (!Door->Activate()) { UE_LOG(LogTemp,Fatal,TEXT("Door activation invariant changed after synchronous preflight")); }
    }
    for (const auto& Pair : Members)
    { if (!Candidate.Contains(Pair.Key) && Pair.Value.Projection.IsValid()) { Pair.Value.Projection->Destroy(); } }
    for (const auto& Pair : Doors)
    {
        const auto* Retained = CandidateDoors.Find(Pair.Key);
        if ((!Retained || Retained->Projection != Pair.Value.Projection) && Pair.Value.Projection.IsValid()) { Pair.Value.Projection->Destroy(); }
    }
    Members = MoveTemp(Candidate);
    Doors = MoveTemp(CandidateDoors);
    Instability = MoveTemp(NextInstability);
    DoorInstability = MoveTemp(NextDoorInstability);
    Revision = NextRevision;
    NextCollapseRetryAt = 0.0;
    LastCollapseFailure.Reset();
    ConfirmationController.Reset();
    return true;
    };
    if (!Push)
    {
        if (!Publish()) { Rollback(); return false; }
        return true;
    }
    auto* Capsule = Push->Capsule.Get();
    auto* OldDoor = Push->Door.Get();
    auto* Character = Capsule ? Cast<ASFE1Character>(Capsule->GetOwner()) : nullptr;
    auto* CharacterMovement = Character ? Character->GetCharacterMovement() : nullptr;
    const auto* Current = OldDoor ? Doors.Find(OldDoor->GetOriginCell()) : nullptr;
    if (!Capsule || !CharacterMovement || CharacterMovement->UpdatedComponent != Capsule
        || !Current || Current->Projection.Get() != OldDoor || !Current->bOpen
        || Push->Revision != Revision || !Capsule->GetComponentLocation().Equals(Push->From,0.001))
    { Rollback(); Result = TEXT("Door push context changed; construction preserved"); return false; }
    {
        FScopedMovementUpdate Movement(Capsule,EScopedUpdate::DeferredUpdates);
        FHitResult Hit;
        const bool bMoved = Capsule->MoveComponent(Push->To-Push->From,Capsule->GetComponentQuat(),true,&Hit);
        if (!bMoved || Hit.bBlockingHit || Hit.bStartPenetrating
            || !Capsule->GetComponentLocation().Equals(Push->To,0.01))
        {
            Movement.RevertMove(); Rollback();
            Result = TEXT("Door push sweep blocked; construction and character preserved"); return false;
        }
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
        if (bTestFailAfterDoorPush)
        {
            Movement.RevertMove(); Rollback();
            Result = TEXT("Editor fixture interrupted staged door after reversible push"); return false;
        }
#endif
        if (!Publish()) { Movement.RevertMove(); Rollback(); return false; }
        // Direct capsule displacement leaves CharacterMovement's cached floor
        // at the old pose; schedule its normal next floor check after success.
        CharacterMovement->bForceNextFloorCheck = true;
    } // Deferred movement callbacks observe the published door and revision.
    return true;
}

void ASFConstructionSandboxState::ProcessDeadlines(double Now)
{
    if ((Instability.Num() == 0 && DoorInstability.Num() == 0) || !FMath::IsFinite(Now) || Now < NextCollapseRetryAt) { return; }
    TArray<FIntVector> Due, DueDoors;
    if (!Instability.Due(Now,Due) || !DoorInstability.Due(Now,DueDoors) || (Due.IsEmpty() && DueDoors.IsEmpty())) { return; }
    if (!ValidateEnvironment().IsEmpty()) { NextCollapseRetryAt = Now + RetrySeconds; return; }
    TArray<FSFConstructionSupportResult> Support;
    TArray<FSFConstructionDoorSupportResult> DoorSupport;
    if (!SolveCombined(Members,Doors,Support,DoorSupport).IsEmpty()) { NextCollapseRetryAt = Now + RetrySeconds; return; }
    TSet<FIntVector> Unsupported, UnsupportedDoors;
    for (const auto& Entry : Support)
    { if (Entry.State == ESFConstructionSupportState::Unsupported) { Unsupported.Add(Entry.Address); } }
    for (const auto& Entry : DoorSupport)
    { if (Entry.State == ESFConstructionSupportState::Unsupported) { UnsupportedDoors.Add(Entry.OriginCell); } }
    auto Candidate = Members;
    auto CandidateDoors = Doors;
    TArray<FIntVector> Removed;
    int32 RemovedDoors = 0;
    for (const auto& Cell : Due)
    { if (Unsupported.Contains(Cell) && Candidate.Remove(Cell) > 0) { Removed.Add(Cell); } }
    for (const auto& Origin : DueDoors)
    { if (UnsupportedDoors.Contains(Origin)) { RemovedDoors += CandidateDoors.Remove(Origin); } }
    if (Removed.IsEmpty() && RemovedDoors == 0)
    {
        auto Recovered = Instability;
        auto RecoveredDoors = DoorInstability;
        if (Recovered.Refresh(Unsupported,Now,GraceSeconds) && RecoveredDoors.Refresh(UnsupportedDoors,Now,GraceSeconds))
        { Instability = MoveTemp(Recovered); DoorInstability = MoveTemp(RecoveredDoors); }
        return;
    }
    FString Result;
    if (CommitCombined(MoveTemp(Candidate),MoveTemp(CandidateDoors),Removed,Now,Result))
    { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_COLLAPSE Members=%d Doors=%d Revision=%llu EconomicSalvageDisabled=1 RubbleCollision=0"),Removed.Num(),RemovedDoors,Revision); }
    else
    {
        NextCollapseRetryAt = Now + RetrySeconds;
        if (LastCollapseFailureRevision != Revision || LastCollapseFailure != Result)
        { UE_LOG(LogTemp,Warning,TEXT("CONSTRUCTION_COLLAPSE_RETRY DeadlinesPreserved=1 Reason=%s"),*Result); }
        LastCollapseFailure = Result;
        LastCollapseFailureRevision = Revision;
    }
}
void ASFConstructionSandboxState::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // The only periodic work is checking pending deadlines. No arbitrary support
    // rescan: this fixture has no damage or terrain mutation route.
    if (Instability.Num() > 0 || DoorInstability.Num() > 0) { ProcessDeadlines(AuthorityTime()); }
}

FString ASFConstructionSandboxState::GetInstabilitySummary() const
{
    double NextDeadline;
    double DoorDeadline;
    const bool bStructural = Instability.GetNextDeadline(NextDeadline);
    const bool bDoors = DoorInstability.GetNextDeadline(DoorDeadline);
    if (!bStructural && !bDoors) { return TEXT("Support stable/strained; no unstable members or doors"); }
    if (bDoors) { NextDeadline = bStructural ? FMath::Min(NextDeadline,DoorDeadline) : DoorDeadline; }
    if (AuthorityTime() >= NextDeadline)
    {
        return FString::Printf(TEXT("UNSTABLE: %d pieces / %d doors | collapse pending; edits blocked until authoritative wave/retry resolves | economic salvage disabled"), Instability.Num(),DoorInstability.Num());
    }
    return FString::Printf(TEXT("UNSTABLE: %d pieces / %d doors | next collapse in %.1f s | add supported columns to recover | economic salvage disabled"),
        Instability.Num(),DoorInstability.Num(),FMath::Max(0.0, NextDeadline - AuthorityTime()));
}

void ASFConstructionSandboxState::EndPlay(const EEndPlayReason::Type Reason)
{
    for (const auto& Pair : Members) { if (Pair.Value.Projection.IsValid()) { Pair.Value.Projection->Destroy(); } }
    Members.Reset();
    for (const auto& Pair : Doors) { if (Pair.Value.Projection.IsValid()) { Pair.Value.Projection->Destroy(); } }
    Doors.Reset();
    Super::EndPlay(Reason);
}
