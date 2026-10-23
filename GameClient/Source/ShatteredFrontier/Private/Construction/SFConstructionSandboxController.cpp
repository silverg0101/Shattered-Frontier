#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionLine.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Construction/SFConstructionSandboxPalette.h"
#include "Components/InputComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
    SetIgnoreLookInput(true); bOwnsLookSuppression = true;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Legacy finite input fixtures retain deterministic view-derived rays; they
    // exercise the same targeting path without capturing the desktop pointer.
    for (const TCHAR* Flag : {TEXT("ConstructionInputScenario"),TEXT("ConstructionDoorInputScenario"),
        TEXT("ConstructionLineInputScenario"),TEXT("ConstructionSaveRestartWriter"),TEXT("ConstructionSaveRestartReader")})
    { bScenarioCursor |= FParse::Param(FCommandLine::Get(),Flag); }
#endif
    if (GetLocalPlayer() && GetLocalPlayer()->ViewportClient)
    {
        SandboxPalette = USFConstructionSandboxPalette::Create(this);
        if (SandboxPalette)
        {
            SandboxPalette->OnAction.BindUObject(this,&ThisClass::PaletteAction);
            SandboxPalette->AddToViewport(); SandboxPalette->SetPositionInViewport(FVector2D(16,16));
        }
        if (FSlateApplication::IsInitialized())
        { ActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this,&ThisClass::ApplicationActivationChanged); }
    }
    ApplyCursorMode();
}

void ASFConstructionSandboxController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ActivationHandle.IsValid() && FSlateApplication::IsInitialized())
    { FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle); }
    if (SandboxPalette) { SandboxPalette->OnAction.Unbind(); SandboxPalette->RemoveFromParent(); SandboxPalette = nullptr; }
    if (bOwnsLookSuppression) { SetIgnoreLookInput(false); bOwnsLookSuppression = false; }
    Super::EndPlay(Reason);
}

void ASFConstructionSandboxController::ClientRestart_Implementation(APawn* NewPawn)
{
    // Engine restart clears the stacked ignore flags after controller BeginPlay.
    Super::ClientRestart_Implementation(NewPawn);
    bOwnsLookSuppression = false;
    if (State.IsValid())
    {
        bCameraLookHeld = false; CancelRemoval();
        SetIgnoreLookInput(true); bOwnsLookSuppression = true; ApplyCursorMode();
    }
}

bool ASFConstructionSandboxController::CanUseConstructionControls() const
{
    if (!State.IsValid() || !bApplicationActive || bCameraLookHeld) { return false; }
    if (bDispatchingPaletteAction) { return true; }
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bScenarioCursor) { return !bSyntheticPointerOverPalette; }
#endif
    return !SandboxPalette || !SandboxPalette->IsPointerOverPalette();
}

bool ASFConstructionSandboxController::GetConstructionRay(FVector& Start, FVector& Direction) const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bScenarioCursor)
    {
        if (bSyntheticRayOverride) { Start = SyntheticRayStart; Direction = SyntheticRayDirection; }
        else { FRotator View; GetPlayerViewPoint(Start,View); Direction = View.Vector(); }
        return !Start.ContainsNaN() && !Direction.ContainsNaN() && Direction.IsNormalized();
    }
#endif
    float X = 0, Y = 0; int32 Width = 0, Height = 0;
    GetViewportSize(Width,Height);
    return GetMousePosition(X,Y) && FMath::IsFinite(X) && FMath::IsFinite(Y)
        && X >= 0 && Y >= 0 && X < Width && Y < Height
        && DeprojectScreenPositionToWorld(X,Y,Start,Direction)
        && !Start.ContainsNaN() && !Direction.ContainsNaN() && Direction.IsNormalized();
}

bool ASFConstructionSandboxController::CanCommitCursorSample()
{
    if (!CanUseConstructionControls()) { return false; }
    // Viewport-less unit fixtures exercise captured-intent fences directly.
    if (!GetLocalPlayer()
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        && !bScenarioCursor
#endif
        ) { return true; }
    FVector Start, Direction;
    if (!bHasCursorSample || !GetConstructionRay(Start,Direction))
    { LastResult = TEXT("Wait for a current cursor preview"); return false; }
    if (!Start.Equals(SampleRayStart,1.e-4) || !Direction.Equals(SampleRayDirection,1.e-6))
    {
        const uint64 ObservedRevision = bHasLineEndpoint ? LinePreviewIntent.ExpectedRevision : PreviewIntent.ExpectedRevision;
        if (ObservedRevision != State->GetRevision() || (bHasLineSeed && LineSeedRevision != State->GetRevision()))
        { CancelRemoval(); LastResult = TEXT("Construction changed; choose a fresh preview"); return false; }
        // Mouse/view motion between frame sampling and click uses the current
        // ray, but never refreshes a stale assembly revision or retries an edit.
        UpdateConstructionPreview();
        if (!bHasCursorSample) { LastResult = TEXT("Cursor has no available target"); return false; }
    }
    return true;
}

