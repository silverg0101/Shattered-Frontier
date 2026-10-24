#include "UI/SFEquipmentWidget.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"

// WBP_Equipment binds its parts, fills the grid with only the equippable bag items (per tab), shows equipped items on the paper doll and the
// slot ghost while empty, and resolves drops. Needs WBP_Equipment (tools/ui/create_widgets.py, SF_UI_ONLY=WBP_EquipSlot,WBP_Equipment).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFEquipmentWidgetTest, "ShatteredFrontier.Equipment.Widget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFEquipmentWidgetTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const TSubclassOf<USFEquipmentWidget> Class = LoadClass<USFEquipmentWidget>(nullptr, USFEquipmentWidget::AssetPath);
    if (!TestNotNull(TEXT("WBP_Equipment loads"), Class.Get()) || !World) { return false; }
    USFEquipmentWidget* Window = CreateWidget<USFEquipmentWidget>(World, Class);
    if (!TestNotNull(TEXT("WBP_Equipment constructs"), Window)) { return false; }
    for (const TCHAR* Name : {TEXT("ItemGrid"), TEXT("GridScroll"), TEXT("Preview"), TEXT("TabBar"), TEXT("Gold"), TEXT("WeightText"), TEXT("WeightBar"), TEXT("Tooltip"), TEXT("TipTitle")})
    { TestNotNull(FString::Printf(TEXT("%s bound"), Name), Window->GetWidgetFromName(Name)); }
    for (const TCHAR* Slot : {TEXT("Head"), TEXT("Chest"), TEXT("Hands"), TEXT("Legs"), TEXT("Feet"), TEXT("MainHand"), TEXT("OffHand"), TEXT("Back"), TEXT("Neck"), TEXT("Ring")})
    { TestNotNull(FString::Printf(TEXT("%s paper-doll slot"), Slot), Window->GetDollSlot(Slot)); }

    auto Entry = [](const TCHAR* Name, const TCHAR* Slot, bool bEquipped = false)
    {
        FSFInventoryEntry E; E.Name = FText::FromString(Name); E.Count = 1; E.bEquipped = bEquipped; E.UnitWeight = 1.f;
        E.EquipSlot = Slot ? FName(Slot) : NAME_None; E.ItemPath = FSoftObjectPath(FString::Printf(TEXT("/Game/Test/%s.%s"), Name, Name));
        E.Category = Slot ? ESFInventoryTab::Gear : ESFInventoryTab::Materials; return E;
    };
    const TArray<FSFInventoryEntry> Bag = {Entry(TEXT("Helm"), TEXT("Head")), Entry(TEXT("Sword"), TEXT("MainHand")), Entry(TEXT("Ring"), TEXT("Ring")), Entry(TEXT("Log"), nullptr)};
    TMap<FName, FSFInventoryEntry> Equipped; Equipped.Add(TEXT("MainHand"), Entry(TEXT("Axe"), TEXT("MainHand"), true));
    Window->Present(Bag, Equipped, 8, 4.f, 24.f);
    TestEqual(TEXT("One cell per shown item, no empty cells"), Window->CellCount(), 3);
    TestEqual(TEXT("Only the equippable items show"), Window->ShownCount(), 3);
    TestTrue(TEXT("A shown item can be dragged"), Window->GetCell(0) && Window->GetCell(0)->CanDrag());
    if (USFSlotWidget* Hand = Window->GetDollSlot(TEXT("MainHand"))) { TestTrue(TEXT("The equipped main hand can be dragged off"), Hand->CanDrag()); }
    if (USFSlotWidget* Head = Window->GetDollSlot(TEXT("Head")))
    {
        TestFalse(TEXT("An empty doll slot cannot be dragged"), Head->CanDrag());
        TestFalse(TEXT("An empty doll slot shows no icon (its ghost instead)"), Head->HasIcon());
        TestNotNull(TEXT("The doll slot has a ghost"), Head->GetWidgetFromName(TEXT("Ghost")));
    }
    TestEqual(TEXT("Weight line"), Window->GetWeightLine().ToString(), FString(TEXT("Weight 4.0 / 24 kg")));
    for (const TPair<ESFEquipmentTab, int32>& Tab : TArray<TPair<ESFEquipmentTab, int32>>{{ESFEquipmentTab::Weapons, 1}, {ESFEquipmentTab::Armor, 1}, {ESFEquipmentTab::Accessories, 1}})
    {
        Window->SelectTab(Tab.Key);
        Window->Present(Bag, Equipped, 8, 4.f, 24.f);
        TestEqual(FString::Printf(TEXT("%s tab"), *USFEquipmentWidget::TabLabel(Tab.Key)), Window->ShownCount(), Tab.Value);
    }

    const FSoftObjectPath Helm(TEXT("/Game/Test/Helm.Helm"));
    TestTrue(TEXT("A bag item onto its own slot equips"), USFEquipmentWidget::ResolveDrop(Helm, TEXT("Head"), NAME_None, TEXT("Head"), false) == ESFEquipDrop::Equip);
    TestTrue(TEXT("A bag item onto another slot does nothing"), USFEquipmentWidget::ResolveDrop(Helm, TEXT("Head"), NAME_None, TEXT("Chest"), false) == ESFEquipDrop::None);
    TestTrue(TEXT("A doll item onto the grid unequips"), USFEquipmentWidget::ResolveDrop(FSoftObjectPath(), NAME_None, TEXT("Head"), NAME_None, true) == ESFEquipDrop::Unequip);
    TestTrue(TEXT("A doll item onto a doll slot does nothing"), USFEquipmentWidget::ResolveDrop(FSoftObjectPath(), NAME_None, TEXT("Head"), TEXT("Chest"), false) == ESFEquipDrop::None);
    TestTrue(TEXT("A bag item onto the grid does nothing"), USFEquipmentWidget::ResolveDrop(Helm, TEXT("Head"), NAME_None, NAME_None, true) == ESFEquipDrop::None);
    TestTrue(TEXT("A bag item released off the grid is worn in its own slot"), USFEquipmentWidget::ResolveDrop(Helm, TEXT("Head"), NAME_None, NAME_None, false) == ESFEquipDrop::Equip);
    TestTrue(TEXT("A doll item released anywhere else unequips"), USFEquipmentWidget::ResolveDrop(FSoftObjectPath(), NAME_None, TEXT("Head"), NAME_None, false) == ESFEquipDrop::Unequip);
    TestTrue(TEXT("A non-equippable item does nothing"), USFEquipmentWidget::ResolveDrop(Helm, NAME_None, NAME_None, NAME_None, false) == ESFEquipDrop::None);
    return true;
}

