#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Interaction/SFE1InteractionTarget.h"
#include "Framework/SFPlayerController.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFE1Settings.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFE1InteractionAuthorityTest, "ShatteredFrontier.E1.Interaction.AuthorityValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFE1InteractionAuthorityTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Isolated authority world: proves server rule/collision behavior, not RPC connection ownership or two-client transport."));
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld())
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Fixture.GetTestWorld();
    // No GameMode/login in this isolated fixture; dispatch the normal actor lifecycle explicitly.
    World->GetWorldSettings()->NotifyBeginPlay();
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ASFPlayerController* Controller = World->SpawnActor<ASFPlayerController>();
    ASFE1Character* Pawn = World->SpawnActor<ASFE1Character>(FVector(0, 0, 100), FRotator::ZeroRotator, Spawn);
    ASFE1InteractionTarget* Target = World->SpawnActor<ASFE1InteractionTarget>(FVector(200, 0, 100), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Controller spawned"), Controller) || !TestNotNull(TEXT("Pawn spawned"), Pawn)
        || !TestNotNull(TEXT("Target spawned"), Target)) { return false; }
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
    Controller->Possess(Pawn);
    const auto Expect = [this, Controller](const TCHAR* Label, AActor* Proposed, uint32 Revision, ESFE1InteractionResult Expected)
    {
        TestEqual(Label, static_cast<uint8>(Controller->ProcessInteraction(Proposed, Revision)), static_cast<uint8>(Expected));
    };
    const uint32 OriginalRevision = Target->GetInteractionRevision();
    Expect(TEXT("Live visible target accepted"), Target, OriginalRevision, ESFE1InteractionResult::Accepted);
    TestEqual(TEXT("Acceptance preserves target revision"), Target->GetInteractionRevision(), OriginalRevision);
    TestTrue(TEXT("Acceptance preserves enabled state"), Target->IsInteractionEnabled());
    Expect(TEXT("Immediate repeat rejected by server clock"), Target, OriginalRevision, ESFE1InteractionResult::Cooldown);
    // Small ticks preserve the engine's normal delta clamp while advancing authoritative world time.
    const int32 CooldownTicks = FMath::CeilToInt((Controller->AcceptedCooldownSeconds + 0.05f) / 0.05f);
    for (int32 Index = 0; Index < CooldownTicks; ++Index) { Fixture.TickTestWorld(0.05f); }
    Expect(TEXT("Accepted cooldown expires by world time"), Target, OriginalRevision, ESFE1InteractionResult::Accepted);

    Target->SetActorLocation(FVector(Controller->InteractionRangeCm + 1.0f, 0, 100));
    Expect(TEXT("One cm beyond declared range rejected"), Target, OriginalRevision, ESFE1InteractionResult::OutOfRange);
    Target->SetActorLocation(FVector(Controller->InteractionRangeCm, 0, 100));
    TestEqual(TEXT("Exact declared range spatially available"), static_cast<uint8>(Controller->ValidateTarget(Target, OriginalRevision)),
        static_cast<uint8>(ESFE1InteractionResult::Accepted));
    Target->SetActorLocation(FVector(200, 0, 100));

    AActor* Wall = World->SpawnActor<AActor>();
    UBoxComponent* WallBox = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(WallBox);
    WallBox->SetBoxExtent(FVector(10, 100, 100));
    WallBox->SetCollisionProfileName(TEXT("BlockAll"));
    WallBox->RegisterComponent();
    Wall->SetActorLocation(FVector(100, 0, 125));
    Expect(TEXT("Real Visibility wall rejects server eye trace"), Target, OriginalRevision, ESFE1InteractionResult::Occluded);
    Wall->Destroy();

    Expect(TEXT("Null target rejected"), nullptr, 0, ESFE1InteractionResult::InvalidTarget);
    AActor* WrongType = World->SpawnActor<AActor>();
    Expect(TEXT("Wrong actor type rejected"), WrongType, 1, ESFE1InteractionResult::InvalidTarget);
    Pawn->SetOwner(WrongType);
    Expect(TEXT("Current pawn with mismatched owner rejected"), Target, OriginalRevision, ESFE1InteractionResult::InvalidController);
    Pawn->SetOwner(Controller);
    const double AcceptedBeforeDenial = Controller->LastAcceptedServerSeconds;
    Target->SetInteractionEnabled(false);
    Expect(TEXT("Stale revision rejected"), Target, OriginalRevision, ESFE1InteractionResult::StaleTarget);
    Expect(TEXT("Current disabled target rejected"), Target, Target->GetInteractionRevision(), ESFE1InteractionResult::Disabled);
    TestEqual(TEXT("Denials preserve accepted cooldown timestamp"), Controller->LastAcceptedServerSeconds, AcceptedBeforeDenial);
    Target->SetInteractionEnabled(true);
    Expect(TEXT("Changed and reenabled target still rejects old selection"), Target, OriginalRevision, ESFE1InteractionResult::StaleTarget);

    FTestWorldWrapper OtherFixture;
    if (OtherFixture.CreateTestWorld(EWorldType::GameRPC) && OtherFixture.BeginPlayInTestWorld())
    {
        OtherFixture.GetTestWorld()->GetWorldSettings()->NotifyBeginPlay();
        ASFE1InteractionTarget* ForeignTarget = OtherFixture.GetTestWorld()->SpawnActor<ASFE1InteractionTarget>();
        Expect(TEXT("Foreign-world target rejected"), ForeignTarget, ForeignTarget->GetInteractionRevision(), ESFE1InteractionResult::InvalidTarget);
    }
    else { OtherFixture.ForwardErrorMessages(this); }

    Controller->UnPossess();
    Expect(TEXT("Unpossessed controller rejected"), Target, Target->GetInteractionRevision(), ESFE1InteractionResult::InvalidController);
    Controller->Possess(Pawn);
    const uint32 RemovedRevision = Target->GetInteractionRevision();
    Target->Destroy();
    Expect(TEXT("Removed selected target rejected"), Target, RemovedRevision, ESFE1InteractionResult::InvalidTarget);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFE1InteractionConfigurationTest, "ShatteredFrontier.E1.Interaction.Configuration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFE1InteractionConfigurationTest::RunTest(const FString& Parameters)
{
    USFE1Settings* Settings = NewObject<USFE1Settings>();
    FString Error;
    TestTrue(TEXT("Shipped configuration validates"), Settings->Validate(Error));
    Settings->InteractionRangeMetres = 0.49f;
    TestFalse(TEXT("Range below safe bound rejected"), Settings->Validate(Error));
    Settings->InteractionRangeMetres = 5.01f;
    TestFalse(TEXT("Range above safe bound rejected"), Settings->Validate(Error));
    Settings->InteractionRangeMetres = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Non-finite range rejected"), Settings->Validate(Error));
    Settings->InteractionRangeMetres = 3.0f;
    Settings->InteractionAcceptedCooldownSeconds = 0.09f;
    TestFalse(TEXT("Cooldown below safe bound rejected"), Settings->Validate(Error));
    Settings->InteractionAcceptedCooldownSeconds = 10.01f;
    TestFalse(TEXT("Cooldown above safe bound rejected"), Settings->Validate(Error));
    Settings->InteractionAcceptedCooldownSeconds = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Non-finite cooldown rejected"), Settings->Validate(Error));
    Settings->InteractionAcceptedCooldownSeconds = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("NaN cooldown rejected"), Settings->Validate(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFE1InteractionFeedbackTest, "ShatteredFrontier.E1.Interaction.Feedback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFE1InteractionFeedbackTest::RunTest(const FString& Parameters)
{
    USFE1Settings* Settings = NewObject<USFE1Settings>();
    Settings->InteractionAcceptedCooldownSeconds = 10.0f;
    FString Error;
    TestTrue(TEXT("Ten-second release configuration is permitted"), Settings->Validate(Error));
    FSFE1InteractionFeedback Feedback;
    const auto ExpectState = [this, &Feedback](const TCHAR* Label, double Now, ESFE1LocalInteractionState Expected)
    {
        TestEqual(Label, static_cast<uint8>(Feedback.GetState(Now)), static_cast<uint8>(Expected));
    };
    TestTrue(TEXT("First input starts request"), Feedback.TryBeginRequest(0.0));
    ExpectState(TEXT("Pending result is unavailable"), 0.01, ESFE1LocalInteractionState::Pending);
    TestFalse(TEXT("No additional request while pending"), Feedback.TryBeginRequest(0.015));
    Feedback.ReceiveServerResult(0.02, 0.0f); // A denial with no existing accepted cooldown.
    ExpectState(TEXT("Denial retains only short coalescing interval"), 0.05, ESFE1LocalInteractionState::Coalescing);
    TestFalse(TEXT("Rapid denied input is coalesced"), Feedback.TryBeginRequest(0.05));
    TestTrue(TEXT("Corrected request may retry after 0.1s despite 10s accepted cooldown setting"), Feedback.TryBeginRequest(0.11));
    Feedback.ReceiveServerResult(0.12, Settings->InteractionAcceptedCooldownSeconds);
    ExpectState(TEXT("Accepted result starts server-reported cooldown"), 0.13, ESFE1LocalInteractionState::Cooldown);
    ExpectState(TEXT("Cooldown remains unavailable after 2s result text expires"), 2.2, ESFE1LocalInteractionState::Cooldown);
    // A further denial reports the existing remaining duration rather than restarting ten seconds.
    Feedback.ReceiveServerResult(5.12, 5.0f);
    TestFalse(TEXT("Input matches unavailable advisory during long cooldown"), Feedback.TryBeginRequest(9.0));
    ExpectState(TEXT("Existing cooldown remains until its reported end"), 10.11, ESFE1LocalInteractionState::Cooldown);
    ExpectState(TEXT("Availability returns when reported cooldown elapses"), 10.13, ESFE1LocalInteractionState::Ready);
    TestTrue(TEXT("Input and advisory become ready together"), Feedback.TryBeginRequest(10.13));
    return true;
}

#endif