void ASFConstructionSandboxController::ApplyCursorMode()
{
    bShowMouseCursor = !bCameraLookHeld;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bScenarioCursor) { return; }
#endif
    if (!GetLocalPlayer() || !GetLocalPlayer()->ViewportClient) { return; }
    if (bCameraLookHeld)
    { FInputModeGameOnly Mode; Mode.SetConsumeCaptureMouseDown(false); SetInputMode(Mode); }
    else
    {
        FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
    }
}

void ASFConstructionSandboxController::BeginCameraLook()
{
    if (!CanUseConstructionControls()) { return; }
    ClearCameraSample(); bCameraLookHeld = true;
    if (bOwnsLookSuppression) { SetIgnoreLookInput(false); bOwnsLookSuppression = false; }
    ApplyCursorMode();
}
void ASFConstructionSandboxController::EndCameraLook()
{
    if (!bCameraLookHeld) { return; }
    bCameraLookHeld = false; ClearCameraSample();
    if (!bOwnsLookSuppression) { SetIgnoreLookInput(true); bOwnsLookSuppression = true; }
    ApplyCursorMode();
}
void ASFConstructionSandboxController::ClearCameraSample()
{
    // Camera movement invalidates the endpoint, but a current line start keeps
    // its originally captured revision so the player can turn between clicks.
    const bool bKeepSeed = bHasLineSeed && State.IsValid() && LineSeedRevision == State->GetRevision();
    const FIntVector Seed = LineSeed; const uint64 SeedRevision = LineSeedRevision;
    CancelRemoval();
    if (bKeepSeed) { bHasLineSeed = true; LineSeed = Seed; LineSeedRevision = SeedRevision; }
}
void ASFConstructionSandboxController::ApplicationActivationChanged(bool bActive)
{
    bApplicationActive = bActive;
    if (!bActive)
    {
        EndCameraLook(); CancelRemoval();
        if (PlayerInput) { PlayerInput->FlushPressedKeys(); }
        if (GetPawn()) { GetPawn()->ConsumeMovementInputVector(); }
    }
}

void ASFConstructionSandboxController::ToggleDemolition()
{ if (CanUseConstructionControls()) { CancelRemoval(); bDemolitionMode = !bDemolitionMode; } }
void ASFConstructionSandboxController::TogglePlacementPlane()
{ if (CanUseConstructionControls()) { CancelRemoval(); bLayerPlacement = !bLayerPlacement; } }
void ASFConstructionSandboxController::LayerDown()
{ if (CanUseConstructionControls()) { CancelRemoval(); bLayerPlacement = true; PlacementLayer = FMath::Max(0,PlacementLayer-1); } }
void ASFConstructionSandboxController::LayerUp()
{ if (CanUseConstructionControls()) { CancelRemoval(); bLayerPlacement = true; PlacementLayer = FMath::Min(19,PlacementLayer+1); } }
void ASFConstructionSandboxController::PaletteAction(ESFConstructionPaletteAction Action)
{
    if (!bApplicationActive || bCameraLookHeld || !State.IsValid()) { return; }
    TGuardValue<bool> Guard(bDispatchingPaletteAction,true);
    switch (Action)
    {
    case ESFConstructionPaletteAction::Cube: SelectCube(); break;
    case ESFConstructionPaletteAction::Slab: SelectSlab(); break;
    case ESFConstructionPaletteAction::Slope: SelectSlope(); break;
    case ESFConstructionPaletteAction::Door: SelectDoor(); break;
    case ESFConstructionPaletteAction::Single: CancelRemoval(); bLineMode = false; bDemolitionMode = false; break;
    case ESFConstructionPaletteAction::Line: CancelRemoval(); bLineMode = true; bDoorSelected = false; bDemolitionMode = false; break;
    case ESFConstructionPaletteAction::Demolish: ToggleDemolition(); break;
    case ESFConstructionPaletteAction::Rotate: Rotate(); break;
    case ESFConstructionPaletteAction::Surface: CancelRemoval(); bLayerPlacement = false; break;
    case ESFConstructionPaletteAction::Layer: CancelRemoval(); bLayerPlacement = true; break;
    case ESFConstructionPaletteAction::LayerDown: LayerDown(); break;
    case ESFConstructionPaletteAction::LayerUp: LayerUp(); break;
    case ESFConstructionPaletteAction::Confirm: ConfirmRemoval(); break;
    case ESFConstructionPaletteAction::Cancel: CancelRemoval(); break;
    case ESFConstructionPaletteAction::Save: SaveDevelopment(); break;
    case ESFConstructionPaletteAction::Load: LoadDevelopment(); break;
    }
}

