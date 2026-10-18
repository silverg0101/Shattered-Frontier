#include "Construction/SFConstructionSandboxState.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSupport.h"
#include "Construction/SFConstructionSandboxFoundation.h"
#include "Construction/SFConstructionClearance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace
{
const FIntVector Adjacent[] = { {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1} };
bool Finite(const FVector& P) { return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z); }
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
    GraceSeconds = Settings->SupportLossGraceSeconds;
    RetrySeconds = Settings->CollapseRetrySeconds;
    bReady = true;
    UE_LOG(LogTemp, Display, TEXT("CONSTRUCTION_TOOLS_READY Standalone=1 UnlimitedDevelopmentStock=1 Support=FixedFoundation Collapse=GraceWaves EconomicSalvage=0 Save=0 Network=0 ReachMetres=%.1f MaximumMembers=%d"),
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
        if (!Pair.Value.Projection.IsValid() || Pair.Value.Projection->IsActorBeingDestroyed())
        { return TEXT("Construction projection unavailable; edits blocked pending rebuild"); }
    }
    return FString();
}

FString ASFConstructionSandboxState::ValidateIntent(const ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent) const
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
    if (Intent.ExpectedRevision != Revision) { return TEXT("Construction changed; preview refreshed"); }
    double Deadline;
    if (Instability.GetNextDeadline(Deadline) && AuthorityTime() >= Deadline)
    { return TEXT("Support collapse due; edits wait for the authoritative wave to resolve"); }
    if (Revision == MAX_uint64) { return TEXT("Revision exhausted; restart fixture"); }
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
    if (!SFConstructionSupport::Evaluate(Candidate, FSFConstructionSupportProfile(), Results))
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
    const FString Error = SolveSupport(Candidate, Results);
    if (!Error.IsEmpty()) { return Error; }
    bool bStrained = false;
    int32 UnstableCount = 0;
    int32 NewlyUnstableCount = 0;
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
    if (NewlyUnstableCount > 0 && !Intent.bConfirmSupportLoss)
    { return FString::Printf(TEXT("Removal destabilizes %d pieces; explicit confirmation required"), NewlyUnstableCount); }
    if (SupportHint)
    { *SupportHint = UnstableCount > 0 ? FString::Printf(TEXT("UNSTABLE: %d existing pieces in grace; new piece supported"), UnstableCount)
        : bStrained ? TEXT("STRAINED support - shorten span or add a column") : TEXT("Stable support (rough-timber-v1; synthetic foundation)"); }
    return FString();
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

bool ASFConstructionSandboxState::PreviewRemoval(ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent, TArray<FIntVector>& Affected, FString& Result)
{
    Result = ValidateIntent(Controller, Intent);
    if (!Result.IsEmpty()) { return false; }
    if (!Intent.bRemove || Intent.bConfirmSupportLoss) { Result = TEXT("Removal preview requires unconfirmed removal intent"); return false; }
    auto Candidate = Members;
    Candidate.Remove(Intent.Cell);
    TArray<FSFConstructionSupportResult> Results;
    Result = SolveSupport(Candidate, Results);
    if (!Result.IsEmpty()) { return false; }
    Affected.Reset();
    for (const auto& Support : Results)
    {
        double Deadline;
        if (Support.State == ESFConstructionSupportState::Unsupported && !Instability.GetDeadline(Support.Address, Deadline))
        { Affected.Add(Support.Address); }
    }
    ConfirmationController.Reset();
    if (!Affected.IsEmpty())
    {
        ConfirmationController = Controller;
        ConfirmationCell = Intent.Cell;
        ConfirmationRevision = Revision;
        Result = FString::Printf(TEXT("Remove %s: %d pieces lose support. Enter confirms this revision; Esc cancels."), *Intent.Cell.ToString(), Affected.Num());
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
    Result = ValidateEnvironment();
    if (!Result.IsEmpty()) { return false; }
    if (Revision == MAX_uint64) { Result = TEXT("Revision exhausted; restart fixture"); return false; }
    TArray<FSFConstructionSupportResult> Support;
    Result = SolveSupport(Candidate, Support);
    if (!Result.IsEmpty()) { return false; }
    TSet<FIntVector> Unsupported;
    for (const auto& Entry : Support)
    { if (Entry.State == ESFConstructionSupportState::Unsupported) { Unsupported.Add(Entry.Address); } }
    auto NextInstability = Instability;
    if (!NextInstability.Refresh(Unsupported, Now, GraceSeconds))
    { Result = TEXT("Invalid support lifecycle clock; construction preserved"); return false; }
    const uint64 NextRevision = Revision + 1;
    TArray<ASFConstructionSandboxPreview*> Added;
    for (auto& Pair : Candidate)
    {
        if (Members.Contains(Pair.Key)) { continue; }
        auto* Projection = GetWorld()->SpawnActor<ASFConstructionSandboxPreview>(CellCorner(Pair.Key), FRotator::ZeroRotator);
        if (!Projection)
        {
            for (auto* NewActor : Added) { NewActor->Destroy(); }
            Result = TEXT("Unable to stage projection"); return false;
        }
        Pair.Value.Projection = Projection;
        Pair.Value.Generation = NextRevision;
        Added.Add(Projection);
    }
    TArray<ASFConstructionSandboxPreview*> Prepared;
    TArray<FIntVector> Affected;
    for (const FIntVector& Changed : ChangedCells)
    {
        Affected.AddUnique(Changed);
        for (const FIntVector& Delta : Adjacent) { Affected.AddUnique(Changed + Delta); }
    }
    for (const auto& Cell : Affected)
    {
        const auto* Member = Candidate.Find(Cell);
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
            for (auto* Staged : Prepared) { Staged->DiscardPrepared(); }
            for (auto* NewActor : Added) { NewActor->Destroy(); }
            Result = TEXT("Projection staging failed; construction preserved"); return false;
        }
        Prepared.Add(Projection);
    }
    for (auto* Projection : Prepared)
    {
        if (!Projection->CanActivatePrepared())
        {
            for (auto* Staged : Prepared) { Staged->DiscardPrepared(); }
            for (auto* NewActor : Added) { NewActor->Destroy(); }
            Result = TEXT("Projection activation preflight failed; construction preserved"); return false;
        }
    }
    // No callbacks/yields or fallible cooking between preflight and activation.
    // Treat an unexpected activation failure as an invariant violation rather
    // than publishing a member revision over partially installed geometry.
    for (auto* Projection : Prepared)
    {
        const bool bActivated = Projection->ActivatePrepared();
        checkf(bActivated, TEXT("Construction activation changed after synchronous preflight"));
        if (!bActivated) { UE_LOG(LogTemp, Fatal, TEXT("Construction activation invariant failed")); }
    }
    for (const auto& Pair : Members)
    { if (!Candidate.Contains(Pair.Key) && Pair.Value.Projection.IsValid()) { Pair.Value.Projection->Destroy(); } }
    Members = MoveTemp(Candidate);
    Instability = MoveTemp(NextInstability);
    Revision = NextRevision;
    NextCollapseRetryAt = 0.0;
    LastCollapseFailure.Reset();
    ConfirmationController.Reset();
    return true;
}

