#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Construction/SFConstructionGeometry.h"
#include "Framework/SFE1GameMode.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionSandboxSettingsTest, "ShatteredFrontier.Construction.Controls.ReleaseBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionSandboxSettingsTest::RunTest(const FString& Parameters)
{
    auto* Settings = NewObject<USFConstructionSandboxSettings>();
    FString Error;
    TestTrue(TEXT("Shipped development settings valid"), Settings->Validate(Error));
    TestEqual(TEXT("Construction baseline reach is eight metres"), Settings->BuildReachMetres, 8.0f);
    Settings->BuildReachMetres = 8.1f;
    TestFalse(TEXT("Reach cannot exceed baseline safety bound"), Settings->Validate(Error));
    Settings->BuildReachMetres = 8.0f;
    Settings->MaximumMembers = 4097;
    TestFalse(TEXT("Cell capacity bounded"), Settings->Validate(Error));
    Settings->MaximumMembers = 1;
    TestTrue(TEXT("Minimum bounded capacity valid"), Settings->Validate(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionSandboxAuthorityTest, "ShatteredFrontier.Construction.Controls.StandaloneAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionSandboxAuthorityTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Fixed-foundation support/standalone authority, not collapse/save/network or actual player-input acceptance."));
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = ASFConstructionSandboxGameMode::StaticClass();
    if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Foundation = World->SpawnActor<AStaticMeshActor>();
    if (!TestNotNull(TEXT("Synthetic test foundation"), Foundation)) { return false; }
    Foundation->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Foundation->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Foundation->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Foundation->SetActorLocation(FVector(0,0,-50));
    Foundation->SetActorScale3D(FVector(20,20,1));
    Foundation->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
    // Test worlds have no local-player admission. Designate local control before
    // BeginPlay, matching GameMode's normal admission rather than bypassing the
    // runtime state/controller guard or calling BeginPlay twice.
    auto* Controller = World->SpawnActorDeferred<ASFConstructionSandboxController>(
        ASFConstructionSandboxController::StaticClass(), FTransform::Identity);
    if (!TestNotNull(TEXT("Deferred controller"), Controller)) { return false; }
    Controller->SetAsLocalPlayerController();
    Controller->FinishSpawning(FTransform::Identity);
    auto* Pawn = World->SpawnActor<ADefaultPawn>(FVector(300,25,100), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Controller"), Controller) || !TestNotNull(TEXT("Pawn"), Pawn)) { return false; }
    Controller->Possess(Pawn);
    TestTrue(TEXT("Fixture uses local authoritative owned pawn"), Controller->IsLocalController()
        && Pawn->HasAuthority() && Pawn->GetOwner() == Controller && Pawn->GetController() == Controller);
    ASFConstructionSandboxState* State = nullptr;
    for (TActorIterator<ASFConstructionSandboxState> It(World); It; ++It) { State = *It; break; }
    if (!TestNotNull(TEXT("Sandbox-owned transient state"), State)) { return false; }
    FSFConstructionSandboxIntent Intent;
    Intent.Cell = FIntVector(10,0,0);
    Intent.Rotation = SFConstructionGeometry::IdentityRotation();
    FString Result;
    TestTrue(TEXT("Approved bounded standalone placement"), State->Apply(Controller, Intent, Result));
    TestEqual(TEXT("Exactly one new member"), State->GetMemberCount(), 1);
    TestFalse(TEXT("Accepted intent replay is stale"), State->Apply(Controller, Intent, Result));
    TestEqual(TEXT("Replay cannot duplicate member"), State->GetMemberCount(), 1);
    Intent.ExpectedRevision = State->GetRevision();
    TestFalse(TEXT("Occupied address never silently replaced"), State->Apply(Controller, Intent, Result));
    Intent.Cell = FIntVector(11,0,0);
    Intent.Rotation = 24;
    TestFalse(TEXT("Invalid orientation rejects"), State->Apply(Controller, Intent, Result));
    Intent.Rotation = SFConstructionGeometry::IdentityRotation();
    Intent.Cell = FIntVector(-19,0,0);
    TestFalse(TEXT("Authority recomputes reach"), State->Apply(Controller, Intent, Result));
    Intent.Cell = FIntVector(20,0,0);
    TestFalse(TEXT("Outside fixed plot rejects"), State->Apply(Controller, Intent, Result));
    Intent.Cell = FIntVector(6,0,2);
    TestFalse(TEXT("Owned pawn physical volume cannot be built through"), State->Apply(Controller, Intent, Result));
    Intent.Cell = FIntVector(12,0,2);
    TestFalse(TEXT("New floating construction fails support"), State->Apply(Controller, Intent, Result));
    TestTrue(TEXT("Floating rejection identifies support path"), Result.Contains(TEXT("Unsupported")));
    ASFConstructionSandboxPreview* Existing = nullptr;
    for (TActorIterator<ASFConstructionSandboxPreview> It(World); It; ++It)
    {
        FIntVector Cell;
        if (State->FindCell(*It, Cell) && Cell == FIntVector(10,0,0)) { Existing = *It; break; }
    }
    if (!TestNotNull(TEXT("Editable projection found independently of demos"), Existing)) { return false; }
    const auto* PriorMesh = Existing->GetMesh();
    const uint64 PriorGeneration = Existing->GetGeneration();
    const uint64 PriorRevision = State->GetRevision();
    int32 PriorActors = 0;
    for (TActorIterator<ASFConstructionSandboxPreview> It(World); It; ++It) { ++PriorActors; }
    // Invalid existing projection transform forces its preparation to fail after
    // the newly added member stages. A smaller scale keeps clearance unblocked.
    Existing->SetActorScale3D(FVector(0.5));
    Pawn->SetActorLocation(FVector(575,225,100));
    Intent.Cell = FIntVector(11,0,0);
    TestTrue(TEXT("Failure injection reaches preparation after validation"), State->Evaluate(Controller, Intent).IsEmpty());
    TestFalse(TEXT("Later neighbor preparation failure rejects whole edit"), State->Apply(Controller, Intent, Result));
    TestEqual(TEXT("Rejected multi-member edit preserves member count"), State->GetMemberCount(), 1);
    TestEqual(TEXT("Rejected multi-member edit preserves revision"), State->GetRevision(), PriorRevision);
    TestTrue(TEXT("Rejected multi-member edit preserves active mesh/collision"), Existing->GetMesh() == PriorMesh);
    TestEqual(TEXT("Rejected multi-member edit preserves projection generation"), Existing->GetGeneration(), PriorGeneration);
    int32 AfterActors = 0;
    for (TActorIterator<ASFConstructionSandboxPreview> It(World); It; ++It)
    { if (!It->IsActorBeingDestroyed()) { ++AfterActors; } }
    TestEqual(TEXT("Rejected member leaves no live projection actor"), AfterActors, PriorActors);
    Existing->SetActorScale3D(FVector::OneVector);
    Pawn->SetActorLocation(FVector(300,25,100));
    Intent.Cell = FIntVector(10,0,1);
    Intent.ExpectedRevision = State->GetRevision();
    TestTrue(TEXT("Grounded column supports member above"), State->Apply(Controller, Intent, Result));
    Intent.ExpectedRevision = State->GetRevision();
    Intent.Cell = FIntVector(10,0,0);
    Intent.bRemove = true;
    TestFalse(TEXT("Unsafe demolition blocked until grace/confirmation checkpoint"), State->Apply(Controller, Intent, Result));
    TestTrue(TEXT("Removal explains pending support-loss flow"), Result.Contains(TEXT("destabilizes")));
    Intent.Cell = FIntVector(10,0,1);
    TestTrue(TEXT("Top member safely removable"), State->Apply(Controller, Intent, Result));
    Intent.ExpectedRevision = State->GetRevision();
    Intent.Cell = FIntVector(10,0,0);
    Intent.bRemove = true;
    TestTrue(TEXT("Bounded removal of own editable member"), State->Apply(Controller, Intent, Result));
    TestEqual(TEXT("Member removed"), State->GetMemberCount(), 0);
    Intent.ExpectedRevision = State->GetRevision();
    TestFalse(TEXT("Removal of absent member rejects"), State->Apply(Controller, Intent, Result));
    Intent.bRemove = false;
    TestTrue(TEXT("Grounded fixture rebuilt for unavailable-projection probe"), State->Apply(Controller, Intent, Result));
    ASFConstructionSandboxPreview* Unavailable = nullptr;
    for (TActorIterator<ASFConstructionSandboxPreview> It(World); It; ++It)
    {
        FIntVector Cell;
        if (State->FindCell(*It, Cell) && Cell == Intent.Cell) { Unavailable = *It; break; }
    }
    if (!TestNotNull(TEXT("Unavailable projection probe"), Unavailable)) { return false; }
    Unavailable->Destroy();
    const uint64 UnavailableRevision = State->GetRevision();
    Intent.Cell = FIntVector(11,0,0);
    Intent.ExpectedRevision = UnavailableRevision;
    TestFalse(TEXT("Missing member projection cannot provide virtual support"), State->Apply(Controller, Intent, Result));
    TestTrue(TEXT("Missing projection fences edits"), Result.Contains(TEXT("unavailable")));
    TestEqual(TEXT("Unavailable projection preserves authority revision"), State->GetRevision(), UnavailableRevision);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionSandboxNormalModeTest, "ShatteredFrontier.Construction.Controls.NormalModeIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionSandboxNormalModeTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = ASFE1GameMode::StaticClass();
    if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Controller = World->SpawnActorDeferred<ASFConstructionSandboxController>(
        ASFConstructionSandboxController::StaticClass(), FTransform::Identity);
    if (!Controller) { AddError(TEXT("Isolation controller spawn failed")); return false; }
    Controller->SetAsLocalPlayerController();
    Controller->FinishSpawning(FTransform::Identity);
    auto* Pawn = World->SpawnActor<ADefaultPawn>(FVector(300,25,100), FRotator::ZeroRotator);
    auto* State = World->SpawnActor<ASFConstructionSandboxState>();
    if (!Controller || !Pawn || !State) { AddError(TEXT("Isolation fixture spawn failed")); return false; }
    Controller->Possess(Pawn);
    FSFConstructionSandboxIntent Intent;
    Intent.Cell = FIntVector(10,0,0);
    Intent.Rotation = SFConstructionGeometry::IdentityRotation();
    FString Result;
    TestFalse(TEXT("Injected controller/state cannot mutate normal E1 world"), State->Apply(Controller, Intent, Result));
    TestEqual(TEXT("Normal E1 injected state remains empty"), State->GetMemberCount(), 0);
    TestEqual(TEXT("Normal E1 injected state revision unchanged"), State->GetRevision(), uint64(0));
    return true;
}

#endif