#include "Tests/AutomationCommon.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFPlayerController.h"
#include "Gathering/SFGatheringPilot.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/SFInventory.h"

// Server-side equip validation on the real pilot inventory: slot match, ownership (only this player's bag), swap back to the bag, unequip to the
// bag and its space rule, and the gathering tools on the main hand.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFEquipmentValidationTest, "ShatteredFrontier.Equipment.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFEquipmentValidationTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    UWorld* World = Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC = World->SpawnActor<ASFPlayerController>();
    auto* Other = World->SpawnActor<ASFPlayerController>();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn = World->SpawnActor<ASFE1Character>(FVector(0, 0, 100), FRotator::ZeroRotator, Spawn);
    if (!PC || !Other || !Pawn) { return false; }
    PC->Possess(Pawn); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
    auto* Gear = PC->FindComponentByClass<USFGatheringPilotComponent>();
    auto* OtherGear = Other->FindComponentByClass<USFGatheringPilotComponent>();
    auto* Bag = PC->FindComponentByClass<USFInventoryComponent>();
    auto* OtherBag = Other->FindComponentByClass<USFInventoryComponent>();
    if (!Gear || !OtherGear || !Bag || !OtherBag) { return false; }
    for (USFGatheringPilotComponent* Component : {Gear, OtherGear})
    {
        Component->CapturedSettings = DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(), Component);
        Component->bEnabled = true; Component->bTestWorldAdmission = true;
    }
    // Test-only definitions (never assets): the slot is data on the definition.
    auto Define = [](const TCHAR* Name, FName Slot)
    {
        USFItemDefinition* Item = NewObject<USFItemDefinition>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), USFItemDefinition::StaticClass(), Name), RF_Transient);
        Item->UseType = Slot.IsNone() ? ESFItemUseType::None : ESFItemUseType::Equip;
        Item->EquipmentSlot = Slot; Item->MaxStackSize = 1; Item->Weight = 1.f; Item->DisplayName = FText::FromString(Name);
        return Item;
    };
    USFItemDefinition* Helm = Define(TEXT("SFTestHelm"), TEXT("Head"));
    USFItemDefinition* Cap = Define(TEXT("SFTestCap"), TEXT("Head"));
    USFItemDefinition* Ring = Define(TEXT("SFTestRing"), TEXT("Ring"));
    USFItemDefinition* Stone = Define(TEXT("SFTestStone"), NAME_None);
    auto Grant = [](USFInventoryComponent* Into, std::initializer_list<USFItemDefinition*> Items)
    {
        TArray<FSFItemQuantity> Adds;
        for (USFItemDefinition* Item : Items) { FSFItemQuantity Q; Q.Item = TSoftObjectPtr<USFItemDefinition>(Item); Q.Quantity = 1; Adds.Add(Q); }
        return SFInventoryTransactions::ExchangeInventoryItems(Into, {}, Adds) == ESFInventoryResult::Success;
    };
    if (!TestTrue(TEXT("Grant the test gear"), Grant(Bag, {Helm, Cap, Stone}) && Grant(OtherBag, {Ring}))) { return false; }
    const FSoftObjectPath HelmPath(Helm), CapPath(Cap), RingPath(Ring), StonePath(Stone);
    const FName Head(TEXT("Head"));

    TestEqual(TEXT("Wrong slot is refused"), Gear->ApplyEquip(HelmPath, TEXT("Chest")), ESFEquipResult::WrongSlot);
    TestEqual(TEXT("Unknown slot is refused"), Gear->ApplyEquip(HelmPath, TEXT("Tail")), ESFEquipResult::InvalidSlot);
    TestEqual(TEXT("Another player's item is refused"), Gear->ApplyEquip(RingPath, TEXT("Ring")), ESFEquipResult::NotOwned);
    TestTrue(TEXT("The other player's ring stays in their bag"), OtherGear->FindBagSlot(RingPath) != INDEX_NONE && OtherGear->GetEquipped(TEXT("Ring")).IsNull());
    TestEqual(TEXT("A non-equippable item is refused"), Gear->ApplyEquip(StonePath, Head), ESFEquipResult::NotEquippable);
    TestTrue(TEXT("Refusals changed nothing"), Gear->GetEquipped(Head).IsNull() && Gear->FindBagSlot(HelmPath) != INDEX_NONE);

    TestEqual(TEXT("Helm equips into Head"), Gear->ApplyEquip(HelmPath, Head), ESFEquipResult::Ok);
    TestTrue(TEXT("Head holds the helm"), Gear->GetEquipped(Head) == HelmPath);
    TestEqual(TEXT("The helm left the bag"), Gear->FindBagSlot(HelmPath), int32(INDEX_NONE));
    TestEqual(TEXT("Cap swaps in"), Gear->ApplyEquip(CapPath, Head), ESFEquipResult::Ok);
    TestTrue(TEXT("Head holds the cap"), Gear->GetEquipped(Head) == CapPath);
    TestTrue(TEXT("The swapped helm is back in the bag"), Gear->FindBagSlot(HelmPath) != INDEX_NONE);
    TestEqual(TEXT("Unequip to the bag"), Gear->ApplyUnequip(Head), ESFEquipResult::Ok);
    TestTrue(TEXT("Head is empty and the cap is in the bag"), Gear->GetEquipped(Head).IsNull() && Gear->FindBagSlot(CapPath) != INDEX_NONE);
    TestEqual(TEXT("Unequipping an empty slot is refused"), Gear->ApplyUnequip(Head), ESFEquipResult::NotEquipped);

    TestTrue(TEXT("Request path equips on the server"), Gear->RequestEquip(HelmPath, Head));
    const int32 Slots = Bag->NumSlots;
    Bag->NumSlots = 2;  // cap + stone fill it
    TestEqual(TEXT("Unequip into a full bag is refused"), Gear->ApplyUnequip(Head), ESFEquipResult::NoSpace);
    TestTrue(TEXT("The helm stays equipped"), Gear->GetEquipped(Head) == HelmPath);
    Bag->NumSlots = Slots;
    TestTrue(TEXT("Request path unequips"), Gear->RequestUnequip(Head) && Gear->GetEquipped(Head).IsNull());

    // Gathering tools are main-hand items through the same path.
    TestTrue(TEXT("Starter tools"), Gear->InitializeStarterTools());
    TestTrue(TEXT("Axe equips"), Gear->TryEquipTool(true));
    TestTrue(TEXT("The axe satisfies the oak tool rule"), Gear->HasRequiredTool(ESFGatheringPilotKind::Oak));
    TestEqual(TEXT("The axe is in the main hand"), Gear->GetEquipped(USFGatheringPilotComponent::MainHandSlot()).GetAssetName(), FString(TEXT("DA_IronAxe")));
    TestTrue(TEXT("Pickaxe swaps in"), Gear->TryEquipTool(false) && Gear->HasRequiredTool(ESFGatheringPilotKind::Iron) && !Gear->HasRequiredTool(ESFGatheringPilotKind::Oak));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Containers/Ticker.h"
