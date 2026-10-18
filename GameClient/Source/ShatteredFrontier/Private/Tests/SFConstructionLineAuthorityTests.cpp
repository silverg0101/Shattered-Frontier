#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Construction/SFConstructionGeometry.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
struct FLineFixture
{
    FTestWorldWrapper Fixture;
    UWorld* World = nullptr;
    ASFConstructionSandboxController* Controller = nullptr;
    ASFConstructionSandboxState* State = nullptr;
    ADefaultPawn* Pawn = nullptr;
    AStaticMeshActor* Floor = nullptr;
    FString Result;
    bool Create(FAutomationTestBase& Test)
    {
        if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
        World = Fixture.GetTestWorld();
        World->GetWorldSettings()->DefaultGameMode = ASFConstructionSandboxGameMode::StaticClass();
        if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { return false; }
        World->GetWorldSettings()->NotifyBeginPlay();
        // Keep the reference display collidable in a separate fixture area.
        for (TActorIterator<ASFConstructionSandboxPreview> It(World); It; ++It)
        { It->SetActorLocation(It->GetActorLocation()+FVector(0,-800,0)); }
        Floor = World->SpawnActor<AStaticMeshActor>();
        Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        Floor->SetActorLocation(FVector(0,0,-50)); Floor->SetActorScale3D(FVector(20,20,1));
        Floor->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
        Controller = World->SpawnActorDeferred<ASFConstructionSandboxController>(ASFConstructionSandboxController::StaticClass(),FTransform::Identity);
        if (!Controller) { return false; }
        Controller->SetAsLocalPlayerController(); Controller->FinishSpawning(FTransform::Identity);
        Pawn = World->SpawnActor<ADefaultPawn>(FVector(75,225,100),FRotator::ZeroRotator);
        if (!Pawn) { return false; }
        Controller->Possess(Pawn);
        for (TActorIterator<ASFConstructionSandboxState> It(World); It; ++It) { State = *It; break; }
        return Test.TestNotNull(TEXT("Finite line authority state"),State);
    }
    FSFConstructionLineIntent Line(FIntVector Start,FIntVector End) const
    {
        FSFConstructionLineIntent Intent;
        Intent.StartCell = Start; Intent.EndCell = End;
        Intent.Rotation = SFConstructionGeometry::IdentityRotation(); Intent.ExpectedRevision = State->GetRevision();
        return Intent;
    }
    bool Apply(FIntVector Start,FIntVector End) { return State->ApplyLine(Controller,Line(Start,End),Result); }
    bool Seed(TArray<FSFConstructionSavedCell> Cells,TArray<FSFConstructionSavedDoor> Doors = {})
    {
        FSFConstructionSandboxSnapshot Snapshot; Snapshot.Cells = MoveTemp(Cells); Snapshot.Doors = MoveTemp(Doors);
        return State->ImportSnapshot(Controller,State->GetRevision(),Snapshot,Result);
    }
    UBoxComponent* Box(FVector Centre,FVector Extent)
    {
        auto* Actor = World->SpawnActor<AActor>();
        auto* Component = NewObject<UBoxComponent>(Actor); Actor->SetRootComponent(Component);
        Component->SetBoxExtent(Extent); Component->SetCollisionProfileName(TEXT("BlockAll"));
        Component->SetGenerateOverlapEvents(false); Component->SetWorldLocation(Centre); Component->RegisterComponent();
        return Component;
    }
    int32 LivePreviewCount() const
    {
        int32 Count = 0;
        for (TActorIterator<ASFConstructionSandboxPreview> It(World); It; ++It)
        { if (!It->IsActorBeingDestroyed()) { ++Count; } }
        return Count;
    }
    bool Trace(FIntVector Cell,const AActor* Expected) const
    {
        const FVector Corner = ASFConstructionSandboxState::CellCorner(Cell);
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(LineAuthoritySimple),false,Pawn);
        return World->LineTraceSingleByChannel(Hit,Corner+FVector(25,-20,25),Corner+FVector(25,70,25),ECC_Visibility,Query)
            && Hit.GetActor() == Expected;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionLineAuthorityTest,"ShatteredFrontier.Construction.Line.AuthorityAtomicSupportAndRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionLineAuthorityTest::RunTest(const FString& Parameters)
{
    const int32 Rotation = SFConstructionGeometry::IdentityRotation();
    {
        auto* Settings = NewObject<USFConstructionSandboxSettings>(); FString Error;
        TestEqual(TEXT("Shipped atomic line limit"),Settings->MaximumLineCells,8);
        Settings->MaximumLineCells = 0; TestFalse(TEXT("Zero line limit rejects"),Settings->Validate(Error));
        Settings->MaximumLineCells = 33; TestFalse(TEXT("Over invariant cap rejects"),Settings->Validate(Error));
        Settings->MaximumLineCells = 1; TestTrue(TEXT("Minimum line limit validates"),Settings->Validate(Error));
        Settings->MaximumLineCells = 32; TestTrue(TEXT("Maximum line limit validates"),Settings->Validate(Error));
    }
    for (bool bReverse : {false,true})
    {
        FLineFixture F; if (!F.Create(*this)) { return false; }
        TestEqual(TEXT("Startup captured default line limit"),F.State->GetMaximumLineCells(),8);
        if (!TestTrue(TEXT("Seed retained neighbor"),F.Seed({{{-1,0,0},0,Rotation}}))) { AddError(F.Result); return false; }
        const auto Old = F.State->Members.FindChecked({-1,0,0});
        auto Request = bReverse ? F.Line({2,0,0},{0,0,0}) : F.Line({0,0,0},{2,0,0});
        const int32 BeforeActors = F.LivePreviewCount();
        FString Hint;
        TestTrue(TEXT("Whole-line evaluation succeeds read-only"),F.State->EvaluateLine(F.Controller,Request,&Hint).IsEmpty());
        TestEqual(TEXT("Evaluation does not allocate projections"),F.LivePreviewCount(),BeforeActors);
        TestEqual(TEXT("Evaluation preserves revision"),F.State->GetRevision(),uint64(1));
        if (!TestTrue(TEXT("Ground line commits atomically in either direction"),F.State->ApplyLine(F.Controller,Request,F.Result))) { AddError(F.Result); return false; }
        TestEqual(TEXT("Three cells publish exactly one revision"),F.State->GetRevision(),uint64(2));
        TestEqual(TEXT("All cells present"),F.State->GetMemberCount(),4);
        const auto Retained = F.State->Members.FindChecked({-1,0,0});
        TestTrue(TEXT("Retained neighbor keeps actor and occupancy stamp"),Retained.Projection == Old.Projection && Retained.Generation == Old.Generation);
        TestEqual(TEXT("Retained neighbor mesh advances once"),Retained.ProjectionGeneration,uint64(2));
        for (const auto& Pair : F.State->Members)
        {
            auto* Projection = Pair.Value.Projection.Get();
            TestTrue(TEXT("Every released projection remains intact"),Projection && Projection->IsProjectionIntact(Pair.Value.Shape,Pair.Value.Rotation,
                Pair.Value.ProjectionGeneration,ASFConstructionSandboxState::CellCorner(Pair.Key)));
            TestTrue(TEXT("Every cell retains simple physics"),F.Trace(Pair.Key,Projection));
            if (Pair.Key != FIntVector(-1,0,0)) { TestEqual(TEXT("New occupancy shares one revision"),Pair.Value.Generation,uint64(2)); }
        }
        TestFalse(TEXT("Successful line replay is stale"),F.State->ApplyLine(F.Controller,Request,F.Result));
    }
    {
        FLineFixture F; if (!F.Create(*this)) { return false; }
        F.Pawn->SetActorLocation(FVector(275,225,175));
        if (!TestTrue(TEXT("Vertical line supported by its own complete lower stack"),F.Apply({5,0,3},{5,0,0}))) { AddError(F.Result); return false; }
        TestEqual(TEXT("Four-layer line is one revision"),F.State->GetRevision(),uint64(1));
        TestEqual(TEXT("Four-layer line has four cells"),F.State->GetMemberCount(),4);
    }
    {
        FLineFixture F; if (!F.Create(*this)) { return false; }
        if (!TestTrue(TEXT("Seed negative-case retained cell"),F.Seed({{{-1,0,0},0,Rotation}}))) { AddError(F.Result); return false; }
        const auto OldMembers = F.State->Members;
        const uint64 Revision = F.State->GetRevision(); const int32 Actors = F.LivePreviewCount();
        auto Preserved = [&]()
        {
            bool bIntact = F.State->GetRevision() == Revision && F.State->GetMemberCount() == OldMembers.Num() && F.LivePreviewCount() == Actors;
            for (const auto& Pair : OldMembers)
            {
                const auto* Now = F.State->Members.Find(Pair.Key);
                bIntact &= Now && Now->Projection == Pair.Value.Projection && Now->Generation == Pair.Value.Generation
                    && Now->ProjectionGeneration == Pair.Value.ProjectionGeneration && F.Trace(Pair.Key,Pair.Value.Projection.Get());
            }
            return bIntact;
        };
        auto Reject = [&](const TCHAR* Label,FSFConstructionLineIntent Request)
        { TestFalse(Label,F.State->ApplyLine(F.Controller,Request,F.Result)); TestTrue(TEXT("Rejected line preserves complete existing authority/physics"),Preserved()); };
        Reject(TEXT("Occupied interior rejects"),F.Line({-2,0,0},{0,0,0}));
        Reject(TEXT("Occupied endpoint rejects"),F.Line({-1,0,0},{1,0,0}));
        Reject(TEXT("Diagonal rejects"),F.Line({0,0,0},{1,1,0}));
        Reject(TEXT("Captured length limit rejects"),F.Line({0,0,0},{8,0,0}));
        Reject(TEXT("Plot endpoint rejects"),F.Line({19,0,0},{20,0,0}));
        Reject(TEXT("Negative height rejects"),F.Line({0,0,-1},{0,0,0}));
        Reject(TEXT("Maximum height rejects"),F.Line({0,0,19},{0,0,20}));
        Reject(TEXT("Out of reach rejects"),F.Line({19,19,0},{19,19,0}));
        Reject(TEXT("Floating line rejects whole candidate"),F.Line({0,0,3},{2,0,3}));
        auto Invalid = F.Line({0,0,0},{2,0,0}); Invalid.Shape = 3; Reject(TEXT("Unknown shape rejects"),Invalid);
        Invalid = F.Line({0,0,0},{2,0,0}); Invalid.Rotation = 24; Reject(TEXT("Unknown rotation rejects"),Invalid);
        Invalid = F.Line({0,0,0},{2,0,0}); --Invalid.ExpectedRevision; Reject(TEXT("Stale line rejects"),Invalid);
        TestFalse(TEXT("Wrong owner rejects"),F.State->ApplyLine(nullptr,F.Line({0,0,0},{2,0,0}),F.Result));
        TestTrue(TEXT("Wrong owner preserves"),Preserved());
        F.Pawn->SetOwner(nullptr);
        Reject(TEXT("Pawn no longer owned by caller rejects"),F.Line({0,0,0},{2,0,0}));
        F.Pawn->SetOwner(F.Controller);
        const int32 SavedBudget = F.State->MaximumMembers; F.State->MaximumMembers = 3;
        Reject(TEXT("Prospective whole-line budget rejects"),F.Line({0,0,0},{2,0,0})); F.State->MaximumMembers = SavedBudget;
        auto* Blocker = F.Box(FVector(75,125,75),FVector(100,5,100));
        Reject(TEXT("Current pawn sight blocked rejects"),F.Line({0,0,0},{2,0,0})); Blocker->GetOwner()->Destroy();
        Blocker = F.Box(FVector(75,25,25),FVector(10));
        Blocker->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
        Reject(TEXT("Interior simple prop rejects whole line"),F.Line({0,0,0},{2,0,0}));
        TestTrue(TEXT("Prop case reaches canonical clearance"),F.Result.Contains(TEXT("clearance")));
        Blocker->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
        Reject(TEXT("Encountered unsupported body fails closed"),F.Line({0,0,0},{2,0,0}));
        TestTrue(TEXT("Unsupported body reports unavailable clearance"),F.Result.Contains(TEXT("clearance unavailable")));
        Blocker->GetOwner()->Destroy();
        F.Pawn->SetActorLocation(FVector(75,25,40));
        Reject(TEXT("Actual owned pawn clearance rejects"),F.Line({0,0,0},{2,0,0})); F.Pawn->SetActorLocation(FVector(75,225,100));
        F.Floor->Tags.Reset(); Reject(TEXT("Missing known foundation rejects"),F.Line({0,0,0},{2,0,0})); F.Floor->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
        F.State->TestFailAfterPreparedCount = 2;
        Reject(TEXT("Rollback after two real prepared projections"),F.Line({0,0,0},{2,0,0}));
        TestTrue(TEXT("Failure reached prepared interruption"),F.Result.Contains(TEXT("interrupted after prepared")));
        TArray<UProceduralMeshComponent*> Components;
        OldMembers.FindChecked({-1,0,0}).Projection->GetComponents(Components);
        TestEqual(TEXT("No staged neighbor mesh leaked"),Components.Num(),1);
        FHitResult EmptyHit; FCollisionQueryParams EmptyQuery(SCENE_QUERY_STAT(LineRollbackEmpty),false,F.Pawn);
        TestFalse(TEXT("Rollback leaves no new-cell collision"),F.World->LineTraceSingleByChannel(EmptyHit,
            FVector(25,-20,25),FVector(25,70,25),ECC_Visibility,EmptyQuery));
        F.State->TestFailAfterPreparedCount = INDEX_NONE;
        TestTrue(TEXT("Same line retries after rollback"),F.Apply({0,0,0},{2,0,0}));
    }
    {
        FLineFixture F; if (!F.Create(*this)) { return false; }
        if (!TestTrue(TEXT("Seed active door"),F.Seed({},{{{8,8,0},0,false}}))) { AddError(F.Result); return false; }
        F.Pawn->SetActorLocation(FVector(425,650,100));
        const uint64 Revision = F.State->GetRevision();
        TestFalse(TEXT("Canonical door reservation blocks line"),F.Apply({8,9,0},{9,9,0}));
        TestEqual(TEXT("Door reservation failure preserves revision"),F.State->GetRevision(),Revision);
        TestTrue(TEXT("Door reservation failure preserves real closed projection"),F.State->FindDoor({8,8,0})->Projection->IsProjectionIntact());
    }
    {
        FLineFixture F; if (!F.Create(*this)) { return false; }
        F.State->TestTimeOverride = 0;
        if (!TestTrue(TEXT("Seed supported raised door and shared column"),F.Seed(
            {{{8,8,0},0,Rotation},{{8,8,1},0,Rotation},{{9,8,1},0,Rotation}},{{{8,8,2},0,false}}))) { AddError(F.Result); return false; }
        F.Pawn->SetActorLocation(FVector(425,625,75));
        FSFConstructionSandboxIntent Remove; Remove.Cell = {8,8,0}; Remove.Rotation = Rotation;
        Remove.ExpectedRevision = F.State->GetRevision(); Remove.bRemove = true;
        TArray<FIntVector> Affected;
        if (!TestTrue(TEXT("Real support-loss confirmation preview"),F.State->PreviewRemoval(F.Controller,Remove,Affected,F.Result))) { AddError(F.Result); return false; }
        TestTrue(TEXT("Pending authority confirmation exists"),F.State->ConfirmationController.Get() == F.Controller);
        TestFalse(TEXT("Failed line preserves pending authority confirmation"),F.Apply({8,8,0},{9,8,0}));
        TestTrue(TEXT("Rejected line does not cancel confirmation"),F.State->ConfirmationController.Get() == F.Controller);
        Remove.bConfirmSupportLoss = true;
        if (!TestTrue(TEXT("Confirmed removal creates actual dual grace"),F.State->Apply(F.Controller,Remove,F.Result))) { AddError(F.Result); return false; }
        double StructuralDeadline = -1, DoorDeadline = -1;
        TestTrue(TEXT("Structural grace recorded"),F.State->Instability.GetDeadline({8,8,1},StructuralDeadline));
        TestTrue(TEXT("Door grace recorded"),F.State->DoorInstability.GetDeadline({8,8,2},DoorDeadline));
        F.State->TestTimeOverride = 2;
        F.Pawn->SetActorLocation(FVector(-175,225,100));
        TestFalse(TEXT("Grace never exempts a new unsupported line"),F.Apply({-4,0,3},{-3,0,3}));
        if (!TestTrue(TEXT("Unrelated supported line admitted during existing dual grace"),F.Apply({-4,0,0},{-3,0,0}))) { AddError(F.Result); return false; }
        double Deadline = -1;
        F.State->Instability.GetDeadline({8,8,1},Deadline); TestEqual(TEXT("Line preserves structural deadline"),Deadline,StructuralDeadline);
        F.State->DoorInstability.GetDeadline({8,8,2},Deadline); TestEqual(TEXT("Line preserves door deadline"),Deadline,DoorDeadline);
        const uint64 Revision = F.State->GetRevision();
        F.State->TestTimeOverride = StructuralDeadline;
        TestFalse(TEXT("Exact due wave fences line before state tick"),F.Apply({-2,0,0},{-1,0,0}));
        TestEqual(TEXT("Due failure preserves revision"),F.State->GetRevision(),Revision);
        F.State->TestTimeOverride = 3;
        F.Pawn->SetActorLocation(FVector(425,625,75));
        F.State->TestFailAfterPreparedCount = 1;
        TestFalse(TEXT("Repair line staged failure preserves dual grace"),F.Apply({8,8,0},{9,8,0}));
        TestTrue(TEXT("Repair rollback reached real preparation"),F.Result.Contains(TEXT("interrupted after prepared")));
        F.State->Instability.GetDeadline({8,8,1},Deadline); TestEqual(TEXT("Repair rollback retains structural deadline"),Deadline,StructuralDeadline);
        F.State->DoorInstability.GetDeadline({8,8,2},Deadline); TestEqual(TEXT("Repair rollback retains door deadline"),Deadline,DoorDeadline);
        TestEqual(TEXT("Repair rollback retains revision"),F.State->GetRevision(),Revision);
        F.State->TestFailAfterPreparedCount = INDEX_NONE;
        if (!TestTrue(TEXT("One whole repair line restores structure and door"),F.Apply({8,8,0},{9,8,0}))) { AddError(F.Result); return false; }
        TestEqual(TEXT("Recovery cancels structural episodes"),F.State->Instability.Num(),0);
        TestEqual(TEXT("Recovery cancels door episode"),F.State->DoorInstability.Num(),0);
        TestEqual(TEXT("Whole repair publishes exactly once"),F.State->GetRevision(),Revision+1);
        TestFalse(TEXT("Successful line clears authority confirmation"),F.State->ConfirmationController.IsValid());
    }
    return true;
}
#endif
