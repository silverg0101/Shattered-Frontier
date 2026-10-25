#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Characters/SFE1Character.h"
#include "Characters/SFBackpedalMovement.h"
#include "Framework/SFE1Settings.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/WorldSettings.h"

namespace
{
constexpr float MovementTick = 1.0f / 120.0f;

struct FMovementFixture
{
    FTestWorldWrapper Simulation;
    ASFE1Character* Pawn = nullptr;

    AActor* Box(const FVector& Location, const FVector& Extent)
    {
        AActor* Actor = Simulation.GetTestWorld()->SpawnActor<AActor>();
        UBoxComponent* Shape = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Shape);
        Shape->SetBoxExtent(Extent);
        Shape->SetCollisionProfileName(TEXT("BlockAll"));
        Shape->SetWorldLocation(Location);
        Shape->RegisterComponent();
        return Actor;
    }

    bool Create(FAutomationTestBase& Test)
    {
        if (!Simulation.CreateTestWorld(EWorldType::GameRPC) || !Simulation.BeginPlayInTestWorld())
        {
            Simulation.ForwardErrorMessages(&Test);
            return false;
        }
        UWorld* World = Simulation.GetTestWorld();
        World->GetWorldSettings()->NotifyBeginPlay();
        Box(FVector(0, 0, -50), FVector(3000, 1500, 50));
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Pawn = World->SpawnActor<ASFE1Character>(FVector(0, 0, 110), FRotator::ZeroRotator, Spawn);
        APlayerController* Controller = World->SpawnActor<APlayerController>();
        if (!Test.TestNotNull(TEXT("Character spawned"), Pawn) || !Test.TestNotNull(TEXT("Controller spawned"), Controller)) { return false; }
        // This world has no GameMode admission: designate the controller as local
        // as GameMode normally does, so real controlled CharacterMovement runs.
        Controller->SetAsLocalPlayerController();
        Controller->Possess(Pawn);
        if (!Test.TestTrue(TEXT("Fixture controller drives local authoritative character"), Pawn->IsLocallyControlled() && Pawn->HasAuthority())) { return false; }
        if (!Tick(120)) { Simulation.ForwardErrorMessages(&Test); return false; }
        return Test.TestTrue(TEXT("Character settles on real collision floor"), Pawn->GetCharacterMovement()->IsMovingOnGround()
            && Pawn->GetCharacterMovement()->CurrentFloor.IsWalkableFloor());
    }

