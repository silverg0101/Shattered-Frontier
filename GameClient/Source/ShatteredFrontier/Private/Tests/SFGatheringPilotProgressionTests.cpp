#include "Gathering/SFGatheringPilot.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && SF_WITH_MINIMAL_INVENTORY
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "GameFramework/WorldSettings.h"
#include "Construction/SFConstructionNetwork.h"
#include "Characters/SFE1Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryTransactionLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringPilotProgressionTest,
    "ShatteredFrontier.Gathering.Pilot.Progression",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFGatheringPilotProgressionTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC)||!Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC=World->SpawnActor<ASFConstructionNetworkController>();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn=World->SpawnActor<ASFE1Character>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
    if (!PC||!Pawn) { return false; }
    PC->Possess(Pawn); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None); Pawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Gather=PC->FindComponentByClass<USFGatheringPilotComponent>(); auto* Bag=PC->FindComponentByClass<UInventoryComponent>();
    if (!Gather||!Bag) { return false; }
    Gather->bEnabled=true; Gather->bTestWorldAdmission=true;
    Gather->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),Gather);
    Gather->CapturedSettings->HarvestSeconds=0.5f;
    Gather->ProgressionSession.TestRoll=0;
    FInventoryItemQuantity Axe; Axe.Item=TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronAxe.DA_IronAxe"))); Axe.Quantity=1;
    if (UInventoryTransactionLibrary::ExchangeInventoryItems(Bag,{}, {Axe})!=EInventoryTransactionResult::Success) { return false; }
    bool bEquipped=false;
    for (int32 I=0;I<Bag->NumSlots;++I) { TSoftObjectPtr<UInventoryItemDefinition> Item; int32 Count=0; if (Bag->GetSlot(I,Item,Count)&&Item==Axe.Item) { bEquipped=Bag->EquipFromSlot(I); break; } }
    if (!TestTrue(TEXT("Own actual tier-one axe equipped"),bEquipped)) { return false; }
    auto* Wood=World->SpawnActor<ASFGatheringPilotNode>(FVector(200,0,100),FRotator::ZeroRotator,Spawn);
    if (!Wood) { return false; } Wood->State.Charges=3;
    const auto Tick=[&Fixture]() { for (int32 I=0;I<12;++I) { Fixture.TickTestWorld(0.05f); } };
    Gather->ServerGatheringAction_Implementation(Wood,0,1);
    TestEqual(TEXT("Starting channel does not grant practice"),Gather->Skills.PracticeCount,int64(0));
    Tick();
    TestEqual(TEXT("Actual committed inventory transaction produces practice"),Gather->Feedback.Result,ESFGatheringPilotResult::Granted);
    TestEqual(TEXT("One completed harvest produces one practice record"),Gather->Skills.PracticeCount,int64(1));
    TestEqual(TEXT("Injected passing server roll advances logging"),Gather->Skills.Get(ESFGatheringSkill::Logging),1);
    TestTrue(TEXT("Owner feedback reports actual gain"),Gather->Feedback.bSkillGained);
    TestEqual(TEXT("Mining does not gain from logging"),Gather->Skills.Get(ESFGatheringSkill::Mining),0);
    Gather->ServerGatheringAction_Implementation(Wood,0,1); Tick();
    TestEqual(TEXT("Request replay cannot duplicate practice"),Gather->Skills.PracticeCount,int64(1));
    TestEqual(TEXT("Request replay cannot duplicate skill point"),Gather->Skills.Get(ESFGatheringSkill::Logging),1);
    Gather->ServerGatheringAction_Implementation(Wood,1,2); Gather->ServerCancelHarvest_Implementation(2); Tick();
    TestEqual(TEXT("Canceled channel gives no practice"),Gather->Skills.PracticeCount,int64(1));
    Bag->MaxWeight=0.1f; Gather->ServerGatheringAction_Implementation(Wood,1,3); Tick();
    TestEqual(TEXT("Failed capacity preflight gives no practice"),Gather->Skills.PracticeCount,int64(1));
    TestEqual(TEXT("Failed/canceled channels preserve remaining charges"),Wood->State.Charges,2);
    Bag->MaxWeight=24;
    Gather->Skills.Locks[int32(ESFGatheringSkill::Logging)]=ESFGatheringSkillLock::Locked;
    Gather->ServerGatheringAction_Implementation(Wood,1,4); Tick();
    TestEqual(TEXT("Locked skill still permits beginner harvesting"),Gather->Feedback.Result,ESFGatheringPilotResult::Granted);
    TestEqual(TEXT("Locked successful harvest still records practice"),Gather->Skills.PracticeCount,int64(2));
    TestEqual(TEXT("Locked skill cannot gain"),Gather->Skills.Get(ESFGatheringSkill::Logging),1);
    TestFalse(TEXT("No gain notification fabricated for locked skill"),Gather->Feedback.bSkillGained);
    // Test-only content seam exercises future authored tier gates without publishing
    // advanced material identities or exposing a live tier mutation setting/RPC.
    Wood->TestProfileTier=ESFGatheringMaterialTier::Uncommon; Gather->Skills.Values[1]=99;
    Gather->ServerGatheringAction_Implementation(Wood,2,5);
    TestEqual(TEXT("Below authored skill threshold rejects before reservation"),Gather->Feedback.Result,ESFGatheringPilotResult::SkillRequired);
    TestFalse(TEXT("Rejected skill gate never locks the node"),Wood->State.bBusy);
    TestEqual(TEXT("Owner sees exact required skill"),Gather->Feedback.RequiredSkill,100);
    TestEqual(TEXT("Rejected skill gate produces no practice"),Gather->Skills.PracticeCount,int64(2));
    Gather->Skills.Values[1]=100;
    Gather->ServerGatheringAction_Implementation(Wood,2,6);
    Gather->Skills.Values[1]=99; Tick();
    TestEqual(TEXT("Skill requirement rechecked during channel"),Gather->Feedback.Result,ESFGatheringPilotResult::SkillRequired);
    TestEqual(TEXT("Losing required skill cannot generate practice"),Gather->Skills.PracticeCount,int64(2));
    Gather->Skills.Values[1]=100;
    Gather->ServerGatheringAction_Implementation(Wood,2,7); Tick();
    TestEqual(TEXT("Exact skill threshold allows real harvest"),Gather->Feedback.Result,ESFGatheringPilotResult::Granted);
    TestEqual(TEXT("Passing threshold records a new practice"),Gather->Skills.PracticeCount,int64(3));
    Gather->Skills.Values[1]=1;
    const FString Status=Gather->GetStatusText(FVector(0,0,160),FVector(1,0,0));
    TestTrue(TEXT("Player can see actual session logging skill"),Status.Contains(TEXT("Logging 1")));
    return true;
}
#endif
