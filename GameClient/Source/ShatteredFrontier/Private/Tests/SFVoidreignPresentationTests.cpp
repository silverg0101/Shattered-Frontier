#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Creatures/SFVoidreignCharacter.h"
#include "Creatures/SFCompanionStateComponent.h"
#include "Creatures/SFRestAreaVolume.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"

namespace
{
constexpr float FrameStep = 1.f / 30.f;

int32 VfxCount(const AActor* Actor)
{
    TArray<UStaticMeshComponent*> Planes;
    Actor->GetComponents(Planes);
    return Planes.Num();
}

/** Ticks until the action ends or MaxSeconds elapse; returns elapsed seconds and the farthest distance from Start. */
float RunAction(ASFVoidreignCharacter* Horse, const FVector& Start, float MaxSeconds, float& MaxDistance, int32& PeakVfx)
{
    float Time = 0.f;
    while (Horse->IsPerformingAction() && Time < MaxSeconds)
    {
        Horse->Tick(FrameStep);
        Time += FrameStep;
        MaxDistance = FMath::Max(MaxDistance, FVector::Dist2D(Start, Horse->GetActorLocation()));
        PeakVfx = FMath::Max(PeakVfx, VfxCount(Horse));
    }
    return Time;
}

/** Ticks until Done() or MaxSeconds; returns elapsed seconds. */
template <typename FDone>
float RunUntil(ASFVoidreignCharacter* Horse, float MaxSeconds, FDone Done)
{
    float Time = 0.f;
    while (!Done() && Time < MaxSeconds) { Horse->Tick(FrameStep); Time += FrameStep; }
    return Time;
}

struct FVoidreignWorld
{
    FTestWorldWrapper Fixture;
    ASFVoidreignCharacter* Horse = nullptr;
    bool Create(FAutomationTestBase& Test, const FVector& Start)
    {
        UClass* HorseClass = LoadClass<ASFVoidreignCharacter>(nullptr, TEXT("/Game/Frontier/Creatures/Voidreign/V1/BP_Voidreign.BP_Voidreign_C"));
        if (!Test.TestNotNull(TEXT("BP_Voidreign derives from ASFVoidreignCharacter"), HorseClass)) return false;
        if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(&Test); return false; }
        Fixture.GetTestWorld()->GetWorldSettings()->NotifyBeginPlay();
        Horse = Fixture.GetTestWorld()->SpawnActor<ASFVoidreignCharacter>(HorseClass, Start, FRotator::ZeroRotator);
        return Test.TestNotNull(TEXT("Horse spawns"), Horse);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFVoidreignPresentationTest,
    "ShatteredFrontier.Creatures.Voidreign.RiderAttackAndRiftRecall",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFVoidreignPresentationTest::RunTest(const FString& Parameters)
{
    UClass* HorseClass = LoadClass<ASFVoidreignCharacter>(nullptr, TEXT("/Game/Frontier/Creatures/Voidreign/V1/BP_Voidreign.BP_Voidreign_C"));
    if (!TestNotNull(TEXT("BP_Voidreign derives from ASFVoidreignCharacter"), HorseClass)) return false;
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
    World->GetWorldSettings()->NotifyBeginPlay();

    const FVector Start(0, 0, 200);
    ASFVoidreignCharacter* Horse = World->SpawnActor<ASFVoidreignCharacter>(HorseClass, Start, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Horse spawns"), Horse)) return false;

    TestNotNull(TEXT("Rider has a mesh"), Horse->RiderMesh->GetSkeletalMeshAsset());
    TestEqual(TEXT("Rider rides the RiderSeat bone"), Horse->RiderMesh->GetAttachSocketName(), FName(TEXT("RiderSeat")));
    TestTrue(TEXT("Mount skeleton has RiderSeat"), Horse->GetMesh()->DoesSocketExist(TEXT("RiderSeat")));
    TestNotNull(TEXT("Rider seat blend space assigned"), Horse->RiderLocomotion.Get());

    float Reach = 0.f;
    int32 PeakVfx = 0;
    const int32 BaseVfx = VfxCount(Horse);
    Horse->PlayBasicAttack();
    TestTrue(TEXT("Attack starts"), Horse->IsPerformingAction());
    const float AttackTime = RunAction(Horse, Start, 5.f, Reach, PeakVfx);
    TestFalse(TEXT("Attack finishes"), Horse->IsPerformingAction());
    TestTrue(TEXT("Attack lasts its clip"), FMath::IsNearlyEqual(AttackTime, Horse->MountAttack->GetPlayLength(), 2 * FrameStep));
    TestTrue(TEXT("Attack spawns its hit spark"), PeakVfx > BaseVfx);
    TestTrue(TEXT("Attack stays in place"), Reach < 1.f);

    Reach = 0.f;
    PeakVfx = 0;
    const FVector Anchor = Horse->GetActorLocation();
    Horse->PlayRiftRecall();
    float RiftTime = RunUntil(Horse, 10.f, [Horse] { return Horse->IsRecallWindowOpen(); });
    TestTrue(TEXT("Rift Charge opens the return window"), Horse->IsRecallWindowOpen());
    Reach = FVector::Dist2D(Anchor, Horse->GetActorLocation());
    Horse->RequestRiftRecall();
    TestEqual(TEXT("Recall accepted inside the window"), (int32)Horse->LastRecallReject(), (int32)ESFRiftRecallReject::None);
    TestTrue(TEXT("Rider abilities locked from recall start"), Horse->IsRiderLockedOut());
    RiftTime += RunAction(Horse, Anchor, 15.f, Reach, PeakVfx);
    TestFalse(TEXT("Rift Recall finishes"), Horse->IsPerformingAction());
    TestTrue(TEXT("Rift Charge covers the charge distance"), Reach >= Horse->ChargeDistance - 1.f);
    TestTrue(TEXT("Recall returns to the anchor"), FVector::Dist(Anchor, Horse->GetActorLocation()) < 1.f);
    TestTrue(TEXT("Rift Recall shows open, anchor and trail effects"), PeakVfx >= 3);
    TestTrue(TEXT("Rift Recall completes in a sane time"), RiftTime > 2.f && RiftTime < 12.f);
    AddInfo(FString::Printf(TEXT("Attack %.2fs, Rift Recall %.2fs, reach %.0f cm, peak VFX %d"), AttackTime, RiftTime, Reach, PeakVfx));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFRiftRecallRulesTest, "ShatteredFrontier.Creatures.Voidreign.RiftRecallRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFRiftRecallRulesTest::RunTest(const FString& Parameters)
{
    using namespace SFRiftRecallRules;
    auto Check = [this](const TCHAR* What, ESFRiftRecallReject Got, ESFRiftRecallReject Want) { TestEqual(What, (int32)Got, (int32)Want); };
    Check(TEXT("valid"), Validate(true, 1.0, 3.0, 1200.0, 1500.0, true, 1.0, false), ESFRiftRecallReject::None);
    Check(TEXT("no anchor"), Validate(false, 1.0, 3.0, 1200.0, 1500.0, true, 1.0, false), ESFRiftRecallReject::NoAnchor);
    Check(TEXT("window closed"), Validate(true, 3.01, 3.0, 1200.0, 1500.0, true, 1.0, false), ESFRiftRecallReject::WindowClosed);
    Check(TEXT("too far"), Validate(true, 1.0, 3.0, 1501.0, 1500.0, true, 1.0, false), ESFRiftRecallReject::TooFar);
    Check(TEXT("no line of sight"), Validate(true, 1.0, 3.0, 1200.0, 1500.0, false, 1.0, false), ESFRiftRecallReject::NoLineOfSight);
    Check(TEXT("path over 1.5x"), Validate(true, 1.0, 3.0, 1200.0, 1500.0, true, 1.51, false), ESFRiftRecallReject::PathBlocked);
    Check(TEXT("path at 1.5x"), Validate(true, 1.0, 3.0, 1200.0, 1500.0, true, 1.5, false), ESFRiftRecallReject::None);
    Check(TEXT("no path"), Validate(true, 1.0, 3.0, 1200.0, 1500.0, true, 0.0, false), ESFRiftRecallReject::PathBlocked);
    Check(TEXT("incapacitated"), Validate(true, 1.0, 3.0, 1200.0, 1500.0, true, 1.0, true), ESFRiftRecallReject::Incapacitated);

    FString Error;
    USFVoidreignSettings* Settings = NewObject<USFVoidreignSettings>();
    TestTrue(TEXT("shipped defaults are valid"), Settings->Validate(Error));
    Settings->RiftRecallWindowMs = 3100;
    TestFalse(TEXT("window off its 250 ms step is invalid"), Settings->Validate(Error));
    Settings->RiftRecallWindowMs = 3000; Settings->RiftAnchorMaxMetres = 16.f;
    TestFalse(TEXT("distance above 15 m is invalid"), Settings->Validate(Error));
    Settings->RiftAnchorMaxMetres = 15.f; Settings->RiftRiderLockoutMs = 1050;
    TestFalse(TEXT("lockout above 1000 ms is invalid"), Settings->Validate(Error));
    Settings->RiftRiderLockoutMs = 500; Settings->LongShadowWindowMs = 5250;
    TestFalse(TEXT("Long Shadow window above 5000 ms is invalid"), Settings->Validate(Error));
    Settings->LongShadowWindowMs = 4500; Settings->RiftCollapseSnareBp = 3050;
    TestFalse(TEXT("Rift Collapse snare off its 100 bp step is invalid"), Settings->Validate(Error));
    Settings->RiftCollapseSnareBp = 3000; Settings->LongShadowHoldMs = 500;
    TestFalse(TEXT("Long Shadow hold below 1000 ms is invalid"), Settings->Validate(Error));
    Settings->LongShadowHoldMs = 2000;
    TestTrue(TEXT("restored defaults are valid"), Settings->Validate(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFRiftRecallServerTest, "ShatteredFrontier.Creatures.Voidreign.RiftRecallServer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFRiftRecallServerTest::RunTest(const FString& Parameters)
{
    FVoidreignWorld World;
    if (!World.Create(*this, FVector(0, 0, 200))) return false;
    ASFVoidreignCharacter* Horse = World.Horse;
    const USFVoidreignSettings* Settings = GetDefault<USFVoidreignSettings>();

    // Window expiry: no re-press, the anchor is lost and a late request is refused.
    Horse->PlayRiftRecall();
    RunUntil(Horse, 10.f, [Horse] { return Horse->IsRecallWindowOpen(); });
    const FVector Impact = Horse->GetActorLocation();
    const float Window = RunUntil(Horse, 10.f, [Horse] { return !Horse->IsPerformingAction(); });
    TestTrue(TEXT("Window lasts the configured time"), FMath::IsNearlyEqual(Window, Settings->RiftRecallWindowMs / 1000.f, 2 * FrameStep));
    TestTrue(TEXT("No re-press: stays at the impact point"), FVector::Dist(Impact, Horse->GetActorLocation()) < 1.f);
    Horse->RequestRiftRecall();
    TestEqual(TEXT("Late recall refused"), (int32)Horse->LastRecallReject(), (int32)ESFRiftRecallReject::NoAnchor);
    TestFalse(TEXT("Late recall starts nothing"), Horse->IsPerformingAction());

    // Wall between impact and anchor: refused, window kept, a clear press in the same window is accepted.
    const FVector Anchor = Horse->GetActorLocation();
    Horse->PlayRiftRecall();
    RunUntil(Horse, 10.f, [Horse] { return Horse->IsRecallWindowOpen(); });
    AStaticMeshActor* Wall = World.Fixture.GetTestWorld()->SpawnActor<AStaticMeshActor>((Anchor + Horse->GetActorLocation()) / 2, FRotator::ZeroRotator);
    Wall->SetMobility(EComponentMobility::Movable);
    Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Wall->SetActorScale3D(FVector(1, 20, 10));
    Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Horse->RequestRiftRecall();
    TestEqual(TEXT("Blocked return refused"), (int32)Horse->LastRecallReject(), (int32)ESFRiftRecallReject::NoLineOfSight);
    TestTrue(TEXT("Refusal keeps the window open"), Horse->IsRecallWindowOpen());
    Wall->Destroy();
    Horse->RequestRiftRecall();
    TestEqual(TEXT("Clear return accepted in the same window"), (int32)Horse->LastRecallReject(), (int32)ESFRiftRecallReject::None);

    // Arrival plus rider lockout.
    RunUntil(Horse, 10.f, [Horse] { return !Horse->IsPerformingAction(); });
    TestTrue(TEXT("Back at the anchor"), FVector::Dist(Anchor, Horse->GetActorLocation()) < 1.f);
    const float Locked = RunUntil(Horse, 3.f, [Horse] { return !Horse->IsRiderLockedOut(); });
    TestTrue(TEXT("Rider lockout ends after arrival"), !Horse->IsRiderLockedOut() && Locked <= Settings->RiftRiderLockoutMs / 1000.f + 2 * FrameStep);
    return true;
}

/** Phase and anchor replication: the replicated properties exist, and a proxy that receives the server's state plays the
 *  matching montage in the DefaultSlot and shows/clears the anchor effect. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFVoidreignReplicationTest, "ShatteredFrontier.Creatures.Voidreign.PhaseReplication",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFVoidreignReplicationTest::RunTest(const FString& Parameters)
{
    for (const TCHAR* Name : { TEXT("Phase"), TEXT("AnchorLocation") })
    {
        const FProperty* Prop = ASFVoidreignCharacter::StaticClass()->FindPropertyByName(Name);
        TestTrue(FString::Printf(TEXT("%s is replicated"), Name), Prop && Prop->HasAnyPropertyFlags(CPF_Net));
    }
    TestTrue(TEXT("Phase notifies clients"), ASFVoidreignCharacter::StaticClass()->FindPropertyByName(TEXT("Phase"))->HasAnyPropertyFlags(CPF_RepNotify));

    FVoidreignWorld World;
    if (!World.Create(*this, FVector(0, 0, 200))) return false;
    ASFVoidreignCharacter* Horse = World.Horse;
    ASFVoidreignCharacter* Proxy = World.Fixture.GetTestWorld()->SpawnActor<ASFVoidreignCharacter>(Horse->GetClass(), FVector(5000, 0, 200), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Proxy spawns"), Proxy)) return false;

    // The clip the proxy's active montage plays in the DefaultSlot (null when none is active).
    auto SlotClip = [this](ASFVoidreignCharacter* H) -> const UAnimSequenceBase*
    {
        UAnimInstance* Anim = H->GetMesh()->GetAnimInstance();
        if (!TestNotNull(TEXT("Mount has an anim instance"), Anim)) return nullptr;
        const UAnimMontage* Montage = Anim->GetCurrentActiveMontage();
        if (!Montage || Montage->SlotAnimTracks.Num() == 0) return nullptr;
        TestEqual(TEXT("Montage plays in DefaultSlot"), Montage->SlotAnimTracks[0].SlotName, FName(TEXT("DefaultSlot")));
        const TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;
        return Segments.Num() ? Segments[0].GetAnimReference().Get() : nullptr;
    };
    auto Receive = [Horse, Proxy]
    {
        Proxy->Phase = Horse->Phase;
        Proxy->AnchorLocation = Horse->AnchorLocation;
        Proxy->OnRep_Phase();
    };
    const TMap<ESFVoidreignPhase, const UAnimSequence*> Expected = {
        { ESFVoidreignPhase::Windup, Horse->RiftWindup }, { ESFVoidreignPhase::Charge, Horse->RiftChargeRun },
        { ESFVoidreignPhase::Impact, Horse->RiftImpact }, { ESFVoidreignPhase::RecallEnter, Horse->RecallEnter },
        { ESFVoidreignPhase::RecallExit, Horse->RecallExit } };

    Horse->PlayRiftRecall();
    ESFVoidreignPhase Seen = ESFVoidreignPhase::None;
    TSet<ESFVoidreignPhase> Phases;
    bool bRecalled = false;
    for (int32 Frame = 0; Frame < 30 * 30 && Horse->IsPerformingAction(); ++Frame)
    {
        if (Horse->GetPhase() != Seen)
        {
            Seen = Horse->GetPhase();
            Receive();
            Phases.Add(Seen);
            TestEqual(TEXT("Proxy phase follows the server"), (int32)Proxy->GetPhase(), (int32)Seen);
            TestTrue(TEXT("Anchor location replicated"), Proxy->AnchorLocation.Equals(Horse->AnchorLocation));
            if (const UAnimSequence* const* Clip = Expected.Find(Seen)) TestTrue(*FString::Printf(TEXT("Proxy plays the clip for phase %d"), (int32)Seen), SlotClip(Proxy) == *Clip);
            const bool bAnchored = Seen >= ESFVoidreignPhase::Windup && Seen <= ESFVoidreignPhase::RecallEnter;
            TestEqual(*FString::Printf(TEXT("Anchor effect in phase %d"), (int32)Seen), Proxy->AnchorVfx != nullptr, bAnchored);
            if (Seen == ESFVoidreignPhase::Hold && !bRecalled) { bRecalled = true; Horse->RequestRiftRecall(); }
        }
        Horse->Tick(FrameStep);
    }
    TestTrue(TEXT("Proxy saw charge, return window, recall and exit"),
        Phases.Contains(ESFVoidreignPhase::Charge) && Phases.Contains(ESFVoidreignPhase::Hold) && Phases.Contains(ESFVoidreignPhase::RecallEnter) && Phases.Contains(ESFVoidreignPhase::RecallExit));
    Receive();
    TestEqual(TEXT("Proxy returns to locomotion"), (int32)Proxy->GetPhase(), (int32)ESFVoidreignPhase::None);
    TestNull(TEXT("Anchor effect cleared"), Proxy->AnchorVfx.Get());

    // A client that joins mid-window gets the anchor from the Hold phase alone.
    Horse->PlayRiftRecall();
    RunUntil(Horse, 10.f, [Horse] { return Horse->IsRecallWindowOpen(); });
    ASFVoidreignCharacter* Late = World.Fixture.GetTestWorld()->SpawnActor<ASFVoidreignCharacter>(Horse->GetClass(), FVector(5000, 500, 200), FRotator::ZeroRotator);
    Late->Phase = Horse->Phase;
    Late->AnchorLocation = Horse->AnchorLocation;
    Late->OnRep_Phase();
    TestNotNull(TEXT("Late joiner sees the anchor"), Late->AnchorVfx.Get());
    return true;
}

namespace
{
/** Picks a level-70 option through the real rest-area gate, as a player standing in a rest area would. */
bool ChooseRiftOption(FAutomationTestBase& Test, FVoidreignWorld& World, FName Option)
{
    UWorld* W = World.Fixture.GetTestWorld();
    W->SpawnActor<ASFRestAreaVolume>(World.Horse->GetActorLocation(), FRotator::ZeroRotator);
    USFCompanionStateComponent* State = NewObject<USFCompanionStateComponent>(World.Horse);
    State->RegisterComponent();
    State->Family = TEXT("voidreign");
    State->CompanionLevel = 70;
    State->Unlocked = { Option };
    State->Companion = World.Horse;
    return Test.TestEqual(TEXT("choice accepted at the rest area"), (int32)State->SelectMilestone(FGuid::NewGuid(), 70, Option), (int32)ESFMilestoneReject::None);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLongShadowTest, "ShatteredFrontier.Creatures.Voidreign.LongShadow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLongShadowTest::RunTest(const FString& Parameters)
{
    FVoidreignWorld World;
    if (!World.Create(*this, FVector(0, 0, 200))) return false;
    ASFVoidreignCharacter* Horse = World.Horse;
    const USFVoidreignSettings* Settings = GetDefault<USFVoidreignSettings>();
    auto WindowLength = [&](bool bHold)
    {
        Horse->PlayRiftRecall();
        RunUntil(Horse, 10.f, [Horse] { return Horse->IsRecallWindowOpen(); });
        if (bHold) Horse->HoldRiftAnchor(true);
        return RunUntil(Horse, 20.f, [Horse] { return !Horse->IsPerformingAction(); });
    };

    // Without the option the hold is refused and the window is the base one.
    Horse->PlayRiftRecall();
    RunUntil(Horse, 10.f, [Horse] { return Horse->IsRecallWindowOpen(); });
    Horse->HoldRiftAnchor(true);
    TestFalse(TEXT("No hold without Long Shadow"), Horse->IsHoldingAnchor());
    RunUntil(Horse, 20.f, [Horse] { return !Horse->IsPerformingAction(); });

    if (!ChooseRiftOption(*this, World, SFCompanionMilestones::LongShadow)) return false;
    TestTrue(TEXT("Return window extended to long_shadow_window_ms"),
        FMath::IsNearlyEqual(WindowLength(false), Settings->LongShadowWindowMs / 1000.f, 2 * FrameStep));
    const float Held = WindowLength(true);
    TestTrue(TEXT("Hold pauses the window for at most long_shadow_hold_ms"),
        FMath::IsNearlyEqual(Held, (Settings->LongShadowWindowMs + Settings->LongShadowHoldMs) / 1000.f, 2 * FrameStep));
    TestFalse(TEXT("Hold budget spent"), Horse->IsHoldingAnchor());

    // A held anchor still obeys the normal validation: recall is accepted from the held window.
    const FVector Anchor = Horse->GetActorLocation();
    Horse->PlayRiftRecall();
    RunUntil(Horse, 10.f, [Horse] { return Horse->IsRecallWindowOpen(); });
    Horse->HoldRiftAnchor(true);
    RunUntil(Horse, Settings->LongShadowWindowMs / 1000.f + 1.f, [] { return false; });
    TestTrue(TEXT("Window still open past the base window while held"), Horse->IsRecallWindowOpen());
    Horse->RequestRiftRecall();
    TestEqual(TEXT("Delayed return accepted"), (int32)Horse->LastRecallReject(), (int32)ESFRiftRecallReject::None);
    RunUntil(Horse, 15.f, [Horse] { return !Horse->IsPerformingAction(); });
    TestTrue(TEXT("Returned to the held anchor"), FVector::Dist(Anchor, Horse->GetActorLocation()) < 1.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFRiftCollapseTest, "ShatteredFrontier.Creatures.Voidreign.RiftCollapse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFRiftCollapseTest::RunTest(const FString& Parameters)
{
    FVoidreignWorld World;
    if (!World.Create(*this, FVector(0, 0, 200))) return false;
    ASFVoidreignCharacter* Horse = World.Horse;
    const USFVoidreignSettings* Settings = GetDefault<USFVoidreignSettings>();
    if (!ChooseRiftOption(*this, World, SFCompanionMilestones::RiftCollapse)) return false;

    int32 Fired = 0, SnareBp = 0, SnareMs = 0;
    float Radius = 0.f;
    FVector Point = FVector::ZeroVector;
    Horse->OnRiftCollapse.AddLambda([&](const FVector& At, float R, int32 Bp, int32 Ms) { ++Fired; Point = At; Radius = R; SnareBp = Bp; SnareMs = Ms; });

    // No successful recall, no collapse.
    Horse->PlayRiftRecall();
    RunUntil(Horse, 20.f, [Horse] { return !Horse->IsPerformingAction(); });
    RunUntil(Horse, 3.f, [] { return false; });
    TestEqual(TEXT("Expired window collapses nothing"), Fired, 0);

    // Recall: the rift at the impact point implodes rift_collapse_delay_ms later, once.
    Horse->PlayRiftRecall();
    RunUntil(Horse, 10.f, [Horse] { return Horse->IsRecallWindowOpen(); });
    const FVector Impact = Horse->GetActorLocation();
    Horse->RequestRiftRecall();
    TestEqual(TEXT("Recall accepted"), (int32)Horse->LastRecallReject(), (int32)ESFRiftRecallReject::None);
    const float Delay = RunUntil(Horse, 5.f, [&Fired] { return Fired > 0; });
    TestEqual(TEXT("Collapse fired once"), Fired, 1);
    TestTrue(TEXT("Collapse comes rift_collapse_delay_ms after the recall"), FMath::IsNearlyEqual(Delay, Settings->RiftCollapseDelayMs / 1000.f, 2 * FrameStep));
    TestTrue(TEXT("Collapse is at the impact point"), FVector::Dist(Point, Impact) < 1.f);
    TestEqual(TEXT("Snare strength"), SnareBp, Settings->RiftCollapseSnareBp);
    TestEqual(TEXT("Snare time"), SnareMs, Settings->RiftCollapseSnareMs);
    TestEqual(TEXT("Snare radius is the release-bound one"), Radius, SFRiftRecallRules::CollapseRadiusCm);
    RunUntil(Horse, 15.f, [Horse] { return !Horse->IsPerformingAction(); });
    RunUntil(Horse, 3.f, [] { return false; });
    TestEqual(TEXT("No second collapse"), Fired, 1);
    return true;
}
#endif
