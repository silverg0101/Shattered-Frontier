#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandboxPalette.h"
#include "Construction/SFConstructionGeometry.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionCursorControlsTest,"ShatteredFrontier.Construction.CursorControls.RayCameraDepthAndFocus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionCursorControlsTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
    UWorld* World = Fixture.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = ASFConstructionSandboxGameMode::StaticClass();
    if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Floor = World->SpawnActor<AStaticMeshActor>();
    if (!Floor) { return false; }
    Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Floor->SetActorLocation(FVector(0,0,-50)); Floor->SetActorScale3D(FVector(20,20,1));
    Floor->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
    auto* Controller = World->SpawnActorDeferred<ASFConstructionSandboxController>(ASFConstructionSandboxController::StaticClass(),FTransform::Identity);
    if (!Controller) { return false; }
    Controller->SetAsLocalPlayerController(); Controller->FinishSpawning(FTransform::Identity);
    auto* Pawn = World->SpawnActor<ADefaultPawn>(FVector(300,25,100),FRotator::ZeroRotator);
    if (!Pawn) { return false; }
    Controller->Possess(Pawn);
    ASFConstructionSandboxState* State = nullptr;
    for (TActorIterator<ASFConstructionSandboxState> It(World); It; ++It) { State = *It; break; }
    if (!State) { return false; }
    // Only synthetic ray samples and private controller boundaries are driven.
    // This test neither moves the OS cursor nor claims native mouse/Slate input.
    Controller->bScenarioCursor = true; Controller->bSyntheticRayOverride = true;
    auto Aim = [&](const FVector& Target)
    {
        Controller->SyntheticRayStart = FVector(300,-200,250);
        Controller->SyntheticRayDirection = (Target-Controller->SyntheticRayStart).GetSafeNormal();
    };
    TestTrue(TEXT("Free cursor begins visible with look suppressed"),Controller->bShowMouseCursor && Controller->IsLookInputIgnored());
    Controller->RotationInput = FRotator::ZeroRotator;
    Controller->AddYawInput(3);
    TestEqual(TEXT("Free cursor ignores look axis deltas"),Controller->RotationInput.Yaw,0.0);
    const FRotator OriginalRotation = Controller->GetControlRotation();
    Aim(FVector(525,25,0)); Controller->Tick(0);
    if (!TestTrue(TEXT("Cursor ray samples ground"),Controller->bHasPreview)) { return false; }
    TestEqual(TEXT("Cursor controls addressed cell"),Controller->PreviewIntent.Cell,FIntVector(10,0,0));
    Aim(FVector(575,25,0));
    TestTrue(TEXT("Action boundary resamples pointer within unchanged revision"),Controller->CanCommitCursorSample());
    TestEqual(TEXT("Resample targets current pointer rather than older cell"),Controller->PreviewIntent.Cell,FIntVector(11,0,0));
    TestEqual(TEXT("Resampling alone is read-only"),State->GetRevision(),uint64(0));
    Aim(FVector(525,25,0)); Controller->Tick(0); Controller->Place();
    if (!TestEqual(TEXT("Fresh cursor placement publishes once"),State->GetRevision(),uint64(1))) { AddError(Controller->GetLastConstructionResult()); return false; }
    TestTrue(TEXT("Cursor movement does not rotate camera"),Controller->GetControlRotation().Equals(OriginalRotation));
    Controller->bHasLineSeed = true; Controller->LineSeed = {10,0,0}; Controller->LineSeedRevision = 1;
    Controller->BeginCameraLook();
    TestTrue(TEXT("RMB enters camera capture and releases own look suppression"),Controller->bCameraLookHeld && !Controller->bShowMouseCursor && !Controller->IsLookInputIgnored());
    Controller->AddYawInput(3);
    TestTrue(TEXT("Held RMB admits look axis deltas"),Controller->RotationInput.Yaw != 0);
    Controller->RotationInput = FRotator::ZeroRotator;
    Controller->Place(); Controller->Remove();
    TestEqual(TEXT("RMB cannot place or demolish"),State->GetRevision(),uint64(1));
    Controller->EndCameraLook(); Controller->EndCameraLook();
    TestTrue(TEXT("RMB release restores free cursor with balanced suppression"),Controller->bShowMouseCursor && Controller->IsLookInputIgnored());
    TestTrue(TEXT("Turning camera preserves current line seed and original revision"),Controller->HasLineSeed() && Controller->LineSeedRevision == 1);
    TestFalse(TEXT("Turning camera clears transient endpoint sample"),Controller->bHasCursorSample);
    Controller->bSyntheticPointerOverPalette = true;
    Controller->ToggleLine(); Controller->Rotate(); Controller->Place(); Controller->Remove(); Controller->Interact();
    TestFalse(TEXT("UI hover blocks world keyboard shortcuts"),Controller->bLineMode);
    TestEqual(TEXT("UI hover blocks world transactions"),State->GetRevision(),uint64(1));
    Controller->PaletteAction(ESFConstructionPaletteAction::Slab);
    TestEqual(TEXT("Explicit palette callback selects without clicking through"),Controller->Shape,1);
    TestEqual(TEXT("Palette selection does not edit world"),State->GetRevision(),uint64(1));
    Controller->bSyntheticPointerOverPalette = false;
    Controller->LayerUp(); Aim(FVector(525,125,50)); Controller->Tick(0);
    TestTrue(TEXT("Layer uses one lattice step"),Controller->bLayerPlacement && Controller->PlacementLayer == 1);
    TestEqual(TEXT("Layer ghost lies on selected Z plane"),Controller->PreviewIntent.Cell,FIntVector(10,2,1));
    Controller->Place();
    TestEqual(TEXT("Layer cannot bypass unsupported placement"),State->GetRevision(),uint64(1));
    for (int32 Index = 0; Index < 25; ++Index) { Controller->LayerDown(); }
    TestEqual(TEXT("Layer lower bound"),Controller->PlacementLayer,0);
    for (int32 Index = 0; Index < 25; ++Index) { Controller->LayerUp(); }
    TestEqual(TEXT("Layer upper bound"),Controller->PlacementLayer,19);
    Controller->SyntheticRayDirection = FVector::ForwardVector; Controller->Tick(0);
    TestFalse(TEXT("Parallel ray has no fabricated plane sample"),Controller->bHasCursorSample);
    Controller->bHasLineSeed = true; Controller->LineSeedRevision = 1;
    Controller->LayerDown(); TestFalse(TEXT("Depth changes invalidate line seed"),Controller->HasLineSeed());
    Controller->BeginCameraLook(); Controller->ApplicationActivationChanged(false);
    TestTrue(TEXT("Focus loss releases held camera and restores cursor"),!Controller->bCameraLookHeld && Controller->bShowMouseCursor && Controller->IsLookInputIgnored());
    Controller->Place(); TestEqual(TEXT("Inactive window cannot edit"),State->GetRevision(),uint64(1));
    Controller->ApplicationActivationChanged(true);
    TestFalse(TEXT("Focus return carries no stale cursor sample"),Controller->bHasCursorSample);
    Controller->bLayerPlacement = false; Aim(FVector(525,25,25)); Controller->Tick(0);
    if (!TestTrue(TEXT("Cursor samples existing object"),Controller->bHasRemoval)) { return false; }
    Controller->BeginCameraLook(); Controller->EndCameraLook();
    TestEqual(TEXT("RMB tap in build mode cannot delete"),State->GetRevision(),uint64(1));
    Controller->PaletteAction(ESFConstructionPaletteAction::Demolish); Controller->Tick(0);
    Controller->BeginCameraLook(); Controller->AddYawInput(1); Controller->AddYawInput(-1); Controller->EndCameraLook();
    TestEqual(TEXT("Drag out and back cannot become a delete click"),State->GetRevision(),uint64(1));
    Controller->Tick(0); Controller->BeginCameraLook(); Controller->EndCameraLook();
    TestEqual(TEXT("RMB tap in demolition mode is also read-only"),State->GetRevision(),uint64(1));
    Controller->Tick(0); Controller->Place();
    TestEqual(TEXT("Demolition LMB removes captured object once"),State->GetRevision(),uint64(2));
    TestEqual(TEXT("Demolition LMB removed only captured object"),State->GetMemberCount(),0);
    return true;
}
#endif
