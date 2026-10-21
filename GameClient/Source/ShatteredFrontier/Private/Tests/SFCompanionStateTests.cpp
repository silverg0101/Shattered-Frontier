#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Creatures/SFCompanionStateComponent.h"
#include "Creatures/SFRestAreaVolume.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"
#include "Net/UnrealNetwork.h"

/** Rest-area geometry and bounds, the server gate on the milestone choice, and owner-only replication of the result. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCompanionStateTest, "ShatteredFrontier.Creatures.Companion.RestAreaChoice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCompanionStateTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("extent clamped to 2-50 m"), ASFRestAreaVolume::ClampExtent(FVector(1, 100, 10)).Equals(FVector(2, 50, 10)));
    const FTransform Turned(FRotator(0, 90, 0), FVector::ZeroVector);
    const FVector Extent(10, 2, 3);
    TestTrue(TEXT("box follows the placement yaw"), ASFRestAreaVolume::Contains(Turned, Extent, FVector(0, 900, 0)));
    TestFalse(TEXT("box excludes the unrotated long axis"), ASFRestAreaVolume::Contains(Turned, Extent, FVector(900, 0, 0)));
    TestFalse(TEXT("box has a ceiling"), ASFRestAreaVolume::Contains(Turned, Extent, FVector(0, 0, 301)));

    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    World->GetWorldSettings()->NotifyBeginPlay();

    ASFRestAreaVolume* Rest = World->SpawnActor<ASFRestAreaVolume>(FVector(10000, 0, 0), FRotator::ZeroRotator);
    APawn* Pawn = World->SpawnActor<ADefaultPawn>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("rest area spawns"), Rest) || !TestNotNull(TEXT("pawn spawns"), Pawn)) return false;
    USFCompanionStateComponent* State = NewObject<USFCompanionStateComponent>(Pawn);
    State->RegisterComponent();
    State->Family = TEXT("voidreign");
    State->CompanionLevel = 70;
    State->Unlocked = { SFCompanionMilestones::LongShadow, SFCompanionMilestones::RiftCollapse };
    auto Reason = [State](ESFMilestoneReject Got) { return SFCompanionMilestones::ReasonCode(Got); };

    // Outside every rest area: refused with a stable reason, nothing changes.
    TestEqual(TEXT("field swap refused"), Reason(State->SelectMilestone(FGuid::NewGuid(), 70, SFCompanionMilestones::LongShadow)), FName(TEXT("not_at_rest_area")));
    TestEqual(TEXT("reason recorded for the owner"), State->LastReason(), FName(TEXT("not_at_rest_area")));
    TestEqual(TEXT("no active choice"), State->ActiveOption(70), FName(NAME_None));
    TestEqual(TEXT("no revision"), State->GetRevision(), 0);

    // Inside: committed, replicated fields updated, idempotent per operation.
    Pawn->SetActorLocation(Rest->GetActorLocation() + FVector(100, 0, 100));
    const FGuid Op = FGuid::NewGuid();
    TestEqual(TEXT("rest-area swap accepted"), Reason(State->SelectMilestone(Op, 70, SFCompanionMilestones::LongShadow)), FName(TEXT("ok")));
    TestEqual(TEXT("choice active"), State->ActiveOption(70), SFCompanionMilestones::LongShadow);
    TestEqual(TEXT("one revision"), State->GetRevision(), 1);
    TestEqual(TEXT("retry is a no-op"), Reason(State->SelectMilestone(Op, 70, SFCompanionMilestones::RiftCollapse)), FName(TEXT("ok")));
    TestEqual(TEXT("retry changed nothing"), State->ActiveOption(70), SFCompanionMilestones::LongShadow);
    TestEqual(TEXT("retry added no revision"), State->GetRevision(), 1);

    // Leaving the rest area locks the choice again; combat also blocks it.
    Pawn->SetActorLocation(FVector(0, 0, 100));
    TestEqual(TEXT("swap refused after leaving"), Reason(State->SelectMilestone(FGuid::NewGuid(), 70, SFCompanionMilestones::RiftCollapse)), FName(TEXT("not_at_rest_area")));
    TestEqual(TEXT("active choice kept"), State->ActiveOption(70), SFCompanionMilestones::LongShadow);
    Pawn->SetActorLocation(Rest->GetActorLocation() + FVector(0, 0, 100));
    State->bOwnerInCombat = true;
    TestEqual(TEXT("no swap in combat even at a rest area"), Reason(State->SelectMilestone(FGuid::NewGuid(), 70, SFCompanionMilestones::RiftCollapse)), FName(TEXT("in_combat")));
    State->bOwnerInCombat = false;
    State->RequestMilestone(70, SFCompanionMilestones::RiftCollapse);  // authority path of the client entry point
    TestEqual(TEXT("request swaps the choice"), State->ActiveOption(70), SFCompanionMilestones::RiftCollapse);

    // The result is replicated state (owner-only via COND_OwnerOnly in GetLifetimeReplicatedProps).
    for (const TCHAR* Name : { TEXT("Active50"), TEXT("Active70"), TEXT("Revision") })
    {
        const FProperty* Prop = USFCompanionStateComponent::StaticClass()->FindPropertyByName(Name);
        TestTrue(FString::Printf(TEXT("%s is replicated"), Name), Prop && Prop->HasAnyPropertyFlags(CPF_Net));
    }
    return true;
}
#endif
