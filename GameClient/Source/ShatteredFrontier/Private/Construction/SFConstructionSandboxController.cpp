#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionGeometry.h"
#include "Components/InputComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

void ASFConstructionSandboxController::BeginPlay()
{
    Super::BeginPlay();
    Rotation = SFConstructionGeometry::IdentityRotation();
    if (!HasAuthority() || !IsLocalController() || GetNetMode() != NM_Standalone
        || !GetWorld()->GetAuthGameMode<ASFConstructionSandboxGameMode>()) { return; }
    for (TActorIterator<ASFConstructionSandboxState> It(GetWorld()); It; ++It)
    { if (IsValid(*It) && !It->IsActorBeingDestroyed()) { State = *It; break; } }
    if (!State.IsValid()) { State = GetWorld()->SpawnActor<ASFConstructionSandboxState>(); }
}

void ASFConstructionSandboxController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (!InputComponent) { return; }
    InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ThisClass::ToggleBuild);
    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ThisClass::SelectCube);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ThisClass::SelectSlab);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ThisClass::SelectSlope);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::Rotate);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ThisClass::Place);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ThisClass::Remove);
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ThisClass::ConfirmRemoval);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::CancelRemoval);
}

void ASFConstructionSandboxController::ToggleBuild() { CancelRemoval(); bBuilding = !bBuilding; }
void ASFConstructionSandboxController::SelectCube() { CancelRemoval(); Shape = 0; }
void ASFConstructionSandboxController::SelectSlab() { CancelRemoval(); Shape = 1; }
void ASFConstructionSandboxController::SelectSlope() { CancelRemoval(); Shape = 2; }
void ASFConstructionSandboxController::Rotate() { CancelRemoval(); Rotation = (Rotation + 1) % SFConstructionGeometry::RotationCount; }
void ASFConstructionSandboxController::Place() { Commit(false); }
void ASFConstructionSandboxController::Remove() { Commit(true); }

void ASFConstructionSandboxController::CancelRemoval()
{
    if (State.IsValid()) { State->CancelRemoval(this); }
    bPendingRemoval = false;
    RemovalPrompt.Reset();
    // Clearing a selection also invalidates sampled edits until the next tick.
    bHasPreview = false;
    bHasRemoval = false;
}

void ASFConstructionSandboxController::ConfirmRemoval()
{
    if (!bBuilding || !State.IsValid() || !bPendingRemoval) { return; }
    // Send the recorded cell/revision exactly. Never refresh it on Enter.
    State->Apply(this, PendingRemoval, LastResult);
    CancelRemoval();
}

void ASFConstructionSandboxController::Commit(bool bRemove)
{
    if (!bBuilding || !State.IsValid() || (!bRemove && !bHasPreview) || (bRemove && !bHasRemoval)) { return; }
    auto Intent = PreviewIntent;
    Intent.bRemove = bRemove;
    if (bRemove)
    {
        Intent.Cell = RemovalCell;
        CancelRemoval();
        TArray<FIntVector> Affected;
        if (!State->PreviewRemoval(this, Intent, Affected, LastResult)) { return; }
        bPendingRemoval = !Affected.IsEmpty();
        if (bPendingRemoval)
        {
            PendingRemoval = Intent;
            PendingRemoval.bConfirmSupportLoss = true;
            RemovalPrompt = LastResult;
            for (const auto& Cell : Affected)
            {
                RemovalPrompt += TEXT(" ") + Cell.ToString();
                if (RemovalPrompt.Len() > 500) { RemovalPrompt += TEXT(" ..."); break; }
            }
            return;
        }
    }
    else { CancelRemoval(); }
    State->Apply(this, Intent, LastResult);
    // Never retry automatically: the next tick refreshes the visible revision.
}

