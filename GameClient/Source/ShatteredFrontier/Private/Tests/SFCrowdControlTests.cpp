#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Combat/SFCrowdControlComponent.h"
#include "Creatures/SFCompanionMilestones.h"
#include "Creatures/SFVoidreignCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"

namespace
{
/** A plain character with a crowd-control component and a 600 cm/s walk speed in a begun test world. */
struct FCcWorld
{
    FTestWorldWrapper Fixture;
    bool Begin(FAutomationTestBase& Test)
    {
        if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(&Test); return false; }
        Fixture.GetTestWorld()->GetWorldSettings()->NotifyBeginPlay();
        return true;
    }
    USFCrowdControlComponent* Spawn(const FVector& At, ACharacter*& OutCharacter)
    {
        OutCharacter = Fixture.GetTestWorld()->SpawnActor<ACharacter>(ACharacter::StaticClass(), At, FRotator::ZeroRotator);
        OutCharacter->GetCharacterMovement()->MaxWalkSpeed = 600.f;
        USFCrowdControlComponent* Cc = NewObject<USFCrowdControlComponent>(OutCharacter);
        Cc->RegisterComponent();
        return Cc;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCrowdControlRulesTest, "ShatteredFrontier.Combat.CrowdControl.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCrowdControlRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("DR 1st"), SFCrowdControlRules::DrStepPercent(0), 100);
    TestEqual(TEXT("DR 2nd"), SFCrowdControlRules::DrStepPercent(1), 50);
    TestEqual(TEXT("DR 3rd"), SFCrowdControlRules::DrStepPercent(2), 25);
    TestEqual(TEXT("DR 4th immune"), SFCrowdControlRules::DrStepPercent(3), 0);
    FString Error;
    USFCrowdControlSettings* Settings = NewObject<USFCrowdControlSettings>();
    TestTrue(TEXT("shipped defaults are valid"), Settings->Validate(Error));
    Settings->DrWindowMs = 500;
    TestFalse(TEXT("window below 5 s is invalid"), Settings->Validate(Error));
    Settings->DrWindowMs = 18000; Settings->SnareCapBp = 9500;
    TestFalse(TEXT("snare cap above 90% is invalid"), Settings->Validate(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCrowdControlStatusTest, "ShatteredFrontier.Combat.CrowdControl.ApplyExpireStack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCrowdControlStatusTest::RunTest(const FString& Parameters)
{
    FCcWorld World;
    if (!World.Begin(*this)) return false;
    ACharacter* Target = nullptr;
    USFCrowdControlComponent* Cc = World.Spawn(FVector(0, 0, 200), Target);
    UCharacterMovementComponent* Move = Target->GetCharacterMovement();

    // Snare: applies, strongest wins instead of stacking, expires per instance.
    TestEqual(TEXT("snare applies its full time"), Cc->Apply(ESFCrowdControl::Snare, 3000, 2000), 2000);
    TestEqual(TEXT("30% snare"), Cc->GetSpeedMultBp(), 7000);
    TestEqual(TEXT("walk speed scaled on the server"), Move->MaxWalkSpeed, 420.f);
    Cc->Advance(1.0f);
    Cc->Apply(ESFCrowdControl::Snare, 5000, 500);
    TestEqual(TEXT("stronger snare wins, not 80%"), Cc->GetSpeedMultBp(), 5000);
    Cc->Apply(ESFCrowdControl::Snare, 1000, 500);
    TestEqual(TEXT("weaker snare does not stack"), Cc->GetSpeedMultBp(), 5000);
    Cc->Advance(0.6f);
    TestEqual(TEXT("strong snare expired, 30% remains"), Cc->GetSpeedMultBp(), 7000);
    Cc->Advance(0.5f);
    TestEqual(TEXT("all snares expired"), Cc->GetSpeedMultBp(), 10000);
    TestEqual(TEXT("walk speed restored"), Move->MaxWalkSpeed, 600.f);
    Cc->Apply(ESFCrowdControl::Snare, 9000, 1000);
    TestEqual(TEXT("snare capped at 70%"), Cc->GetSpeedMultBp(), 3000);
    Cc->Advance(1.1f);

    // Root: zero speed, extends rather than stacks; stun is separate.
    Cc->Apply(ESFCrowdControl::Root, 0, 1000);
    TestTrue(TEXT("rooted"), Cc->IsRooted());
    TestEqual(TEXT("root stops movement"), Move->MaxWalkSpeed, 0.f);
    Cc->Advance(0.5f);
    Cc->Apply(ESFCrowdControl::Root, 0, 250);  // 2nd hit is halved to 125 ms: ends before the first, which keeps its end
    Cc->Advance(0.6f);
    TestFalse(TEXT("root expired at the later end only"), Cc->IsRooted());
    TestEqual(TEXT("speed back after root"), Move->MaxWalkSpeed, 600.f);

    // Diminishing returns on stun (Combat Part 3 worked example) and the window reset.
    Cc->Advance(20.f);
    TestEqual(TEXT("stun 1st"), Cc->Apply(ESFCrowdControl::Stun, 0, 4000, 1500), 3400);
    TestTrue(TEXT("stunned"), Cc->IsStunned());
    TestTrue(TEXT("immobilised"), Cc->IsImmobilised());
    TestEqual(TEXT("stun 2nd"), Cc->Apply(ESFCrowdControl::Stun, 0, 4000, 1500), 1700);
    TestEqual(TEXT("stun 3rd"), Cc->Apply(ESFCrowdControl::Stun, 0, 4000, 1500), 850);
    TestEqual(TEXT("stun 4th immune"), Cc->Apply(ESFCrowdControl::Stun, 0, 4000, 1500), 0);
    TestEqual(TEXT("reduction is capped at 40%"), Cc->Apply(ESFCrowdControl::Root, 0, 1000, 9000), 600);
    Cc->Advance(5.f);
    TestFalse(TEXT("stun expired"), Cc->IsStunned());
    Cc->Advance(18.5f);
    TestEqual(TEXT("DR resets after the window"), Cc->Apply(ESFCrowdControl::Stun, 0, 4000, 1500), 3400);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCrowdControlRadiusTest, "ShatteredFrontier.Combat.CrowdControl.RadiusAndRiftCollapse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCrowdControlRadiusTest::RunTest(const FString& Parameters)
{
    FCcWorld World;
    if (!World.Begin(*this)) return false;
    ACharacter *Near = nullptr, *Far = nullptr;
    USFCrowdControlComponent* NearCc = World.Spawn(FVector(0, 0, 200), Near);
    USFCrowdControlComponent* FarCc = World.Spawn(FVector(2000, 0, 200), Far);
    TestEqual(TEXT("one target in range"), USFCrowdControlComponent::ApplyInRadius(World.Fixture.GetTestWorld(), FVector(100, 0, 200), 500.f, nullptr, ESFCrowdControl::Snare, 3000, 1000), 1);
    TestEqual(TEXT("near target snared"), NearCc->GetSpeedMultBp(), 7000);
    TestEqual(TEXT("far target untouched"), FarCc->GetSpeedMultBp(), 10000);
    NearCc->Advance(1.1f);

    // Rift Collapse: the Voidreign recalls, the rift implodes at the recall point and snares what stands there.
    UClass* HorseClass = LoadClass<ASFVoidreignCharacter>(nullptr, TEXT("/Game/Frontier/Creatures/Voidreign/V1/BP_Voidreign.BP_Voidreign_C"));
    if (!TestNotNull(TEXT("BP_Voidreign loads"), HorseClass)) return false;
    ASFVoidreignCharacter* Horse = World.Fixture.GetTestWorld()->SpawnActor<ASFVoidreignCharacter>(HorseClass, FVector(0, 5000, 200), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Horse spawns"), Horse)) return false;
    Horse->SetRiftOption(SFCompanionMilestones::RiftCollapse);
    Horse->PlayRiftRecall();
    for (float T = 0.f; !Horse->IsRecallWindowOpen() && T < 10.f; T += 1.f / 30.f) Horse->Tick(1.f / 30.f);
    TestTrue(TEXT("return window opens"), Horse->IsRecallWindowOpen());
    ACharacter* Victim = nullptr;
    USFCrowdControlComponent* VictimCc = World.Spawn(Horse->GetActorLocation() + FVector(150, 0, 0), Victim);
    Horse->RequestRiftRecall();
    TestEqual(TEXT("recall accepted"), (int32)Horse->LastRecallReject(), (int32)ESFRiftRecallReject::None);
    for (float T = 0.f; T < 4.f; T += 1.f / 30.f) Horse->Tick(1.f / 30.f);
    TestEqual(TEXT("Rift Collapse snared the target by the configured strength"), VictimCc->GetSpeedMultBp(), 10000 - GetDefault<USFVoidreignSettings>()->RiftCollapseSnareBp);
    TestEqual(TEXT("the Voidreign is not snared by its own rift"), Horse->CrowdControl->GetSpeedMultBp(), 10000);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCrowdControlRecallTest, "ShatteredFrontier.Combat.CrowdControl.RiftRecallRefusedWhileStunned",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCrowdControlRecallTest::RunTest(const FString& Parameters)
{
    FCcWorld World;
    if (!World.Begin(*this)) return false;
    UClass* HorseClass = LoadClass<ASFVoidreignCharacter>(nullptr, TEXT("/Game/Frontier/Creatures/Voidreign/V1/BP_Voidreign.BP_Voidreign_C"));
    if (!TestNotNull(TEXT("BP_Voidreign loads"), HorseClass)) return false;
    ASFVoidreignCharacter* Horse = World.Fixture.GetTestWorld()->SpawnActor<ASFVoidreignCharacter>(HorseClass, FVector(0, 0, 200), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Horse spawns"), Horse)) return false;
    Horse->PlayRiftRecall();
    for (float T = 0.f; !Horse->IsRecallWindowOpen() && T < 10.f; T += 1.f / 30.f) Horse->Tick(1.f / 30.f);
    TestTrue(TEXT("return window opens"), Horse->IsRecallWindowOpen());

    Horse->CrowdControl->Apply(ESFCrowdControl::Stun, 0, 1000);
    Horse->RequestRiftRecall();
    TestEqual(TEXT("stunned recall refused"), (int32)Horse->LastRecallReject(), (int32)ESFRiftRecallReject::Incapacitated);
    TestTrue(TEXT("anchor kept: window still open"), Horse->IsRecallWindowOpen());
    Horse->CrowdControl->Advance(1.1f);
    Horse->CrowdControl->Apply(ESFCrowdControl::Root, 0, 1000);
    Horse->RequestRiftRecall();
    TestEqual(TEXT("rooted recall refused"), (int32)Horse->LastRecallReject(), (int32)ESFRiftRecallReject::Incapacitated);
    Horse->CrowdControl->Advance(1.1f);
    Horse->RequestRiftRecall();
    TestEqual(TEXT("recall accepted once free"), (int32)Horse->LastRecallReject(), (int32)ESFRiftRecallReject::None);
    return true;
}
#endif
