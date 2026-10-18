#include "Construction/SFConstructionSandboxState.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Construction/SFConstructionGeometry.h"
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
    PrimaryActorTick.bCanEverTick = false;
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
    bReady = true;
    UE_LOG(LogTemp, Display, TEXT("CONSTRUCTION_TOOLS_READY Standalone=1 PlacementOnly=1 UnlimitedDevelopmentStock=1 Support=0 Save=0 Network=0 ReachMetres=%.1f MaximumMembers=%d"),
        ReachCentimetres / 100.0f, MaximumMembers);
}

FVector ASFConstructionSandboxState::CellCorner(const FIntVector& Cell)
{
    return FVector(Cell) * SFConstructionGeometry::CellCentimetres;
}

bool ASFConstructionSandboxState::FindCell(const AActor* Projection, FIntVector& OutCell) const
{
    for (const auto& Pair : Members)
    {
        if (Pair.Value.Projection.Get() == Projection) { OutCell = Pair.Key; return true; }
    }
    return false;
}

FString ASFConstructionSandboxState::Evaluate(const ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent) const
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
        // Conservative canonical-solid bounds clearance, including actual character
        // capsules and fixture props. Touching shared faces remain legal. Fine
        // partial-solid/world intersection remains a later acceptance gate.
        const FBox Bounds(Solid.Vertices);
        const FVector Extent = (Bounds.GetExtent() - FVector(0.1)).ComponentMax(FVector(0.01));
        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_Pawn);
        Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
        Objects.AddObjectTypesToQuery(ECC_WorldStatic);
        FCollisionQueryParams Clearance(SCENE_QUERY_STAT(ConstructionClearance), false);
        if (GetWorld()->OverlapAnyTestByObjectType(CellCorner(Intent.Cell) + Bounds.GetCenter(), FQuat::Identity,
            Objects, FCollisionShape::MakeBox(Extent), Clearance))
        { return TEXT("Occupied solid, character or prop clearance"); }
    }
    return FString();
}

bool ASFConstructionSandboxState::Apply(ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent, FString& Result)
{
    Result = Evaluate(Controller, Intent);
    if (!Result.IsEmpty()) { return false; }
    const uint64 NextRevision = Revision + 1;
    auto Candidate = Members;
    TWeakObjectPtr<ASFConstructionSandboxPreview> Removed;
    ASFConstructionSandboxPreview* Added = nullptr;
    if (Intent.bRemove)
    { Removed = Candidate.FindChecked(Intent.Cell).Projection; Candidate.Remove(Intent.Cell); }
    else
    {
        Added = GetWorld()->SpawnActor<ASFConstructionSandboxPreview>(CellCorner(Intent.Cell), FRotator::ZeroRotator);
        if (!Added) { Result = TEXT("Unable to stage projection"); return false; }
        FSFConstructionSandboxMember NewMember;
        NewMember.Shape = Intent.Shape; NewMember.Rotation = Intent.Rotation;
        NewMember.Generation = NextRevision; NewMember.Projection = Added;
        Candidate.Add(Intent.Cell, NewMember);
    }
    TArray<ASFConstructionSandboxPreview*> Prepared;
    TArray<FIntVector> Affected = { Intent.Cell };
    for (const FIntVector& Delta : Adjacent) { Affected.Add(Intent.Cell + Delta); }
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
            if (Added) { Added->Destroy(); }
            Result = TEXT("Projection staging failed; construction preserved"); return false;
        }
        Prepared.Add(Projection);
    }
    for (auto* Projection : Prepared)
    {
        if (!Projection->CanActivatePrepared())
        {
            for (auto* Staged : Prepared) { Staged->DiscardPrepared(); }
            if (Added) { Added->Destroy(); }
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
    if (Removed.IsValid()) { Removed->Destroy(); }
    Members = MoveTemp(Candidate);
    Revision = NextRevision;
    Result = Intent.bRemove ? TEXT("Removed (support/collapse not implemented)") : TEXT("Placed (support not validated)");
    return true;
}

void ASFConstructionSandboxState::EndPlay(const EEndPlayReason::Type Reason)
{
    for (const auto& Pair : Members) { if (Pair.Value.Projection.IsValid()) { Pair.Value.Projection->Destroy(); } }
    Members.Reset();
    Super::EndPlay(Reason);
}
