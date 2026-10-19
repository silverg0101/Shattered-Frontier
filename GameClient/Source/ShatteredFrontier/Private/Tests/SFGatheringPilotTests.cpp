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
        Component->CapturedSettings->HarvestSeconds=0.5f; Component->bEnabled=true; Component->bTestWorldAdmission=true;
    }
    auto* Wood=World->SpawnActor<ASFGatheringPilotNode>(FVector(200,0,100),FRotator::ZeroRotator,Spawn);
    auto* Ore=World->SpawnActor<ASFGatheringPilotNode>(FVector(200,140,100),FRotator::ZeroRotator,Spawn);
    if (!Wood || !Ore) { return false; }
    Wood->State.Charges=1; Ore->State.Charges=1; Ore->Resource=ESFGatheringPilotResource::Iron;
    if (!TestTrue(TEXT("Generated catalog and real plugin available"),Gather->InventoryReady())) { return false; }
    const TSoftObjectPtr<UInventoryItemDefinition> WoodItem(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_OakLog.DA_OakLog")));
    const TSoftObjectPtr<UInventoryItemDefinition> OreItem(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronOre.DA_IronOre")));
    const auto Tick=[&Fixture](int32 Count) { for (int32 I=0; I<Count; ++I) { Fixture.TickTestWorld(0.05f); } };
    Gather->ServerHarvest_Implementation(Wood,0,1);
    TestTrue(TEXT("One player acquires channel without consuming charge"),Wood->State.bBusy && Wood->State.Charges==1);
    OtherGather->ServerHarvest_Implementation(Wood,0,1);
    TestEqual(TEXT("Competing final charge is busy"),OtherGather->Feedback.Result,ESFGatheringPilotResult::Busy);
    Tick(4);
    TestEqual(TEXT("No early reward"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,WoodItem),int64(0));
    Tick(8);
    TestEqual(TEXT("Timed harvest grants exactly one unit"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,WoodItem),int64(1));
    TestTrue(TEXT("Final charge depletes once and releases lock"),Wood->State.Charges==0 && Wood->State.Revision==1 && !Wood->State.bBusy);
    TestEqual(TEXT("Other player receives no grant"),UInventoryTransactionLibrary::GetItemQuantity(OtherInventory,WoodItem),int64(0));
    Gather->ServerHarvest_Implementation(Wood,0,1);
    TestEqual(TEXT("Completed request replay does not duplicate grant"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,WoodItem),int64(1));
    Gather->ServerHarvest_Implementation(Ore,0,1);
    TestEqual(TEXT("Changed-payload replay cannot reserve another node"),Ore->State.bBusy,false);
    OtherGather->ServerHarvest_Implementation(Wood,1,2);
    TestEqual(TEXT("Later request sees depleted resource"),OtherGather->Feedback.Result,ESFGatheringPilotResult::Depleted);

    OtherGather->ServerHarvest_Implementation(Ore,0,3);
    OtherPawn->SetActorLocation(FVector(10,100,100)); Tick(1);
    TestEqual(TEXT("Moving cancels channel"),OtherGather->Feedback.Result,ESFGatheringPilotResult::Moved);
    TestTrue(TEXT("Movement preserves charge and lock becomes available"),Ore->State.Charges==1 && !Ore->State.bBusy);
    Inventory->MaxWeight=1; // Already contains one 1 kg wood unit.
    Gather->ServerHarvest_Implementation(Ore,0,2); Tick(12);
    TestEqual(TEXT("Full bag rejects entire grant"),Gather->Feedback.Result,ESFGatheringPilotResult::InventoryFull);
    TestTrue(TEXT("Rejected grant preserves charge and revision"),Ore->State.Charges==1 && Ore->State.Revision==0 && !Ore->State.bBusy);
    TestEqual(TEXT("No partial ore grant"),UInventoryTransactionLibrary::GetItemQuantity(Inventory,OreItem),int64(0));

    auto* Wall=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(20,200,200));
    Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Wall->SetActorLocation(FVector(100,70,100));
    Gather->ServerHarvest_Implementation(Ore,0,3);
    TestEqual(TEXT("Server rejects occluded resource"),Gather->Feedback.Result,ESFGatheringPilotResult::Occluded);
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pawn->SetActorLocation(FVector(1000,0,100)); Gather->ServerHarvest_Implementation(Ore,0,4);
    TestEqual(TEXT("Server rejects distant resource"),Gather->Feedback.Result,ESFGatheringPilotResult::OutOfReach);
    Pawn->SetActorLocation(FVector(0,0,100)); Gather->ServerHarvest_Implementation(Ore,99,5);
    TestEqual(TEXT("Stale revision rejected"),Gather->Feedback.Result,ESFGatheringPilotResult::Stale);
    OtherGather->ServerHarvest_Implementation(Ore,0,4); OtherGather->ServerCancelHarvest_Implementation(4);
    TestEqual(TEXT("Explicit cancel preserves node"),OtherGather->Feedback.Result,ESFGatheringPilotResult::Canceled);
    TestTrue(TEXT("Canceled node remains available"),Ore->State.Charges==1 && !Ore->State.bBusy);
    OtherGather->ServerHarvest_Implementation(Ore,0,5); OtherGather->bTestWorldAdmission=false; Tick(1);
    TestEqual(TEXT("Loss of admitted world cancels active harvest"),OtherGather->Feedback.Result,ESFGatheringPilotResult::InvalidContext);
    TestTrue(TEXT("Admission loss preserves charge and releases lock"),Ore->State.Charges==1 && !Ore->State.bBusy);
    OtherGather->bTestWorldAdmission=true;
    OtherGather->ServerHarvest_Implementation(Ore,0,6); Other->Destroy();
    TestFalse(TEXT("Disconnect lifecycle releases lock"),Ore->State.bBusy);
    return true;
}
#endif