void ASFConstructionSandboxController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (!InputComponent) { return; }
    InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ThisClass::ToggleBuild);
    InputComponent->BindKey(EKeys::L, IE_Pressed, this, &ThisClass::ToggleLine);
    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ThisClass::SelectCube);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ThisClass::SelectSlab);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ThisClass::SelectSlope);
    InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ThisClass::SelectDoor);
    // Controller keys sit above the pawn's input component; consume E/F so the pawn's polled IA_Interact cannot dispatch twice.
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ThisClass::Interact);
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ThisClass::Interact);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::Rotate);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ThisClass::Place);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ThisClass::BeginCameraLook);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ThisClass::EndCameraLook);
    InputComponent->BindKey(EKeys::Delete, IE_Pressed, this, &ThisClass::ToggleDemolition);
    InputComponent->BindKey(EKeys::X, IE_Pressed, this, &ThisClass::ToggleDemolition);
    InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ThisClass::TogglePlacementPlane);
    InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &ThisClass::LayerDown);
    InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &ThisClass::LayerUp);
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ThisClass::ConfirmRemoval);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::CancelRemoval);
    InputComponent->BindKey(EKeys::F5,IE_Pressed,this,&ThisClass::SaveDevelopment);
    InputComponent->BindKey(EKeys::F9,IE_Pressed,this,&ThisClass::LoadDevelopment);
}

