#include "Inventory/SFInventoryPanel.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFInventoryPanelRowsTest,
    "ShatteredFrontier.Inventory.Panel.Rows",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFInventoryPanelRowsTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Empty inventory shows Empty"),USFInventoryPanel::FormatRows(USFInventoryPanel::BuildRows({})),FString(TEXT("Empty")));

    // Fake inventory: split oak stacks, an empty slot, an unnamed entry and an equipped tool.
    const TArray<TPair<FText,int32>> Fake={
        {FText::FromString(TEXT("Oak log")),5},
        {FText::FromString(TEXT("Iron ore")),2},
        {FText::FromString(TEXT("Oak log")),3},
        {FText::FromString(TEXT("Rough timber")),0},
        {FText::GetEmpty(),4},
        {FText::FromString(TEXT("Iron axe (equipped)")),1}};
    const TArray<TPair<FText,int32>> Rows=USFInventoryPanel::BuildRows(Fake);
    if (!TestEqual(TEXT("Merged, non-empty rows"),Rows.Num(),3)) { return false; }
    TestEqual(TEXT("Sorted first"),Rows[0].Key.ToString(),FString(TEXT("Iron axe (equipped)")));
    TestEqual(TEXT("Iron ore name"),Rows[1].Key.ToString(),FString(TEXT("Iron ore")));
    TestEqual(TEXT("Iron ore count"),Rows[1].Value,2);
    TestEqual(TEXT("Oak stacks merge"),Rows[2].Value,8);
    TestEqual(TEXT("Formatted text"),USFInventoryPanel::FormatRows(Rows),FString(TEXT("Iron axe (equipped)  x1\nIron ore  x2\nOak log  x8")));

    // A later harvest grant changes the count the panel shows.
    TArray<TPair<FText,int32>> AfterHarvest=Fake; AfterHarvest.Emplace(FText::FromString(TEXT("Oak log")),1);
    TestEqual(TEXT("Count updates after grant"),USFInventoryPanel::BuildRows(AfterHarvest)[2].Value,9);
    return true;
}

#if SF_WITH_MINIMAL_INVENTORY
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionNetwork.h"
#include "Gathering/SFGatheringPilot.h"
#include "GameFramework/WorldSettings.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryTransactionLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFInventoryPanelAccessorTest,
    "ShatteredFrontier.Inventory.Panel.Accessor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFInventoryPanelAccessorTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    UWorld* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC=World->SpawnActor<ASFConstructionNetworkController>();
    auto* Gather=PC ? PC->FindComponentByClass<USFGatheringPilotComponent>() : nullptr;
    auto* Bag=PC ? PC->FindComponentByClass<UInventoryComponent>() : nullptr;
    if (!TestNotNull(TEXT("Pilot"),Gather) || !TestNotNull(TEXT("Inventory"),Bag)) { return false; }
    auto Item=[](const TCHAR* Path,int32 Quantity)
    { FInventoryItemQuantity Q; Q.Item=TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(Path)); Q.Quantity=Quantity; return Q; };
    const FInventoryItemQuantity Axe=Item(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronAxe.DA_IronAxe"),1);
    const FInventoryItemQuantity Oak=Item(TEXT("/Game/Frontier/Gameplay/Pilot/DA_OakLog.DA_OakLog"),3);
    if (!TestEqual(TEXT("Grant axe and logs"),UInventoryTransactionLibrary::ExchangeInventoryItems(Bag,{}, {Axe,Oak}),EInventoryTransactionResult::Success)) { return false; }
    for (int32 I=0; I<Bag->NumSlots; ++I)
    {
        TSoftObjectPtr<UInventoryItemDefinition> Slot; int32 Quantity=0;
        if (Bag->GetSlot(I,Slot,Quantity) && Slot==Axe.Item) { TestTrue(TEXT("Equip axe"),Bag->EquipFromSlot(I)); break; }
    }
    const FText OakText=Oak.Item.LoadSynchronous()->DisplayName;
    const FString OakName=OakText.IsEmpty() ? Oak.Item.GetAssetName() : OakText.ToString();
    const TArray<TPair<FText,int32>> Rows=USFInventoryPanel::BuildRows(Gather->GetInventoryItems());
    const TPair<FText,int32>* OakRow=Rows.FindByPredicate([&OakName](const TPair<FText,int32>& Row) { return Row.Key.ToString()==OakName; });
    if (TestNotNull(TEXT("Oak row from real inventory"),OakRow)) { TestEqual(TEXT("Oak count"),OakRow->Value,3); }
    TestTrue(TEXT("Equipped tool listed"),Rows.ContainsByPredicate([](const TPair<FText,int32>& Row) { return Row.Key.ToString().EndsWith(TEXT(" (equipped)")); }));
    return true;
}
#endif
#endif
