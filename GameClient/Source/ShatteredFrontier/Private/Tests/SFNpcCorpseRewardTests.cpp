#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Combat/SFCombatPilotRewards.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFGameMode.h"
#include "Framework/SFPlayerController.h"
#include "Inventory/SFInventoryLedgerClient.h"
#include "Inventory/SFLootWorldSubsystem.h"
#include "Quest/SFQuestPilotEvents.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFGameplayServicesClient.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNpcCorpseRewardTest,
    "ShatteredFrontier.Combat.NpcReuse.CorpseRewardDelivery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFNpcCorpseRewardTest::RunTest(const FString& Parameters)
{
    TArray<FSFGameplayRequest> CorpseRequests;
    int32 DirectPayouts = 0, QuestCredits = 0;
    bool bAvailable = false;
    FSFGameplayServicesClient::SetSendOverrideForTests([&](const FSFGameplayRequest& Request, float,
        TFunction<void(const FSFGameplayResponse&)> Done)
    {
        FSFGameplayResponse Reply; Reply.Status = ESFGameplayStatus::Ok; Reply.GrpcCode = 0;
        if (Request.Rpc == ESFGameplayRpc::CreateCorpseLoot)
        { CorpseRequests.Add(Request); if (!bAvailable) { Reply.Status = ESFGameplayStatus::Unavailable; Reply.GrpcCode = 14; } }
        if (Request.Rpc == ESFGameplayRpc::GrantDefeatReward || Request.Rpc == ESFGameplayRpc::CreateSharedDefeatDrop) ++DirectPayouts;
        Done(Reply);
    });
    auto* Settings = NewObject<USFGameplayServicesSettings>(); Settings->bEnabled = true; Settings->MaxAttempts = 1;
    auto Client = FSFGameplayServicesClient::Create(*Settings);
    FSFGameplayServicesClient::SetSendOverrideForTests({});
    FSFScopedGameplayServicesForTests Services(Client);
    FSFInventoryLedgerClient::SetSendOverrideForTests([](const FSFLedgerRequest&, float, TFunction<void(const FSFLedgerResponse&)> Done)
    { FSFLedgerResponse Reply; Reply.Status = ESFLedgerStatus::Ok; Reply.GrpcCode = 0; Reply.Revision = 1; Done(Reply); });
    auto* LedgerSettings = NewObject<USFInventoryLedgerSettings>(); LedgerSettings->bEnabled = true;
    auto Ledger = FSFInventoryLedgerClient::Create(*LedgerSettings);
    FSFInventoryLedgerClient::SetSendOverrideForTests({});
    FTestWorldWrapper Fixture;
    if (!Client || !Ledger || !Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld();
    auto* Mode = World->SpawnActor<ASFGameMode>();
    Mode->CapturedSettings = GetDefault<USFCombatPilotSettings>();
    auto* Loot = World->GetSubsystem<USFLootWorldSubsystem>();
    if (!TestNotNull(TEXT("Real world loot subsystem"), Loot)) return false;
    TArray<ASFPlayerController*> Controllers;
    TArray<ASFE1Character*> Players;
    TArray<FString> Ids;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        auto* Controller = World->SpawnActor<ASFPlayerController>();
        auto* Player = World->SpawnActor<ASFE1Character>(FVector(100 * Index, 0, 100), FRotator::ZeroRotator);
        if (!Controller || !Player) return false;
        auto* Backpack = USFBackpackComponent::CreateForTests(Controller, Ledger);
        USFBackpackComponent::AssignDevIdentity(Controller, FString::Printf(TEXT("corpse_%d"), Index));
        Controller->Possess(Player); Player->CombatSetHealth(Player->CombatMaximumHealth());
        Controllers.Add(Controller); Players.Add(Player); Ids.Add(Backpack->GetCharacterId());
    }
    Mode->RaidIdByCharacter.Add(Ids[0], TEXT("raid_corpse"));
    Mode->RaidMembersById.Add(TEXT("raid_corpse"), {Ids[0], Ids[1]});
    const FDelegateHandle CreditHandle = SFQuestPilotEvents::OnBoarDefeated().AddLambda(
        [&](APlayerController*, const FString&, int32 Count, int32) { ++QuestCredits; TestEqual(TEXT("Canonical credit census"), Count, 2); });
    ON_SCOPE_EXIT { SFQuestPilotEvents::OnBoarDefeated().Remove(CreditHandle); };
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Npc = World->SpawnActor<ASFPilotBoar>(ASFPilotBoar::StaticClass(), FVector(0, 0, 100), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Personal reward NPC"), Npc)) return false;
    Npc->ConfigureCreature(TEXT("npc/wolf")); Npc->Initialize(Mode, TEXT("arbitrary_group"), 0, {}, 100, 1);
    const FString DeathId = Npc->DeathId();
    Mode->RewardKill(Npc, Players[0]);
    Mode->RewardKill(Npc, Players[0]);
    if (!TestEqual(TEXT("One admission despite transient failure and duplicate death"), CorpseRequests.Num(), 1)) return false;
    const FSFGameplayRequest First = CorpseRequests[0];
    TestTrue(TEXT("Opaque spawn identity retained"), First.OperationId.EndsWith(TEXT(":") + DeathId));
    TestEqual(TEXT("Canonical creature"), First.CreatureId, Npc->GetCreatureId());
    TestEqual(TEXT("Canonical table"), First.TableId, Npc->GetLootTableId());
    TestEqual(TEXT("Canonical killer"), First.KillerCharacterId, Ids[0]);
    TestEqual(TEXT("Canonical raid"), First.RaidGroupId, FString(TEXT("raid_corpse")));
    TestTrue(TEXT("Only nearby raid recipients, excluding observer"), First.EligibleCharacterIds.Num() == 2 &&
        First.EligibleCharacterIds.Contains(Ids[0]) && First.EligibleCharacterIds.Contains(Ids[1]) && !First.EligibleCharacterIds.Contains(Ids[2]));
    TestEqual(TEXT("XP/quest event remains once per eligible character"), QuestCredits, 2);
    TestFalse(TEXT("Personal profile keeps personal loot"), First.bSharedLoot);
    TestTrue(TEXT("Corpse position captured before cleanup"), First.Position.Equals(Npc->GetActorLocation()));
    Players[1]->SetActorLocation(FVector(100000, 0, 100)); Mode->RaidMembersById.Empty(); Npc->Destroy();
    bAvailable = true; Loot->Tick(0);
    if (!TestEqual(TEXT("Subsystem owns retry after body destruction"), CorpseRequests.Num(), 2)) return false;
    TestEqual(TEXT("Retry preserves operation"), CorpseRequests[1].OperationId, First.OperationId);
    TestTrue(TEXT("Retry preserves immutable eligible snapshot"), CorpseRequests[1].EligibleCharacterIds == First.EligibleCharacterIds);
    TestEqual(TEXT("No personal or shared auto-grant after corpse admission"), DirectPayouts, 0);
    SFQuestPilotEvents::OnBoarDefeated().Remove(CreditHandle);
    auto* Shared = World->SpawnActor<ASFPilotBoar>(ASFPilotBoar::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Shared reward NPC"), Shared)) return false;
    Shared->ConfigureCreature(TEXT("npc/wolf_alpha")); Shared->Initialize(Mode, TEXT("alpha_group"), 0, {}, 100, 1);
    Mode->RewardKill(Shared, Players[0]); Mode->RewardKill(Shared, Players[0]);
    TestEqual(TEXT("Shared profile admitted once"), CorpseRequests.Num(), 3);
    TestTrue(TEXT("Shared profile selects shared corpse"), CorpseRequests.Last().bSharedLoot);
    TestEqual(TEXT("Shared route also fences legacy payout"), DirectPayouts, 0);
    auto* Protected = World->SpawnActor<ASFPilotBoar>(ASFPilotBoar::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Protected reward NPC"), Protected)) return false;
    Protected->Initialize(Mode, TEXT("protected_group"), 0, {}, 100, 1);
    Mode->ProtectedAreas = {{FVector2D(-10,-10), FVector2D(10,-10), FVector2D(10,10), FVector2D(-10,10)}};
    Mode->RewardKill(Protected, Players[0]);
    TestEqual(TEXT("Protected death cannot create corpse"), CorpseRequests.Num(), 3);
    TestEqual(TEXT("Protected death cannot auto-grant"), DirectPayouts, 0);
    {
        FSFScopedGameplayServicesForTests Offline(nullptr);
        Mode->ProtectedAreas.Empty();
        SFCombatPilotRewards::AddPilotBag(Controllers[0]);
        auto* Local = World->SpawnActor<ASFPilotBoar>(ASFPilotBoar::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        if (!TestNotNull(TEXT("Local reward NPC"), Local)) return false;
        Local->Initialize(Mode, TEXT("local_fixture"), 0, {}, 100, 1);
        const int32 Before = SFCombatPilotRewards::CountHides(Controllers[0]);
        TestEqual(TEXT("Service-disabled fixture keeps direct hides"), Mode->RewardKill(Local, Players[0]), Mode->CapturedSettings->HidesPerBoar);
        TestEqual(TEXT("Service-disabled duplicate is fenced"), Mode->RewardKill(Local, Players[0]), 0);
        TestEqual(TEXT("Local fallback reaches real bag"), SFCombatPilotRewards::CountHides(Controllers[0]), Before + Mode->CapturedSettings->HidesPerBoar);
        TestEqual(TEXT("Local fallback never calls corpse RPC"), CorpseRequests.Num(), 3);
    }
    return true;
}
#endif
