#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
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
    SaveStore = MakeUnique<FSFConstructionSaveStore>();
}

void ASFConstructionSandboxController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (!InputComponent) { return; }
    InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ThisClass::ToggleBuild);
    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ThisClass::SelectCube);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ThisClass::SelectSlab);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ThisClass::SelectSlope);
    InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ThisClass::SelectDoor);
    // Controller input is above the pawn's input component. Consume the same
    // action so its inherited Interact callback cannot dispatch a second time.
    InputComponent->BindAction(TEXT("Interact"),IE_Pressed,this,&ThisClass::Interact).bConsumeInput = true;
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::Rotate);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ThisClass::Place);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ThisClass::Remove);
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ThisClass::ConfirmRemoval);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::CancelRemoval);
    InputComponent->BindKey(EKeys::F5,IE_Pressed,this,&ThisClass::SaveDevelopment);
    InputComponent->BindKey(EKeys::F9,IE_Pressed,this,&ThisClass::LoadDevelopment);
}

void ASFConstructionSandboxController::SaveDevelopment()
{
    FSFConstructionSandboxSnapshot Snapshot;
    uint64 Generation = 0;
    if (!SaveStore || !State.IsValid()) { LastResult = TEXT("Standalone development save unavailable"); }
    else if (State->ExportSnapshot(this,State->GetRevision(),Snapshot,LastResult)
        && SaveStore->Publish(Snapshot,Generation,LastResult))
    {
        LastPublishedGeneration = Generation;
        LastResult = FString::Printf(TEXT("Saved local development generation %llu. %s"),Generation,*LastResult);
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_SAVE_RESULT Success=1 Generation=%llu SourceRevision=%llu Reason=%s"),Generation,Snapshot.SourceRevision,*LastResult);
        return;
    }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_SAVE_RESULT Success=0 Generation=0 Reason=%s"),*LastResult);
}

void ASFConstructionSandboxController::LoadDevelopment()
{
    FSFConstructionSandboxSnapshot Snapshot;
    uint64 Generation = 0;
    if (!SaveStore || !State.IsValid()) { LastResult = TEXT("Standalone development load unavailable"); }
    else
    {
        const uint64 ExpectedRevision = State->GetRevision();
        if (SaveStore->ReadLatest(Snapshot,Generation,LastResult)
            && State->ImportSnapshot(this,ExpectedRevision,Snapshot,LastResult))
        {
            LastLoadedGeneration = Generation; LastLoadedSourceRevision = Snapshot.SourceRevision;
            LastResult = FString::Printf(TEXT("Loaded local development generation %llu; source revision %llu, current revision %llu."),Generation,Snapshot.SourceRevision,State->GetRevision());
            UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_LOAD_RESULT Success=1 Generation=%llu SourceRevision=%llu LiveRevision=%llu"),Generation,Snapshot.SourceRevision,State->GetRevision());
            return;
        }
    }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_LOAD_RESULT Success=0 Generation=0 Reason=%s"),*LastResult);
}

void ASFConstructionSandboxController::ToggleBuild() { CancelRemoval(); bBuilding = !bBuilding; }
void ASFConstructionSandboxController::SelectCube() { CancelRemoval(); bDoorSelected = false; Shape = 0; }
void ASFConstructionSandboxController::SelectSlab() { CancelRemoval(); bDoorSelected = false; Shape = 1; }
void ASFConstructionSandboxController::SelectSlope() { CancelRemoval(); bDoorSelected = false; Shape = 2; }
void ASFConstructionSandboxController::SelectDoor() { CancelRemoval(); bDoorSelected = true; }
void ASFConstructionSandboxController::Rotate()
{
    CancelRemoval();
    if (bDoorSelected) { DoorYaw = (DoorYaw+1)%4; }
    else { Rotation = (Rotation + 1) % SFConstructionGeometry::RotationCount; }
}
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
    bHasDoorPreview = false;
    bHasDoorRemoval = false;
}

