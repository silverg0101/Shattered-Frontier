#include "Gathering/SFGatheringPilot.h"
#include "Framework/SFPlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "GameFramework/WorldSettings.h"
#include "Construction/SFConstructionNetwork.h"
#include "Characters/SFE1Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Inventory/SFInventory.h"
#include "Inventory/SFInventoryLedgerClient.h"
#include "Gathering/SFGatheringSkillsPanel.h"
#include "UI/SFUiWidgets.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringSkillCapabilityTest,
    "ShatteredFrontier.Gathering.Pilot.SkillLockCapability",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFGatheringSkillCapabilityTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> LedgerMode(GetMutableDefault<USFInventoryLedgerSettings>()->bEnabled,false);
    TGuardValue<bool> ServicesMode(GetMutableDefault<USFGameplayServicesSettings>()->bEnabled,false);
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC)||!Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC=World->SpawnActor<ASFPlayerController>();
    auto* Gather=PC ? PC->FindComponentByClass<USFGatheringPilotComponent>() : nullptr;
    if (!TestNotNull(TEXT("Gathering fixture"),Gather)) { return false; }
    const TSubclassOf<USFGatheringSkillsPanel> PanelClass=LoadClass<USFGatheringSkillsPanel>(nullptr,USFGatheringSkillsPanel::AssetPath);
    auto* Panel=PanelClass ? CreateWidget<USFGatheringSkillsPanel>(World,PanelClass) : nullptr;
    if (!TestNotNull(TEXT("Actual Gathering Skills WBP"),Panel)) { return false; }
    const auto CheckButtons=[&](bool bSupported)
    {
        Panel->Configure(Gather);
        for (int32 Row=0;Row<USFGatheringSkillsPanel::RowCount;++Row)
        {
            auto* Button=Cast<USFButtonWidget>(Panel->GetWidgetFromName(*FString::Printf(TEXT("Skill%dLock"),Row)));
            if (!TestNotNull(TEXT("Skill preference button"),Button)) { continue; }
            TestEqual(TEXT("Visibility follows captured capability"),Button->GetVisibility(),bSupported ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            const bool bExpectedEnabled=bSupported &&
                Gather->GetSkillLockCooldownRemaining(USFGatheringSkillsPanel::Rows[Row])<=0.f;
            TestEqual(TEXT("Button follows capability and restored cooldown"),Button->IsButtonEnabled(),bExpectedEnabled);
        }
    };
    Gather->bEnabled=true;
    Gather->CaptureSkillLockSupport();
    TestFalse(TEXT("Unadmitted offline world has no capability"),Gather->SupportsSkillLocks());
    CheckButtons(false);
    Gather->bTestWorldAdmission=true;
    Gather->CaptureSkillLockSupport();
    TestTrue(TEXT("Explicitly admitted offline adapter supports preferences"),Gather->SupportsSkillLocks());
    CheckButtons(true);
    Gather->bEnabled=false;
    CheckButtons(false);
    Gather->bEnabled=true;
    GetMutableDefault<USFGameplayServicesSettings>()->bEnabled=true;
    Gather->CaptureSkillLockSupport();
    TestFalse(TEXT("Old service host has no durable preference capability"),Gather->SupportsSkillLocks());
    CheckButtons(false);
    TestFalse(TEXT("Service request cannot send a local preference"),Gather->RequestSkillLock(ESFGatheringSkill::Logging,ESFGatheringSkillLock::Locked));
    Gather->ServerChangeSkillLock_Implementation(ESFGatheringSkill::Logging,ESFGatheringSkillLock::Locked);
    TestEqual(TEXT("Old host cannot change a lock"),Gather->Skills.Locks[1],ESFGatheringSkillLock::Up);
    GetMutableDefault<USFGameplayServicesSettings>()->bEnabled=false;
    TestFalse(TEXT("Later configuration or service outage cannot grant session capability"),Gather->SupportsSkillLocks());
    CheckButtons(false);
    TMap<FString,FString> DurableLocks;
    TMap<FString,int64> ReadyAt;
    for (int32 Index=0; Index<6; ++Index)
    {
        const FString Id=TEXT("skill/")+FString(FSFGatheringProgressionSession::SkillName(ESFGatheringSkill(Index))).ToLower();
        DurableLocks.Add(Id,Index==int32(ESFGatheringSkill::Logging) ? TEXT("down") :
            Index==int32(ESFGatheringSkill::Mining) ? TEXT("locked") : TEXT("up"));
        ReadyAt.Add(Id,0);
    }
    const FDateTime ClockNow=FDateTime::UtcNow();
    ReadyAt.FindOrAdd(TEXT("skill/logging"))=ClockNow.ToUnixTimestamp()*1000+ClockNow.GetMillisecond()+5000;
    Gather->RestoreSkillLocks(DurableLocks,ReadyAt,true);
    TestTrue(TEXT("Explicit complete new-host snapshot admits durable locks"),Gather->SupportsSkillLocks());
    TestEqual(TEXT("Server lock snapshot applied"),Gather->Skills.Locks[int32(ESFGatheringSkill::Logging)],ESFGatheringSkillLock::Down);
    TestEqual(TEXT("Locked mode round-trips"),Gather->Skills.Locks[int32(ESFGatheringSkill::Mining)],ESFGatheringSkillLock::Locked);
    TestEqual(TEXT("Up mode round-trips"),Gather->Skills.Locks[int32(ESFGatheringSkill::Herbalism)],ESFGatheringSkillLock::Up);
    TestTrue(TEXT("Persisted server deadline survives restore"),
        Gather->GetSkillLockCooldownRemaining(ESFGatheringSkill::Logging)>3.f);
    CheckButtons(true);
    ReadyAt.Remove(TEXT("skill/fishing"));
    Gather->RestoreSkillLocks(DurableLocks,ReadyAt,true);
    TestFalse(TEXT("Incomplete host snapshot fails closed"),Gather->SupportsSkillLocks());
    CheckButtons(false);
    GetMutableDefault<USFInventoryLedgerSettings>()->bEnabled=true;
    Gather->CaptureSkillLockSupport();
    TestFalse(TEXT("Ledger-backed progression also hides local-only preferences"),Gather->SupportsSkillLocks());
    CheckButtons(false);
    Panel->Configure(nullptr);
    auto* FirstButton=Cast<USFButtonWidget>(Panel->GetWidgetFromName(TEXT("Skill0Lock")));
    TestTrue(TEXT("Unbound panel fails closed"),FirstButton && FirstButton->GetVisibility()==ESlateVisibility::Collapsed && !FirstButton->IsButtonEnabled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringPilotProgressionTest,
    "ShatteredFrontier.Gathering.Pilot.Progression",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFGatheringPilotProgressionTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC)||!Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC=World->SpawnActor<ASFPlayerController>();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn=World->SpawnActor<ASFE1Character>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
    if (!PC||!Pawn) { return false; }
    PC->Possess(Pawn); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None); Pawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Gather=PC->FindComponentByClass<USFGatheringPilotComponent>(); auto* Bag=PC->FindComponentByClass<USFInventoryComponent>();
    if (!Gather||!Bag) { return false; }
    Gather->bEnabled=true; Gather->bTestWorldAdmission=true;
    Gather->bSessionSkillLocksSupported=true; // Explicitly admitted session-adapter fixture.
    Gather->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),Gather);
    Gather->CapturedSettings->HarvestSeconds=0.5f;
    Gather->ProgressionSession.TestRoll=0;
    FSFItemQuantity Axe; Axe.Item=TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronAxe.DA_IronAxe"))); Axe.Quantity=1;
    if (SFInventoryTransactions::ExchangeInventoryItems(Bag,{}, {Axe})!=ESFInventoryResult::Success) { return false; }
    bool bEquipped=false;
    for (int32 I=0;I<Bag->NumSlots;++I) { TSoftObjectPtr<USFItemDefinition> Item; int32 Count=0; if (Bag->GetSlot(I,Item,Count)&&Item==Axe.Item) { bEquipped=Bag->EquipFromSlot(I); break; } }
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
    Gather->ServerChangeSkillLock_Implementation(ESFGatheringSkill(255),ESFGatheringSkillLock::Locked);
    Gather->ServerChangeSkillLock_Implementation(ESFGatheringSkill::Logging,ESFGatheringSkillLock(255));
    TestEqual(TEXT("Malformed skill/lock requests leave preference unchanged"),Gather->Skills.Locks[1],ESFGatheringSkillLock::Up);
    Gather->ServerChangeSkillLock_Implementation(ESFGatheringSkill::Logging,ESFGatheringSkillLock::Up);
    TestEqual(TEXT("Selecting current preference does not start cooldown"),Gather->SkillLockReadyAt[1],0.0);
    Gather->bEnabled=false;
    Gather->ServerChangeSkillLock_Implementation(ESFGatheringSkill::Logging,ESFGatheringSkillLock::Locked);
    TestEqual(TEXT("Disabled endpoint cannot change preferences"),Gather->Skills.Locks[1],ESFGatheringSkillLock::Up);
    Gather->bEnabled=true;
    Gather->ServerChangeSkillLock_Implementation(ESFGatheringSkill::Logging,ESFGatheringSkillLock::Locked);
    TestEqual(TEXT("Player chooses to hold logging"),Gather->Skills.Locks[1],ESFGatheringSkillLock::Locked);
    TestTrue(TEXT("Server starts declared preference cooldown"),Gather->GetSkillLockCooldownRemaining(ESFGatheringSkill::Logging)>4.9f);
    Gather->ServerChangeSkillLock_Implementation(ESFGatheringSkill::Logging,ESFGatheringSkillLock::Up);
    TestEqual(TEXT("Rapid preference change is rejected"),Gather->Skills.Locks[1],ESFGatheringSkillLock::Locked);
    Gather->ServerChangeSkillLock_Implementation(ESFGatheringSkill::Mining,ESFGatheringSkillLock::Down);
    TestEqual(TEXT("Cooldown is per skill"),Gather->Skills.Locks[0],ESFGatheringSkillLock::Down);
    Gather->ServerGatheringAction_Implementation(Wood,1,4); Tick();
    TestEqual(TEXT("Locked skill still permits beginner harvesting"),Gather->Feedback.Result,ESFGatheringPilotResult::Granted);
    TestEqual(TEXT("Locked successful harvest still records practice"),Gather->Skills.PracticeCount,int64(2));
    TestEqual(TEXT("Locked skill cannot gain"),Gather->Skills.Get(ESFGatheringSkill::Logging),1);
    TestFalse(TEXT("No gain notification fabricated for locked skill"),Gather->Feedback.bSkillGained);
    for (int32 I=0;I<100;++I) { Fixture.TickTestWorld(0.05f); }
    Gather->ServerChangeSkillLock_Implementation(ESFGatheringSkill::Logging,ESFGatheringSkillLock::Down);
    TestEqual(TEXT("Preference changes after server cooldown"),Gather->Skills.Locks[1],ESFGatheringSkillLock::Down);
    TestEqual(TEXT("Allow decrease never immediately deletes skill points"),Gather->Skills.Get(ESFGatheringSkill::Logging),1);
    TestEqual(TEXT("Preference changes never fabricate practice"),Gather->Skills.PracticeCount,int64(2));
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
