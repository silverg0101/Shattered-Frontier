#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFGameMode.h"
#include "Framework/SFPlayerController.h"
#include "Inventory/SFInventoryLedgerClient.h"
#include "Inventory/SFLootSourceActor.h"
#include "Inventory/SFLootWorldSubsystem.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFGameplayServicesClient.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNpcDedicatedCorpseTest,
    "ShatteredFrontier.Combat.NpcReuse.DedicatedCorpsePresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFNpcDedicatedCorpseTest::RunTest(const FString& Parameters)
{
    TArray<FSFGameplayRequest> Requests;
    bool bAvailable = false;
    FSFGameplayServicesClient::SetSendOverrideForTests([&](const FSFGameplayRequest& Q, float, TFunction<void(const FSFGameplayResponse&)> Done)
    {
        FSFGameplayResponse Reply; Reply.Status = ESFGameplayStatus::Ok;
        if (Q.Rpc == ESFGameplayRpc::CreateCorpseLoot)
        {
            Requests.Add(Q);
            if (!bAvailable) Reply.Status = ESFGameplayStatus::Unavailable;
            else
            {
                FSFLootSourceSnapshot S; S.Id = TEXT("ctn_loot_headless"); S.ZoneId = Q.ZoneId; S.BodyId = Q.OperationId;
                S.CreatureId = Q.CreatureId; S.Position = Q.Position; S.MeshPath = Q.LootMeshPath;
                S.MeshScale = Q.LootMeshTransform.GetScale3D(); S.MeshRotation = Q.LootMeshTransform.Rotator();
                S.bHasMeshBounds = Q.bHasLootMeshBounds; S.MeshBoundsOriginCm = Q.LootMeshBoundsOriginCm; S.MeshBoundsExtentCm = Q.LootMeshBoundsExtentCm;
                S.EligibleCharacters = Q.EligibleCharacterIds; S.Items.Add({TEXT("item/test"),1}); Reply.LootSources.Add(S);
            }
        }
        Done(Reply);
    });
    auto* Settings = NewObject<USFGameplayServicesSettings>(); Settings->bEnabled = true; Settings->MaxAttempts = 1;
    auto Client = FSFGameplayServicesClient::Create(*Settings); FSFGameplayServicesClient::SetSendOverrideForTests({});
    FSFScopedGameplayServicesForTests Services(Client);
    FSFInventoryLedgerClient::SetSendOverrideForTests([](const FSFLedgerRequest&, float, TFunction<void(const FSFLedgerResponse&)> Done)
    { FSFLedgerResponse Reply; Reply.Status = ESFLedgerStatus::Ok; Reply.Revision = 1; Done(Reply); });
    auto* LedgerSettings = NewObject<USFInventoryLedgerSettings>(); LedgerSettings->bEnabled = true;
    auto Ledger = FSFInventoryLedgerClient::Create(*LedgerSettings); FSFInventoryLedgerClient::SetSendOverrideForTests({});
    FTestWorldWrapper ServerFixture;
    if (!Client || !Ledger || !ServerFixture.CreateTestWorld(EWorldType::Game) || !ServerFixture.BeginPlayInTestWorld()) return false;
    UWorld* World = ServerFixture.GetTestWorld();
    // FTestWorldWrapper cannot create PIE worlds. Use the engine's initial net-mode fallback after its ordinary world setup.
    World->WorldType = EWorldType::PIE; World->SetPlayInEditorInitialNetMode(NM_DedicatedServer);
    ON_SCOPE_EXIT { World->WorldType = EWorldType::Game; };
    if (!TestEqual(TEXT("Real world dedicated-server admission"), World->GetNetMode(), NM_DedicatedServer)) return false;
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Mode = World->SpawnActor<ASFGameMode>();
    auto* Controller = World->SpawnActor<ASFPlayerController>();
    auto* Killer = World->SpawnActor<ASFE1Character>(ASFE1Character::StaticClass(), FVector(1100,2000,300), FRotator::ZeroRotator, Spawn);
    auto* Npc = World->SpawnActor<ASFCombatNpc>(ASFCombatNpc::StaticClass(), FVector(1000,2000,300), FRotator(15,40,0), Spawn);
    if (!Mode || !Controller || !Killer || !Npc) return false;
    Mode->CapturedSettings = GetDefault<USFCombatPilotSettings>();
    USFBackpackComponent::CreateForTests(Controller, Ledger); USFBackpackComponent::AssignDevIdentity(Controller, TEXT("headless"));
    Controller->Possess(Killer);
    auto* Body = Npc->FindComponentByClass<UStaticMeshComponent>();
    if (!Body) return false;
    Body->SetStaticMesh(nullptr); // Dedicated cooks strip this render resource; the component/transform still exists.
    TestTrue(TEXT("Dedicated actor configures from canonical descriptor"), Npc->ConfigureCreature(TEXT("npc/wolf")));
    TestNull(TEXT("Dedicated configuration never loads a render mesh"), Body->GetStaticMesh());
    Npc->SetActorScale3D(FVector(1.5,.5,2)); Body->SetRelativeLocation(FVector(20,-10,15)); Body->SetRelativeRotation(FRotator(0,25,0));
    Npc->Initialize(Mode, TEXT("headless_group"), 0, {}, 100, 1);
    const FTransform Expected = Body->GetComponentTransform();
    const auto Presentation = Npc->GetNpcProfile()->Presentation;
    Mode->RewardKill(Npc, Killer);
    if (!TestEqual(TEXT("Real reward hook admits one headless descriptor"), Requests.Num(), 1)) return false;
    const auto First = Requests[0];
    TestEqual(TEXT("Canonical asset path does not depend on GetStaticMesh"), First.LootMeshPath, Presentation.StaticMeshPath.ToString());
    TestTrue(TEXT("Asset-local bounds captured with explicit presence"), First.bHasLootMeshBounds && First.LootMeshBoundsExtentCm.Equals(Presentation.MeshBoundsExtentCm));
    TestTrue(TEXT("Component translation/rotation/nonuniform world scale captured"), First.LootMeshTransform.Equals(Expected));
    TestTrue(TEXT("Source position is actual offset body origin"), First.Position.Equals(Expected.GetLocation()) && !First.Position.Equals(Npc->GetActorLocation()));
    Npc->Destroy(); bAvailable = true;
    auto* Loot = World->GetSubsystem<USFLootWorldSubsystem>(); if (!Loot) return false; Loot->Tick(0);
    if (!TestEqual(TEXT("Subsystem retries after original actor teardown"), Requests.Num(), 2)) return false;
    TestTrue(TEXT("Retry preserves immutable presentation and opaque operation"), Requests[1].OperationId == First.OperationId &&
        Requests[1].LootMeshTransform.Equals(First.LootMeshTransform) && Requests[1].LootMeshBoundsExtentCm.Equals(First.LootMeshBoundsExtentCm));
    ASFLootSourceActor* Authority = nullptr;
    for (TActorIterator<ASFLootSourceActor> It(World); It; ++It) if (It->GetSnapshot().Id == TEXT("ctn_loot_headless")) Authority = *It;
    if (!TestNotNull(TEXT("Response restores actual dedicated source actor"), Authority)) return false;
    TestNull(TEXT("Dedicated corpse source uses no render mesh"), Authority->Mesh->GetStaticMesh());
    const FVector Extent = Presentation.MeshBoundsExtentCm * Expected.GetScale3D().GetAbs();
    TestTrue(TEXT("Headless hit shape applies scale exactly once"), Authority->HitShape.Extent.Equals(Extent));
    TestTrue(TEXT("Headless corpse retains full captured world pose"), Authority->GetActorLocation().Equals(Expected.GetLocation()) &&
        Authority->GetActorQuat().Equals(Expected.GetRotation()));
    FTestWorldWrapper ClientFixture;
    if (!ClientFixture.CreateTestWorld(EWorldType::Game) || !ClientFixture.BeginPlayInTestWorld()) return false;
    UWorld* ClientWorld = ClientFixture.GetTestWorld(); ClientWorld->WorldType = EWorldType::PIE; ClientWorld->SetPlayInEditorInitialNetMode(NM_Client);
    ON_SCOPE_EXIT { ClientWorld->WorldType = EWorldType::Game; };
    if (!TestEqual(TEXT("Real client world"), ClientWorld->GetNetMode(), NM_Client)) return false;
    auto* View = ClientWorld->SpawnActor<ASFLootSourceActor>(ASFLootSourceActor::StaticClass(), Expected.GetLocation(), Expected.Rotator(), Spawn);
    if (!View) return false;
    View->HitShape = Authority->HitShape; View->OnRep_HitShape();
    const auto OuterHit = [&]
    {
        const FTransform T = View->GetActorTransform(); const double X = Extent.X * .9;
        FHitResult Hit;
        return ClientWorld->LineTraceSingleByChannel(Hit, T.TransformPosition(FVector(X,-Extent.Y*3,0)),
            T.TransformPosition(FVector(X,Extent.Y*3,0)), ECC_Visibility) && Hit.GetActor() == View;
    };
    TestTrue(TEXT("Replicated client outer bounds work before mesh arrives"), OuterHit());
    View->MeshView = Authority->MeshView; View->OnRep_MeshView();
    TestNotNull(TEXT("Client resolves replicated mesh descriptor without server asset NetGUID"), View->Mesh->GetStaticMesh().Get());
    TestTrue(TEXT("Client visual scale matches captured body scale"), View->Mesh->GetRelativeScale3D().Equals(Expected.GetScale3D()));
    TestTrue(TEXT("Replicated bounds still align after client mesh resolution"), OuterHit());
    return true;
}
#endif
