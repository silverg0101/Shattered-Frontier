#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionGeometry.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionLineControlsTest,"ShatteredFrontier.Construction.LineControls.SampleRevisionAndImportCancellation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionLineControlsTest::RunTest(const FString& Parameters)
{
    // Focused controller boundary checks. Real bound-input evidence is owned by
    // the separate finite scenario; no disk save/load or private authority seam.
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
    UWorld* World = Fixture.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = ASFConstructionSandboxGameMode::StaticClass();
    if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Floor = World->SpawnActor<AStaticMeshActor>();
    Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Floor->SetActorLocation(FVector(0,0,-50)); Floor->SetActorScale3D(FVector(20,20,1));
    Floor->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
    auto* Controller = World->SpawnActorDeferred<ASFConstructionSandboxController>(ASFConstructionSandboxController::StaticClass(),FTransform::Identity);
    if (!Controller) { return false; }
    Controller->SetAsLocalPlayerController(); Controller->FinishSpawning(FTransform::Identity);
    auto* Pawn = World->SpawnActor<ADefaultPawn>(FVector(425,625,100),FRotator::ZeroRotator);
    if (!Pawn) { return false; }
    Controller->Possess(Pawn);
    ASFConstructionSandboxState* State = nullptr;
    for (TActorIterator<ASFConstructionSandboxState> It(World); It; ++It) { State = *It; break; }
    if (!State) { return false; }
    const int32 Rotation = SFConstructionGeometry::IdentityRotation();
    auto Sample = [&](uint64 Revision,bool bSeed)
    {
        Controller->bLineMode = true; Controller->bHasLineEndpoint = true; Controller->bHasLineSeed = bSeed;
        Controller->LineSeed = {8,8,0}; Controller->LineSeedRevision = Revision;
        Controller->LinePreviewIntent.StartCell = {8,8,0}; Controller->LinePreviewIntent.EndCell = {9,8,0};
        Controller->LinePreviewIntent.Shape = 0; Controller->LinePreviewIntent.Rotation = Rotation;
        Controller->LinePreviewIntent.ExpectedRevision = Revision;
        Controller->LineGhostCells = {{8,8,0},{9,8,0}}; Controller->bPendingRemoval = true;
    };
    Sample(0,true);
    FSFConstructionSandboxIntent External;
    External.Cell = {8,8,0}; External.Rotation = Rotation; External.ExpectedRevision = 0;
    FString Result;
    if (!TestTrue(TEXT("Intervening public single edit without controller Tick"),State->Apply(Controller,External,Result))) { AddError(Result); return false; }
    TestTrue(TEXT("Seed still carries genuinely old sampled revision before Tick"),Controller->HasLineSeed());
    Controller->Place();
    TestFalse(TEXT("Pre-Tick click invalidates stale seed"),Controller->HasLineSeed());
    TestFalse(TEXT("Pre-Tick invalidation clears endpoint"),Controller->bHasLineEndpoint);
    TestEqual(TEXT("Stale dispatch cannot refresh revision and place"),State->GetRevision(),uint64(1));
    TestEqual(TEXT("Stale dispatch preserves external cell only"),State->GetMemberCount(),1);
    Sample(0,false); Controller->Place();
    TestFalse(TEXT("Stale endpoint cannot become a fresh first seed"),Controller->HasLineSeed());
    TestFalse(TEXT("Stale endpoint is invalidated before Tick"),Controller->bHasLineEndpoint);
    Sample(1,true); Controller->Place();
    TestTrue(TEXT("Failed current whole-line apply retains visible seed"),Controller->HasLineSeed());
    TestTrue(TEXT("Failed apply preserves pending confirmation"),Controller->bPendingRemoval);
    TestEqual(TEXT("Failed apply preserves ghost cells"),Controller->GetLineGhostCells().Num(),2);
    TestEqual(TEXT("Failed apply preserves revision"),State->GetRevision(),uint64(1));
    FSFConstructionSandboxSnapshot Snapshot;
    if (!TestTrue(TEXT("Read-only snapshot for in-memory load-equivalent test"),State->ExportSnapshot(Controller,1,Snapshot,Result))) { AddError(Result); return false; }
    auto Invalid = Snapshot; Invalid.Cells[0].Shape = 99;
    TestFalse(TEXT("Invalid restore rejects without filesystem IO"),State->ImportSnapshot(Controller,1,Invalid,Result));
    TestTrue(TEXT("Failed restore preserves seed and confirmation"),Controller->HasLineSeed() && Controller->bPendingRemoval);
    if (!TestTrue(TEXT("Successful in-memory import exercises F9 cancellation path"),State->ImportSnapshot(Controller,1,Snapshot,Result))) { AddError(Result); return false; }
    TestFalse(TEXT("Success immediately clears line seed"),Controller->HasLineSeed());
    TestFalse(TEXT("Success immediately clears sampled endpoint"),Controller->bHasLineEndpoint);
    TestFalse(TEXT("Success immediately clears confirmation"),Controller->bPendingRemoval);
    TestTrue(TEXT("Success immediately clears complete line ghost"),Controller->GetLineGhostCells().IsEmpty());
    TestEqual(TEXT("Import publishes one fresh live revision"),State->GetRevision(),uint64(2));
    return true;
}
#endif
