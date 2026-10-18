#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Characters/SFE1Character.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
struct FDoorInteractionFixture
{
    FTestWorldWrapper Wrapper;
    UWorld* World = nullptr;
    ASFConstructionSandboxController* Controller = nullptr;
    ASFE1Character* Character = nullptr;
    ASFConstructionSandboxState* State = nullptr;
    FString Result;
    const FIntVector Origin{4,4,0};
    FSFConstructionDoorProjectionProfile Profile;
    bool Create(FAutomationTestBase& Test, int32 Yaw)
    {
        if (!Wrapper.CreateTestWorld(EWorldType::Game)) { return false; }
        World = Wrapper.GetTestWorld();
        World->GetWorldSettings()->DefaultGameMode = ASFConstructionSandboxGameMode::StaticClass();
        if (!World->SetGameMode(FURL()) || !Wrapper.BeginPlayInTestWorld()) { return false; }
        World->GetWorldSettings()->NotifyBeginPlay();
        auto* Foundation = World->SpawnActor<AStaticMeshActor>();
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
        if (!ASFConstructionSandboxDoorProjection::BuildProfile(Origin,Yaw,false,Profile)) { return false; }
        Character = World->SpawnActor<ASFE1Character>(At(50,-160,94),FRotator::ZeroRotator);
        if (!Character) { return false; }
        Controller->Possess(Character);
        for (TActorIterator<ASFConstructionSandboxState> It(World); It; ++It) { State = *It; break; }
        if (!State) { State = World->SpawnActor<ASFConstructionSandboxState>(); }
        return Test.TestNotNull(TEXT("Interaction state"),State);
    }
    FVector At(double X,double Y,double Z) const { return Profile.Transform.TransformPosition(FVector(X,Y,Z)); }
    FSFConstructionDoorIntent Intent(ESFConstructionDoorAction Action, int32 Yaw) const
    {
        FSFConstructionDoorIntent I;
        I.OriginCell = Origin; I.Action = Action; I.YawQuarterTurns = Yaw; I.ExpectedRevision = State->GetRevision();
        return I;
    }
    bool Apply(ESFConstructionDoorAction Action,int32 Yaw)
    { return State->ApplyDoor(Controller,Intent(Action,Yaw),Result); }
    UBoxComponent* Block(double X,double Y,double Z,const FVector& Extent,AActor* Owner = nullptr)
    {
        if (!Owner) { Owner = World->SpawnActor<AActor>(); }
        auto* Box = NewObject<UBoxComponent>(Owner);
        if (!Owner->GetRootComponent()) { Owner->SetRootComponent(Box); }
        else { Box->SetupAttachment(Owner->GetRootComponent()); }
        Box->SetBoxExtent(Extent);
        Box->SetCollisionProfileName(TEXT("BlockAll"));
        Box->SetGenerateOverlapEvents(false);
        Box->SetWorldLocation(At(X,Y,Z));
        Box->RegisterComponent();
        return Box;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionDoorInteractionTest,
    "ShatteredFrontier.Construction.DoorInteraction.VirtualTargetAndSafeClose",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionDoorInteractionTest::RunTest(const FString& Parameters)
{
    using Action = ESFConstructionDoorAction;
    for (int32 Yaw = 0; Yaw < 4; ++Yaw)
    {
        FDoorInteractionFixture F;
        if (!F.Create(*this,Yaw)) { return false; }
        if (!F.Apply(Action::Place,Yaw)) { AddError(F.Result); return false; }
        FIntVector Found(99,99,99);
        FVector Point(999);
        const FVector RayStart = F.At(50,-160,110), RayEnd = F.At(50,100,110);
        TestTrue(TEXT("Closed doorway centre targets virtual envelope for each yaw"),
            F.State->FindDoorAlongRay(F.Controller,RayStart,RayEnd,Found,&Point) && Found == F.Origin);
        TestTrue(TEXT("Virtual point is on the released envelope"),F.Profile.Envelope.IsInsideOrOn(Point));
        TestTrue(TEXT("Oblique cursor ray targets exact virtual boundary"),
            F.State->FindDoorAlongRay(F.Controller,F.At(-174.378,-256.987,145.122),F.At(50,25,100),Found,&Point));
        TestTrue(TEXT("Computed oblique point remains inside exact released envelope"),F.Profile.Envelope.IsInsideOrOn(Point));
        TestTrue(TEXT("Ray starting inside envelope still selects owned doorway"),
            F.State->FindDoorAlongRay(F.Controller,F.At(50,25,110),F.At(50,150,110),Found));
        auto* Occluder = F.Block(50,-60,110,FVector(20,5,25));
        TestFalse(TEXT("Closer prop occludes doorway"),
            F.State->FindDoorAlongRay(F.Controller,RayStart,RayEnd,Found));
        Occluder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Occluder->GetOwner()->Destroy();
        const auto Saved = Found;
        TestFalse(TEXT("Nonfinite ray rejected"),F.State->FindDoorAlongRay(F.Controller,FVector(NAN),RayEnd,Found));
        TestTrue(TEXT("Failure preserves output"),Found == Saved);
        TestFalse(TEXT("Degenerate ray rejected"),F.State->FindDoorAlongRay(F.Controller,RayStart,RayStart,Found));
        F.Character->SetActorLocation(F.At(50,-900,94));
        TestFalse(TEXT("Out-of-reach pawn cannot select doorway"),F.State->FindDoorAlongRay(F.Controller,F.At(50,-900,110),RayEnd,Found));
        F.Character->SetActorLocation(F.At(50,-125,94));
        auto Selected = F.Intent(Action::Toggle,Yaw);
        Selected.bHasInteractionPoint = true; Selected.InteractionPoint = Point;
        TestTrue(TEXT("Virtual point independently accepted by authority"),F.State->EvaluateDoor(F.Controller,Selected).IsEmpty());
        Selected.InteractionPoint = F.At(50,-1,110);
        TestFalse(TEXT("Forged point outside released envelope rejected"),F.State->EvaluateDoor(F.Controller,Selected).IsEmpty());
        if (!F.Apply(Action::Toggle,Yaw)) { AddError(F.Result); return false; }
        TestTrue(TEXT("Open doorway centre remains targetable"),
            F.State->FindDoorAlongRay(F.Controller,RayStart,RayEnd,Found,&Point) && Found == F.Origin);
        F.Character->SetActorLocation(F.At(50,25,94));
        const FVector Before = F.Character->GetActorLocation();
        auto* Movement = F.Character->GetCharacterMovement();
        const auto Mode = Movement->MovementMode;
        const FVector Velocity = Movement->Velocity;
        Movement->bForceNextFloorCheck = false;
        const uint64 Revision = F.State->GetRevision();
        auto Close = F.Intent(Action::Toggle,Yaw);
        TestTrue(TEXT("Pre-close authority record is open"),F.State->FindDoor(F.Origin)
            && F.State->FindDoor(F.Origin)->bOpen);
        if (!F.State->ApplyDoor(F.Controller,Close,F.Result)) { AddError(F.Result); return false; }
        TestEqual(TEXT("Close advances one common revision"),F.State->GetRevision(),Revision+1);
        TestFalse(TEXT("Closed endpoint remains closed"),F.State->FindDoor(F.Origin)->bOpen);
        TestTrue(TEXT("Successful push preserves movement mode and velocity and schedules floor check"),
            Movement->MovementMode == Mode && Movement->Velocity.Equals(Velocity,0.001)
            && Movement->bForceNextFloorCheck);
        TestTrue(TEXT("Push is horizontal and reaches nearest front side"),
            F.Character->GetActorLocation().Z == Before.Z
            && F.Profile.Transform.InverseTransformPosition(F.Character->GetActorLocation()).Y < -34.0);
        const FVector After = F.Character->GetActorLocation();
        TestFalse(TEXT("Stale close cannot move player"),F.State->ApplyDoor(F.Controller,Close,F.Result));
        TestTrue(TEXT("Stale close preserves player"),F.Character->GetActorLocation().Equals(After,0.001));
    }
    {
        FDoorInteractionFixture F;
        if (!F.Create(*this,0)) { return false; }
        auto Second = F.Intent(Action::Place,0);
        Second.OriginCell = FIntVector(4,9,0);
        if (!F.State->ApplyDoor(F.Controller,Second,F.Result)) { AddError(F.Result); return false; }
        if (!F.Apply(Action::Place,0)) { AddError(F.Result); return false; }
        FIntVector Found(99,99,99);
        TestTrue(TEXT("Nearest doorway wins shared ray"),F.State->FindDoorAlongRay(F.Controller,
            F.At(50,-160,110),F.At(50,600,110),Found) && Found == F.Origin);
        const FVector SecondStart = F.At(50,210,110);
        TestTrue(TEXT("Farther doorway resolves when nearer is behind ray"),F.State->FindDoorAlongRay(F.Controller,
            SecondStart,F.At(50,600,110),Found) && Found == Second.OriginCell);
    }
    {
        FDoorInteractionFixture F;
        if (!F.Create(*this,0)) { return false; }
        if (!F.Apply(Action::Place,0) || !F.Apply(Action::Toggle,0)) { AddError(F.Result); return false; }
        F.Character->SetActorLocation(F.At(50,25,94));
        const FVector Before = F.Character->GetActorLocation();
        auto* Movement = F.Character->GetCharacterMovement();
        const auto Mode = Movement->MovementMode;
        const FVector Velocity = Movement->Velocity;
        Movement->bForceNextFloorCheck = false;
        const uint64 Revision = F.State->GetRevision();
        auto* OldDoor = F.State->FindDoor(F.Origin)->Projection.Get();
        F.State->bTestFailAfterDoorPush = true;
        TestFalse(TEXT("Staged failure after reversible push rejects"),F.Apply(Action::Toggle,0));
        F.State->bTestFailAfterDoorPush = false;
        TestTrue(TEXT("Staged failure restores capsule"),F.Character->GetActorLocation().Equals(Before,0.001));
        TestTrue(TEXT("Rollback preserves movement bookkeeping"),Movement->MovementMode == Mode
            && Movement->Velocity.Equals(Velocity,0.001) && !Movement->bForceNextFloorCheck);
        TestEqual(TEXT("Staged failure preserves revision"),F.State->GetRevision(),Revision);
        TestTrue(TEXT("Staged failure preserves old open leaf"),F.State->FindDoor(F.Origin)->Projection.Get() == OldDoor
            && F.State->FindDoor(F.Origin)->bOpen && OldDoor->IsProjectionIntact());
        F.Block(50,-55,100,FVector(50,5,100));
        if (!F.Apply(Action::Toggle,0)) { AddError(F.Result); return false; }
        TestTrue(TEXT("Alternate push ends beyond back motion"),
            F.Profile.Transform.InverseTransformPosition(F.Character->GetActorLocation()).Y > 154.0);
    }
    {
        FDoorInteractionFixture F;
        if (!F.Create(*this,0)) { return false; }
        if (!F.Apply(Action::Place,0) || !F.Apply(Action::Toggle,0)) { AddError(F.Result); return false; }
        F.Character->SetActorLocation(F.At(50,25,94));
        F.Block(50,-55,100,FVector(50,5,100));
        F.Block(50,155,100,FVector(50,5,100));
        const FVector Before = F.Character->GetActorLocation();
        const uint64 Revision = F.State->GetRevision();
        TestFalse(TEXT("Both sides trapped rejects close"),F.Apply(Action::Toggle,0));
        TestTrue(TEXT("Trapped close preserves position and open door"),F.Character->GetActorLocation().Equals(Before,0.001)
            && F.State->FindDoor(F.Origin)->bOpen);
        TestEqual(TEXT("Trapped close preserves revision"),F.State->GetRevision(),Revision);
    }
    {
        FDoorInteractionFixture F;
        if (!F.Create(*this,0)) { return false; }
        if (!F.Apply(Action::Place,0) || !F.Apply(Action::Toggle,0)) { AddError(F.Result); return false; }
        F.Character->SetActorLocation(F.At(50,25,94));
        auto* OwnedBlocker = F.Block(50,25,100,FVector(2,2,10),F.Character);
        const uint64 Revision = F.State->GetRevision();
        TestFalse(TEXT("Additional owned blocking component is not omitted with capsule"),F.Apply(Action::Toggle,0));
        TestEqual(TEXT("Additional owned component rejection preserves revision"),F.State->GetRevision(),Revision);
        OwnedBlocker->DestroyComponent();
        auto* Other = F.World->SpawnActor<ASFE1Character>(F.At(50,-40,94),FRotator::ZeroRotator);
        F.Block(50,155,100,FVector(50,5,100));
        TestFalse(TEXT("Other pawn blocks front while back is obstructed"),F.Apply(Action::Toggle,0));
        TestEqual(TEXT("Other pawn rejection preserves revision"),F.State->GetRevision(),Revision);
        Other->Destroy();
    }
    {
        FDoorInteractionFixture F;
        if (!F.Create(*this,0)) { return false; }
        if (!F.Apply(Action::Place,0) || !F.Apply(Action::Toggle,0)) { AddError(F.Result); return false; }
        // The open leaf occupies the hinge-side path. A forced capsule pose
        // inside that old collider must not allow a swept close through it.
        F.Character->SetActorLocation(F.At(8,55,94));
        const uint64 Revision = F.State->GetRevision();
        TestFalse(TEXT("Old open leaf remains a path blocker"),F.Apply(Action::Toggle,0));
        TestEqual(TEXT("Old-leaf rejection preserves revision"),F.State->GetRevision(),Revision);
    }
    return true;
}
#endif
