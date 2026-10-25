#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Combat/SFCombatPart6.h"
#include "Combat/SFSpiritReleaseRules.h"
#include "Combat/SFResurrectionAcceptRules.h"
#include "Combat/SFCastingComponent.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFGameMode.h"
#include "Framework/SFPlayerController.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFGameplayServicesClient.h"
#include "Inventory/SFInventoryLedgerClient.h"
#include "Gathering/SFGatheringPilot.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNpcProductionParityTest,
    "ShatteredFrontier.Combat.NpcReuse.ProductionServicesDefenseAndDeath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFNpcProductionParityTest::RunTest(const FString& Parameters)
{
    const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT(".."));
    const FString Fixture = Root / TEXT("tests/fixtures/npc-profiles");
    const FString Rules = Root / TEXT("content/creatures/loot_rules");
    const FString Scratch = FPaths::ProjectSavedDir() / TEXT("NpcProductionParity") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    IFileManager::Get().MakeDirectory(*Scratch, true);
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Scratch, false, true); };
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(Rules / TEXT("*.json")), true, false);
    for (const FString& File : Files) IFileManager::Get().Copy(*(Scratch / File), *(Rules / File));
    IFileManager::Get().Copy(*(Scratch / TEXT("reuse_archer_fixture.json")), *(Fixture / TEXT("loot_rules/reuse_archer_fixture.json")));
    TMap<FString, SFNpcProfiles::FProfile> Profiles;
    FString Error;
    if (!TestTrue(TEXT("Validated fixture catalogue"), SFNpcProfiles::Load(Fixture / TEXT("profiles.json"), Scratch, Profiles, Error))) return false;
    SFNpcProfiles::FScopedTestCatalogue Catalogue(Profiles);
    const FString MapId = TEXT("/Game/Frontier/Maps/Greenlands/NpcServiceParity");
    SFNpcZoneContent::FScopedTestMap ApprovedMap(MapId, Fixture / TEXT("production-zone.json"));

    FSFGameplayResponse::FBoarSlot Slot;
    Slot.GroupId = TEXT("rangers_across_the_river"); Slot.PointId = TEXT("ranger_clearing");
    Slot.DefinitionId = TEXT("npc/reuse_archer_fixture"); Slot.bActive = true;
    TArray<FSFGameplayResponse::FBoarSlot> Rows{Slot};
    TArray<FSFGameplayRequest> DeathRequests, ClearRequests;
    int32 SlotCalls = 0;
    FSFGameplayServicesClient::SetSendOverrideForTests([&](const FSFGameplayRequest& Request, float, TFunction<void(const FSFGameplayResponse&)> Done)
    {
        FSFGameplayResponse Reply; Reply.Status = ESFGameplayStatus::Ok; Reply.GrpcCode = 0;
        if (Request.Rpc == ESFGameplayRpc::AdvanceWorldSlots) { ++SlotCalls; Reply.BoarSlots = Rows; }
        if (Request.Rpc == ESFGameplayRpc::ReportDeath)
        {
            DeathRequests.Add(Request); Reply.bHasDeath = Reply.bIsDead = true;
            Reply.DeathEventId = Request.DeathEventId;
            Reply.RespawnAtUnixMs = Request.DeathAtUnixMs + Request.RespawnDelayMs;
        }
        if (Request.Rpc == ESFGameplayRpc::ClearDeath)
        { ClearRequests.Add(Request); Reply.bHasDeath = true; Reply.DeathEventId = Request.DeathEventId; }
        Done(Reply);
    });
    USFGameplayServicesSettings* ClientSettings = NewObject<USFGameplayServicesSettings>();
    ClientSettings->bEnabled = true;
    auto Client = FSFGameplayServicesClient::Create(*ClientSettings);
    FSFGameplayServicesClient::SetSendOverrideForTests({});
    if (!TestTrue(TEXT("Isolated services transport"), Client.IsValid())) return false;
    FSFScopedGameplayServicesForTests Services(Client);
    TGuardValue<bool> Enabled(GetMutableDefault<USFGameplayServicesSettings>()->bEnabled, true);
    FTestWorldWrapper WorldFixture;
    if (!WorldFixture.CreateTestWorld(EWorldType::Game)) return false;
    UWorld* World = WorldFixture.GetTestWorld();
    if (!TestTrue(TEXT("Named authored world"), World->GetOutermost()->Rename(*MapId, nullptr, REN_DontCreateRedirectors))) return false;
    World->GetWorldSettings()->DefaultGameMode = ASFGameMode::StaticClass();
    if (!WorldFixture.BeginPlayInTestWorld()) return false;
    ASFGameMode* Mode = World->GetAuthGameMode<ASFGameMode>();
    if (!TestNotNull(TEXT("Real authority game mode"), Mode)) return false;
    TestTrue(TEXT("New map uses common production rules"), SFNpcZoneContent::IsProductionMap(World) && SFCombatPart6::Active(World));
    TestTrue(TEXT("Actual services callback accepts authored one-slot census"), SlotCalls > 0 && Mode->bSlotBridgeHealthy && Mode->ActiveBoars.Num() == 1);
    ASFPilotBoar* Npc = Mode->ActiveBoars.FindRef(TEXT("rangers_across_the_river:0")).Get();
    if (!TestNotNull(TEXT("Service reply spawned NPC"), Npc)) return false;
    AddExpectedError(TEXT("COMBAT_PILOT_SLOT_ERROR Gameplay.Host"), EAutomationExpectedErrorFlags::Contains, 5);
    Rows.Empty(); Mode->RunWorldSlots(TEXT("tick"));
    TestFalse(TEXT("Incomplete census rejected through callback"), Mode->bSlotBridgeHealthy);
    Rows = {Slot}; Rows[0].SlotIndex = 1; Mode->RunWorldSlots(TEXT("tick"));
    TestFalse(TEXT("Out-of-census slot rejected through callback"), Mode->bSlotBridgeHealthy);
    Rows = {Slot}; Rows[0].DefinitionId = TEXT("npc/unknown"); Mode->RunWorldSlots(TEXT("tick"));
    TestFalse(TEXT("Unknown definition rejected through callback"), Mode->bSlotBridgeHealthy);
    Rows = {Slot}; Rows[0].GroupId = TEXT("foreign_group"); Mode->RunWorldSlots(TEXT("tick"));
    TestFalse(TEXT("Foreign group rejected through callback"), Mode->bSlotBridgeHealthy);
    Mode->Groups.FindChecked(Slot.GroupId).ActiveCount = 2;
    Rows = {Slot, Slot}; Mode->RunWorldSlots(TEXT("tick"));
    TestFalse(TEXT("Duplicate slots rejected even at expected total count"), Mode->bSlotBridgeHealthy);
    Mode->Groups.FindChecked(Slot.GroupId).ActiveCount = 1;
    Rows = {Slot}; Mode->RunWorldSlots(TEXT("tick"));
    TestTrue(TEXT("Valid callback recovers without duplicate spawn"), Mode->bSlotBridgeHealthy && Mode->ActiveBoars.Num() == 1);

    FSFInventoryLedgerClient::SetSendOverrideForTests([](const FSFLedgerRequest&, float, TFunction<void(const FSFLedgerResponse&)> Done)
    { FSFLedgerResponse Reply; Reply.Status = ESFLedgerStatus::Ok; Reply.GrpcCode = 0; Reply.Revision = 1; Done(Reply); });
    USFInventoryLedgerSettings* LedgerSettings = NewObject<USFInventoryLedgerSettings>(); LedgerSettings->bEnabled = true;
    auto Ledger = FSFInventoryLedgerClient::Create(*LedgerSettings);
    FSFInventoryLedgerClient::SetSendOverrideForTests({});
    ASFPlayerController* Controller = World->SpawnActor<ASFPlayerController>();
    ASFE1Character* Player = World->SpawnActor<ASFE1Character>(FVector(1500, 0, 200), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Owned controller"), Controller) || !TestNotNull(TEXT("Owned pawn"), Player) || !Ledger) return false;
    USFBackpackComponent::CreateForTests(Controller, Ledger);
    USFBackpackComponent::AssignDevIdentity(Controller, TEXT("npc_parity"));
    Controller->Possess(Player);
    FSFGameplayResponse Snapshot; Snapshot.Level = 1;
    Snapshot.CombatAttributes = {TEXT("fighter"), 1, 10, 10, 10, 10, 10, 10};
    Player->RestoreCombatSnapshot(Snapshot);
    Player->CombatSetHealth(Player->CombatMaximumHealth()); Player->SetCombatTarget(Npc);
    TestTrue(TEXT("Profile-driven defenses are nonzero on new production map"), Npc->CombatDodge() > 0 && Npc->CombatParry() > 0);
    auto AttackWithAvoidance = [&](double Low, double High, bool bFront)
    {
        int32 Seed = 1;
        for (; Seed < 100000; ++Seed)
        {
            FRandomStream Probe(Seed); const auto Draws = SFCombatResolution::Draw(Probe);
            if (Draws.Hit > .95 && Draws.Avoid > Low && Draws.Avoid < High) break;
        }
        TestTrue(TEXT("Deterministic real attack draw found"), Seed < 100000);
        Mode->PlayerCombatStreams.Add(Player, FRandomStream(Seed)); Mode->PlayerNextSwing.Remove(Player);
        Player->SetActorLocation(Npc->GetActorLocation() + FVector(bFront ? 150 : -150, 0, 0));
        Player->SetActorRotation(FRotator(0, bFront ? 180 : 0, 0));
        Mode->TryPlayerAttack(Player, Npc);
    };
    const int32 NpcHealth = Npc->Health();
    AttackWithAvoidance(0, Npc->CombatDodge(), true);
    TestEqual(TEXT("Actual front attack can dodge on approved new map"), Npc->Health(), NpcHealth);
    AttackWithAvoidance(Npc->CombatDodge(), Npc->CombatDodge() + Npc->CombatParry(), true);
    TestEqual(TEXT("Actual front attack can parry on approved new map"), Npc->Health(), NpcHealth);
    AttackWithAvoidance(0, Npc->CombatDodge(), false);
    TestTrue(TEXT("Same avoidance draw cannot dodge/parry a rear attack"), Npc->Health() < NpcHealth);

    Npc->Destroy(); Mode->ActiveBoars.Remove(TEXT("rangers_across_the_river:0"));
    Rows[0].DefinitionId = TEXT("npc/bandit_captain"); Mode->RunWorldSlots(TEXT("tick"));
    Npc = Mode->ActiveBoars.FindRef(TEXT("rangers_across_the_river:0")).Get();
    if (!TestNotNull(TEXT("Declared caster spawned through same services callback"), Npc)) return false;
    Snapshot.Level = Snapshot.CombatAttributes.Level = 10; ++Snapshot.ProgressionRevision;
    auto* Progression = Controller->FindComponentByClass<USFGatheringPilotComponent>();
    if (!Progression) { Progression = NewObject<USFGatheringPilotComponent>(Controller); Progression->RegisterComponent(); }
    Progression->RestoreLevel(10);
    Player->RestoreCombatSnapshot(Snapshot); Player->CombatSetHealth(Player->CombatMaximumHealth());
    TestEqual(TEXT("Published interrupt has real progression unlock"), Player->CombatCharacterLevel(), 10);
    Player->SetCombatTarget(Npc);
    Player->SetActorLocation(Npc->GetActorLocation() + FVector(150, 0, 0)); Player->SetActorRotation(FRotator(0, 180, 0));
    Npc->ReceivePilotDamage(Player, Npc->MaximumHealth() * 3 / 4);
    TestTrue(TEXT("Declared caster starts real interruptible Mend"), Npc->GetCasting()->TryBeginNpcMend());
    const float StaminaBefore = Player->GetStamina();
    Player->Casting->RequestPublishedInterrupt(false, Npc);
    TestTrue(TEXT("Actual published interrupt reaches new-map caster"), Npc->GetCasting()->IsNpcSchoolLocked());
    TestTrue(TEXT("Actual new-map interrupt pays its resource cost"), Player->GetStamina() < StaminaBefore);

    ASFGameMode::HitPlayer(Npc, Player, Player->CombatHealth(), true);
    if (!TestEqual(TEXT("Actual lethal hit reports durable death"), DeathRequests.Num(), 1)) return false;
    TestEqual(TEXT("New production map uses resurrection window"), DeathRequests[0].RespawnDelayMs,
        FMath::RoundToInt(Mode->CapturedSettings->ResAcceptSeconds * 1000));
    TestTrue(TEXT("Actual spirit release admitted on approved map"), Mode->ReleaseSpirit(Player));
    if (!TestEqual(TEXT("Spirit release clears durable death"), ClearRequests.Num(), 1)) return false;
    TestFalse(TEXT("Spirit release selects authored respawn position"), ClearRequests[0].bResurrectAtDeathPosition);
    TestEqual(TEXT("Spirit release restores declared health fraction"), Player->CombatHealth(),
        FSFSpiritReleaseRules::RestoredHealth(Player->CombatMaximumHealth(), Mode->CapturedSettings->SpiritResFraction));
    TestFalse(TEXT("Living release replay rejected"), Mode->ReleaseSpirit(Player));
    ASFGameMode::HitPlayer(Npc, Player, Player->CombatHealth(), true);
    if (!TestEqual(TEXT("Second actual death reported"), DeathRequests.Num(), 2)) return false;
    const FString DeathId = DeathRequests.Last().DeathEventId;
    Player->BeginResurrectionOffer(World->GetTimeSeconds() + Mode->CapturedSettings->ResAcceptSeconds, DeathId);
    const uint32 OfferId = Player->GetResurrectionOfferId();
    TestTrue(TEXT("Actual resurrection acceptance admitted on approved map"), Mode->AcceptResurrection(Player, OfferId, DeathId));
    if (!TestEqual(TEXT("Acceptance clears durable death"), ClearRequests.Num(), 2)) return false;
    TestTrue(TEXT("Acceptance requests death-position resurrection"), ClearRequests.Last().bResurrectAtDeathPosition);
    TestEqual(TEXT("Acceptance restores declared health fraction"), Player->CombatHealth(),
        FSFResurrectionAcceptRules::RestoredHealth(Player->CombatMaximumHealth(), Mode->CapturedSettings->PlayerResFraction));
    TestFalse(TEXT("Acceptance replay rejected"), Mode->AcceptResurrection(Player, OfferId, DeathId));
    TestTrue(TEXT("Switch to genuine spirit-release fixture"), World->GetOutermost()->Rename(
        TEXT("/Game/Frontier/Maps/Sandbox/SB_SpiritRelease"), nullptr, REN_DontCreateRedirectors));
    Player->CombatSetHealth(0);
    TestFalse(TEXT("Sandbox is never classified as production"), SFNpcZoneContent::IsProductionMap(World));
    TestFalse(TEXT("Services-backed sandbox cannot use fixture release"), Mode->ReleaseSpirit(Player));
    {
        FSFScopedGameplayServicesForTests DisabledServices(nullptr);
        TestTrue(TEXT("Service-disabled fixture retains health-only release"), Mode->ReleaseSpirit(Player));
        TestTrue(TEXT("Switch to genuine resurrection fixture"), World->GetOutermost()->Rename(
            TEXT("/Game/Frontier/Maps/Sandbox/SB_ResurrectionAccept"), nullptr, REN_DontCreateRedirectors));
        Player->CombatSetHealth(0);
        Player->BeginResurrectionOffer(World->GetTimeSeconds() + Mode->CapturedSettings->ResAcceptSeconds);
        TestTrue(TEXT("Service-disabled fixture retains health-only offer acceptance"),
            Mode->AcceptResurrection(Player, Player->GetResurrectionOfferId(), FString()));
    }
    TestEqual(TEXT("Fixture health-only paths never clear a durable death"), ClearRequests.Num(), 2);
    return true;
}
#endif