void ASFConstructionSandboxState::ProcessDeadlines(double Now)
{
    if (Instability.Num() == 0 || !FMath::IsFinite(Now) || Now < NextCollapseRetryAt) { return; }
    TArray<FIntVector> Due;
    if (!Instability.Due(Now, Due) || Due.IsEmpty()) { return; }
    if (!ValidateEnvironment().IsEmpty()) { NextCollapseRetryAt = Now + RetrySeconds; return; }
    TArray<FSFConstructionSupportResult> Support;
    if (!SolveSupport(Members, Support).IsEmpty()) { return; }
    TSet<FIntVector> Unsupported;
    for (const auto& Entry : Support)
    { if (Entry.State == ESFConstructionSupportState::Unsupported) { Unsupported.Add(Entry.Address); } }
    auto Candidate = Members;
    TArray<FIntVector> Removed;
    for (const auto& Cell : Due)
    { if (Unsupported.Contains(Cell) && Candidate.Remove(Cell) > 0) { Removed.Add(Cell); } }
    if (Removed.IsEmpty())
    {
        auto Recovered = Instability;
        if (Recovered.Refresh(Unsupported, Now, GraceSeconds)) { Instability = MoveTemp(Recovered); }
        return;
    }
    FString Result;
    if (CommitCandidate(MoveTemp(Candidate), Removed, Now, Result))
    { UE_LOG(LogTemp, Display, TEXT("CONSTRUCTION_COLLAPSE Members=%d Revision=%llu EconomicSalvageDisabled=1 RubbleCollision=0"), Removed.Num(), Revision); }
    else
    {
        NextCollapseRetryAt = Now + RetrySeconds;
        if (LastCollapseFailureRevision != Revision || LastCollapseFailure != Result)
        { UE_LOG(LogTemp, Warning, TEXT("CONSTRUCTION_COLLAPSE_RETRY DeadlinesPreserved=1 Reason=%s"), *Result); }
        LastCollapseFailure = Result;
        LastCollapseFailureRevision = Revision;
    }
}

void ASFConstructionSandboxState::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // The only periodic work is checking pending deadlines. No arbitrary support
    // rescan: this fixture has no damage or terrain mutation route.
    if (Instability.Num() > 0) { ProcessDeadlines(AuthorityTime()); }
}

FString ASFConstructionSandboxState::GetInstabilitySummary() const
{
    double NextDeadline;
    if (!Instability.GetNextDeadline(NextDeadline)) { return TEXT("Support stable/strained; no unstable members"); }
    if (AuthorityTime() >= NextDeadline)
    {
        return FString::Printf(TEXT("UNSTABLE: %d pieces | collapse pending; edits blocked until authoritative wave/retry resolves | economic salvage disabled"), Instability.Num());
    }
    return FString::Printf(TEXT("UNSTABLE: %d pieces | next collapse in %.1f s | add supported columns to recover | economic salvage disabled"),
        Instability.Num(), FMath::Max(0.0, NextDeadline - AuthorityTime()));
}

void ASFConstructionSandboxState::EndPlay(const EEndPlayReason::Type Reason)
{
    for (const auto& Pair : Members) { if (Pair.Value.Projection.IsValid()) { Pair.Value.Projection->Destroy(); } }
    Members.Reset();
    Super::EndPlay(Reason);
}
