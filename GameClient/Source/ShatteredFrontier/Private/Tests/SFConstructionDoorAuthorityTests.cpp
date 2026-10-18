#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionClearance.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/SFE1GameMode.h"

namespace
{
struct FDoorAuthorityFixture
{
    FTestWorldWrapper Fixture;
    UWorld* World = nullptr;
    ASFConstructionSandboxController* Controller = nullptr;
    ADefaultPawn* Pawn = nullptr;
    ASFConstructionSandboxState* State = nullptr;
    AStaticMeshActor* Foundation = nullptr;
    FString Result;
    bool Create(FAutomationTestBase& Test, bool bSandbox = true)
    {
        if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
        World = Fixture.GetTestWorld();
        World->GetWorldSettings()->DefaultGameMode = bSandbox ? ASFConstructionSandboxGameMode::StaticClass() : ASFE1GameMode::StaticClass();
        if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { return false; }
        World->GetWorldSettings()->NotifyBeginPlay();
        Foundation = World->SpawnActor<AStaticMeshActor>();
        Foundation->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Foundation->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Foundation->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        Foundation->SetActorLocation(FVector(0,0,-50));
        Foundation->SetActorScale3D(FVector(20,20,1));
        Foundation->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
        Controller = World->SpawnActorDeferred<ASFConstructionSandboxController>(ASFConstructionSandboxController::StaticClass(),FTransform::Identity);
        if (!Controller) { return false; }
        Controller->SetAsLocalPlayerController();
        Controller->FinishSpawning(FTransform::Identity);
        Pawn = World->SpawnActor<ADefaultPawn>(FVector(-500,-500,100),FRotator::ZeroRotator);
        if (!Pawn) { return false; }
        Controller->Possess(Pawn);
        for (TActorIterator<ASFConstructionSandboxState> It(World); It; ++It) { State = *It; break; }
        if (!State) { State = World->SpawnActor<ASFConstructionSandboxState>(); }
        return Test.TestNotNull(TEXT("Door authority fixture state"),State);
    }
    FSFConstructionDoorIntent DoorIntent(FIntVector Origin, ESFConstructionDoorAction Action = ESFConstructionDoorAction::Place, int32 Yaw = 0)
    {
        FSFConstructionDoorProjectionProfile Profile;
        if (ASFConstructionSandboxDoorProjection::BuildProfile(Origin,Yaw,false,Profile))
        { Pawn->SetActorLocation(Profile.Transform.TransformPosition(FVector(-125,25,100))); }
        FSFConstructionDoorIntent Intent;
        Intent.OriginCell = Origin; Intent.Action = Action; Intent.YawQuarterTurns = Yaw; Intent.ExpectedRevision = State->GetRevision();
        return Intent;
    }
    FSFConstructionSandboxIntent StructuralIntent(FIntVector Cell, bool bRemove = false)
    {
        // Approach along Y so another X-adjacent floor receiver cannot occlude
        // this target. The raised door is above this floor-facing sight line.
        Pawn->SetActorLocation(ASFConstructionSandboxState::CellCorner(Cell)+FVector(25,225,75));
        FSFConstructionSandboxIntent Intent;
        Intent.Cell = Cell; Intent.bRemove = bRemove; Intent.Rotation = SFConstructionGeometry::IdentityRotation(); Intent.ExpectedRevision = State->GetRevision();
        return Intent;
    }
    bool Place(FIntVector Cell, int32 Shape = 0, int32 Rotation = INDEX_NONE)
    {
        auto Intent = StructuralIntent(Cell); Intent.Shape = Shape;
        if (Rotation != INDEX_NONE) { Intent.Rotation = Rotation; }
        return State->Apply(Controller,Intent,Result);
    }
    bool Door(FIntVector Origin, ESFConstructionDoorAction Action = ESFConstructionDoorAction::Place, int32 Yaw = 0)
    { return State->ApplyDoor(Controller,DoorIntent(Origin,Action,Yaw),Result); }
    bool Remove(FIntVector Cell, bool bConfirm = true)
    {
        auto Intent = StructuralIntent(Cell,true);
        if (bConfirm)
        {
            TArray<FIntVector> Affected;
            if (!State->PreviewRemoval(Controller,Intent,Affected,Result)) { return false; }
            Intent.bConfirmSupportLoss = !Affected.IsEmpty();
        }
        return State->Apply(Controller,Intent,Result);
    }
    ASFConstructionSandboxPreview* Projection(FIntVector Cell)
    {
        for (TActorIterator<ASFConstructionSandboxPreview> It(World); It; ++It)
        { FIntVector Found; if (State->FindCell(*It,Found) && Found == Cell) { return *It; } }
        return nullptr;
    }
    UBoxComponent* Box(FVector Centre, FVector Extent)
    {
        auto* Actor = World->SpawnActor<AActor>();
        if (!Actor) { return nullptr; }
        auto* Component = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Component);
        Component->SetBoxExtent(Extent);
        Component->SetCollisionProfileName(TEXT("BlockAll"));
        Component->SetGenerateOverlapEvents(false);
        Component->SetWorldLocation(Centre);
        Component->RegisterComponent();
        return Component;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionDoorAuthorityTest,"ShatteredFrontier.Construction.DoorAuthority.ValidationReservationsAndPublication",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionDoorAuthorityTest::RunTest(const FString& Parameters)
{
    using Action = ESFConstructionDoorAction;
    {
        FDoorAuthorityFixture F;
        if (!F.Create(*this)) { return false; }
        const FBox Touch(FVector(400,400,0),FVector(500,450,200));
        TestTrue(TEXT("Reservation permits actual exact foundation contact"),SFConstructionClearance::EvaluateBox(F.World,Touch)==SFConstructionClearance::EResult::Clear);
        const FBox Intrusion(Touch.Min-FVector(0,0,0.01),Touch.Max);
        TestTrue(TEXT("Reservation still rejects one-hundredth centimetre actual intrusion"),SFConstructionClearance::EvaluateBox(F.World,Intrusion)==SFConstructionClearance::EResult::Blocked);
        const FBox Above(FVector(200,200,50),FVector(300,250,250));
        TestTrue(TEXT("Exact canonical full-floor contact is clear"),SFConstructionClearance::IntersectsReservation(0,SFConstructionGeometry::IdentityRotation(),FVector(200,200,0),Above)==SFConstructionClearance::EResult::Clear);
        TestTrue(TEXT("Exact canonical positive floor intrusion rejects"),SFConstructionClearance::IntersectsReservation(0,SFConstructionGeometry::IdentityRotation(),FVector(200,200,0),FBox(Above.Min-FVector(0,0,0.01),Above.Max))==SFConstructionClearance::EResult::Blocked);
    }
    {
        auto* Settings = NewObject<USFConstructionSandboxSettings>();
        FString Error;
        TestEqual(TEXT("Shipped door budget"),Settings->MaximumDoors,16);
        Settings->MaximumDoors = 0; TestFalse(TEXT("Zero door budget rejects"),Settings->Validate(Error));
        Settings->MaximumDoors = 65; TestFalse(TEXT("Excess door budget rejects"),Settings->Validate(Error));
        Settings->MaximumDoors = 64; TestTrue(TEXT("Bounded door budget validates"),Settings->Validate(Error));
    }
    for (int32 Yaw = 0; Yaw < 4; ++Yaw)
    {
        FDoorAuthorityFixture F;
        if (!F.Create(*this)) { return false; }
        const FIntVector Origin(10,10,0);
        auto Intent = F.DoorIntent(Origin,Action::Place,Yaw);
        TestTrue(TEXT("Evaluation accepts direct known-foundation door without spawning"),F.State->EvaluateDoor(F.Controller,Intent).IsEmpty());
        TestEqual(TEXT("Read-only evaluation does not allocate door"),F.State->GetDoorCount(),0);
        TestTrue(TEXT("All four yaw foundation placements commit"),F.State->ApplyDoor(F.Controller,Intent,F.Result));
        if (!F.State->FindDoor(Origin)) { AddError(F.Result); return false; }
        const uint64 Revision = F.State->GetRevision();
        TestFalse(TEXT("Accepted place replay rejects"),F.State->ApplyDoor(F.Controller,Intent,F.Result));
        TestEqual(TEXT("Replay preserves common revision"),F.State->GetRevision(),Revision);
        auto* Closed = F.State->FindDoor(Origin)->Projection.Get();
        TestTrue(TEXT("Door record points to intact active closed projection"),Closed && Closed->IsActive() && Closed->IsProjectionIntact() && !Closed->IsOpen());
        FIntVector Found;
        TestTrue(TEXT("Projection lookup preserves origin identity"),F.State->FindDoorOrigin(Closed,Found) && Found == Origin);
        auto Toggle = F.DoorIntent(Origin,Action::Toggle,Yaw);
        FSFConstructionDoorProjectionProfile Profile;
        ASFConstructionSandboxDoorProjection::BuildProfile(Origin,Yaw,false,Profile);
        F.Pawn->SetActorLocation(Profile.Transform.TransformPosition(FVector(50,70,100)));
        TestFalse(TEXT("Acting character in swing blocks toggle"),F.State->ApplyDoor(F.Controller,Toggle,F.Result));
        TestTrue(TEXT("Blocked toggle preserves old physics pose"),F.State->FindDoor(Origin)->Projection.Get() == Closed && !Closed->IsOpen());
        TestTrue(TEXT("Clear conservative motion permits endpoint swap"),F.Door(Origin,Action::Toggle,Yaw));
        auto* Open = F.State->FindDoor(Origin)->Projection.Get();
        TestTrue(TEXT("Published open pose has real intact collision"),Open && Open != Closed && Open->IsOpen() && Open->IsProjectionIntact());
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(DoorAuthoritySweep),false);
        Query.AddIgnoredActor(F.Pawn);
        TestFalse(TEXT("Authoritative open pose permits inherited capsule"),F.World->SweepSingleByChannel(Hit,
            Profile.Transform.TransformPosition(FVector(52.5,-100,100)),Profile.Transform.TransformPosition(FVector(52.5,150,100)),
            FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(34,88),Query));
        TestTrue(TEXT("Authority removal frees record and reservations"),F.Door(Origin,Action::Remove,Yaw));
        TestEqual(TEXT("Door removed"),F.State->GetDoorCount(),0);
    }
    {
        FDoorAuthorityFixture F;
        if (!F.Create(*this)) { return false; }
        auto Invalid = F.DoorIntent({4,4,0});
        Invalid.Action = static_cast<Action>(99);
        TestFalse(TEXT("Forged action enum rejects"),F.State->ApplyDoor(F.Controller,Invalid,F.Result));
        Invalid = F.DoorIntent({4,4,0}); Invalid.YawQuarterTurns = 4;
        TestFalse(TEXT("Forged yaw rejects"),F.State->ApplyDoor(F.Controller,Invalid,F.Result));
        Invalid = F.DoorIntent({19,4,0});
        TestFalse(TEXT("Envelope outside plot rejects"),F.State->ApplyDoor(F.Controller,Invalid,F.Result));
        Invalid = F.DoorIntent({4,4,0}); Invalid.ExpectedRevision = 99;
        TestFalse(TEXT("Stale door edit rejects"),F.State->ApplyDoor(F.Controller,Invalid,F.Result));
        Invalid = F.DoorIntent({4,4,0}); F.Pawn->SetOwner(nullptr);
        TestFalse(TEXT("Unowned pawn rejects"),F.State->ApplyDoor(F.Controller,Invalid,F.Result));
        F.Pawn->SetOwner(F.Controller);
        F.Pawn->SetActorLocation(FVector(-900,-900,100));
        TestFalse(TEXT("Out of reach rejects"),F.State->ApplyDoor(F.Controller,Invalid,F.Result));
        F.DoorIntent({4,4,0});
        auto* Obstruction = F.Box(FVector(140,225,100),FVector(5,20,70));
        TestFalse(TEXT("Actual simple line of sight obstruction rejects"),F.State->ApplyDoor(F.Controller,Invalid,F.Result));
        Obstruction->GetOwner()->Destroy();
        Obstruction = F.Box(FVector(250,160,100),FVector(5,5,5));
        TestFalse(TEXT("Approach reservation blocks even without leaf collision"),F.State->ApplyDoor(F.Controller,Invalid,F.Result));
        Obstruction->GetOwner()->Destroy();
        TestTrue(TEXT("Clear ground door accepted"),F.Door({4,4,0}));
        if (!F.State->FindDoor({4,4,0})) { AddError(F.Result); return false; }
        TestFalse(TEXT("Other door reservation overlaps reject"),F.Door({4,6,0}));
        const auto Structural = F.StructuralIntent({4,2,0});
        // Inspect the front approach from its unobstructed side, so this probe
        // reaches the reservation gate rather than correctly failing door LoS.
        F.Pawn->SetActorLocation(ASFConstructionSandboxState::CellCorner(Structural.Cell)+FVector(25,-225,75));
        TestTrue(TEXT("Structural candidate reports reservation invasion"),F.State->Evaluate(F.Controller,Structural).Contains(TEXT("reservation")));
        TestFalse(TEXT("Structural candidate cannot invade approach"),F.State->Apply(F.Controller,Structural,F.Result));
        auto* Door = F.State->FindDoor({4,4,0})->Projection.Get();
        const FTransform Original = Door->GetActorTransform();
        Door->AddActorWorldOffset(FVector(300,0,0));
        TestTrue(TEXT("General canonical clearance fences broken active projection globally"),SFConstructionClearance::Evaluate(F.World,0,0,FVector(-500,-500,300)) == SFConstructionClearance::EResult::Unavailable);
        TestFalse(TEXT("Moved active door fences edits"),F.Place({-4,-4,0}));
        Door->SetActorTransform(Original);
        F.State->MaximumDoors = 1;
        TestFalse(TEXT("Captured door budget enforced"),F.Door({-6,4,0}));
        F.Foundation->SetActorLocation(FVector(0,0,-60));
        TestFalse(TEXT("Missing known foundation fences door edits"),F.Door({4,4,0},Action::Remove));
    }
    {
        FDoorAuthorityFixture F;
        if (!F.Create(*this,false)) { return false; }
        TestFalse(TEXT("Normal E1 cannot mutate manually injected door state"),F.Door({4,4,0}));
        TestEqual(TEXT("Normal mode has no door output"),F.State->GetDoorCount(),0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionDoorLifecycleTest,"ShatteredFrontier.Construction.DoorAuthority.SupportGraceWavesAndRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionDoorLifecycleTest::RunTest(const FString& Parameters)
{
    using Action = ESFConstructionDoorAction;
    {
        FDoorAuthorityFixture F;
        if (!F.Create(*this)) { return false; }
        F.State->TestTimeOverride = 0;
        TestFalse(TEXT("Floating raised door rejects"),F.Door({4,4,1}));
        TestTrue(TEXT("First full floor receiver"),F.Place({4,4,0}));
        TestFalse(TEXT("One receiver cannot replace fixed missing share"),F.Door({4,4,1}));
        TestTrue(TEXT("Second full floor receiver"),F.Place({5,4,0}));
        TestTrue(TEXT("Raised supported door commits"),F.Door({4,4,1}));
        if (!F.State->FindDoor({4,4,1})) { AddError(F.Result); return false; }
        TArray<FSFConstructionSupportResult> Structural;
        TArray<FSFConstructionDoorSupportResult> Doors;
        TestTrue(TEXT("Authority combined load snapshot evaluates"),F.State->SolveCombined(F.State->Members,F.State->Doors,Structural,Doors).IsEmpty());
        for (const auto& Entry : Structural) { TestTrue(TEXT("Each full foundation receiver carries retained half door load"),FMath::IsNearlyEqual(Entry.LoadRatio,1.5/64.0)); }
        auto Removal = F.StructuralIntent({4,4,0},true);
        Removal.bConfirmSupportLoss = true;
        TestFalse(TEXT("Truth flag cannot forge door support-loss confirmation"),F.State->Apply(F.Controller,Removal,F.Result));
        Removal.bConfirmSupportLoss = false;
        TArray<FIntVector> Affected;
        FSFConstructionRemovalConsequences Consequences;
        if (!TestTrue(TEXT("Door-only support loss can be previewed"),F.State->PreviewRemoval(F.Controller,Removal,Affected,F.Result,&Consequences))) { AddError(F.Result); return false; }
        TestEqual(TEXT("No structural survivor loses support"),Consequences.StructuralCells.Num(),0);
        TestEqual(TEXT("Distinct door origin is listed"),Consequences.DoorOrigins.Num(),1);
        TestEqual(TEXT("Legacy nonempty confirmation path includes door-only loss"),Affected.Num(),1);
        TestTrue(TEXT("Unrelated successful door edit invalidates structural confirmation"),F.Door({-6,-6,0}));
        Removal.ExpectedRevision = F.State->GetRevision(); Removal.bConfirmSupportLoss = true;
        F.StructuralIntent(Removal.Cell,true);
        TestFalse(TEXT("Refreshing only revision cannot resurrect old door-loss confirmation"),F.State->Apply(F.Controller,Removal,F.Result));
        TestTrue(TEXT("Remove unrelated direct-foundation door"),F.Door({-6,-6,0},Action::Remove));
        Removal = F.StructuralIntent({4,4,0},true);
        TestTrue(TEXT("Fresh door-only loss preview can be cancelled"),F.State->PreviewRemoval(F.Controller,Removal,Affected,F.Result,&Consequences));
        F.State->CancelRemoval(F.Controller);
        Removal.bConfirmSupportLoss = true;
        TestFalse(TEXT("Cancelled door support loss rejects"),F.State->Apply(F.Controller,Removal,F.Result));
        TestTrue(TEXT("Confirmed receiver removal starts door-only grace"),F.Remove({4,4,0}));
        TestEqual(TEXT("Structural clock remains empty"),F.State->Instability.Num(),0);
        TestEqual(TEXT("Door clock separately tracks episode"),F.State->DoorInstability.Num(),1);
        double Deadline = -1;
        F.State->GetDoorDeadline({4,4,1},Deadline);
        TestEqual(TEXT("Door deadline exact"),Deadline,10.0);
        TestFalse(TEXT("Unstable door cannot toggle"),F.Door({4,4,1},Action::Toggle));
        const auto* GraceDoor = F.State->FindDoor({4,4,1})->Projection.Get();
        auto* RetainedReceiver = F.Projection({5,4,0});
        if (!RetainedReceiver) { AddError(TEXT("Retained receiver missing")); return false; }
        RetainedReceiver->SetActorScale3D(FVector(.5));
        const uint64 BeforeFailedRepair = F.State->GetRevision();
        F.State->TestTimeOverride = 1;
        TestFalse(TEXT("Failed recovery staging preserves support-loss episode"),F.Place({4,4,0}));
        TestEqual(TEXT("Failed repair preserves common revision"),F.State->GetRevision(),BeforeFailedRepair);
        TestTrue(TEXT("Failed repair preserves door endpoint"),F.State->FindDoor({4,4,1})->Projection.Get() == GraceDoor);
        F.State->GetDoorDeadline({4,4,1},Deadline);
        TestEqual(TEXT("Failed repair cannot extend door deadline"),Deadline,10.0);
        RetainedReceiver->SetActorScale3D(FVector::OneVector);
        F.State->TestTimeOverride = 2;
        TestTrue(TEXT("Supported structural receiver recovery permitted"),F.Place({4,4,0}));
        TestEqual(TEXT("Receiver repair cancels door episode"),F.State->DoorInstability.Num(),0);
        F.State->TestTimeOverride = 4;
        TestTrue(TEXT("New loss starts distinct episode"),F.Remove({4,4,0}));
        F.State->GetDoorDeadline({4,4,1},Deadline);
        TestEqual(TEXT("New door episode deadline"),Deadline,14.0);
        const uint64 Revision = F.State->GetRevision();
        F.State->TestTimeOverride = 14;
        TestFalse(TEXT("Exact deadline fences recovery before tick"),F.Place({4,4,0}));
        TestFalse(TEXT("Exact deadline fences direct door removal before tick"),F.Door({4,4,1},Action::Remove));
        F.State->ProcessDeadlines(14);
        TestEqual(TEXT("Door-only wave removes due door"),F.State->GetDoorCount(),0);
        TestEqual(TEXT("Door-only wave publishes one common revision"),F.State->GetRevision(),Revision+1);
        TestNotNull(TEXT("Other grounded receiver survives door collapse"),F.Projection({5,4,0}));
    }
    {
        FDoorAuthorityFixture F;
        if (!F.Create(*this)) { return false; }
        F.State->TestTimeOverride = 0;
        TestTrue(TEXT("Mixed-wave sole anchor"),F.Place({4,4,0}));
        TestTrue(TEXT("Mixed-wave first raised receiver"),F.Place({4,4,1}));
        TestTrue(TEXT("Mixed-wave second raised receiver supported laterally"),F.Place({5,4,1}));
        TestTrue(TEXT("Mixed-wave raised door"),F.Door({4,4,2}));
        if (!F.State->FindDoor({4,4,2})) { AddError(F.Result); return false; }
        TestTrue(TEXT("Independent noncontact neighbor anchor"),F.Place({6,4,0}));
        int32 FarRotation = INDEX_NONE;
        for (int32 Rotation = 0; Rotation < 24; ++Rotation)
        {
            FSFConstructionSolid Solid; SFConstructionGeometry::Build(1,Rotation,Solid);
            if (FBox(Solid.Vertices).Min.Equals(FVector(25,0,0)) && FBox(Solid.Vertices).Max.Equals(FVector(50,50,50))) { FarRotation = Rotation; break; }
        }
        if (!TestTrue(TEXT("Known noncontact slab orientation"),FarRotation != INDEX_NONE)) { return false; }
        TestTrue(TEXT("Independent survivor does not bridge receiver gap"),F.Place({6,4,1},1,FarRotation));
        auto Removal = F.StructuralIntent({4,4,0},true);
        TArray<FIntVector> Affected;
        FSFConstructionRemovalConsequences Consequences;
        if (!TestTrue(TEXT("Mixed consequence preview"),F.State->PreviewRemoval(F.Controller,Removal,Affected,F.Result,&Consequences))) { AddError(F.Result); return false; }
        TestEqual(TEXT("Two distinct receiver cells lose structural support"),Consequences.StructuralCells.Num(),2);
        TestEqual(TEXT("One distinct functional origin loses support"),Consequences.DoorOrigins.Num(),1);
        Removal.bConfirmSupportLoss = true;
        TestTrue(TEXT("Bound confirmation starts both real lifecycle maps"),F.State->Apply(F.Controller,Removal,F.Result));
        TestEqual(TEXT("Two structural deadlines"),F.State->Instability.Num(),2);
        TestEqual(TEXT("One functional deadline"),F.State->DoorInstability.Num(),1);
        double StructuralDeadline = -1, DoorDeadline = -1;
        F.State->Instability.GetDeadline({4,4,1},StructuralDeadline);
        F.State->DoorInstability.GetDeadline({4,4,2},DoorDeadline);
        TestEqual(TEXT("Mixed structural deadline"),StructuralDeadline,10.0);
        TestEqual(TEXT("Mixed door deadline"),DoorDeadline,10.0);
        auto* Receiver = F.Projection({4,4,1});
        auto* Survivor = F.Projection({6,4,1});
        auto* Door = F.State->FindDoor({4,4,2})->Projection.Get();
        if (!Receiver || !Survivor || !Door) { AddError(TEXT("Mixed-wave physical setup missing")); return false; }
        const auto* ReceiverMesh = Receiver->GetMesh();
        const uint64 Before = F.State->GetRevision();
        const auto TraceDoor = [&]()
        {
            FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(DoorWaveRollback),false);
            return F.World->LineTraceSingleByChannel(Hit,FVector(250,175,200),FVector(250,275,200),ECC_Visibility,Query) && Hit.GetActor() == Door;
        };
        TestTrue(TEXT("Due door still has old closed-leaf collision"),TraceDoor());
        F.State->TestFailAfterPreparedCount = 1;
        F.State->TestTimeOverride = 10;
        AddExpectedError(TEXT("CONSTRUCTION_COLLAPSE_RETRY"),EAutomationExpectedErrorFlags::Contains,1);
        F.State->ProcessDeadlines(10);
        TestEqual(TEXT("Failed mixed wave preserves common revision"),F.State->GetRevision(),Before);
        TestEqual(TEXT("Failed mixed wave preserves all structural members"),F.State->GetMemberCount(),4);
        TestTrue(TEXT("Failed mixed wave preserves receiver projection and mesh"),F.Projection({4,4,1}) == Receiver && Receiver->GetMesh() == ReceiverMesh);
        TestTrue(TEXT("Failed mixed wave preserves door endpoint and actual collision"),F.State->FindDoor({4,4,2})->Projection.Get() == Door && Door->IsProjectionIntact() && TraceDoor());
        F.State->Instability.GetDeadline({4,4,1},StructuralDeadline);
        F.State->DoorInstability.GetDeadline({4,4,2},DoorDeadline);
        TestEqual(TEXT("Failed mixed wave preserves structural deadline"),StructuralDeadline,10.0);
        TestEqual(TEXT("Failed mixed wave preserves functional deadline"),DoorDeadline,10.0);
        F.State->TestFailAfterPreparedCount = INDEX_NONE;
        F.State->TestTimeOverride = 10.25;
        F.State->ProcessDeadlines(10.25);
        TestEqual(TEXT("Successful retry publishes one common revision"),F.State->GetRevision(),Before+1);
        TestEqual(TEXT("Successful mixed wave removes only due receivers"),F.State->GetMemberCount(),2);
        TestEqual(TEXT("Successful mixed wave removes due door"),F.State->GetDoorCount(),0);
        TestEqual(TEXT("Structural episode consumed only after commit"),F.State->Instability.Num(),0);
        TestEqual(TEXT("Door episode consumed only after commit"),F.State->DoorInstability.Num(),0);
        TestFalse(TEXT("Successful wave removes old door collision"),TraceDoor());
    }
    {
        FDoorAuthorityFixture F;
        if (!F.Create(*this)) { return false; }
        F.State->TestTimeOverride = 0;
        TestTrue(TEXT("Independent stable foundation door"),F.Door({-6,4,0}));
        TestTrue(TEXT("Receiver A"),F.Place({4,4,0})); TestTrue(TEXT("Receiver B"),F.Place({5,4,0}));
        TestTrue(TEXT("Raised door"),F.Door({4,4,1}));
        TestTrue(TEXT("Door loss at zero"),F.Remove({4,4,0}));
        TestTrue(TEXT("Independent structural base"),F.Place({12,-4,0})); TestTrue(TEXT("Independent structural top"),F.Place({12,-4,1}));
        F.State->TestTimeOverride = 2;
        TestTrue(TEXT("Structural loss starts later"),F.Remove({12,-4,0}));
        F.State->TestTimeOverride = 10;
        F.State->ProcessDeadlines(10);
        TestEqual(TEXT("Only raised due door removed"),F.State->GetDoorCount(),1);
        TestNotNull(TEXT("Non-due structural wave survives"),F.Projection({12,-4,1}));
        double Deadline;
        F.State->Instability.GetDeadline({12,-4,1},Deadline);
        TestEqual(TEXT("Door wave does not extend structural deadline"),Deadline,12.0);
        F.State->TestTimeOverride = 12;
        F.State->ProcessDeadlines(12);
        TestNull(TEXT("Later structural wave resolves separately"),F.Projection({12,-4,1}));
        TestNotNull(TEXT("Independent foundation door survives both waves"),F.State->FindDoor({-6,4,0}));
    }
    {
        FDoorAuthorityFixture F;
        if (!F.Create(*this)) { return false; }
        F.State->TestTimeOverride = 0;
        TestTrue(TEXT("Rollback ground door"),F.Door({-6,4,0}));
        TestTrue(TEXT("Rollback structural member"),F.Place({4,4,0}));
        if (!F.State->FindDoor({-6,4,0}) || !F.Projection({4,4,0})) { AddError(F.Result); return false; }
        auto* Structural = F.Projection({4,4,0});
        auto* Door = F.State->FindDoor({-6,4,0})->Projection.Get();
        const auto* Mesh = Structural->GetMesh();
        const uint64 Revision = F.State->GetRevision();
        // Exercise the common private staging transaction with an actual failed
        // structural precondition after a new door endpoint has been prepared.
        Structural->SetActorScale3D(FVector(.5));
        auto CandidateDoors = F.State->Doors;
        CandidateDoors.FindChecked({-6,4,0}).bOpen = true;
        TestFalse(TEXT("Later structural staging failure aborts prepared door swap"),F.State->CommitCombined(F.State->Members,CandidateDoors,{{4,4,0}},0,F.Result));
        TestEqual(TEXT("Rollback preserves revision"),F.State->GetRevision(),Revision);
        TestTrue(TEXT("Rollback preserves old closed projection and collision"),F.State->FindDoor({-6,4,0})->Projection.Get() == Door && !Door->IsOpen() && Door->IsProjectionIntact());
        TestTrue(TEXT("Rollback preserves structural mesh"),Structural->GetMesh() == Mesh);
        int32 LiveDoors = 0;
        for (TActorIterator<ASFConstructionSandboxDoorProjection> It(F.World); It; ++It) { if (!It->IsActorBeingDestroyed()) { ++LiveDoors; } }
        TestEqual(TEXT("Aborted staged endpoint leaves no live extra door"),LiveDoors,1);
        Structural->SetActorScale3D(FVector::OneVector);
        Door->Destroy();
        TestFalse(TEXT("Missing owned door projection fences structural edits"),F.Place({8,-4,0}));
    }
    return true;
}
#endif