void ASFConstructionSandboxController::SaveDevelopment()
{
    if (!CanUseConstructionControls()) { return; }
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
    if (!CanUseConstructionControls()) { return; }
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

void ASFConstructionSandboxController::ToggleBuild() { if (CanUseConstructionControls()) { CancelRemoval(); bBuilding = !bBuilding; } }
void ASFConstructionSandboxController::ToggleLine() { if (CanUseConstructionControls()) { CancelRemoval(); bDoorSelected = false; bDemolitionMode = false; bLineMode = !bLineMode; } }
void ASFConstructionSandboxController::SelectCube() { if (CanUseConstructionControls()) { CancelRemoval(); bDemolitionMode = false; bDoorSelected = false; Shape = 0; } }
void ASFConstructionSandboxController::SelectSlab() { if (CanUseConstructionControls()) { CancelRemoval(); bDemolitionMode = false; bDoorSelected = false; Shape = 1; } }
void ASFConstructionSandboxController::SelectSlope() { if (CanUseConstructionControls()) { CancelRemoval(); bDemolitionMode = false; bDoorSelected = false; Shape = 2; } }
void ASFConstructionSandboxController::SelectDoor() { if (CanUseConstructionControls()) { CancelRemoval(); bDemolitionMode = false; bDoorSelected = true; bLineMode = false; } }
void ASFConstructionSandboxController::Rotate()
{
    if (!CanUseConstructionControls()) { return; }
    CancelRemoval();
    if (bDoorSelected) { DoorYaw = (DoorYaw+1)%4; }
    else { Rotation = (Rotation + 1) % SFConstructionGeometry::RotationCount; }
}
void ASFConstructionSandboxController::Place() { if (!CanCommitCursorSample()) { return; } if (bDemolitionMode) { Commit(true); } else if (bLineMode && !bDoorSelected) { CommitLine(); } else { Commit(false); } }
void ASFConstructionSandboxController::Remove() { if (bDemolitionMode && CanCommitCursorSample()) { Commit(true); } }

void ASFConstructionSandboxController::CancelRemoval()
{
    if (State.IsValid()) { State->CancelRemoval(this); }
    bPendingRemoval = false;
    bHasCursorSample = false;
    RemovalPrompt.Reset();
    // Clearing a selection also invalidates sampled edits until the next tick.
    bHasPreview = false;
    bHasRemoval = false;
    bHasDoorPreview = false;
    bHasDoorRemoval = false;
    bHasLineSeed = false;
    bHasLineEndpoint = false;
    bLinePreviewAllowed = false;
    LineGhostCells.Reset();
    LinePreviewReason.Reset();
    bHasCursorSample = false;
}

void ASFConstructionSandboxController::CommitLine()
{
    if (!bBuilding || !State.IsValid()) { return; }
    // Fence before consulting endpoint availability: input can run before Tick.
    if ((bHasLineSeed && LineSeedRevision != State->GetRevision())
        || (bHasLineEndpoint && LinePreviewIntent.ExpectedRevision != State->GetRevision()))
    { CancelRemoval(); LastResult = TEXT("Line preview stale after construction changed; choose a new start"); return; }
    if (!bHasLineEndpoint) { return; }
    const FSFConstructionLineIntent Sample = LinePreviewIntent;
    if (!bHasLineSeed)
    {
        CancelRemoval();
        LineSeed = Sample.EndCell; LineSeedRevision = Sample.ExpectedRevision; bHasLineSeed = true;
        LastResult = TEXT("Line start selected; aim at a straight-axis endpoint and click");
        return;
    }
    if (Sample.ExpectedRevision != LineSeedRevision || Sample.StartCell != LineSeed)
    { CancelRemoval(); LastResult = TEXT("Line samples differ; choose a new start"); return; }
    // Preserve both observed revisions. Authority rejects any intervening edit.
    State->ApplyLine(this,Sample,LastResult);
}

void ASFConstructionSandboxController::Interact()
{
    if (!CanUseConstructionControls()) { return; }
    FVector Start, Direction;
    if (State.IsValid() && GetPawn() && GetConstructionRay(Start,Direction))
    {
        FIntVector Origin; FVector Target;
        const double RayLength = State->GetReachCentimetres()+FVector::Distance(Start,GetPawn()->GetActorLocation());
        if (State->FindDoorAlongRay(this,Start,Start+Direction*RayLength,Origin,&Target))
        {
            const auto* Door = State->FindDoor(Origin);
            CancelRemoval();
            if (!Door) { LastResult = TEXT("Door target changed; aim again"); return; }
            FSFConstructionDoorIntent Intent;
            Intent.OriginCell = Origin; Intent.YawQuarterTurns = Door->YawQuarterTurns;
            Intent.ExpectedRevision = State->GetRevision(); Intent.Action = ESFConstructionDoorAction::Toggle;
            Intent.bHasInteractionPoint = true; Intent.InteractionPoint = Target;
            State->ApplyDoor(this,Intent,LastResult); return;
        }
    }
    Super::InteractWithSelectedTarget();
}

void ASFConstructionSandboxController::ConfirmRemoval()
{
    if (!CanUseConstructionControls()) { return; }
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
        Intent.bHasInteractionPoint = bDoorRemovalUsesVirtualPoint; Intent.InteractionPoint = DoorRemovalPoint;
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
    if (bRemove) { Intent.Cell = RemovalCell; CommitCapturedRemoval(Intent); return; }
    CancelRemoval(); State->Apply(this,Intent,LastResult);
    // Never retry automatically: a new visible preview must capture a new revision.
}

void ASFConstructionSandboxController::CommitCapturedRemoval(FSFConstructionSandboxIntent Intent)
{
    if (!State.IsValid()) { return; }
    CancelRemoval();
    TArray<FIntVector> Affected;
    if (!State->PreviewRemoval(this,Intent,Affected,LastResult)) { return; }
    bPendingRemoval = !Affected.IsEmpty();
    if (bPendingRemoval)
    {
        PendingRemoval = Intent; PendingRemoval.bConfirmSupportLoss = true; RemovalPrompt = LastResult;
        for (const auto& Cell : Affected)
        {
            RemovalPrompt += TEXT(" ")+Cell.ToString();
            if (RemovalPrompt.Len() > 500) { RemovalPrompt += TEXT(" ..."); break; }
        }
        return;
    }
    State->Apply(this,Intent,LastResult);
}

void ASFConstructionSandboxController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateConstructionPreview();
}