#include "Inventory/SFGear.h"
#include "Inventory/SFInventoryLedgerClient.h"
#include "Services/SFBackpackComponent.h"

// With the ledger on, gear moves backpack <-> equipment container before the slot changes, and a re-read projects the
// equipment container back onto the slots (the restore path). A fake ledger holds both containers.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFEquipmentLedgerTest, "ShatteredFrontier.Equipment.Ledger", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFEquipmentLedgerTest::RunTest(const FString& Parameters)
{
    struct FContainer { int64 Revision = 0; TMap<FString, int64> Items; };
    TSharedRef<TMap<FString, FContainer>> Ledger = MakeShared<TMap<FString, FContainer>>();
    FSFInventoryLedgerClient::SetSendOverrideForTests([Ledger](const FSFLedgerRequest& R, float, TFunction<void(const FSFLedgerResponse&)> Done)
    {
        FSFLedgerResponse Out; Out.GrpcCode = 0; Out.Status = ESFLedgerStatus::Ok;
        if (R.Rpc == ESFLedgerRpc::Transfer)
        {
            FContainer& From = Ledger->FindOrAdd(R.ContainerId); FContainer& To = Ledger->FindOrAdd(R.ToContainerId);
            if (From.Revision != R.ExpectedRevision || To.Revision != R.ToExpectedRevision) { Out.Status = ESFLedgerStatus::Stale; Out.GrpcCode = 10; }
            else if (From.Items.FindRef(R.DefinitionId) < R.Quantity) { Out.Status = ESFLedgerStatus::Rejected; Out.GrpcCode = 9; }
            else { From.Items[R.DefinitionId] -= R.Quantity; To.Items.FindOrAdd(R.DefinitionId) += R.Quantity; Out.Revision = ++From.Revision; Out.ToRevision = ++To.Revision; }
        }
        else { const FContainer& View = Ledger->FindOrAdd(R.ContainerId); Out.Revision = View.Revision; Out.Quantities = View.Items; }
        Done(Out);
    });
    auto* Settings = DuplicateObject<USFInventoryLedgerSettings>(GetDefault<USFInventoryLedgerSettings>(), GetTransientPackage());
    Settings->bEnabled = true; Settings->Endpoint = TEXT("127.0.0.1:50071");
    TSharedPtr<FSFInventoryLedgerClient> Client = FSFInventoryLedgerClient::Create(*Settings);
    FSFInventoryLedgerClient::SetSendOverrideForTests(nullptr);

    FTestWorldWrapper Fixture;
    if (!TestTrue(TEXT("Stubbed client"), Client.IsValid()) || !Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    UWorld* World = Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC = World->SpawnActor<ASFPlayerController>();
    auto* Gear = PC ? PC->FindComponentByClass<USFGatheringPilotComponent>() : nullptr;
    auto* Bag = PC ? PC->FindComponentByClass<USFInventoryComponent>() : nullptr;
    if (!Gear || !Bag) { return false; }
    Gear->CapturedSettings = DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(), Gear);
    Gear->bEnabled = true; Gear->bTestWorldAdmission = true;
    USFBackpackComponent* Pack = USFBackpackComponent::CreateForTests(PC, Client);
    const auto Tick = [&Fixture](int32 Count) { for (int32 I = 0; I < Count; ++I) { Fixture.TickTestWorld(0.05f); FTSTicker::GetCoreTicker().Tick(0.3f); } };

    const FSoftObjectPath CapPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_HideCap.DA_HideCap"));
    FSFItemQuantity Cap; Cap.Item = TSoftObjectPtr<USFItemDefinition>(CapPath); Cap.Quantity = 1;
    if (!TestTrue(TEXT("The cap asset is in the bag"), SFInventoryTransactions::ExchangeInventoryItems(Bag, {}, {Cap}) == ESFInventoryResult::Success)) { return false; }
    Ledger->FindOrAdd(Pack->GetContainerId()).Items.Add(TEXT("item/hide_cap"), 1);
    const FString EquipmentId = Pack->GetEquipmentContainerId();
    const FName Head(TEXT("Head"));

    TestEqual(TEXT("Gear equip waits for the ledger"), Gear->ApplyEquip(CapPath, Head), ESFEquipResult::Pending);
    Tick(6);
    TestEqual(TEXT("The cap is worn after the transfer"), Gear->GetEquipped(Head), CapPath);
    TestEqual(TEXT("The ledger holds it as equipped"), Ledger->FindOrAdd(EquipmentId).Items.FindRef(TEXT("item/hide_cap")), int64(1));
    TestEqual(TEXT("Worn armour counts"), SFGear::Equipped(PC).Armor, 1);

    TestEqual(TEXT("Gear unequip waits for the ledger"), Gear->ApplyUnequip(Head), ESFEquipResult::Pending);
    Tick(6);
    TestTrue(TEXT("Unequipped back to the bag"), Gear->GetEquipped(Head).IsNull() && Gear->FindBagSlot(CapPath) != INDEX_NONE);
    TestEqual(TEXT("The ledger holds it in the backpack"), Ledger->FindOrAdd(Pack->GetContainerId()).Items.FindRef(TEXT("item/hide_cap")), int64(1));

    // Restore: a fresh session sees the cap in the equipment container; the re-read wears it again.
    Ledger->FindOrAdd(Pack->GetContainerId()).Items.Add(TEXT("item/hide_cap"), 0);
    Ledger->FindOrAdd(EquipmentId).Items.Add(TEXT("item/hide_cap"), 1);
    Pack->Reconcile(); Tick(6);
    TestEqual(TEXT("Reconcile wears the saved cap"), Gear->GetEquipped(Head), CapPath);
    TestEqual(TEXT("And the bag does not keep a copy"), Gear->FindBagSlot(CapPath), int32(INDEX_NONE));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Inventory/SFGear.h"
#include "Misc/AutomationTest.h"

// Gear stats come from content/items; graded ids share the base stats, armour never takes a hit below 1.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGearStatsTest, "ShatteredFrontier.Equipment.GearStats", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFGearStatsTest::RunTest(const FString& Parameters)
{
    const FSFGearStats* Jerkin = SFGear::Find(TEXT("item/hide_jerkin"));
    if (!TestNotNull(TEXT("Shipped gear loads"), Jerkin)) { return false; }
    TestEqual(TEXT("Jerkin is chest armour"), Jerkin->Slot, FName(TEXT("Chest")));
    TestEqual(TEXT("A graded id shares its base stats"), SFGear::Find(TEXT("item/hide_jerkin/fine")), Jerkin);
    TestNull(TEXT("Tools are not gear"), SFGear::Find(TEXT("item/iron_axe")));
    TestEqual(TEXT("Armour lowers a hit"), SFGear::MitigatedDamage(14, 5), 9);
    TestEqual(TEXT("A hit always lands for at least 1"), SFGear::MitigatedDamage(14, 40), 1);
    TMap<FString, FSFGearStats> Parsed; FString Error;
    TestFalse(TEXT("Negative armour is refused"), SFGear::Parse(TEXT("{\"gear\":[{\"definition_id\":\"item/x\",\"slot\":\"Head\",\"armor\":-1,\"damage\":0}]}"), Parsed, Error));
    TestFalse(TEXT("Duplicate ids are refused"), SFGear::Parse(TEXT("{\"gear\":[{\"definition_id\":\"item/x\",\"slot\":\"Head\",\"armor\":1,\"damage\":0},{\"definition_id\":\"item/x\",\"slot\":\"Head\",\"armor\":1,\"damage\":0}]}"), Parsed, Error));
    return true;
}
#endif