    bool Tick(int32 Count, FVector Input = FVector::ZeroVector, float* PeakSpeed = nullptr, float* PeakZ = nullptr)
    {
        for (int32 Index = 0; Index < Count; ++Index)
        {
            if (!Input.IsNearlyZero()) { Pawn->AddMovementInput(Input, 1.0f, true); }
            if (!Simulation.TickTestWorld(MovementTick)) { return false; }
            if (PeakSpeed) { *PeakSpeed = FMath::Max(*PeakSpeed, Pawn->GetVelocity().Size2D()); }
            if (PeakZ) { *PeakZ = FMath::Max(*PeakZ, static_cast<float>(Pawn->GetActorLocation().Z)); }
        }
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFE1WalkAndWallTest, "ShatteredFrontier.E1.Movement.WalkAndWall",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFE1WalkAndWallTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Isolated authoritative CharacterMovement/collision; no network prediction, rendered traversal or capacity acceptance."));
    FMovementFixture Fixture;
    if (!Fixture.Create(*this)) { return false; }
    const FVector Start = Fixture.Pawn->GetActorLocation();
    const float DeclaredSpeed = GetDefault<USFE1Settings>()->RunSpeedMetresPerSecond * 100.0f;
    float PeakSpeed = 0;
    TestTrue(TEXT("Open-floor simulation ticks"), Fixture.Tick(120, FVector(1, 1, 0).GetSafeNormal(), &PeakSpeed));
    TestTrue(TEXT("Sustained input translates the actual capsule"), FVector::Dist2D(Start, Fixture.Pawn->GetActorLocation()) > DeclaredSpeed * 0.5f);
    TestTrue(TEXT("Diagonal input reaches declared run speed"), PeakSpeed >= DeclaredSpeed * 0.98f);
    TestTrue(TEXT("Diagonal input cannot exceed declared speed cap"), PeakSpeed <= DeclaredSpeed + 1.0f);
    AddInfo(FString::Printf(TEXT("Open-floor displacement=%.2f cm peak speed=%.2f cm/s declared=%.2f cm/s"), FVector::Dist2D(Start, Fixture.Pawn->GetActorLocation()), PeakSpeed, DeclaredSpeed));

    Fixture.Pawn->SetActorLocation(Start, false, nullptr, ETeleportType::TeleportPhysics);
    Fixture.Pawn->GetCharacterMovement()->StopMovementImmediately();
    Fixture.Box(FVector(600, 0, 200), FVector(20, 200, 200));
    TestTrue(TEXT("Wall simulation ticks"), Fixture.Tick(240, FVector::ForwardVector));
    const float WallFront = 580.0f;
    const float CapsuleFront = Fixture.Pawn->GetActorLocation().X + Fixture.Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    AddInfo(FString::Printf(TEXT("Wall front=%.2f cm capsule front=%.2f cm final speed=%.2f cm/s"), WallFront, CapsuleFront, Fixture.Pawn->GetVelocity().Size2D()));
    TestTrue(TEXT("Character reaches wall rather than failing to move"), CapsuleFront > WallFront - 3.0f);
    TestTrue(TEXT("Solid wall prevents capsule penetration"), CapsuleFront <= WallFront + 0.5f);
    TestTrue(TEXT("Wall blocks horizontal velocity"), Fixture.Pawn->GetVelocity().Size2D() < 1.0f);
    TestTrue(TEXT("Character stays grounded at wall"), Fixture.Pawn->GetCharacterMovement()->IsMovingOnGround());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFE1CrouchTest, "ShatteredFrontier.E1.Movement.Crouch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFE1CrouchTest::RunTest(const FString& Parameters)
{
    FMovementFixture Fixture;
    if (!Fixture.Create(*this)) { return false; }
    const float StandingHalfHeight = Fixture.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float CrouchCap = GetDefault<USFE1Settings>()->CrouchSpeedMetresPerSecond * 100.0f;
    Fixture.Pawn->Crouch();
    float CrouchPeak = 0;
    TestTrue(TEXT("Crouched movement ticks"), Fixture.Tick(120, FVector::ForwardVector, &CrouchPeak));
    TestTrue(TEXT("Server character crouches"), Fixture.Pawn->bIsCrouched);
    TestTrue(TEXT("Crouch lowers capsule"), Fixture.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() < StandingHalfHeight);
    TestTrue(TEXT("Crouch reaches authored speed"), CrouchPeak >= CrouchCap * 0.95f);
    TestTrue(TEXT("Crouch stays under server cap"), CrouchPeak <= CrouchCap + 1.0f);
    Fixture.Pawn->UnCrouch();
    float RunPeak = 0;
    TestTrue(TEXT("Return to run ticks"), Fixture.Tick(120, FVector::ForwardVector, &RunPeak));
    TestFalse(TEXT("Server character stands"), Fixture.Pawn->bIsCrouched);
    TestTrue(TEXT("Standing capsule restored"), FMath::IsNearlyEqual(Fixture.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), StandingHalfHeight, 1.0f));
    TestTrue(TEXT("Run speed restored"), RunPeak >= GetDefault<USFE1Settings>()->RunSpeedMetresPerSecond * 95.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFE1BackpedalTest, "ShatteredFrontier.E1.Movement.Backpedal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFE1BackpedalTest::RunTest(const FString& Parameters)
{
    const USFE1Settings* Settings = GetDefault<USFE1Settings>();
    TestTrue(TEXT("Shipped reverse factor is valid"), Settings->BackpedalFactor > 0.0f && Settings->BackpedalFactor <= 1.0f);
    TestEqual(TEXT("World section movement default"), Settings->BackpedalFactor, 0.6f);
    TestTrue(TEXT("Pure backward input is backpedal"), USFBackpedalMovement::IsBackpedalling(FVector::ForwardVector, -FVector::ForwardVector));
    TestTrue(TEXT("Input just beyond 120 degrees is backpedal"), USFBackpedalMovement::IsBackpedalling(FVector::ForwardVector, FVector(-0.51f, 0.86f, 0)));
    TestFalse(TEXT("Input inside front 240-degree sector keeps run cap"), USFBackpedalMovement::IsBackpedalling(FVector::ForwardVector, FVector(-0.49f, 0.87f, 0)));
    TestFalse(TEXT("Stopped character does not backpedal"), USFBackpedalMovement::IsBackpedalling(FVector::ForwardVector, FVector::ZeroVector));
    TestFalse(TEXT("Strafe keeps run cap"), USFBackpedalMovement::IsBackpedalling(FVector::ForwardVector, FVector::RightVector));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFE1JumpAndLandingTest, "ShatteredFrontier.E1.Movement.JumpAndLanding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFE1JumpAndLandingTest::RunTest(const FString& Parameters)
{
    FMovementFixture Fixture;
    if (!Fixture.Create(*this)) { return false; }
    const FVector Start = Fixture.Pawn->GetActorLocation();
    float PeakZ = static_cast<float>(Start.Z);
    Fixture.Pawn->Jump();
    TestTrue(TEXT("Jump simulation ticks"), Fixture.Tick(1, FVector::ZeroVector, nullptr, &PeakZ));
    Fixture.Pawn->StopJumping();
    TestTrue(TEXT("Jump leaves the floor"), Fixture.Pawn->GetCharacterMovement()->IsFalling());
    for (int32 Index = 0; Index < 240 && Fixture.Pawn->GetCharacterMovement()->IsFalling(); ++Index)
    {
        if (!TestTrue(TEXT("Airborne simulation tick"), Fixture.Tick(1, FVector::ForwardVector, nullptr, &PeakZ))) { return false; }
    }
    const float Rise = PeakZ - Start.Z;
    const float DeclaredHeight = GetDefault<USFE1Settings>()->JumpHeightMetres * 100.0f;
    AddInfo(FString::Printf(TEXT("Measured jump rise=%.2f cm declared=%.2f cm landed=%d"), Rise, DeclaredHeight, Fixture.Pawn->GetCharacterMovement()->IsMovingOnGround()));
    // Five cm allows discrete integration/floor clearance, but rejects a materially wrong jump height.
    TestTrue(FString::Printf(TEXT("Measured apex %.2f cm matches declared %.2f cm within 5 cm"), Rise, DeclaredHeight), FMath::Abs(Rise - DeclaredHeight) <= 5.0f);
    TestTrue(TEXT("No air control adds horizontal displacement"), FVector::Dist2D(Start, Fixture.Pawn->GetActorLocation()) < 1.0f);
    TestTrue(TEXT("Character lands again"), Fixture.Pawn->GetCharacterMovement()->IsMovingOnGround());
    TestTrue(TEXT("Landing returns to floor height"), FMath::Abs(Fixture.Pawn->GetActorLocation().Z - Start.Z) < 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFE1StepThresholdTest, "ShatteredFrontier.E1.Movement.StepThreshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFE1StepThresholdTest::RunTest(const FString& Parameters)
{
    const float StepHeight = GetDefault<USFE1Settings>()->StepHeightMetres * 100.0f;
    for (bool bTraversable : { true, false })
    {
        FMovementFixture Fixture;
        if (!Fixture.Create(*this)) { return false; }
        const float Height = bTraversable ? StepHeight * 0.75f : StepHeight + 20.0f;
        Fixture.Box(FVector(250, 0, Height / 2.0f), FVector(100, 200, Height / 2.0f));
        float PeakZ = static_cast<float>(Fixture.Pawn->GetActorLocation().Z);
        const float StartZ = PeakZ;
        TestTrue(TEXT("Step simulation ticks"), Fixture.Tick(180, FVector::ForwardVector, nullptr, &PeakZ));
        AddInfo(FString::Printf(TEXT("Step height=%.2f cm declared threshold=%.2f cm final X=%.2f cm rise=%.2f cm"), Height, StepHeight, Fixture.Pawn->GetActorLocation().X, PeakZ - StartZ));
        if (bTraversable)
        {
            TestTrue(TEXT("Below-threshold step is crossed"), Fixture.Pawn->GetActorLocation().X > 400.0f);
            TestTrue(TEXT("Capsule actually rises onto step"), PeakZ - StartZ >= Height - 3.0f);
        }
        else
        {
            const float Front = Fixture.Pawn->GetActorLocation().X + Fixture.Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
            TestTrue(TEXT("Above-threshold step is reached"), Front > 147.0f);
            TestTrue(TEXT("Above-threshold step blocks the capsule"), Front <= 150.5f);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFE1CameraCollisionTest, "ShatteredFrontier.E1.Movement.CameraCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFE1CameraCollisionTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Headless spring-arm collision query only; visual camera acceptance remains separate."));
    FMovementFixture Fixture;
    if (!Fixture.Create(*this)) { return false; }
    USpringArmComponent* Boom = Fixture.Pawn->FindComponentByClass<USpringArmComponent>();
    if (!TestNotNull(TEXT("Actual character spring arm exists"), Boom)) { return false; }
    AActor* Occluder = Fixture.Box(FVector(-150, 0, 150), FVector(20, 200, 150));
    TestTrue(TEXT("Occluded camera ticks"), Fixture.Tick(2));
    const FVector Origin = Boom->GetComponentLocation();
    const float BlockedLength = FVector::Distance(Origin, Boom->GetSocketLocation(USpringArmComponent::SocketName));
    TestTrue(TEXT("Real wall retracts camera boom before obstruction"), BlockedLength > 50.0f && BlockedLength < 140.0f);
    Occluder->Destroy();
    TestTrue(TEXT("Cleared camera ticks"), Fixture.Tick(2));
    const float RestoredLength = FVector::Distance(Origin, Boom->GetSocketLocation(USpringArmComponent::SocketName));
    AddInfo(FString::Printf(TEXT("Camera boom blocked=%.2f cm restored=%.2f cm"), BlockedLength, RestoredLength));
    TestTrue(TEXT("Camera boom recovers after obstruction removal"), FMath::Abs(RestoredLength - Boom->TargetArmLength) < 1.0f);
    return true;
}

#endif
