#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Creatures/SFVoidreignCharacter.h"
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
#endif
