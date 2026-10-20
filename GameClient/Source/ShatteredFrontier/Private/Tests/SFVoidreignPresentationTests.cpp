#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Creatures/SFVoidreignCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/WorldSettings.h"

namespace
{
constexpr float Step = 1.f / 30.f;

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
        Horse->Tick(Step);
        Time += Step;
        MaxDistance = FMath::Max(MaxDistance, FVector::Dist2D(Start, Horse->GetActorLocation()));
        PeakVfx = FMath::Max(PeakVfx, VfxCount(Horse));
    }
    return Time;
}
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
    TestTrue(TEXT("Attack lasts its clip"), FMath::IsNearlyEqual(AttackTime, Horse->MountAttack->GetPlayLength(), 2 * Step));
    TestTrue(TEXT("Attack spawns its hit spark"), PeakVfx > BaseVfx);
    TestTrue(TEXT("Attack stays in place"), Reach < 1.f);

    Reach = 0.f;
    PeakVfx = 0;
    const FVector Anchor = Horse->GetActorLocation();
    Horse->PlayRiftRecall();
    const float RiftTime = RunAction(Horse, Anchor, 15.f, Reach, PeakVfx);
    TestFalse(TEXT("Rift Recall finishes"), Horse->IsPerformingAction());
    TestTrue(TEXT("Rift Charge covers the charge distance"), Reach >= Horse->ChargeDistance - 1.f);
    TestTrue(TEXT("Recall returns to the anchor"), FVector::Dist(Anchor, Horse->GetActorLocation()) < 1.f);
    TestTrue(TEXT("Rift Recall shows open, anchor and trail effects"), PeakVfx >= 3);
    TestTrue(TEXT("Rift Recall completes in a sane time"), RiftTime > 2.f && RiftTime < 12.f);
    AddInfo(FString::Printf(TEXT("Attack %.2fs, Rift Recall %.2fs, reach %.0f cm, peak VFX %d"), AttackTime, RiftTime, Reach, PeakVfx));
    return true;
}
#endif
