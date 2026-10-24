#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Framework/SFPlayerController.h"
#include "Tests/AutomationCommon.h"
#include "Gathering/SFGatheringPilot.h"
#include "Inventory/SFInventoryLedgerClient.h"
#include "Quest/SFQuestPilot.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFGameplayServicesClient.h"
#include "Containers/Ticker.h"
#include "Construction/SFConstructionNetwork.h"
#include "Characters/SFE1Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Inventory/SFInventory.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringPilotAuthorityTest,
    "ShatteredFrontier.Gathering.Pilot.Authority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFGatheringPilotAuthorityTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Actual actor/physics/plugin transactions in one authority world; network transport/private delivery require separate clients."));
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC=World->SpawnActor<ASFPlayerController>();
    auto* Other=World->SpawnActor<ASFPlayerController>();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn=World->SpawnActor<ASFE1Character>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
    auto* OtherPawn=World->SpawnActor<ASFE1Character>(FVector(0,100,100),FRotator::ZeroRotator,Spawn);
    if (!PC || !Other || !Pawn || !OtherPawn) { return false; }
    PC->Possess(Pawn); Other->Possess(OtherPawn);
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None); OtherPawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
    Pawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OtherPawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Gather=PC->FindComponentByClass<USFGatheringPilotComponent>();
    auto* OtherGather=Other->FindComponentByClass<USFGatheringPilotComponent>();
    auto* Inventory=PC->FindComponentByClass<USFInventoryComponent>();
    auto* OtherInventory=Other->FindComponentByClass<USFInventoryComponent>();
    if (!Gather || !OtherGather || !Inventory || !OtherInventory) { return false; }
    for (auto* Component : {Gather,OtherGather})
    {
        Component->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),Component);
        Component->CapturedSettings->HarvestSeconds=0.5f; Component->CapturedSettings->RefineSeconds=0.5f; Component->bEnabled=true; Component->bTestWorldAdmission=true;
    }
    const auto GiveAndEquip=[this](USFInventoryComponent* Bag, bool bAxe)
    {
        FSFItemQuantity Tool;
        Tool.Item=TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(bAxe
            ? TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronAxe.DA_IronAxe")
            : TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronPickaxe.DA_IronPickaxe")));
        Tool.Quantity=1;
        if (SFInventoryTransactions::ExchangeInventoryItems(Bag,{}, {Tool})!=ESFInventoryResult::Success) { return false; }
        for (int32 I=0; I<Bag->NumSlots; ++I)
        {
            TSoftObjectPtr<USFItemDefinition> Item; int32 Quantity=0;
            if (Bag->GetSlot(I,Item,Quantity) && Item==Tool.Item) { return Bag->EquipFromSlot(I); }
        }
        return false;
    };
    if (!TestTrue(TEXT("Authority fixture equips axe"),GiveAndEquip(Inventory,true))
        || !TestTrue(TEXT("Other fixture equips pickaxe"),GiveAndEquip(OtherInventory,false))) { return false; }
    auto* Wood=World->SpawnActor<ASFGatheringPilotNode>(FVector(200,0,100),FRotator::ZeroRotator,Spawn);
    auto* Ore=World->SpawnActor<ASFGatheringPilotNode>(FVector(200,140,100),FRotator::ZeroRotator,Spawn);
    if (!Wood || !Ore) { return false; }
    Wood->State.Charges=1; Ore->State.Charges=1; Ore->Kind=ESFGatheringPilotKind::Iron;
    if (!TestTrue(TEXT("Generated catalog and real plugin available"),Gather->InventoryReady())) { return false; }
    const TSoftObjectPtr<USFItemDefinition> WoodItem(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_OakLog.DA_OakLog")));
    const TSoftObjectPtr<USFItemDefinition> OreItem(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronOre.DA_IronOre")));
    const auto Tick=[&Fixture](int32 Count) { for (int32 I=0; I<Count; ++I) { Fixture.TickTestWorld(0.05f); } };
    Gather->ServerGatheringAction_Implementation(Wood,0,1);
    TestTrue(TEXT("One player acquires channel without consuming charge"),Wood->State.bBusy && Wood->State.Charges==1);
    OtherGather->ServerGatheringAction_Implementation(Wood,0,1);
    TestEqual(TEXT("Competing final charge is busy"),OtherGather->Feedback.Result,ESFGatheringPilotResult::Busy);
    Tick(4);
    TestEqual(TEXT("No early reward"),SFInventoryTransactions::GetItemQuantity(Inventory,WoodItem),int64(0));
    Tick(8);
    TestEqual(TEXT("Timed harvest grants exactly one unit"),SFInventoryTransactions::GetItemQuantity(Inventory,WoodItem),int64(1));
    TestTrue(TEXT("Final charge depletes once and releases lock"),Wood->State.Charges==0 && Wood->State.Revision==1 && !Wood->State.bBusy);
    TestEqual(TEXT("Other player receives no grant"),SFInventoryTransactions::GetItemQuantity(OtherInventory,WoodItem),int64(0));
    Gather->ServerGatheringAction_Implementation(Wood,0,1);
    TestEqual(TEXT("Completed request replay does not duplicate grant"),SFInventoryTransactions::GetItemQuantity(Inventory,WoodItem),int64(1));
    Gather->ServerGatheringAction_Implementation(Ore,0,1);
    TestEqual(TEXT("Changed-payload replay cannot reserve another node"),Ore->State.bBusy,false);
    OtherGather->ServerGatheringAction_Implementation(Wood,1,2);
    TestEqual(TEXT("Later request sees depleted resource"),OtherGather->Feedback.Result,ESFGatheringPilotResult::Depleted);

    OtherGather->ServerGatheringAction_Implementation(Ore,0,3);
    OtherPawn->SetActorLocation(FVector(10,100,100)); Tick(1);
    TestEqual(TEXT("Moving cancels channel"),OtherGather->Feedback.Result,ESFGatheringPilotResult::Moved);
    TestTrue(TEXT("Movement preserves charge and lock becomes available"),Ore->State.Charges==1 && !Ore->State.bBusy);
    if (!TestTrue(TEXT("Ore test switches to pickaxe"),GiveAndEquip(Inventory,false))) { return false; }
    FSFItemQuantity ReturnedAxe; ReturnedAxe.Item=TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronAxe.DA_IronAxe"))); ReturnedAxe.Quantity=1;
    TestEqual(TEXT("Fixture removes returned axe to isolate raw-material capacity"),SFInventoryTransactions::ExchangeInventoryItems(Inventory,{ReturnedAxe},{}),ESFInventoryResult::Success);
    Inventory->MaxWeight=1; // Already contains one 1 kg wood unit.
    Gather->ServerGatheringAction_Implementation(Ore,0,2); Tick(12);
    TestEqual(TEXT("Full bag rejects entire grant"),Gather->Feedback.Result,ESFGatheringPilotResult::InventoryFull);
    TestTrue(TEXT("Rejected grant preserves charge and revision"),Ore->State.Charges==1 && Ore->State.Revision==0 && !Ore->State.bBusy);
    TestEqual(TEXT("No partial ore grant"),SFInventoryTransactions::GetItemQuantity(Inventory,OreItem),int64(0));

    auto* Wall=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(20,200,200));
    Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Wall->SetActorLocation(FVector(100,70,100));
    Gather->ServerGatheringAction_Implementation(Ore,0,3);
    TestEqual(TEXT("Server rejects occluded resource"),Gather->Feedback.Result,ESFGatheringPilotResult::Occluded);
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pawn->SetActorLocation(FVector(1000,0,100)); Gather->ServerGatheringAction_Implementation(Ore,0,4);
    TestEqual(TEXT("Server rejects distant resource"),Gather->Feedback.Result,ESFGatheringPilotResult::OutOfReach);
    Pawn->SetActorLocation(FVector(0,0,100)); Gather->ServerGatheringAction_Implementation(Ore,99,5);
    TestEqual(TEXT("Stale revision rejected"),Gather->Feedback.Result,ESFGatheringPilotResult::Stale);
    OtherGather->ServerGatheringAction_Implementation(Ore,0,4); OtherGather->ServerCancelHarvest_Implementation(4);
    TestEqual(TEXT("Explicit cancel preserves node"),OtherGather->Feedback.Result,ESFGatheringPilotResult::Canceled);
    TestTrue(TEXT("Canceled node remains available"),Ore->State.Charges==1 && !Ore->State.bBusy);
    OtherGather->ServerGatheringAction_Implementation(Ore,0,5); OtherGather->bTestWorldAdmission=false; Tick(1);
    TestEqual(TEXT("Loss of admitted world cancels active harvest"),OtherGather->Feedback.Result,ESFGatheringPilotResult::InvalidContext);
    TestTrue(TEXT("Admission loss preserves charge and releases lock"),Ore->State.Charges==1 && !Ore->State.bBusy);
    OtherGather->bTestWorldAdmission=true;
    OtherGather->ServerGatheringAction_Implementation(Ore,0,6); Other->Destroy();
    TestFalse(TEXT("Disconnect lifecycle releases lock"),Ore->State.bBusy);
    Inventory->MaxWeight=24;
    auto* Sawmill=World->SpawnActor<ASFGatheringPilotNode>(FVector(200,-140,100),FRotator::ZeroRotator,Spawn);
    auto* Forge=World->SpawnActor<ASFGatheringPilotNode>(FVector(-200,140,100),FRotator::ZeroRotator,Spawn);
    if (!Sawmill || !Forge) { return false; }
    Sawmill->Kind=ESFGatheringPilotKind::Sawmill; Sawmill->State.Charges=0;
    Forge->Kind=ESFGatheringPilotKind::Forge; Forge->State.Charges=0;
    const TSoftObjectPtr<USFItemDefinition> TimberItem(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_RoughTimber.DA_RoughTimber")));
    const TSoftObjectPtr<USFItemDefinition> IngotItem(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronIngot.DA_IronIngot")));
    Gather->ServerGatheringAction_Implementation(Sawmill,0,6); Tick(4);
    TestEqual(TEXT("Recipe does not consume inputs before its server channel completes"),SFInventoryTransactions::GetItemQuantity(Inventory,WoodItem),int64(1));
    Tick(8);
    TestEqual(TEXT("Recipe consumes exactly one log"),SFInventoryTransactions::GetItemQuantity(Inventory,WoodItem),int64(0));
    TestEqual(TEXT("Recipe creates design-authored four rough timber"),SFInventoryTransactions::GetItemQuantity(Inventory,TimberItem),int64(4));
    TestTrue(TEXT("Station increments revision without depleting"),Sawmill->State.Revision==1 && Sawmill->State.Charges==0 && !Sawmill->State.bBusy);
    Gather->ServerGatheringAction_Implementation(Sawmill,0,6);
    TestEqual(TEXT("Recipe replay does not multiply output"),SFInventoryTransactions::GetItemQuantity(Inventory,TimberItem),int64(4));
    Gather->ServerGatheringAction_Implementation(Sawmill,1,7);
    TestEqual(TEXT("Missing materials rejected before channel"),Gather->Feedback.Result,ESFGatheringPilotResult::MissingMaterials);
    FSFItemQuantity TimberCost; TimberCost.Item=TimberItem; TimberCost.Quantity=4;
    FSFItemQuantity OneLog; OneLog.Item=WoodItem; OneLog.Quantity=1;
    TestEqual(TEXT("Prepare one-slot bag through real transaction"),SFInventoryTransactions::ExchangeInventoryItems(Inventory,{TimberCost},{OneLog}),ESFInventoryResult::Success);
    Inventory->NumSlots=1; Inventory->MaxWeight=1;
    Gather->ServerGatheringAction_Implementation(Sawmill,1,8);
    TestEqual(TEXT("Overweight recipe rejected entirely"),Gather->Feedback.Result,ESFGatheringPilotResult::InventoryFull);
    TestEqual(TEXT("Capacity denial preserves ingredient"),SFInventoryTransactions::GetItemQuantity(Inventory,WoodItem),int64(1));
    TestEqual(TEXT("Capacity denial creates no output"),SFInventoryTransactions::GetItemQuantity(Inventory,TimberItem),int64(0));
    Inventory->MaxWeight=24;
    Gather->ServerGatheringAction_Implementation(Sawmill,1,9); Sawmill->Kind=ESFGatheringPilotKind::Forge; Tick(12);
    TestEqual(TEXT("Changing recipe during channel rejects as stale"),Gather->Feedback.Result,ESFGatheringPilotResult::Stale);
    TestEqual(TEXT("Changed recipe consumes no log"),SFInventoryTransactions::GetItemQuantity(Inventory,WoodItem),int64(1));
    TestTrue(TEXT("Changed recipe preserves revision and releases lock"),Sawmill->State.Revision==1 && !Sawmill->State.bBusy);
    Sawmill->Kind=ESFGatheringPilotKind::Sawmill;
    Gather->ServerGatheringAction_Implementation(Sawmill,1,9);
    TestEqual(TEXT("Same key retains original canceled outcome after content restored"),Gather->Feedback.Result,ESFGatheringPilotResult::Stale);
    Gather->ServerGatheringAction_Implementation(Sawmill,1,10); Tick(12);
    TestEqual(TEXT("Removing last ingredient frees the slot for the result"),SFInventoryTransactions::GetItemQuantity(Inventory,TimberItem),int64(4));
    FSFItemQuantity OneOre; OneOre.Item=OreItem; OneOre.Quantity=1;
    TestEqual(TEXT("Prepare ore through inventory transaction"),SFInventoryTransactions::ExchangeInventoryItems(Inventory,{TimberCost},{OneOre}),ESFInventoryResult::Success);
    Gather->ServerGatheringAction_Implementation(Forge,0,11); Tick(12);
    TestEqual(TEXT("Forge consumes one ore"),SFInventoryTransactions::GetItemQuantity(Inventory,OreItem),int64(0));
    TestEqual(TEXT("Forge creates exactly one design-authored ingot"),SFInventoryTransactions::GetItemQuantity(Inventory,IngotItem),int64(1));
    Gather->ServerGatheringAction_Implementation(Forge,1,12);
    TestEqual(TEXT("Forge cannot craft from nothing"),Gather->Feedback.Result,ESFGatheringPilotResult::MissingMaterials);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringPilotToolTest,
    "ShatteredFrontier.Gathering.Pilot.Tools",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFGatheringPilotToolTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC=World->SpawnActor<ASFPlayerController>();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn=World->SpawnActor<ASFE1Character>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
    auto* Wood=World->SpawnActor<ASFGatheringPilotNode>(FVector(200,0,100),FRotator::ZeroRotator,Spawn);
    if (!PC || !Pawn || !Wood) { return false; }
    PC->Possess(Pawn); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
    auto* Gather=PC->FindComponentByClass<USFGatheringPilotComponent>();
    auto* Bag=PC->FindComponentByClass<USFInventoryComponent>(); if (!Gather || !Bag) { return false; }
    Gather->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),Gather);
    Gather->CapturedSettings->HarvestSeconds=0.5f; Gather->bTestWorldAdmission=true; Gather->bEnabled=true;
    Wood->State.Charges=1;
    TestTrue(TEXT("Two-tool starter loadout commits"),Gather->InitializeStarterTools());
    TestEqual(TEXT("Starter loadout has exactly two items"),Bag->GetTotalItemCount(),2);
    TestTrue(TEXT("Loadout initialization can repeat safely"),Gather->InitializeStarterTools());
    TestEqual(TEXT("Repeated initialization cannot duplicate tools"),Bag->GetTotalItemCount(),2);
    Gather->ServerGatheringAction_Implementation(Wood,0,1);
    TestEqual(TEXT("Owning a tool without equipping it is insufficient"),Gather->Feedback.Result,ESFGatheringPilotResult::ToolRequired);
    TestFalse(TEXT("Missing tool does not lock node"),Wood->State.bBusy);
    TestTrue(TEXT("Plugin equips owned pickaxe"),Gather->TryEquipTool(false));
    Gather->ServerGatheringAction_Implementation(Wood,0,2);
    TestEqual(TEXT("Wrong category cannot log wood"),Gather->Feedback.Result,ESFGatheringPilotResult::ToolRequired);
    TestTrue(TEXT("Plugin swaps to axe"),Gather->TryEquipTool(true));
    Gather->ServerGatheringAction_Implementation(Wood,0,3);
    TestTrue(TEXT("Correct equipped tool starts channel"),Wood->State.bBusy);
    // Direct plugin equipment change exercises server revalidation even when the
    // normal key path's explicit cancel is bypassed.
    for (int32 I=0; I<Bag->NumSlots; ++I)
    {
        TSoftObjectPtr<USFItemDefinition> Item; int32 Quantity=0;
        if (Bag->GetSlot(I,Item,Quantity) && Item.ToSoftObjectPath()==FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronPickaxe.DA_IronPickaxe")))
        { TestTrue(TEXT("Mid-channel raw equipment RPC can swap tool"),Bag->EquipFromSlot(I)); break; }
    }
    Fixture.TickTestWorld(0.05f);
    TestEqual(TEXT("Changing required tool cancels before grant"),Gather->Feedback.Result,ESFGatheringPilotResult::ToolRequired);
    TestTrue(TEXT("Tool cancellation preserves charge/revision and releases lock"),Wood->State.Charges==1 && Wood->State.Revision==0 && !Wood->State.bBusy);
    Gather->ServerGatheringAction_Implementation(Wood,0,3);
    TestEqual(TEXT("Replay retains canceled tool outcome"),Gather->Feedback.Result,ESFGatheringPilotResult::ToolRequired);
    TestTrue(TEXT("Re-equip axe for a new action"),Gather->TryEquipTool(true));
    Gather->ServerGatheringAction_Implementation(Wood,0,4);
    for (int32 I=0; I<12; ++I) { Fixture.TickTestWorld(0.05f); }
    TestEqual(TEXT("Equipped tool permits exactly one final harvest"),Wood->State.Revision,uint32(1));
    TestEqual(TEXT("Successful harvest outcome"),Gather->Feedback.Result,ESFGatheringPilotResult::Granted);

    auto* SmallPC=World->SpawnActor<ASFPlayerController>(); if (!SmallPC) { return false; }
    auto* SmallGather=SmallPC->FindComponentByClass<USFGatheringPilotComponent>();
    auto* SmallBag=SmallPC->FindComponentByClass<USFInventoryComponent>(); if (!SmallGather || !SmallBag) { return false; }
    SmallGather->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),SmallGather);
    SmallGather->bTestWorldAdmission=true; SmallGather->bEnabled=true; SmallBag->NumSlots=1;
    TestFalse(TEXT("Starter loadout fails atomically when only one slot fits"),SmallGather->InitializeStarterTools());
    TestEqual(TEXT("Failed starter grant leaves bag empty"),SmallBag->GetTotalItemCount(),0);
    SmallBag->NumSlots=2;
    TestTrue(TEXT("Loadout retries after capacity correction"),SmallGather->InitializeStarterTools());
    TestEqual(TEXT("Retry grants exactly the two tools"),SmallBag->GetTotalItemCount(),2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringPilotLifecycleTest,
    "ShatteredFrontier.Gathering.Pilot.Lifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFGatheringPilotLifecycleTest::RunTest(const FString& Parameters)
{
    USFGatheringPilotSettings* Bounds=NewObject<USFGatheringPilotSettings>(); FString Error;
    TestEqual(TEXT("Shipped common-node recovery is 300 seconds"),Bounds->RespawnSeconds,300.0f);
    Bounds->RespawnSeconds=0; TestFalse(TEXT("Zero recovery rejected"),Bounds->Validate(Error));
    Bounds->RespawnSeconds=86401; TestFalse(TEXT("Unbounded recovery rejected"),Bounds->Validate(Error));
    Bounds->RespawnSeconds=1; TestTrue(TEXT("Declared minimum recovery accepted"),Bounds->Validate(Error));
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* PC=World->SpawnActor<ASFPlayerController>();
    auto* Pawn=World->SpawnActor<ASFE1Character>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
    auto* Wood=World->SpawnActor<ASFGatheringPilotNode>(FVector(200,0,100),FRotator::ZeroRotator,Spawn);
    if (!PC || !Pawn || !Wood) { return false; }
    PC->Possess(Pawn); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
    Pawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Gather=PC->FindComponentByClass<USFGatheringPilotComponent>();
    auto* Bag=PC->FindComponentByClass<USFInventoryComponent>(); if (!Gather || !Bag) { return false; }
    Gather->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),Gather);
    Gather->CapturedSettings->HarvestSeconds=0.5f; Gather->bEnabled=true; Gather->bTestWorldAdmission=true;
    if (!TestTrue(TEXT("Real starter loadout"),Gather->InitializeStarterTools())
        || !TestTrue(TEXT("Real equipped axe"),Gather->TryEquipTool(true))) { return false; }
    TestEqual(TEXT("Initial presentation captures node capacity"),Wood->GetPresentationState().Capacity,3);
    TestEqual(TEXT("Fresh resource has no extraction damage"),Wood->GetPresentationState().ExtractedFraction,0.0f);
    // This one-charge fixture covers the legacy no-profile timer; growth with staged charges has its own focused test.
    Wood->GrowthStages.Reset(); Wood->State.Capacity=1; Wood->State.Charges=1; Wood->CapturedRespawnSeconds=1;
    const auto Tick=[&Fixture](int32 Count) { for (int32 I=0; I<Count; ++I) { Fixture.TickTestWorld(0.05f); } };
    const TSoftObjectPtr<USFItemDefinition> Item(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_OakLog.DA_OakLog")));
    Gather->ServerGatheringAction_Implementation(Wood,0,1);
    TestTrue(TEXT("Observers can reconstruct busy state without bag contents"),Wood->GetPresentationState().bBusy);
    TestEqual(TEXT("Preview channel has no persistent damage"),Wood->GetPresentationState().ExtractedFraction,0.0f);
    Tick(12);
    TestEqual(TEXT("Final harvest grants one log"),SFInventoryTransactions::GetItemQuantity(Bag,Item),int64(1));
    TestEqual(TEXT("Depleted appearance is fully extracted"),Wood->GetPresentationState().ExtractedFraction,1.0f);
    TestTrue(TEXT("Recovery deadline is server-owned and in the future"),Wood->State.RecoveryAtServerSeconds>World->GetTimeSeconds());
    const double Deadline=Wood->State.RecoveryAtServerSeconds;
    Wood->Recover();
    TestEqual(TEXT("Early recovery callback cannot restore a charge"),Wood->State.Charges,0);
    TestEqual(TEXT("Early callback preserves deadline"),Wood->State.RecoveryAtServerSeconds,Deadline);
    Tick(10);
    TestEqual(TEXT("Resource stays depleted before recovery"),Wood->State.Charges,0);
    Tick(14);
    TestEqual(TEXT("Recovery restores captured capacity"),Wood->State.Charges,1);
    TestEqual(TEXT("Recovery advances revision beyond depletion"),Wood->State.Revision,uint32(2));
    TestEqual(TEXT("New generation has no extraction damage"),Wood->GetPresentationState().ExtractedFraction,0.0f);
    TestEqual(TEXT("Recovery clears its deadline"),Wood->State.RecoveryAtServerSeconds,0.0);
    TestEqual(TEXT("Recovery never creates inventory items"),SFInventoryTransactions::GetItemQuantity(Bag,Item),int64(1));
    Gather->ServerGatheringAction_Implementation(Wood,0,1);
    TestEqual(TEXT("Old successful replay does not harvest renewed resource"),Wood->State.Charges,1);
    TestEqual(TEXT("Old replay never duplicates material"),SFInventoryTransactions::GetItemQuantity(Bag,Item),int64(1));
    Gather->ServerGatheringAction_Implementation(Wood,0,2);
    TestEqual(TEXT("Old-generation new request is stale"),Gather->Feedback.Result,ESFGatheringPilotResult::Stale);
    Gather->ServerGatheringAction_Implementation(Wood,2,3); Gather->ServerCancelHarvest_Implementation(3);
    TestEqual(TEXT("Cancellation preserves renewed resource appearance"),Wood->GetPresentationState().ExtractedFraction,0.0f);
    TestEqual(TEXT("Cancellation does not schedule recovery"),Wood->State.RecoveryAtServerSeconds,0.0);
    Gather->ServerGatheringAction_Implementation(Wood,2,4); Tick(12);
    TestEqual(TEXT("New generation can be harvested once"),SFInventoryTransactions::GetItemQuantity(Bag,Item),int64(2));
    TestEqual(TEXT("Second depletion has its own revision"),Wood->State.Revision,uint32(3));
    TestEqual(TEXT("Renewal advances a separate visual generation"),Wood->State.Generation,uint32(1));
    FTimerHandle Timer=Wood->RecoveryTimer;
    TestTrue(TEXT("Second depletion schedules recovery"),World->GetTimerManager().IsTimerActive(Timer));
    Wood->Destroy();
    TestFalse(TEXT("Destroyed resource clears owned timer"),World->GetTimerManager().TimerExists(Timer));
    auto* Station=World->SpawnActor<ASFGatheringPilotNode>(); if (!Station) { return false; }
    Station->Kind=ESFGatheringPilotKind::Forge; Station->State.Charges=0; Station->BeginRecovery();
    TestFalse(TEXT("Refining station never schedules resource recovery"),World->GetTimerManager().IsTimerActive(Station->RecoveryTimer));
    TestEqual(TEXT("Station has no extraction damage"),Station->GetPresentationState().ExtractedFraction,0.0f);
    Station->Kind=ESFGatheringPilotKind::Oak; Station->State.Revision=MAX_uint32;
    Station->State.RecoveryAtServerSeconds=World->GetTimeSeconds()-0.1;
    Station->Recover();
    TestEqual(TEXT("Exhausted revision never wraps into fresh resource"),Station->State.Revision,MAX_uint32);
    TestEqual(TEXT("Revision exhaustion stays depleted"),Station->State.Charges,0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringPilotSurfaceProtocolTest,
    "ShatteredFrontier.Gathering.Pilot.SurfaceProtocol",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFGatheringPilotSurfaceProtocolTest::RunTest(const FString& Parameters)
{
    FSFHarvestVisualCut Cut;
    Cut.Point=FVector(12.345,67.891,23.456); Cut.Normal=FVector(1,2,3).GetSafeNormal();
    Cut.RadiusCm=12; Cut.DepthCm=8;
    TestTrue(TEXT("Server cutter adopts actual quantized wire values"),Cut.CanonicalizeWire());
    const FSFHarvestVisualCut Once=Cut;
    TestTrue(TEXT("Wire conversion is repeatable"),Cut.CanonicalizeWire());
    TestTrue(TEXT("Point remains identical after repeated serialization"),Cut.Point.Equals(Once.Point,0));
    TestTrue(TEXT("Normal remains identical after repeated serialization"),Cut.Normal.Equals(Once.Normal,0));
    FVector_NetQuantize10 Point=FVector(100,200,300);
    TestFalse(TEXT("Nonfinite point rejected"),FSFHarvestVisualCut::CanonicalizePoint(FVector(std::numeric_limits<double>::infinity(),0,0),Point));
    TestTrue(TEXT("Rejected point leaves output unchanged"),Point.Equals(FVector(100,200,300),0));
    auto* Component=NewObject<USFGatheringPilotComponent>();
    Component->CapturedSettings=NewObject<USFGatheringPilotSettings>(Component);
    Component->LastRevision=7; Component->bLastHadSurface=true; Component->LastRequestedSurface=Cut.Point;
    TestTrue(TEXT("Identical surface replay matches"),Component->MatchesRequestBinding(nullptr,7,true,Cut.Point));
    TestFalse(TEXT("Plain replay cannot replace surface variant"),Component->MatchesRequestBinding(nullptr,7,false,FVector::ZeroVector));
    TestFalse(TEXT("Changed contact cannot replace original"),Component->MatchesRequestBinding(nullptr,7,true,FVector(Cut.Point)+FVector(1,0,0)));
    Component->bLastHadSurface=false;
    TestFalse(TEXT("Surface replay cannot replace plain variant"),Component->MatchesRequestBinding(nullptr,7,true,Cut.Point));
    TestTrue(TEXT("First geometry attempt admitted"),Component->TryBeginGeometryAttempt(10));
    TestFalse(TEXT("Repeated failed geometry cannot bypass cooldown"),Component->TryBeginGeometryAttempt(10.49));
    TestTrue(TEXT("Default half-second boundary admitted"),Component->TryBeginGeometryAttempt(10.5));
    FString Error; Component->CapturedSettings->GeometryAttemptCooldownSeconds=0.09f;
    TestFalse(TEXT("Cooldown below declared bound fails configuration"),Component->CapturedSettings->Validate(Error));
    Component->CapturedSettings->GeometryAttemptCooldownSeconds=2.01f;
    TestFalse(TEXT("Cooldown above declared bound fails configuration"),Component->CapturedSettings->Validate(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringPilotLedgerTest,
    "ShatteredFrontier.Gathering.Pilot.Ledger",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFGatheringPilotLedgerTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Stubbed InventoryLedger transport (INT-03 section 4); the gRPC host path is tools/Test-GatheringPilot.ps1 -Ledger."));
    // In-memory stand-in for InventoryLedger.Host: revision compare-and-set plus idempotent receipts.
    struct FFakeLedger
    {
        int64 Revision=0; int32 Commits=0; bool bDropFirstGrantReply=false; bool bAllUnavailable=false; bool bHold=false;
        TMap<FString,int64> Receipts; TArray<FSFLedgerRequest> Requests; TArray<TFunction<void(const FSFLedgerResponse&)>> Held;
        FSFLedgerResponse Reply(const FSFLedgerRequest& Request)
        {
            FSFLedgerResponse Out; Out.GrpcCode=0; Out.Status=ESFLedgerStatus::Ok; Out.Revision=Revision;
            if (bAllUnavailable) { Out.Status=ESFLedgerStatus::Unavailable; Out.GrpcCode=14; return Out; }
            if (Request.Rpc!=ESFLedgerRpc::Grant) { return Out; }
            if (const int64* Prior=Receipts.Find(Request.OperationId)) { Out.Revision=*Prior; Out.GrantedDelta=Request.Quantity; return Out; }
            if (Request.ExpectedRevision!=Revision) { Out.Status=ESFLedgerStatus::Stale; Out.GrpcCode=10; return Out; }
            Receipts.Add(Request.OperationId,++Revision); ++Commits; Out.Revision=Revision; Out.GrantedDelta=Request.Quantity;
            if (bDropFirstGrantReply) { bDropFirstGrantReply=false; Out=FSFLedgerResponse(); }  // committed, reply lost
            return Out;
        }
    };
    TSharedRef<FFakeLedger> Fake=MakeShared<FFakeLedger>();
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC=World->SpawnActor<ASFPlayerController>();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn=World->SpawnActor<ASFE1Character>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
    auto* Wood=World->SpawnActor<ASFGatheringPilotNode>(FVector(200,0,100),FRotator::ZeroRotator,Spawn);
    if (!PC || !Pawn || !Wood) { return false; }
    PC->Possess(Pawn); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
    auto* Gather=PC->FindComponentByClass<USFGatheringPilotComponent>();
    auto* Bag=PC->FindComponentByClass<USFInventoryComponent>(); if (!Gather || !Bag) { return false; }
    Gather->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),Gather);
    Gather->CapturedSettings->HarvestSeconds=0.5f; Gather->bTestWorldAdmission=true; Gather->bEnabled=true;
    if (!TestTrue(TEXT("Starter tools and axe"),Gather->InitializeStarterTools() && Gather->TryEquipTool(true))) { return false; }
    Wood->State.Charges=3;

    auto* Settings=DuplicateObject<USFInventoryLedgerSettings>(GetDefault<USFInventoryLedgerSettings>(),GetTransientPackage());
    TestFalse(TEXT("Ledger is off by default"),Settings->bEnabled);
    Settings->bEnabled=true; Settings->MaxAttempts=3;
    FString Error; Settings->Endpoint=TEXT("10.0.0.5:50071");
    TestFalse(TEXT("Non-loopback endpoint fails validation"),Settings->Validate(Error));
    Settings->Endpoint=TEXT("127.0.0.1:50071");
    FSFInventoryLedgerClient::SetSendOverrideForTests([Fake](const FSFLedgerRequest& Request,float,TFunction<void(const FSFLedgerResponse&)> Done)
    {
        Fake->Requests.Add(Request);
        if (Fake->bHold) { Fake->Held.Add(MoveTemp(Done)); return; }
        Done(Fake->Reply(Request));
    });
    TSharedPtr<FSFInventoryLedgerClient> Client=FSFInventoryLedgerClient::Create(*Settings);
    FSFInventoryLedgerClient::SetSendOverrideForTests(nullptr);
    if (!TestTrue(TEXT("Stubbed client created"),Client.IsValid())) { return false; }
    // INT-04: the controller backpack is the single ledger writer the harvest commit goes through.
    USFBackpackComponent* Pack=USFBackpackComponent::CreateForTests(PC,Client);
    Gather->Backpack=Pack;
    // INT-06 requires the quest snapshot before Grant; the generic test world has no Silverbrook quest subsystem to attach it.
    USFQuestPilotComponent* Quest=NewObject<USFQuestPilotComponent>(PC);
    Quest->RegisterComponent();

    const TSoftObjectPtr<USFItemDefinition> WoodItem(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_OakLog.DA_OakLog")));
    const auto Tick=[&Fixture](int32 Count) { for (int32 I=0; I<Count; ++I) { Fixture.TickTestWorld(0.05f); FTSTicker::GetCoreTicker().Tick(0.3f); } };
    const auto Grants=[Fake](const FString& Op) { int32 N=0; for (const auto& R : Fake->Requests) { N+=R.Rpc==ESFLedgerRpc::Grant && R.OperationId==Op; } return N; };

    Gather->ServerGatheringAction_Implementation(Wood,0,1); Tick(12);
    TestEqual(TEXT("Receipt grants"),Gather->Feedback.Result,ESFGatheringPilotResult::Granted);
    TestEqual(TEXT("Projection shows the committed unit"),SFInventoryTransactions::GetItemQuantity(Bag,WoodItem),int64(1));
    TestTrue(TEXT("Charge consumed once after the receipt"),Wood->State.Charges==2 && Wood->State.Revision==1 && !Wood->State.bBusy);
    if (!TestTrue(TEXT("EnsureContainer then Grant"),Fake->Requests.Num()==2 && Fake->Requests[0].Rpc==ESFLedgerRpc::EnsureContainer)) { return false; }
    const FSFLedgerRequest First=Fake->Requests[1];
    TestTrue(TEXT("Server-generated operation id"),First.OperationId.StartsWith(TEXT("gather/")) && First.OperationId.Len()==43);
    TestEqual(TEXT("Causation is container:client operation"),First.CausationId,FString(TEXT("ctn_dev_local:1")));
    TestTrue(TEXT("Grant payload"),First.ContainerId==TEXT("ctn_dev_local") && First.ExpectedRevision==0 && First.DefinitionId==TEXT("item/oak_log")
        && First.Quantity==1 && First.Source==TEXT("gathering"));
    TestTrue(TEXT("Grant carries the captured follow-up intent"),!First.FollowUpPayload.IsEmpty() && First.FollowUpDigest.Len()==64);
    TestEqual(TEXT("Container capacity from typed settings"),Fake->Requests[0].Capacity,Gather->CapturedSettings->InventorySlots*Gather->CapturedSettings->MaterialStackUnits);

    Fake->bDropFirstGrantReply=true;
    Gather->ServerGatheringAction_Implementation(Wood,1,2); Tick(14);
    const FString Second=Fake->Requests.Last().OperationId;
    TestEqual(TEXT("Lost reply is retried with the same operation id"),Grants(Second),2);
    TestEqual(TEXT("Exactly one ledger commit per harvest"),Fake->Commits,2);
    TestEqual(TEXT("Retry projects one unit"),SFInventoryTransactions::GetItemQuantity(Bag,WoodItem),int64(2));
    TestNotEqual(TEXT("Each harvest has its own operation id"),Second,First.OperationId);

    Fake->bHold=true;
    Gather->ServerGatheringAction_Implementation(Wood,2,3); Tick(12);
    TestEqual(TEXT("Awaiting receipt"),Gather->Feedback.Phase,ESFGatheringPilotPhase::Committing);
    TestTrue(TEXT("Node stays locked while committing"),Wood->State.bBusy && Wood->State.Charges==1);
    Gather->ServerGatheringAction_Implementation(Wood,2,4);
    TestEqual(TEXT("No new action while committing"),Gather->Feedback.Operation,uint32(3));
    Fake->bHold=false; Fake->bAllUnavailable=true;
    if (!TestEqual(TEXT("One attempt in flight"),Fake->Held.Num(),1)) { return false; }
    const FString Third=Fake->Requests.Last().OperationId;
    FSFLedgerResponse Down; Fake->Held[0](Down); Fake->Held.Reset(); Tick(10);
    TestEqual(TEXT("Exhausted retries reject as unavailable"),Gather->Feedback.Result,ESFGatheringPilotResult::Unavailable);
    TestEqual(TEXT("MaxAttempts bounds retries"),Grants(Third),3);
    TestTrue(TEXT("Unavailable keeps charge and revision and releases lock"),Wood->State.Charges==1 && Wood->State.Revision==2 && !Wood->State.bBusy);
    TestEqual(TEXT("Nothing projected"),SFInventoryTransactions::GetItemQuantity(Bag,WoodItem),int64(2));

    Fake->bAllUnavailable=false; Pack->Revision=Fake->Revision; Fake->Revision+=5;  // another writer moved the container
    const int32 CommitsBefore=Fake->Commits;
    Gather->ServerGatheringAction_Implementation(Wood,2,4); Tick(14);
    const FString StaleOperation=Fake->Requests.Last().OperationId;
    TestEqual(TEXT("Stale revision rejects the captured intent"),Gather->Feedback.Result,ESFGatheringPilotResult::Stale);
    TestEqual(TEXT("Stale intent is sent once with its original operation id"),Grants(StaleOperation),1);
    TestEqual(TEXT("Stale grant does not commit"),Fake->Commits,CommitsBefore);
    TestTrue(TEXT("Stale grant preserves charge and releases lock"),Wood->State.Charges==1 && !Wood->State.bBusy);
    Gather->ServerGatheringAction_Implementation(Wood,2,5); Tick(14);
    TestEqual(TEXT("New harvest refreshes the revision and commits"),Gather->Feedback.Result,ESFGatheringPilotResult::Granted);
    TestNotEqual(TEXT("New harvest captures a distinct intent"),Fake->Requests.Last().OperationId,StaleOperation);
    TestEqual(TEXT("Exactly one commit after refresh"),Fake->Commits,CommitsBefore+1);
    TestTrue(TEXT("Successful new harvest consumes one charge"),Wood->State.Charges==0 && !Wood->State.bBusy);
    Wood->State.Charges=1;

    Fake->bHold=true;
    Gather->ServerGatheringAction_Implementation(Wood,Wood->State.Revision,6); Tick(12);
    if (!TestEqual(TEXT("Disconnect case reaches committing"),Fake->Held.Num(),1)) { return false; }
    PC->Destroy();
    TestFalse(TEXT("Disconnect releases the node lock"),Wood->State.bBusy);
    FSFLedgerResponse Late=Fake->Reply(Fake->Requests.Last()); Fake->Held[0](Late);
    TestEqual(TEXT("Ledger resolved the outcome after the owner left"),Fake->Commits,CommitsBefore+2);
    return true;
}

// GAT-01: stage -> appearance. Pure rule, declared scales, and a server node regrowing young -> mature.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringGrowthVisualTest,
    "ShatteredFrontier.Gathering.Growth.Visual",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFGatheringGrowthVisualTest::RunTest(const FString& Parameters)
{
    const TArray<FSFGrowthStageDef> Stages={{TEXT("young"),0.5f,0.3f},{TEXT("growing"),0.5f,0.5f},{TEXT("mature"),0.f,1.f}};
    const double Window=150;  // respawn 300 s * grow_fraction 0.5, as the Silverbrook iron profile
    TestEqual(TEXT("t=0 is young"),ASFGatheringPilotNode::StageAt(Stages,Window,0),FName(TEXT("young")));
    TestEqual(TEXT("just before the boundary is young"),ASFGatheringPilotNode::StageAt(Stages,Window,74),FName(TEXT("young")));
    TestEqual(TEXT("the boundary is growing"),ASFGatheringPilotNode::StageAt(Stages,Window,75),FName(TEXT("growing")));
    TestEqual(TEXT("the end of the window is mature"),ASFGatheringPilotNode::StageAt(Stages,Window,150),FName(TEXT("mature")));
    TestEqual(TEXT("after the window stays mature"),ASFGatheringPilotNode::StageAt(Stages,Window,9999),FName(TEXT("mature")));
    TestTrue(TEXT("no profile gives no stage"),ASFGatheringPilotNode::StageAt({},Window,10).IsNone());

    const auto* Visuals=GetDefault<USFGatheringGrowthSettings>(); FString Error;
    TestTrue(TEXT("Shipped appearance table is valid"),Visuals->Validate(Error));
    for (const auto Kind : {ESFGatheringPilotKind::Oak,ESFGatheringPilotKind::Iron,ESFGatheringPilotKind::Stone})
    {
        const float Young=Visuals->ScaleFor(Kind,TEXT("young")),Growing=Visuals->ScaleFor(Kind,TEXT("growing")),Mature=Visuals->ScaleFor(Kind,TEXT("mature"));
        TestTrue(TEXT("Scales grow with the stage and mature is full size"),Young<Growing && Growing<Mature && FMath::IsNearlyEqual(Mature,1.f));
        TestEqual(TEXT("An undeclared stage is full size"),Visuals->ScaleFor(Kind,TEXT("ancient")),1.f);
    }
    USFGatheringGrowthSettings* Bad=NewObject<USFGatheringGrowthSettings>();
    Bad->StageVisuals.Add({ESFGatheringPilotKind::Oak,TEXT("young"),3.f});
    TestFalse(TEXT("Out-of-bounds scale is rejected"),Bad->Validate(Error));

    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    const FTransform At(FRotator::ZeroRotator,FVector(0,0,70));
    auto* Node=World->SpawnActorDeferred<ASFGatheringPilotNode>(ASFGatheringPilotNode::StaticClass(),At);
    if (!Node) { return false; }
    Node->Kind=ESFGatheringPilotKind::Iron; Node->SetGrowthProfile(Stages,0.5f); Node->FinishSpawning(At);
    const FVector Base(0.7,0.7,1.4);
    TestEqual(TEXT("A fresh node is mature"),Node->State.GrowthStage,FName(TEXT("mature")));
    TestTrue(TEXT("and full size"),Node->Mesh->GetRelativeScale3D().Equals(Base));
    Node->CapturedRespawnSeconds=300; Node->StartGrowth();
    TestEqual(TEXT("Regrowth starts young"),Node->State.GrowthStage,FName(TEXT("young")));
    TestTrue(TEXT("and shows the declared young scale"),Node->Mesh->GetRelativeScale3D().Equals(Base*Visuals->ScaleFor(ESFGatheringPilotKind::Iron,TEXT("young"))));
    Node->State.GrowthStage=TEXT("growing"); Node->OnRep_State();  // what a client does when the stage replicates
    TestTrue(TEXT("A replicated stage change updates the scale"),Node->Mesh->GetRelativeScale3D().Equals(Base*Visuals->ScaleFor(ESFGatheringPilotKind::Iron,TEXT("growing"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringStageChargesTest,
    "ShatteredFrontier.Gathering.Growth.StageCharges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFGatheringStageChargesTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    const FTransform At(FRotator::ZeroRotator,FVector(0,0,70));
    auto* Node=World->SpawnActorDeferred<ASFGatheringPilotNode>(ASFGatheringPilotNode::StaticClass(),At);
    if (!Node) { return false; }
    Node->Kind=ESFGatheringPilotKind::Iron;
    Node->SetGrowthProfile({{TEXT("young"),0.5f,0.3f},{TEXT("growing"),0.5f,0.5f},{TEXT("mature"),0.f,1.f}},0.5f);
    Node->FinishSpawning(At);
    Node->State.Capacity=5; Node->State.Charges=0; Node->CapturedRespawnSeconds=2; Node->RecoveryKind=Node->Kind;
    Node->State.RecoveryAtServerSeconds=World->GetTimeSeconds()+2;
    Node->Recover();
    TestEqual(TEXT("Before emergence there are no charges"),Node->State.Charges,0);
    Node->State.RecoveryAtServerSeconds=World->GetTimeSeconds()+0.9;
    Node->Recover();
    TestTrue(TEXT("Emergence replicates young with two of five charges"),Node->State.GrowthStage==TEXT("young") && Node->State.Charges==2);
    Node->HarvestedThisGrowth=1; Node->State.Charges=1;
    for (int32 I=0; I<11; ++I) { Fixture.TickTestWorld(0.05f); }
    TestTrue(TEXT("Growing stage adds only its new allowance"),Node->State.GrowthStage==TEXT("growing") && Node->State.Charges==2);
    for (int32 I=0; I<11; ++I) { Fixture.TickTestWorld(0.05f); }
    TestTrue(TEXT("Maturity exposes full capacity minus harvested charges"),Node->State.GrowthStage==TEXT("mature") && Node->State.Charges==4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringGrowthCollisionTest,
    "ShatteredFrontier.Gathering.Growth.Collision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFGatheringGrowthCollisionTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    const FTransform At(FRotator::ZeroRotator,FVector(0,0,70));
    auto* Node=World->SpawnActorDeferred<ASFGatheringPilotNode>(ASFGatheringPilotNode::StaticClass(),At);
    if (!Node) { return false; }
    Node->Kind=ESFGatheringPilotKind::Iron;
    Node->SetGrowthProfile({{TEXT("young"),0.5f,0.3f,false},{TEXT("growing"),0.5f,0.5f},{TEXT("mature"),0.f,1.f}},0.5f);
    Node->FinishSpawning(At);
    Node->CapturedRespawnSeconds=2; Node->StartGrowth();
    TestEqual(TEXT("Growth begins young"),Node->State.GrowthStage,FName(TEXT("young")));
    TestTrue(TEXT("Nonblocking young node remains traceable"),
        Node->Mesh->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Ignore &&
        Node->Mesh->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Block);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn=World->SpawnActor<ASFE1Character>(FVector(0,0,70),FRotator::ZeroRotator,Spawn);
    if (!Pawn) { return false; }
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
    Pawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TestTrue(TEXT("Declared nonblocking stage never waits"),Node->CanAdvanceToStage(TEXT("young")));
    Node->GrowthStartedAt=World->GetTimeSeconds()-2;
    Node->AdvanceGrowth();
    TestEqual(TEXT("Overlapped mature footprint waits"),Node->State.GrowthStage,FName(TEXT("young")));
    Pawn->SetActorLocation(FVector(500,0,70));
    auto* OtherNode=World->SpawnActorDeferred<ASFGatheringPilotNode>(ASFGatheringPilotNode::StaticClass(),At);
    if (!OtherNode) { return false; }
    OtherNode->FinishSpawning(At);
    OtherNode->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Node->AdvanceGrowth();
    TestEqual(TEXT("Another node blocks even with collision disabled"),Node->State.GrowthStage,FName(TEXT("young")));
    OtherNode->Destroy();
    Fixture.TickTestWorld(0.05f);
    TestEqual(TEXT("Clear mature footprint advances on retry"),Node->State.GrowthStage,FName(TEXT("mature")));
    TestEqual(TEXT("Mature stage blocks pawns"),Node->Mesh->GetCollisionResponseToChannel(ECC_Pawn),ECR_Block);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringPilotServiceTest,
    "ShatteredFrontier.Gathering.Pilot.Services",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFGatheringPilotServiceTest::RunTest(const FString& Parameters)
{
    struct FFakeNode { int32 Charges=2, Harvests=0; bool bPendingFirst=true, bDelayNextRead=false;
        TArray<FSFGameplayRequest> Requests; TSet<FString> Receipts;
        FSFGameplayResponse DelayedReply; TFunction<void(const FSFGameplayResponse&)> DelayedDone; };
    TSharedRef<FFakeNode> NodeHost=MakeShared<FFakeNode>();
    USFGameplayServicesSettings* ServiceSettings=NewObject<USFGameplayServicesSettings>();
    ServiceSettings->bEnabled=true;
    FSFGameplayServicesClient::SetSendOverrideForTests([NodeHost](const FSFGameplayRequest& Request,float,TFunction<void(const FSFGameplayResponse&)> Done)
    {
        NodeHost->Requests.Add(Request);
        FSFGameplayResponse Reply; Reply.Status=ESFGameplayStatus::Ok; Reply.GrpcCode=0;
        Reply.bHasNode=true; Reply.NodeId=TEXT("zone/greenlands_silverbrook/eastwood_iron_ore/0");
        Reply.NodeDefinitionId=TEXT("gathering_node/iron_vein"); Reply.NodeGeneration=2; Reply.NodeCapacity=5;
        Reply.GrowthStage=NodeHost->Charges>0 ? TEXT("young") : TEXT("");
        if (Request.Rpc==ESFGameplayRpc::HarvestGatheringNode)
        {
            if (!NodeHost->Receipts.Contains(Request.OperationId) && NodeHost->Charges>0)
            { NodeHost->Receipts.Add(Request.OperationId); --NodeHost->Charges; ++NodeHost->Harvests; }
            Reply.HarvestStatus=NodeHost->Receipts.Contains(Request.OperationId) ? TEXT("granted") : TEXT("depleted");
            if (Reply.HarvestStatus==TEXT("granted")) { Reply.ResourceId=TEXT("item/iron_ore"); Reply.HarvestQuantity=1; }
            if (NodeHost->bPendingFirst) { NodeHost->bPendingFirst=false; Reply.HarvestStatus=TEXT("pending"); }
            Reply.GrowthStage=NodeHost->Charges>0 ? TEXT("young") : TEXT("");
        }
        Reply.NodeCharges=NodeHost->Charges;
        if (Request.Rpc==ESFGameplayRpc::GetGatheringNode && NodeHost->bDelayNextRead)
        { NodeHost->bDelayNextRead=false; NodeHost->DelayedReply=Reply; NodeHost->DelayedDone=MoveTemp(Done); return; }
        Done(Reply);
    });
    TSharedPtr<FSFGameplayServicesClient> Service=FSFGameplayServicesClient::Create(*ServiceSettings);
    FSFGameplayServicesClient::SetSendOverrideForTests(nullptr);
    if (!TestTrue(TEXT("Fake host transport created"),Service.IsValid())) { return false; }
    int32 LedgerGrants=0;
    FSFInventoryLedgerClient::SetSendOverrideForTests([&LedgerGrants,NodeHost](const FSFLedgerRequest& Request,float,TFunction<void(const FSFLedgerResponse&)> Done)
    {
        FSFLedgerResponse Reply; Reply.Status=ESFLedgerStatus::Ok; Reply.GrpcCode=0;
        if (Request.Rpc==ESFLedgerRpc::Grant) { ++LedgerGrants; Reply.GrantedDelta=Request.Quantity; }
        Reply.Revision=NodeHost->Harvests;
        if (Request.Rpc==ESFLedgerRpc::GetContainer) { Reply.Quantities.Add(TEXT("item/iron_ore"),NodeHost->Harvests); }
        Done(Reply);
    });
    USFInventoryLedgerSettings* LedgerSettings=NewObject<USFInventoryLedgerSettings>(); LedgerSettings->bEnabled=true;
    TSharedPtr<FSFInventoryLedgerClient> Ledger=FSFInventoryLedgerClient::Create(*LedgerSettings);
    FSFInventoryLedgerClient::SetSendOverrideForTests(nullptr);
    if (!TestTrue(TEXT("Fake ledger transport created"),Ledger.IsValid())) { return false; }
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC=World->SpawnActor<ASFPlayerController>();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn=World->SpawnActor<ASFE1Character>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
    const FTransform At(FRotator::ZeroRotator,FVector(200,0,100));
    auto* Iron=World->SpawnActorDeferred<ASFGatheringPilotNode>(ASFGatheringPilotNode::StaticClass(),At);
    if (!PC || !Pawn || !Iron) { return false; }
    Iron->Kind=ESFGatheringPilotKind::Iron;
    Iron->SetServiceNodeId(TEXT("zone/greenlands_silverbrook/eastwood_iron_ore/0")); Iron->FinishSpawning(At);
    Iron->bUsesServices=true; Iron->ServiceClient=Service; Iron->RefreshServiceState();
    TestTrue(TEXT("Immature snapshot projects two of five charges"),Iron->State.Charges==2 && Iron->State.Capacity==5 && Iron->State.GrowthStage==TEXT("young"));
    FSFGameplayResponse TimedSnapshot; TimedSnapshot.bHasNode=true;
    TimedSnapshot.NodeId=TEXT("zone/greenlands_silverbrook/eastwood_iron_ore/0");
    TimedSnapshot.NodeDefinitionId=TEXT("gathering_node/iron_vein"); TimedSnapshot.NodeGeneration=2;
    TimedSnapshot.NodeCapacity=5; TimedSnapshot.NodeCharges=2; TimedSnapshot.GrowthStage=TEXT("young");
    TimedSnapshot.NodeNextTransitionUnixMs=(FDateTime::UtcNow()-FDateTime::FromUnixTimestamp(0)).GetTotalMilliseconds()+1000;
    TestTrue(TEXT("Host transition deadline schedules the next server read"),Iron->ApplyServiceSnapshot(TimedSnapshot) &&
        World->GetTimerManager().GetTimerRemaining(Iron->ServiceRefreshTimer)>0.05f &&
        World->GetTimerManager().GetTimerRemaining(Iron->ServiceRefreshTimer)<2.f);
    TimedSnapshot.NodeNextTransitionUnixMs=0; Iron->ApplyServiceSnapshot(TimedSnapshot);
    PC->Possess(Pawn); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
    auto* Gather=PC->FindComponentByClass<USFGatheringPilotComponent>();
    if (!Gather) { return false; }
    Gather->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),Gather);
    Gather->CapturedSettings->HarvestSeconds=0.5f; Gather->bTestWorldAdmission=true; Gather->bEnabled=true;
    if (!TestTrue(TEXT("Pickaxe equipped"),Gather->InitializeStarterTools() && Gather->TryEquipTool(false))) { return false; }
    Gather->Backpack=USFBackpackComponent::CreateForTests(PC,Ledger);
    auto* Quest=NewObject<USFQuestPilotComponent>(PC); Quest->RegisterComponent();
    const auto Tick=[&Fixture](int32 Count=12) { for (int32 I=0; I<Count; ++I) { Fixture.TickTestWorld(0.05f); FTSTicker::GetCoreTicker().Tick(0.3f); } };
    Gather->ServerGatheringAction_Implementation(Iron,Iron->State.Revision,1); Tick(32);
    TestTrue(TEXT("First host harvest reduces immature charge"),Iron->State.Charges==1 && NodeHost->Harvests==1 && Gather->Feedback.Result==ESFGatheringPilotResult::Granted);
    NodeHost->bDelayNextRead=true; Iron->RefreshServiceState();
    if (!TestTrue(TEXT("Older node read is held while the next harvest commits"),bool(NodeHost->DelayedDone))) { return false; }
    Gather->ServerGatheringAction_Implementation(Iron,Iron->State.Revision,2); Tick();
    TestTrue(TEXT("Second host harvest depletes and replicates stage"),Iron->State.Charges==0 && Iron->State.GrowthStage.IsNone() && NodeHost->Harvests==2);
    NodeHost->DelayedDone(NodeHost->DelayedReply);
    TestTrue(TEXT("Pre-harvest node read cannot restore depleted charges"),Iron->State.Charges==0 && Iron->State.GrowthStage.IsNone());
    Gather->ServerGatheringAction_Implementation(Iron,Iron->State.Revision,3); Tick();
    TestEqual(TEXT("Host-owned harvest never issues a separate ledger grant"),LedgerGrants,0);
    const auto Harvests=NodeHost->Requests.FilterByPredicate([](const FSFGameplayRequest& R) { return R.Rpc==ESFGameplayRpc::HarvestGatheringNode; });
    if (!TestEqual(TEXT("Pending reply retries once, then second harvest"),Harvests.Num(),3)) { return false; }
    TestTrue(TEXT("Harvest carries host grant and INT-06 intent"),Harvests[0].OperationId.StartsWith(TEXT("gather/")) &&
        Harvests[0].ContainerId==TEXT("ctn_dev_local") && Harvests[0].CharacterId==TEXT("chr_dev_local") &&
        Harvests[0].NodeGeneration==2 && Harvests[0].FollowUpVersion==1 &&
        !Harvests[0].FollowUpPayload.IsEmpty() && Harvests[0].FollowUpDigest.Len()==64);
    TestTrue(TEXT("Pending retry keeps the whole harvest request immutable"),Harvests[1].OperationId==Harvests[0].OperationId &&
        Harvests[1].ContainerId==Harvests[0].ContainerId && Harvests[1].NodeGeneration==Harvests[0].NodeGeneration &&
        Harvests[1].FollowUpPayload==Harvests[0].FollowUpPayload && Harvests[1].FollowUpDigest==Harvests[0].FollowUpDigest);
    TestNotEqual(TEXT("New harvest uses a fresh operation"),Harvests[2].OperationId,Harvests[0].OperationId);
    return true;
}
#endif
