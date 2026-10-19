#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && SF_WITH_MINIMAL_INVENTORY
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Gathering/SFGatheringPilot.h"
#include "Construction/SFConstructionNetwork.h"
#include "Characters/SFE1Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryTransactionLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringPilotAuthorityTest,
    "ShatteredFrontier.Gathering.Pilot.Authority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFGatheringPilotAuthorityTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Actual actor/physics/plugin transactions in one authority world; network transport/private delivery require separate clients."));
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC=World->SpawnActor<ASFConstructionNetworkController>();
    auto* Other=World->SpawnActor<ASFConstructionNetworkController>();
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
    auto* Inventory=PC->FindComponentByClass<UInventoryComponent>();
    auto* OtherInventory=Other->FindComponentByClass<UInventoryComponent>();
    if (!Gather || !OtherGather || !Inventory || !OtherInventory) { return false; }
    for (auto* Component : {Gather,OtherGather})
    {
        Component->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),Component);
        Component->CapturedSettings->HarvestSeconds=0.5f; Component->CapturedSettings->RefineSeconds=0.5f; Component->bEnabled=true; Component->bTestWorldAdmission=true;
    }
    const auto GiveAndEquip=[this](UInventoryComponent* Bag, bool bAxe)
    {
        FInventoryItemQuantity Tool;
        Tool.Item=TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(bAxe
            ? TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronAxe.DA_IronAxe")
            : TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronPickaxe.DA_IronPickaxe")));
        Tool.Quantity=1;
        if (UInventoryTransactionLibrary::ExchangeInventoryItems(Bag,{}, {Tool})!=EInventoryTransactionResult::Success) { return false; }
        for (int32 I=0; I<Bag->NumSlots; ++I)
        {
            TSoftObjectPtr<UInventoryItemDefinition> Item; int32 Quantity=0;
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
    const TSoftObjectPtr<UInventoryItemDefinition> WoodItem(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_OakLog.DA_OakLog")));
    const TSoftObjectPtr<UInventoryItemDefinition> OreItem(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronOre.DA_IronOre")));
    const auto Tick=[&Fixture](int32 Count) { for (int32 I=0; I<Count; ++I) { Fixture.TickTestWorld(0.05f); } };
    Gather->ServerGatheringAction_Implementation(Wood,0,1);
    TestTrue(TEXT("One player acquires channel without consuming charge"),Wood->State.bBusy && Wood->State.Charges==1);
    OtherGather->ServerGatheringAction_Implementation(Wood,0,1);
    TestEqual(TEXT("Competing final charge is busy"),OtherGather->Feedback.Result,ESFGatheringPilotResult::Busy);
    Tick(4);
    TestEqual(TEXT("No early reward"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,WoodItem),int64(0));
    Tick(8);
    TestEqual(TEXT("Timed harvest grants exactly one unit"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,WoodItem),int64(1));
    TestTrue(TEXT("Final charge depletes once and releases lock"),Wood->State.Charges==0 && Wood->State.Revision==1 && !Wood->State.bBusy);
    TestEqual(TEXT("Other player receives no grant"),UInventoryTransactionLibrary::GetItemQuantity(OtherInventory,WoodItem),int64(0));
    Gather->ServerGatheringAction_Implementation(Wood,0,1);
    TestEqual(TEXT("Completed request replay does not duplicate grant"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,WoodItem),int64(1));
    Gather->ServerGatheringAction_Implementation(Ore,0,1);
    TestEqual(TEXT("Changed-payload replay cannot reserve another node"),Ore->State.bBusy,false);
    OtherGather->ServerGatheringAction_Implementation(Wood,1,2);
    TestEqual(TEXT("Later request sees depleted resource"),OtherGather->Feedback.Result,ESFGatheringPilotResult::Depleted);

    OtherGather->ServerGatheringAction_Implementation(Ore,0,3);
    OtherPawn->SetActorLocation(FVector(10,100,100)); Tick(1);
    TestEqual(TEXT("Moving cancels channel"),OtherGather->Feedback.Result,ESFGatheringPilotResult::Moved);
    TestTrue(TEXT("Movement preserves charge and lock becomes available"),Ore->State.Charges==1 && !Ore->State.bBusy);
    if (!TestTrue(TEXT("Ore test switches to pickaxe"),GiveAndEquip(Inventory,false))) { return false; }
    FInventoryItemQuantity ReturnedAxe; ReturnedAxe.Item=TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronAxe.DA_IronAxe"))); ReturnedAxe.Quantity=1;
    TestEqual(TEXT("Fixture removes returned axe to isolate raw-material capacity"),UInventoryTransactionLibrary::ExchangeInventoryItems(Inventory,{ReturnedAxe},{}),EInventoryTransactionResult::Success);
    Inventory->MaxWeight=1; // Already contains one 1 kg wood unit.
    Gather->ServerGatheringAction_Implementation(Ore,0,2); Tick(12);
    TestEqual(TEXT("Full bag rejects entire grant"),Gather->Feedback.Result,ESFGatheringPilotResult::InventoryFull);
    TestTrue(TEXT("Rejected grant preserves charge and revision"),Ore->State.Charges==1 && Ore->State.Revision==0 && !Ore->State.bBusy);
    TestEqual(TEXT("No partial ore grant"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,OreItem),int64(0));

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
    const TSoftObjectPtr<UInventoryItemDefinition> TimberItem(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_RoughTimber.DA_RoughTimber")));
    const TSoftObjectPtr<UInventoryItemDefinition> IngotItem(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronIngot.DA_IronIngot")));
    Gather->ServerGatheringAction_Implementation(Sawmill,0,6); Tick(4);
    TestEqual(TEXT("Recipe does not consume inputs before its server channel completes"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,WoodItem),int64(1));
    Tick(8);
    TestEqual(TEXT("Recipe consumes exactly one log"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,WoodItem),int64(0));
    TestEqual(TEXT("Recipe creates design-authored four rough timber"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,TimberItem),int64(4));
    TestTrue(TEXT("Station increments revision without depleting"),Sawmill->State.Revision==1 && Sawmill->State.Charges==0 && !Sawmill->State.bBusy);
    Gather->ServerGatheringAction_Implementation(Sawmill,0,6);
    TestEqual(TEXT("Recipe replay does not multiply output"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,TimberItem),int64(4));
    Gather->ServerGatheringAction_Implementation(Sawmill,1,7);
    TestEqual(TEXT("Missing materials rejected before channel"),Gather->Feedback.Result,ESFGatheringPilotResult::MissingMaterials);
    FInventoryItemQuantity TimberCost; TimberCost.Item=TimberItem; TimberCost.Quantity=4;
    FInventoryItemQuantity OneLog; OneLog.Item=WoodItem; OneLog.Quantity=1;
    TestEqual(TEXT("Prepare one-slot bag through real transaction"),UInventoryTransactionLibrary::ExchangeInventoryItems(Inventory,{TimberCost},{OneLog}),EInventoryTransactionResult::Success);
    Inventory->NumSlots=1; Inventory->MaxWeight=1;
    Gather->ServerGatheringAction_Implementation(Sawmill,1,8);
    TestEqual(TEXT("Overweight recipe rejected entirely"),Gather->Feedback.Result,ESFGatheringPilotResult::InventoryFull);
    TestEqual(TEXT("Capacity denial preserves ingredient"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,WoodItem),int64(1));
    TestEqual(TEXT("Capacity denial creates no output"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,TimberItem),int64(0));
    Inventory->MaxWeight=24;
    Gather->ServerGatheringAction_Implementation(Sawmill,1,9); Sawmill->Kind=ESFGatheringPilotKind::Forge; Tick(12);
    TestEqual(TEXT("Changing recipe during channel rejects as stale"),Gather->Feedback.Result,ESFGatheringPilotResult::Stale);
    TestEqual(TEXT("Changed recipe consumes no log"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,WoodItem),int64(1));
    TestTrue(TEXT("Changed recipe preserves revision and releases lock"),Sawmill->State.Revision==1 && !Sawmill->State.bBusy);
    Sawmill->Kind=ESFGatheringPilotKind::Sawmill;
    Gather->ServerGatheringAction_Implementation(Sawmill,1,9);
    TestEqual(TEXT("Same key retains original canceled outcome after content restored"),Gather->Feedback.Result,ESFGatheringPilotResult::Stale);
    Gather->ServerGatheringAction_Implementation(Sawmill,1,10); Tick(12);
    TestEqual(TEXT("Removing last ingredient frees the slot for the result"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,TimberItem),int64(4));
    FInventoryItemQuantity OneOre; OneOre.Item=OreItem; OneOre.Quantity=1;
    TestEqual(TEXT("Prepare ore through inventory transaction"),UInventoryTransactionLibrary::ExchangeInventoryItems(Inventory,{TimberCost},{OneOre}),EInventoryTransactionResult::Success);
    Gather->ServerGatheringAction_Implementation(Forge,0,11); Tick(12);
    TestEqual(TEXT("Forge consumes one ore"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,OreItem),int64(0));
    TestEqual(TEXT("Forge creates exactly one design-authored ingot"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,IngotItem),int64(1));
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
    auto* PC=World->SpawnActor<ASFConstructionNetworkController>();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn=World->SpawnActor<ASFE1Character>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
    auto* Wood=World->SpawnActor<ASFGatheringPilotNode>(FVector(200,0,100),FRotator::ZeroRotator,Spawn);
    if (!PC || !Pawn || !Wood) { return false; }
    PC->Possess(Pawn); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
    auto* Gather=PC->FindComponentByClass<USFGatheringPilotComponent>();
    auto* Bag=PC->FindComponentByClass<UInventoryComponent>(); if (!Gather || !Bag) { return false; }
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
        TSoftObjectPtr<UInventoryItemDefinition> Item; int32 Quantity=0;
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

    auto* SmallPC=World->SpawnActor<ASFConstructionNetworkController>(); if (!SmallPC) { return false; }
    auto* SmallGather=SmallPC->FindComponentByClass<USFGatheringPilotComponent>();
    auto* SmallBag=SmallPC->FindComponentByClass<UInventoryComponent>(); if (!SmallGather || !SmallBag) { return false; }
    SmallGather->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),SmallGather);
    SmallGather->bTestWorldAdmission=true; SmallGather->bEnabled=true; SmallBag->NumSlots=1;
    TestFalse(TEXT("Starter loadout fails atomically when only one slot fits"),SmallGather->InitializeStarterTools());
    TestEqual(TEXT("Failed starter grant leaves bag empty"),SmallBag->GetTotalItemCount(),0);
    SmallBag->NumSlots=2;
    TestTrue(TEXT("Loadout retries after capacity correction"),SmallGather->InitializeStarterTools());
    TestEqual(TEXT("Retry grants exactly the two tools"),SmallBag->GetTotalItemCount(),2);
    return true;
}
#endif