void ASFConstructionSandboxController::Interact()
{
    if (State.IsValid() && GetPawn())
    {
        FVector Eyes; FRotator View;
        GetPlayerViewPoint(Eyes,View);
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionDoorInteraction),false,GetPawn());
        if (GetWorld()->LineTraceSingleByChannel(Hit,Eyes,Eyes+View.Vector()*State->GetReachCentimetres(),ECC_Visibility,Query)
            && Cast<ASFConstructionSandboxDoorProjection>(Hit.GetActor()))
        {
            FIntVector Origin;
            CancelRemoval();
            if (!State->FindDoorOrigin(Hit.GetActor(),Origin)) { LastResult = TEXT("No editable door target"); return; }
            const auto* Door = State->FindDoor(Origin);
            if (!Door) { LastResult = TEXT("Door target changed; aim again"); return; }
            FSFConstructionDoorIntent Intent;
            Intent.OriginCell = Origin; Intent.YawQuarterTurns = Door->YawQuarterTurns;
            Intent.ExpectedRevision = State->GetRevision(); Intent.Action = ESFConstructionDoorAction::Toggle;
            State->ApplyDoor(this,Intent,LastResult);
            return;
        }
    }
    Super::InteractWithSelectedTarget();
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
    if (bBuilding && State.IsValid() && bRemove && bHasDoorRemoval)
    {
        const auto* Door = State->FindDoor(DoorRemovalOrigin);
        if (!Door) { CancelRemoval(); LastResult = TEXT("Door target changed; aim again"); return; }
        FSFConstructionDoorIntent Intent;
        Intent.OriginCell = DoorRemovalOrigin; Intent.YawQuarterTurns = Door->YawQuarterTurns;
        Intent.ExpectedRevision = PreviewIntent.ExpectedRevision; Intent.Action = ESFConstructionDoorAction::Remove;
        CancelRemoval(); State->ApplyDoor(this,Intent,LastResult); return;
    }
    if (bDoorSelected && !bRemove)
    {
        if (!bBuilding || !State.IsValid() || !bHasDoorPreview) { return; }
        const auto Intent = DoorPreviewIntent;
        CancelRemoval(); State->ApplyDoor(this,Intent,LastResult); return;
    }
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
    bHasDoorPreview = false;
    bHasDoorRemoval = false;
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
                bHasDoorRemoval = State->FindDoorOrigin(Hit.GetActor(),DoorRemovalOrigin);
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
                if (bDoorSelected)
                {
                    DoorPreviewIntent.OriginCell = PreviewIntent.Cell;
                    DoorPreviewIntent.YawQuarterTurns = DoorYaw;
                    DoorPreviewIntent.ExpectedRevision = PreviewIntent.ExpectedRevision;
                    DoorPreviewIntent.Action = ESFConstructionDoorAction::Place;
                    bHasDoorPreview = true;
                    Advisory = State->EvaluateDoor(this,DoorPreviewIntent);
                    const bool bAllowed = Advisory.IsEmpty();
                    if (bAllowed) { Advisory = TEXT("Door placement allowed; reservations/support checked"); }
                    FSFConstructionDoorProjectionProfile Profile;
                    if (ASFConstructionSandboxDoorProjection::BuildProfile(DoorPreviewIntent.OriginCell,DoorYaw,false,Profile))
                    {
                        for (const auto& Box : Profile.Boxes)
                        { DrawDebugBox(GetWorld(),Profile.Transform.TransformPosition(Box.Centre),Box.Extent,
                            Profile.Transform.GetRotation()*Box.Rotation,bAllowed ? FColor::Green : FColor::Red,false,-1.f,0,2.f); }
                    }
                }
                else
                {
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
    }
    if (GEngine)
    {
        const TCHAR* Shapes[] = { TEXT("Cube"), TEXT("Half slab"), TEXT("Slope") };
        GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()) + (1ull << 32), 0.1f, FColor::Cyan,
            FString::Printf(TEXT("STANDALONE DEVELOPMENT | UNLIMITED STOCK | Fixed synthetic foundation\nSupport-loss grace/collapse | economic salvage disabled | timber art and networking INCOMPLETE\nB build | 1 Cube  2 Slab  3 Slope  4 Door | R orientation | LMB place | RMB targeted remove | Enter confirm | Esc cancel\nE aimed door toggle (also build paused); otherwise E1 probe | WASD/mouse/Space retained | temporary controls\nF5 save / F9 restore entire plot | fixed Saved/ConstructionSandbox development generations; stable assemblies only\n%s | rough-timber-v1 rules; neutral art | orientation %d/%d | snap 0.5 m | %d cells / %d doors\n%s\n%s\n%s\n%s"),
                bDoorSelected ? TEXT("Ordinary door") : Shapes[Shape], bDoorSelected ? DoorYaw+1 : Rotation+1, bDoorSelected ? 4 : 24,
                State->GetMemberCount(),State->GetDoorCount(),*Advisory,*State->GetInstabilitySummary(),*RemovalPrompt,*LastResult));
    }
}