void ASFConstructionSandboxController::UpdateConstructionPreview()
{
    if (!IsLocalController() || !State.IsValid()) { return; }
    if (bPendingRemoval && PendingRemoval.ExpectedRevision != State->GetRevision())
    { CancelRemoval(); LastResult = TEXT("Removal preview stale after construction changed; preview again"); }
    if (bHasLineSeed && LineSeedRevision != State->GetRevision())
    { CancelRemoval(); LastResult = TEXT("Line preview stale after construction changed; choose a new start"); }
    bHasPreview = false;
    bHasRemoval = false;
    bHasDoorPreview = false;
    bHasDoorRemoval = false;
    bHasLineEndpoint = false;
    bLinePreviewAllowed = false;
    LineGhostCells.Reset();
    LinePreviewReason.Reset();
    bHasCursorSample = false;
    FString Advisory = bBuilding ? TEXT("Point at the floor or an editable object") : TEXT("Build controls paused; B resumes");
    const APawn* ControlledPawn = GetPawn();
    if (bBuilding && ControlledPawn && CanUseConstructionControls())
    {
        FVector Eyes, Direction;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionGhost), false, ControlledPawn);
        FHitResult Hit;
        const bool bHasRay = GetConstructionRay(Eyes,Direction);
        const double RayLength = bHasRay ? State->GetReachCentimetres()+FVector::Distance(Eyes,ControlledPawn->GetActorLocation()) : 0;
        const bool bHit = bHasRay && GetWorld()->LineTraceSingleByChannel(Hit,Eyes,Eyes+Direction*RayLength,ECC_Visibility,Query);
        FIntVector VirtualDoorOrigin = FIntVector::ZeroValue; FVector VirtualDoorPoint = FVector::ZeroVector;
        const bool bVirtualDoor = bDemolitionMode && bHasRay
            && State->FindDoorAlongRay(this,Eyes,Eyes+Direction*RayLength,VirtualDoorOrigin,&VirtualDoorPoint);
        FVector Point = bVirtualDoor ? VirtualDoorPoint : bHit ? Hit.ImpactPoint+Hit.ImpactNormal*0.5 : FVector::ZeroVector;
        bool bHasTarget = bHit || bVirtualDoor;
        if (bLayerPlacement && !bDemolitionMode)
        {
            bHasTarget = false;
            if (bHasRay && FMath::Abs(Direction.Z) > 1.e-8)
            {
                const double T = (PlacementLayer*SFConstructionGeometry::CellCentimetres-Eyes.Z)/Direction.Z;
                if (T >= 0 && T <= RayLength)
                {
                    Point = Eyes+Direction*T;
                    bHasTarget = !bHit || Hit.Distance >= T-1.e-3;
                    if (bHasTarget) { Point.Z = PlacementLayer*SFConstructionGeometry::CellCentimetres; }
                }
            }
            if (!bHasTarget) { Advisory = TEXT("Selected layer is occluded, behind the cursor or parallel/out of reach"); }
        }
        if (bHasTarget)
        {
            SampleRayStart = Eyes; SampleRayDirection = Direction; bHasCursorSample = true;
            const double CellSize = SFConstructionGeometry::CellCentimetres;
            const FVector Snapped = Point / CellSize;
            if (!Snapped.ContainsNaN() && Snapped.GetAbsMax() < 100000)
            {
                PreviewIntent.Cell = FIntVector(FMath::FloorToInt(Snapped.X), FMath::FloorToInt(Snapped.Y), FMath::FloorToInt(Snapped.Z));
                bHasRemoval = !bVirtualDoor && bHit && State->FindCell(Hit.GetActor(), RemovalCell);
                bHasDoorRemoval = bVirtualDoor || (bHit && State->FindDoorOrigin(Hit.GetActor(),DoorRemovalOrigin));
                bDoorRemovalUsesVirtualPoint = bVirtualDoor;
                if (bVirtualDoor) { DoorRemovalOrigin = VirtualDoorOrigin; DoorRemovalPoint = VirtualDoorPoint; }
                if (bHasRemoval && !bLayerPlacement)
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
                if (bDemolitionMode)
                {
                    Advisory = bHasRemoval || bHasDoorRemoval ? TEXT("LMB removes selected object; support loss requires confirmation") : TEXT("Point at an editable object to demolish");
                    if (bHasRemoval) { DrawDebugBox(GetWorld(),ASFConstructionSandboxState::CellCorner(RemovalCell)+FVector(25),FVector(25),FColor::Orange,false,-1.f,0,2.f); }
                    if (bHasDoorRemoval)
                    {
                        const auto* Door = State->FindDoor(DoorRemovalOrigin);
                        FSFConstructionDoorProjectionProfile Profile;
                        if (Door && ASFConstructionSandboxDoorProjection::BuildProfile(DoorRemovalOrigin,Door->YawQuarterTurns,Door->bOpen,Profile))
                        { DrawDebugBox(GetWorld(),Profile.Envelope.GetCenter(),Profile.Envelope.GetExtent(),FColor::Orange,false,-1.f,0,2.f); }
                    }
                }
                else if (bDoorSelected)
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
                else if (bLineMode)
                {
                    const FIntVector Endpoint = PreviewIntent.Cell;
                    bHasLineEndpoint = Endpoint.X >= -20 && Endpoint.X < 20 && Endpoint.Y >= -20 && Endpoint.Y < 20
                        && Endpoint.Z >= 0 && Endpoint.Z < 20;
                    LinePreviewIntent.StartCell = bHasLineSeed ? LineSeed : Endpoint;
                    LinePreviewIntent.EndCell = Endpoint;
                    LinePreviewIntent.Shape = Shape; LinePreviewIntent.Rotation = Rotation;
                    LinePreviewIntent.ExpectedRevision = PreviewIntent.ExpectedRevision;
                    if (!bHasLineEndpoint) { Advisory = TEXT("Line endpoint outside sandbox plot/height"); }
                    else if (!bHasLineSeed)
                    {
                        LineGhostCells.Add(Endpoint);
                        Advisory = TEXT("LMB selects start; support checked on the complete line");
                    }
                    else if (!SFConstructionLine::Enumerate(LineSeed,Endpoint,State->GetMaximumLineCells(),LineGhostCells))
                    { Advisory = TEXT("Line requires one grid axis and must fit the captured cell cap"); }
                    else
                    {
                        FString SupportHint;
                        Advisory = State->EvaluateLine(this,LinePreviewIntent,&SupportHint);
                        bLinePreviewAllowed = Advisory.IsEmpty();
                        if (bLinePreviewAllowed) { Advisory = TEXT("Complete line allowed | ")+SupportHint; }
                    }
                    LinePreviewReason = Advisory;
                    FSFConstructionSolid Solid;
                    if (SFConstructionGeometry::Build(Shape,Rotation,Solid))
                    {
                        for (const auto& Cell : LineGhostCells)
                        {
                            const FVector Corner = ASFConstructionSandboxState::CellCorner(Cell);
                            for (const auto& Face : Solid.Faces)
                            { for (int32 Edge = 0; Edge < Face.Num(); ++Edge)
                                { DrawDebugLine(GetWorld(),Corner+Solid.Vertices[Face[Edge]],Corner+Solid.Vertices[Face[(Edge+1)%Face.Num()]],
                                    !bHasLineSeed ? FColor::Yellow : bLinePreviewAllowed ? FColor::Green : FColor::Red,false,-1.f,0,2.f); } }
                        }
                    }
                    if (bHasLineSeed && LineGhostCells.IsEmpty())
                    { DrawDebugLine(GetWorld(),ASFConstructionSandboxState::CellCorner(LineSeed)+FVector(25),
                        ASFConstructionSandboxState::CellCorner(Endpoint)+FVector(25),FColor::Red,false,-1.f,0,2.f); }
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
    const TCHAR* Shapes[] = {TEXT("Cube"),TEXT("Half slab"),TEXT("Slope")};
    const FString Status = FString::Printf(TEXT("%s | %s | orientation %d/%d\n%s | 0.5 m grid | layer %d (%.1f m)\n%d cells / %d doors | line cap %d\n%s\n%s\n%s\n%s"),
        bDoorSelected ? TEXT("Ordinary door") : Shapes[Shape],bDemolitionMode ? TEXT("DEMOLISH") : bLineMode ? TEXT("LINE") : TEXT("SINGLE"),
        bDoorSelected ? DoorYaw+1 : Rotation+1,bDoorSelected ? 4 : 24,bLayerPlacement ? TEXT("Horizontal layer") : TEXT("Surface snap"),PlacementLayer,PlacementLayer*0.5,
        State->GetMemberCount(),State->GetDoorCount(),State->GetMaximumLineCells(),*Advisory,*State->GetInstabilitySummary(),*RemovalPrompt,*LastResult);
    if (SandboxPalette) { SandboxPalette->SetStatusText(FText::FromString(Status)); }
    else if (GEngine) { GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID())+(1ull<<32),0.1f,FColor::Cyan,Status); }
}
