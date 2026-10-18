#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Construction/SFConstructionClearance.h"
#include "Construction/SFConstructionGeometry.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/SFE1GameMode.h"
#include "ProceduralMeshComponent.h"

namespace
{
struct FRestoreFixture
{
    FTestWorldWrapper Fixture;
    UWorld* World = nullptr;
    ASFConstructionSandboxController* Controller = nullptr;
    ADefaultPawn* Pawn = nullptr;
    ASFConstructionSandboxState* State = nullptr;
    AStaticMeshActor* Floor = nullptr;
    ASFConstructionSandboxPreview* Reference = nullptr;
    FString Result;
    bool Create(FAutomationTestBase& Test,bool bSandbox = true)
    {
        if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
        World = Fixture.GetTestWorld();
        World->GetWorldSettings()->DefaultGameMode = bSandbox ? ASFConstructionSandboxGameMode::StaticClass() : ASFE1GameMode::StaticClass();
        if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { return false; }
        World->GetWorldSettings()->NotifyBeginPlay();
        Floor = World->SpawnActor<AStaticMeshActor>();
        Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        Floor->SetActorLocation(FVector(0,0,-50)); Floor->SetActorScale3D(FVector(20,20,1));
        Floor->Tags.Add(TEXT("SFConstructionSandboxFoundationV1"));
        Controller = World->SpawnActorDeferred<ASFConstructionSandboxController>(ASFConstructionSandboxController::StaticClass(),FTransform::Identity);
        if (!Controller) { return false; }
        Controller->SetAsLocalPlayerController(); Controller->FinishSpawning(FTransform::Identity);
        Pawn = World->SpawnActor<ADefaultPawn>(FVector(0,-600,100),FRotator::ZeroRotator);
        if (!Pawn) { return false; }
        Controller->Possess(Pawn);
        for (TActorIterator<ASFConstructionSandboxState> It(World); It; ++It) { State = *It; break; }
        if (!State) { State = World->SpawnActor<ASFConstructionSandboxState>(); }
        if (!State) { return false; }
        if (bSandbox)
        {
            Reference = World->SpawnActor<ASFConstructionSandboxPreview>(FVector(800,0,0),FRotator::ZeroRotator);
            if (!Reference || !Reference->Rebuild(0,SFConstructionGeometry::IdentityRotation(),1)) { return false; }
        }
        return Test.TestNotNull(TEXT("Finite restore authority fixture"),State);
    }
    FSFConstructionSandboxSnapshot Initial() const
    {
        FSFConstructionSandboxSnapshot Snapshot;
        Snapshot.SourceRevision = 9999;
        const int32 Rotation = SFConstructionGeometry::IdentityRotation();
        Snapshot.Cells = {{{8,8,0},0,Rotation},{{9,8,0},0,Rotation}};
        Snapshot.Doors = {{{-6,8,0},0,false}};
        return Snapshot;
    }
    bool Import(const FSFConstructionSandboxSnapshot& Snapshot)
    { return State->ImportSnapshot(Controller,State->GetRevision(),Snapshot,Result); }
    ASFConstructionSandboxPreview* Projection(FIntVector Cell) const
    {
        for (TActorIterator<ASFConstructionSandboxPreview> It(World); It; ++It)
        { FIntVector Found; if (State->FindCell(*It,Found) && Found == Cell) { return *It; } }
        return nullptr;
    }
    bool Trace(double X,double Z,const AActor* Expected) const
    {
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(SnapshotRestoreCollision),false);
        return World->LineTraceSingleByChannel(Hit,FVector(X,380,Z),FVector(X,470,Z),ECC_Visibility,Query) && Hit.GetActor() == Expected;
    }
    FSFConstructionSandboxIntent Removal(FIntVector Cell)
    {
        Pawn->SetActorLocation(ASFConstructionSandboxState::CellCorner(Cell)+FVector(25,225,75));
        FSFConstructionSandboxIntent Intent;
        Intent.Cell = Cell; Intent.bRemove = true; Intent.Rotation = SFConstructionGeometry::IdentityRotation(); Intent.ExpectedRevision = State->GetRevision();
        return Intent;
    }
    UBoxComponent* ExtraBox(AActor* Owner,FVector WorldCentre)
    {
        if (!Owner) { Owner = World->SpawnActor<AActor>(); }
        auto* Box = NewObject<UBoxComponent>(Owner);
        if (Owner->GetRootComponent()) { Box->SetupAttachment(Owner->GetRootComponent()); }
        else { Owner->SetRootComponent(Box); }
        Box->SetBoxExtent(FVector(5)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->SetGenerateOverlapEvents(false);
        Box->SetWorldLocation(WorldCentre); Box->RegisterComponent(); return Box;
    }
};
double BoundaryArea(UProceduralMeshComponent* Mesh)
{
    const auto* Section = Mesh->GetProcMeshSection(0);
    if (!Section) { return 0; }
    double Sum = 0;
    for (int32 I = 0; I < Section->ProcIndexBuffer.Num(); I += 3)
    {
        const FVector A = Section->ProcVertexBuffer[Section->ProcIndexBuffer[I]].Position;
        const FVector B = Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+1]].Position;
        const FVector C = Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+2]].Position;
        if (A.X == 0 && B.X == 0 && C.X == 0) { Sum += FVector::CrossProduct(B-A,C-A).Size()*.5; }
    }
    return Sum;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionSnapshotRestoreTest,"ShatteredFrontier.Construction.SnapshotRestore.AuthorityProjectionAndRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionSnapshotRestoreTest::RunTest(const FString& Parameters)
{
    {
        FRestoreFixture F; if (!F.Create(*this)) { return false; }
        auto Snapshot = F.Initial();
        if (!TestTrue(TEXT("Initial stable whole-plot restore"),F.Import(Snapshot))) { AddError(F.Result); return false; }
        TestEqual(TEXT("Source provenance cannot replace assembly revision"),F.State->GetRevision(),uint64(1));
        auto* Cell = F.Projection({8,8,0}); auto* Neighbor = F.Projection({9,8,0});
        if (!Cell || !Neighbor || !F.State->FindDoor({-6,8,0})) { AddError(TEXT("Initial restore projections missing")); return false; }
        const uint64 NeighborOccupancy = F.State->Members.FindChecked({9,8,0}).Generation;
        const auto OldContext = F.State->MakeReplacementSet();
        FSFConstructionSandboxSnapshot Exported;
        TestTrue(TEXT("Stable canonical export"),F.State->ExportSnapshot(F.Controller,F.State->GetRevision(),Exported,F.Result));
        TestEqual(TEXT("Export carries current revision provenance"),Exported.SourceRevision,F.State->GetRevision());
        TestEqual(TEXT("Export includes both cells and door pose"),Exported.Cells.Num()+Exported.Doors.Num(),3);
        Snapshot.Cells[0].Shape = 1;
        Snapshot.SourceRevision = 0;
        if (!TestTrue(TEXT("Same-address cube to half slab restore"),F.Import(Snapshot))) { AddError(F.Result); return false; }
        TestTrue(TEXT("Same-address restore retains structural actor handles"),F.Projection({8,8,0}) == Cell && F.Projection({9,8,0}) == Neighbor);
        TestTrue(TEXT("Replacement changed occupancy generation"),F.State->Members.FindChecked({8,8,0}).Generation > 1);
        TestEqual(TEXT("Neighbor occupancy generation remains unchanged"),F.State->Members.FindChecked({9,8,0}).Generation,NeighborOccupancy);
        TestEqual(TEXT("Neighbor projection stamp follows exposed-face rebuild"),F.State->Members.FindChecked({9,8,0}).ProjectionGeneration,F.State->GetRevision());
        TestTrue(TEXT("Replacement and neighbor strong checks pass"),F.State->ValidateEnvironment().IsEmpty());
        TestFalse(TEXT("Half slab actual upper empty space remains open"),F.Trace(412,37,Cell));
        TestTrue(TEXT("Half slab actual occupied portion blocks"),F.Trace(437,12,Cell));
        TestEqual(TEXT("Actual neighbor seam exposes missing upper half"),BoundaryArea(const_cast<UProceduralMeshComponent*>(Neighbor->GetMesh())),1250.0);
        TestTrue(TEXT("Old replacement context expires at publication"),SFConstructionClearance::EvaluateReplacing(F.World,0,0,FVector(400,400,0),OldContext) == SFConstructionClearance::EResult::Unavailable);
        FSFConstructionReplacementSet Unowned;
        TestTrue(TEXT("Default unowned exclusion set fails closed"),SFConstructionClearance::EvaluateReplacing(F.World,0,0,FVector(400,400,0),Unowned) == SFConstructionClearance::EResult::Unavailable);
        Snapshot.Cells[0].Shape = 2; Snapshot.Cells[0].Rotation = 0; Snapshot.SourceRevision = MAX_uint64;
        auto* ClosedDoor = F.State->FindDoor({-6,8,0})->Projection.Get();
        Snapshot.Doors[0].bOpen = true;
        const uint64 Before = F.State->GetRevision();
        if (!TestTrue(TEXT("Rotated slope plus changed door pose restore"),F.Import(Snapshot))) { AddError(F.Result); return false; }
        TestEqual(TEXT("Max source revision remains provenance only"),F.State->GetRevision(),Before+1);
        TestTrue(TEXT("Rotated slope occupied side blocks real physics"),F.Trace(412,37,Cell));
        TestFalse(TEXT("Rotated slope opposite empty side stays open"),F.Trace(437,37,Cell));
        TestEqual(TEXT("Rotated slope zero-area boundary leaves full neighbor face"),BoundaryArea(const_cast<UProceduralMeshComponent*>(Neighbor->GetMesh())),2500.0);
        const auto* OpenDoor = F.State->FindDoor({-6,8,0})->Projection.Get();
        TestTrue(TEXT("Changed door pose installs a new intact endpoint"),OpenDoor && OpenDoor != ClosedDoor && OpenDoor->IsOpen() && OpenDoor->IsProjectionIntact());
        FSFConstructionSandboxSnapshot Empty;
        const uint64 EmptyBefore = F.State->GetRevision();
        TestTrue(TEXT("Whole-plot empty restore permitted"),F.Import(Empty));
        TestEqual(TEXT("Empty restore publishes exactly once"),F.State->GetRevision(),EmptyBefore+1);
        TestEqual(TEXT("Empty restore removes whole owned assembly"),F.State->GetMemberCount()+F.State->GetDoorCount(),0);
        TestTrue(TEXT("Reference actor and known foundation survive replacement"),F.Reference->IsProjectionIntact(0,SFConstructionGeometry::IdentityRotation(),1,FVector(800,0,0)) && F.Floor->GetActorLocation() == FVector(0,0,-50));
    }
    {
        FRestoreFixture F; if (!F.Create(*this)) { return false; }
        const auto Initial = F.Initial();
        if (!F.Import(Initial)) { AddError(F.Result); return false; }
        auto* Cell = F.Projection({8,8,0}); const auto* Mesh = Cell->GetMesh();
        const uint64 Revision = F.State->GetRevision();
        auto Reject = [&](const FSFConstructionSandboxSnapshot& Candidate,const TCHAR* Label)
        {
            TestFalse(Label,F.Import(Candidate));
            TestEqual(TEXT("Rejected restore preserves revision"),F.State->GetRevision(),Revision);
            TestTrue(TEXT("Rejected restore preserves actual active mesh and collision"),F.Projection({8,8,0}) == Cell && Cell->GetMesh() == Mesh && F.Trace(437,37,Cell));
        };
        auto Invalid = Initial; const FSFConstructionSavedCell Duplicate = Invalid.Cells[0];
        Invalid.Cells.Add(Duplicate); Reject(Invalid,TEXT("Codec-invalid duplicate rejects"));
        Invalid = Initial; Invalid.Cells[0].Rotation = 24; Reject(Invalid,TEXT("Unknown released rotation rejects"));
        Invalid = Initial; Invalid.Cells.Add({{14,8,4},0,SFConstructionGeometry::IdentityRotation()}); Reject(Invalid,TEXT("Unsupported floating candidate rejects"));
        Invalid = Initial; Invalid.Cells.Add({{-6,6,0},0,SFConstructionGeometry::IdentityRotation()}); Reject(Invalid,TEXT("Canonical cell-door reservation intrusion rejects"));
        Invalid = Initial; Invalid.Doors.Add({{-6,10,0},0,false}); Reject(Invalid,TEXT("Door-to-door reservations reject"));
        Invalid = Initial; Invalid.Cells.Add({{16,0,0},0,SFConstructionGeometry::IdentityRotation()}); Reject(Invalid,TEXT("Unrelated same-class reference actor is never excluded"));
        Invalid = Initial; Invalid.Cells.Add({{12,8,0},0,SFConstructionGeometry::IdentityRotation()});
        F.Pawn->SetActorLocation(FVector(625,425,25)); Reject(Invalid,TEXT("Current owned pawn blocks replacement occupancy")); F.Pawn->SetActorLocation(FVector(0,-600,100));
        auto* Prop = F.ExtraBox(nullptr,FVector(625,425,25)); Reject(Invalid,TEXT("Unrelated simple-body prop blocks replacement")); Prop->GetOwner()->Destroy();
        auto* ExtraDoorBox = F.ExtraBox(F.State->FindDoor({-6,8,0})->Projection.Get(),FVector(625,425,25));
        Reject(Invalid,TEXT("Extra component on owned door is not actor-wide excluded")); ExtraDoorBox->DestroyComponent();
        F.State->MaximumMembers = 1; Reject(Initial,TEXT("Captured cell limit enforced")); F.State->MaximumMembers = 512;
        F.State->MaximumDoors = 1; Invalid = Initial; Invalid.Doors.Add({{-12,8,0},0,false}); Reject(Invalid,TEXT("Captured door limit enforced")); F.State->MaximumDoors = 16;
        TestFalse(TEXT("Stale restore authority revision rejects"),F.State->ImportSnapshot(F.Controller,Revision-1,Initial,F.Result));
        F.Pawn->SetOwner(nullptr); Reject(Initial,TEXT("Wrong current pawn owner rejects")); F.Pawn->SetOwner(F.Controller);
        F.Floor->SetActorLocation(FVector(0,0,-60)); Reject(Initial,TEXT("Missing actual foundation evidence rejects")); F.Floor->SetActorLocation(FVector(0,0,-50));
        Cell->SetActorLocation(FVector(401,400,0));
        TestFalse(TEXT("Moved old projection fences rather than disappearing from clearance"),F.Import(Initial));
        Cell->SetActorLocation(FVector(400,400,0));
        auto* MutableMesh = const_cast<UProceduralMeshComponent*>(Mesh);
        MutableMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        TestFalse(TEXT("Altered old projection responses fence restore"),F.Import(Initial)); MutableMesh->SetCollisionResponseToAllChannels(ECR_Block);
        FSFConstructionSandboxSnapshot Sentinel; Sentinel.SourceRevision = 77;
        Cell->Destroy();
        TestFalse(TEXT("Destroyed old projection fences restore"),F.Import(Initial));
        TestFalse(TEXT("Destroyed old projection fences export"),F.State->ExportSnapshot(F.Controller,Revision,Sentinel,F.Result));
        TestEqual(TEXT("Failed export preserves caller output"),Sentinel.SourceRevision,uint64(77));
    }
    {
        FRestoreFixture F; if (!F.Create(*this)) { return false; }
        auto Initial = F.Initial();
        Initial.Cells.Add({{12,8,0},0,SFConstructionGeometry::IdentityRotation()}); Initial.Cells.Add({{12,8,1},0,SFConstructionGeometry::IdentityRotation()});
        if (!F.Import(Initial)) { AddError(F.Result); return false; }
        auto Removal = F.Removal({12,8,0}); TArray<FIntVector> Affected;
        if (!F.State->PreviewRemoval(F.Controller,Removal,Affected,F.Result)) { AddError(F.Result); return false; }
        F.Controller->bPendingRemoval = true; F.Controller->PendingRemoval = Removal; F.Controller->bHasPreview = true; F.Controller->bHasDoorPreview = true;
        const uint64 Before = F.State->GetRevision();
        auto* OldCell = F.Projection({8,8,0}); const auto* OldMesh = OldCell->GetMesh(); const uint64 OldStamp = OldCell->GetGeneration();
        auto* OldDoor = F.State->FindDoor({-6,8,0})->Projection.Get();
        auto Candidate = Initial; Candidate.Cells[0].Shape = 1; Candidate.Doors[0].bOpen = true;
        F.State->TestFailAfterPreparedCount = 1;
        TestFalse(TEXT("Interrupt after real first prepared projection rejects whole transaction"),F.Import(Candidate));
        F.State->TestFailAfterPreparedCount = INDEX_NONE;
        TestEqual(TEXT("Interrupted restore preserves revision"),F.State->GetRevision(),Before);
        TestTrue(TEXT("Interrupted restore preserves actors, physics and active generation"),OldCell->GetMesh() == OldMesh && OldCell->GetGeneration() == OldStamp && F.Trace(437,37,OldCell));
        TestTrue(TEXT("Interrupted restore preserves closed door endpoint"),F.State->FindDoor({-6,8,0})->Projection.Get() == OldDoor && !OldDoor->IsOpen() && OldDoor->IsProjectionIntact());
        TestTrue(TEXT("Interrupted restore preserves authority and controller confirmation"),F.State->ConfirmationController.Get() == F.Controller && F.Controller->bPendingRemoval && F.Controller->bHasPreview && F.Controller->bHasDoorPreview);
        TestTrue(TEXT("Interrupted restore leaves strong old environment intact"),F.State->ValidateEnvironment().IsEmpty());
        if (!TestTrue(TEXT("Successful retry restores entire candidate"),F.Import(Candidate))) { AddError(F.Result); return false; }
        TestTrue(TEXT("Success immediately clears both confirmation layers and sampled ghosts"),!F.State->ConfirmationController.IsValid() && !F.Controller->bPendingRemoval && !F.Controller->bHasPreview && !F.Controller->bHasDoorPreview);
        const auto Context = F.State->MakeReplacementSet();
        FRestoreFixture Other; if (!Other.Create(*this)) { return false; }
        TestTrue(TEXT("Validated set cannot exclude another state's or world's actors"),SFConstructionClearance::EvaluateReplacing(Other.World,0,0,FVector(400,400,0),Context) == SFConstructionClearance::EResult::Unavailable);
    }
    for (bool bDoorEpisode : {false,true})
    {
        FRestoreFixture F; if (!F.Create(*this)) { return false; }
        F.State->TestTimeOverride = 0;
        auto Initial = F.Initial();
        if (bDoorEpisode) { Initial.Doors = {{{8,8,1},0,false}}; }
        else { Initial.Doors.Reset(); Initial.Cells = {{{8,8,0},0,SFConstructionGeometry::IdentityRotation()},{{8,8,1},0,SFConstructionGeometry::IdentityRotation()}}; }
        if (!F.Import(Initial)) { AddError(F.Result); return false; }
        auto Removal = F.Removal({8,8,0}); TArray<FIntVector> Affected;
        if (!F.State->PreviewRemoval(F.Controller,Removal,Affected,F.Result)) { AddError(F.Result); return false; }
        Removal.bConfirmSupportLoss = true;
        if (!F.State->Apply(F.Controller,Removal,F.Result)) { AddError(F.Result); return false; }
        auto& Clock = bDoorEpisode ? F.State->DoorInstability : F.State->Instability;
        const FIntVector Origin(8,8,1); double Deadline = -1;
        Clock.GetDeadline(Origin,Deadline); TestEqual(TEXT("Episode starts with exact original deadline"),Deadline,10.0);
        const uint64 Revision = F.State->GetRevision(); FSFConstructionSandboxSnapshot Empty, Sentinel; Sentinel.SourceRevision = 77;
        for (double Now : {1.0,10.0})
        {
            F.State->TestTimeOverride = Now;
            TestFalse(TEXT("Grace/due wave refuses export"),F.State->ExportSnapshot(F.Controller,Revision,Sentinel,F.Result));
            TestFalse(TEXT("Grace/due wave refuses restoring away episode"),F.Import(Empty));
            TestEqual(TEXT("Grace failure preserves export output"),Sentinel.SourceRevision,uint64(77));
            Clock.GetDeadline(Origin,Deadline); TestEqual(TEXT("Failed snapshot action never extends deadline"),Deadline,10.0);
            TestEqual(TEXT("Failed grace/due snapshot action preserves revision"),F.State->GetRevision(),Revision);
        }
    }
    {
        FRestoreFixture F; if (!F.Create(*this,false)) { return false; }
        FSFConstructionSandboxSnapshot Out; Out.SourceRevision = 88;
        TestFalse(TEXT("Normal E1 rejects import API"),F.Import(F.Initial()));
        TestFalse(TEXT("Normal E1 rejects export API"),F.State->ExportSnapshot(F.Controller,0,Out,F.Result));
        TestEqual(TEXT("Normal rejection preserves output"),Out.SourceRevision,uint64(88));
    }
    return true;
}
#endif
