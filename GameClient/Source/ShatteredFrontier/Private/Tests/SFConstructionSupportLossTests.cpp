#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionGeometry.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"

namespace
{
struct FSupportLossFixture
{
    FTestWorldWrapper Fixture;
    UWorld* World = nullptr;
    ASFConstructionSandboxController* Controller = nullptr;
    ADefaultPawn* ControlledPawn = nullptr;
    ASFConstructionSandboxState* State = nullptr;
    AStaticMeshActor* Foundation = nullptr;
    FString Result;

    bool Create(FAutomationTestBase& Test)
    {
        if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(&Test); return false; }
        World = Fixture.GetTestWorld();
        World->GetWorldSettings()->DefaultGameMode = ASFConstructionSandboxGameMode::StaticClass();
        if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(&Test); return false; }
        World->GetWorldSettings()->NotifyBeginPlay();
        Foundation = World->SpawnActor<AStaticMeshActor>();
        Foundation->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Foundation->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Foundation->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        Foundation->SetActorLocation(FVector(0,0,-50));
        Foundation->SetActorScale3D(FVector(20,20,1));
        Foundation->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
        Controller = World->SpawnActorDeferred<ASFConstructionSandboxController>(ASFConstructionSandboxController::StaticClass(), FTransform::Identity);
        if (!Controller) { return false; }
        Controller->SetAsLocalPlayerController();
        Controller->FinishSpawning(FTransform::Identity);
        ControlledPawn = World->SpawnActor<ADefaultPawn>(FVector(300,225,100), FRotator::ZeroRotator);
        if (!ControlledPawn) { return false; }
        Controller->Possess(ControlledPawn);
        for (TActorIterator<ASFConstructionSandboxState> It(World); It; ++It) { State = *It; break; }
        return Test.TestNotNull(TEXT("Support-loss authority fixture"), State);
    }

    FSFConstructionSandboxIntent Intent(FIntVector Cell, bool bRemove = false)
    {
        ControlledPawn->SetActorLocation(ASFConstructionSandboxState::CellCorner(Cell) + FVector(25,225,75));
        FSFConstructionSandboxIntent Request;
        Request.Cell = Cell; Request.bRemove = bRemove;
        Request.Rotation = SFConstructionGeometry::IdentityRotation();
        Request.ExpectedRevision = State->GetRevision();
        return Request;
    }

    bool Place(FIntVector Cell, int32 Shape = 0, int32 Rotation = INDEX_NONE)
    {
        auto Request = Intent(Cell);
        Request.Shape = Shape;
        if (Rotation != INDEX_NONE) { Request.Rotation = Rotation; }
        return State->Apply(Controller, Request, Result);
    }

    bool Remove(FIntVector Cell, bool bConfirm)
    {
        auto Request = Intent(Cell, true);
        if (bConfirm)
        {
            TArray<FIntVector> Affected;
            if (!State->PreviewRemoval(Controller, Request, Affected, Result)) { return false; }
            Request.bConfirmSupportLoss = true;
        }
        return State->Apply(Controller, Request, Result);
    }

    ASFConstructionSandboxPreview* Projection(FIntVector Cell) const
    {
        for (TActorIterator<ASFConstructionSandboxPreview> It(World); It; ++It)
        {
            FIntVector Found;
            if (State->FindCell(*It, Found) && Found == Cell) { return *It; }
        }
        return nullptr;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionSupportLossTest, "ShatteredFrontier.Construction.SupportLoss.AuthorityLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionSupportLossTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Deterministic editor-only clock boundary; runtime uses authoritative world time. No input/network/save/economic acceptance."));
    {
        FSupportLossFixture F;
        if (!F.Create(*this)) { return false; }
        F.State->TestTimeOverride = 0.0;
        TestTrue(TEXT("Base"), F.Place({10,0,0}));
        TestTrue(TEXT("Column layer one"), F.Place({10,0,1}));
        TestTrue(TEXT("Column layer two"), F.Place({10,0,2}));
        TestTrue(TEXT("Column layer three"), F.Place({10,0,3}));
        TestTrue(TEXT("Independent anchored section"), F.Place({14,0,0}));
        auto Removal = F.Intent({10,0,0}, true);
        TestFalse(TEXT("Unconfirmed support-loss rejects"), F.State->Apply(F.Controller, Removal, F.Result));
        Removal.bConfirmSupportLoss = true;
        TestFalse(TEXT("Forged confirmation without authority preview rejects"), F.State->Apply(F.Controller, Removal, F.Result));
        Removal.bConfirmSupportLoss = false;
        TArray<FIntVector> Affected;
        TestTrue(TEXT("Removal preview authority validates"), F.State->PreviewRemoval(F.Controller, Removal, Affected, F.Result));
        TestEqual(TEXT("Preview lists three newly unsupported addresses"), Affected.Num(), 3);
        Removal.bConfirmSupportLoss = true;
        F.State->CancelRemoval(F.Controller);
        TestFalse(TEXT("Cancelled preview cannot confirm"), F.State->Apply(F.Controller, Removal, F.Result));
        Removal.bConfirmSupportLoss = false;
        TestTrue(TEXT("New preview"), F.State->PreviewRemoval(F.Controller, Removal, Affected, F.Result));
        TestTrue(TEXT("Unrelated accepted edit stales removal preview"), F.Place({15,0,0}));
        Removal.bConfirmSupportLoss = true;
        F.Intent(Removal.Cell, true);
        TestFalse(TEXT("Old revision confirmation rejects"), F.State->Apply(F.Controller, Removal, F.Result));
        TestTrue(TEXT("Confirmed support loss commits"), F.Remove({10,0,0}, true));
        TestEqual(TEXT("Three individual grace episodes"), F.State->Instability.Num(), 3);
        double Deadline = -1;
        TestTrue(TEXT("Top has deadline"), F.State->Instability.GetDeadline({10,0,3}, Deadline));
        TestEqual(TEXT("Grace deadline exact"), Deadline, 10.0);
        const uint64 RemovedRevision = F.State->GetRevision();
        TestFalse(TEXT("Accepted confirmation replay cannot execute"), F.State->Apply(F.Controller, Removal, F.Result));
        TestEqual(TEXT("Replay leaves revision unchanged"), F.State->GetRevision(), RemovedRevision);
        F.State->TestTimeOverride = 1.0;
        TestTrue(TEXT("Remove already unstable bottom to require multi-step repair"), F.Remove({10,0,1}, false));
        F.State->TestTimeOverride = 2.0;
        TestTrue(TEXT("Supported first recovery-column step permitted during instability"), F.Place({10,0,0}));
        F.State->Instability.GetDeadline({10,0,3}, Deadline);
        TestEqual(TEXT("Partial repair does not extend original deadline"), Deadline, 10.0);
        TestFalse(TEXT("New floating piece still rejected during grace"), F.Place({16,0,3}));
        F.State->TestTimeOverride = 3.0;
        TestTrue(TEXT("Second supported recovery-column step restores old pieces"), F.Place({10,0,1}));
        TestEqual(TEXT("Restored support cancels all deadlines"), F.State->Instability.Num(), 0);
        F.State->ProcessDeadlines(10.0);
        TestNotNull(TEXT("Recovered top survives old deadline"), F.Projection({10,0,3}));
        F.State->TestTimeOverride = 4.0;
        TestTrue(TEXT("Distinct later support-loss episode"), F.Remove({10,0,0}, true));
        F.State->Instability.GetDeadline({10,0,3}, Deadline);
        TestEqual(TEXT("New episode starts its own grace"), Deadline, 14.0);
        F.State->TestTimeOverride = 13.999;
        F.State->ProcessDeadlines(F.State->AuthorityTime());
        TestNotNull(TEXT("Just before expiry top survives"), F.Projection({10,0,3}));
        const uint64 PreCollapseRevision = F.State->GetRevision();
        F.State->TestTimeOverride = 14.0;
        TestFalse(TEXT("Repair at exact deadline cannot cancel expiry before state tick"), F.Place({10,0,0}));
        TestEqual(TEXT("Late repair preserves assembly revision"), F.State->GetRevision(), PreCollapseRevision);
        F.State->Instability.GetDeadline({10,0,3}, Deadline);
        TestEqual(TEXT("Late repair preserves expired deadline"), Deadline, 14.0);
        F.State->ProcessDeadlines(F.State->AuthorityTime());
        TestNull(TEXT("Exact expiry collapses unsupported top"), F.Projection({10,0,3}));
        TestEqual(TEXT("One wave publishes one assembly revision"), F.State->GetRevision(), PreCollapseRevision + 1);
        TestNotNull(TEXT("Independent anchored section survives collapse"), F.Projection({14,0,0}));
        TestEqual(TEXT("No overdue orphan lifecycle remains"), F.State->Instability.Num(), 0);
    }
    {
        FSupportLossFixture F;
        if (!F.Create(*this)) { return false; }
        F.State->TestTimeOverride = 0.0;
        TestTrue(TEXT("Wave A base"), F.Place({10,0,0}));
        TestTrue(TEXT("Wave A top"), F.Place({10,0,1}));
        TestTrue(TEXT("Wave B base"), F.Place({14,0,0}));
        TestTrue(TEXT("Wave B top"), F.Place({14,0,1}));
        TestTrue(TEXT("Third grounded survivor"), F.Place({18,0,0}));
        TestTrue(TEXT("First independent support loss"), F.Remove({10,0,0}, true));
        F.State->TestTimeOverride = 2.0;
        TestTrue(TEXT("Second independent support loss"), F.Remove({14,0,0}, true));
        double DeadlineA = -1, DeadlineB = -1;
        F.State->Instability.GetDeadline({10,0,1}, DeadlineA);
        F.State->Instability.GetDeadline({14,0,1}, DeadlineB);
        TestEqual(TEXT("First deadline remains ten seconds"), DeadlineA, 10.0);
        TestEqual(TEXT("Second deadline starts at separate loss time"), DeadlineB, 12.0);
        F.State->TestTimeOverride = 10.0;
        F.State->ProcessDeadlines(10.0);
        TestNull(TEXT("First due wave collapses A"), F.Projection({10,0,1}));
        TestNotNull(TEXT("Not-yet-due unstable B survives first wave"), F.Projection({14,0,1}));
        F.State->Instability.GetDeadline({14,0,1}, DeadlineB);
        TestEqual(TEXT("First wave does not extend second deadline"), DeadlineB, 12.0);
        F.State->TestTimeOverride = 12.0;
        F.State->ProcessDeadlines(12.0);
        TestNull(TEXT("Second deadline collapses B separately"), F.Projection({14,0,1}));
        TestNotNull(TEXT("Grounded third section survives both waves"), F.Projection({18,0,0}));
        TestEqual(TEXT("Both deadline waves consumed after successful collapse"), F.State->Instability.Num(), 0);
    }
    {
        FSupportLossFixture F;
        if (!F.Create(*this)) { return false; }
        F.State->TestTimeOverride = 0.0;
        TestTrue(TEXT("Rollback base"), F.Place({10,0,0}));
        TestTrue(TEXT("Rollback elevated member"), F.Place({10,0,1}));
        TestTrue(TEXT("Independent neighbor foundation"), F.Place({11,0,0}));
        int32 FarSlabRotation = INDEX_NONE;
        for (int32 Rotation = 0; Rotation < 24; ++Rotation)
        {
            FSFConstructionSolid Solid;
            SFConstructionGeometry::Build(1, Rotation, Solid);
            const FBox Box(Solid.Vertices);
            if (Box.Min.Equals(FVector(25,0,0)) && Box.Max.Equals(FVector(50,50,50))) { FarSlabRotation = Rotation; break; }
        }
        if (!TestTrue(TEXT("Allowlisted noncontact neighbor orientation"), FarSlabRotation != INDEX_NONE)) { return false; }
        TestTrue(TEXT("Independent noncontact adjacent member"), F.Place({11,0,1}, 1, FarSlabRotation));
        TestTrue(TEXT("Rollback confirmed removal"), F.Remove({10,0,0}, true));
        auto* Survivor = F.Projection({11,0,1});
        auto* Unsupported = F.Projection({10,0,1});
        if (!Survivor || !Unsupported) { AddError(TEXT("Rollback setup missing projection")); return false; }
        const auto TraceSimple = [&F](double X, FHitResult& Hit)
        {
            FCollisionQueryParams Query(SCENE_QUERY_STAT(SupportLossCollisionProbe), false);
            return F.World->LineTraceSingleByChannel(Hit, FVector(X,-25,75), FVector(X,75,75), ECC_Visibility, Query);
        };
        FHitResult Hit;
        TestTrue(TEXT("Due member canonical solid initially blocks real simple trace"), TraceSimple(525, Hit));
        TestTrue(TEXT("Initial due trace hits actual editable member"), Hit.GetActor() == Unsupported);
        TestTrue(TEXT("Independent half-slab solid initially blocks real simple trace"), TraceSimple(587, Hit));
        TestTrue(TEXT("Initial partial-solid trace hits survivor"), Hit.GetActor() == Survivor);
        const auto* SurvivorMesh = Survivor->GetMesh();
        const auto* UnsupportedMesh = Unsupported->GetMesh();
        const uint64 BeforeRevision = F.State->GetRevision();
        const uint64 SurvivorGeneration = Survivor->GetGeneration();
        F.State->TestFailAfterPreparedCount = 1;
        F.State->TestTimeOverride = 10.0;
        F.State->ProcessDeadlines(10.0);
        TestEqual(TEXT("Expired staging failure preserves assembly revision"), F.State->GetRevision(), BeforeRevision);
        TestTrue(TEXT("Expired staging failure preserves survivor collision"), Survivor->GetMesh() == SurvivorMesh);
        TestTrue(TEXT("Expired staging failure preserves due collision"), Unsupported->GetMesh() == UnsupportedMesh);
        TestTrue(TEXT("Due solid still blocks simple trace after failed staging"), TraceSimple(525, Hit));
        TestTrue(TEXT("Failed-stage trace still hits original due member"), Hit.GetActor() == Unsupported);
        double Deadline;
        TestTrue(TEXT("Expired staging failure keeps due episode"), F.State->Instability.GetDeadline({10,0,1}, Deadline));
        TestEqual(TEXT("Expired staging failure does not extend deadline"), Deadline, 10.0);
        F.State->TestFailAfterPreparedCount = INDEX_NONE;
        F.State->ProcessDeadlines(10.1);
        TestEqual(TEXT("Projection failure retry is bounded rather than every tick"), F.State->GetRevision(), BeforeRevision);
        F.State->TestTimeOverride = 10.25;
        F.Foundation->Tags.Reset();
        F.State->ProcessDeadlines(10.25);
        TestEqual(TEXT("Missing foundation fences collapse"), F.State->GetRevision(), BeforeRevision);
        TestFalse(TEXT("Missing foundation fences ordinary edits"), F.Place({14,0,0}));
        F.Foundation->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
        F.State->TestTimeOverride = 10.5;
        F.State->ProcessDeadlines(10.5);
        TestNull(TEXT("Same overdue wave retries after repair"), F.Projection({10,0,1}));
        TestTrue(TEXT("Survivor neighbor projection activated with collapse"), Survivor->GetGeneration() > SurvivorGeneration);
        TestTrue(TEXT("Survivor real collision remains active"), Survivor->GetMesh()->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
        TestFalse(TEXT("Successful collapse removes actual due-body simple collision"), TraceSimple(525, Hit));
        TestTrue(TEXT("Rebuilt survivor half-slab still blocks actual simple trace"), TraceSimple(587, Hit));
        TestTrue(TEXT("Rebuilt-neighbor collision trace hits survivor actor"), Hit.GetActor() == Survivor);
        TestEqual(TEXT("Collapse removed only unsupported due piece"), F.State->GetMemberCount(), 2);
    }
    return true;
}

#endif