void ASFConstructionSandboxController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsLocalController() || !State.IsValid()) { return; }
    if (bPendingRemoval && PendingRemoval.ExpectedRevision != State->GetRevision())
    { CancelRemoval(); LastResult = TEXT("Removal preview stale after construction changed; preview again"); }
    bHasPreview = false;
    bHasRemoval = false;
    FString Advisory = bBuilding ? TEXT("Aim at the floor or an editable member") : TEXT("Build controls paused");
    const APawn* ControlledPawn = GetPawn();
    if (bBuilding && ControlledPawn)
    {
        FVector Eyes; FRotator View;
        GetPlayerViewPoint(Eyes, View);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionGhost), false, ControlledPawn);
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Eyes + View.Vector() * State->GetReachCentimetres(), ECC_Visibility, Query))
        {
            const FVector Point = Hit.ImpactPoint + Hit.ImpactNormal * 0.5;
            const double CellSize = SFConstructionGeometry::CellCentimetres;
            const FVector Snapped = Point / CellSize;
            if (!Snapped.ContainsNaN() && Snapped.GetAbsMax() < 100000)
            {
                PreviewIntent.Cell = FIntVector(FMath::FloorToInt(Snapped.X), FMath::FloorToInt(Snapped.Y), FMath::FloorToInt(Snapped.Z));
                bHasRemoval = State->FindCell(Hit.GetActor(), RemovalCell);
                if (bHasRemoval)
                {
                    // Partial faces can lie inside their addressed cell. Select
                    // the adjacent cell along the dominant face normal instead
                    // of accidentally requesting complementary same-cell solids.
                    const FVector N = Hit.ImpactNormal;
                    const FVector A = N.GetAbs();
                    FIntVector Step = FIntVector::ZeroValue;
                    if (A.Z >= A.X && A.Z >= A.Y) { Step.Z = N.Z >= 0 ? 1 : -1; }
                    else if (A.X >= A.Y) { Step.X = N.X >= 0 ? 1 : -1; }
                    else { Step.Y = N.Y >= 0 ? 1 : -1; }
                    PreviewIntent.Cell = RemovalCell + Step;
                }
                PreviewIntent.Shape = Shape;
                PreviewIntent.Rotation = Rotation;
                PreviewIntent.ExpectedRevision = State->GetRevision();
                PreviewIntent.bRemove = false;
                bHasPreview = true;
                FString SupportHint;
                Advisory = State->Evaluate(this, PreviewIntent, &SupportHint);
                const bool bAllowed = Advisory.IsEmpty();
                if (bAllowed) { Advisory = TEXT("Placement allowed | ") + SupportHint; }
                FSFConstructionSolid Solid;
                if (SFConstructionGeometry::Build(Shape, Rotation, Solid))
                {
                    const FVector Corner = ASFConstructionSandboxState::CellCorner(PreviewIntent.Cell);
                    for (const auto& Face : Solid.Faces)
                    {
                        for (int32 Edge = 0; Edge < Face.Num(); ++Edge)
                        {
                            DrawDebugLine(GetWorld(), Corner + Solid.Vertices[Face[Edge]], Corner + Solid.Vertices[Face[(Edge + 1) % Face.Num()]],
                                bAllowed ? FColor::Green : FColor::Red, false, -1.0f, 0, 2.0f);
                        }
                    }
                }
            }
        }
    }
    if (GEngine)
    {
        const TCHAR* Shapes[] = { TEXT("Cube"), TEXT("Half slab"), TEXT("Slope") };
        GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()) + (1ull << 32), 0.1f, FColor::Cyan,
            FString::Printf(TEXT("STANDALONE DEVELOPMENT | UNLIMITED STOCK | Fixed synthetic foundation\nSupport-loss grace/collapse | economic salvage disabled | saves, timber art and networking INCOMPLETE\nB build | 1 Cube  2 Slab  3 Slope | R orientation | LMB place | RMB remove | Enter confirm | Esc cancel | WASD/mouse/Space/E retained\n%s | rough-timber-v1 rules; neutral art | orientation %d/24 | snap 0.5 m | %d cells\n%s\n%s\n%s\n%s"),
                Shapes[Shape], Rotation + 1, State->GetMemberCount(), *Advisory, *State->GetInstabilitySummary(), *RemovalPrompt, *LastResult));
    }
}
